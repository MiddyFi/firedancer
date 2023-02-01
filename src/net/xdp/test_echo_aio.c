#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/if_xdp.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <linux/if_link.h>
#include <linux/types.h>
#include <poll.h>
#include <arpa/inet.h>
#include <math.h>
#include <time.h>

#include "fd_xdp.h"
#include "fd_xdp_private.h"
#include "fd_xdp_aio.h"

#define DEFAULT_COMP_RING_SIZE 32
#define DEFAULT_FILL_RING_SIZE 1024
#define DEFAULT_RX_RING_SIZE 1024
#define DEFAULT_TX_RING_SIZE 1024

#define LG_FRAME_SIZE 11
#define FRAME_SIZE (1<<LG_FRAME_SIZE)
#define NUM_FRAMES 2048

int64_t
gettime() {
  struct timespec ts;
  clock_gettime( CLOCK_REALTIME, &ts );

  return (int64_t)ts.tv_sec * (int64_t)1e9 + (int64_t)ts.tv_nsec;
}

typedef struct
  __attribute__((__packed__))
{
  uint8_t  eth_dst[6];
  uint8_t  eth_src[6];
  uint16_t eth_proto;

  // ip
  uint8_t  ip_hdrlen;
  uint8_t  ip_tos;
  uint16_t ip_tot_len;
  uint16_t ip_id;
  uint16_t ip_frag_off;
  uint8_t  ip_ttl;
  uint8_t  ip_proto;
  uint16_t ip_check;
  uint32_t ip_src;
  uint32_t ip_dst;

  // udp
  uint16_t udp_src;
  uint16_t udp_dst;
  uint16_t udp_len;
  uint16_t udp_check;

  // datagram
  uint8_t  text[64];
  uint8_t  pad[64];
  uint8_t  pad1[1024];
} packet_t;


void
calc_check( packet_t * pkt ) {
  uint64_t check = 0;

  check += pkt->ip_hdrlen;
  check += (uint32_t)pkt->ip_tos << (uint32_t)8;
  check += pkt->ip_tot_len;
  check += pkt->ip_id;
  check += pkt->ip_frag_off;
  check += pkt->ip_ttl;
  check += (uint32_t)pkt->ip_proto << (uint32_t)8;
  check += pkt->ip_src;
  check += pkt->ip_dst;

  pkt->ip_check = (uint16_t)( 0xffffu ^ ( check % 0xffffu ) );
}

void
calc_check2( packet_t * pkt ) {
#define STAGE(N) \
  uint32_t x##N = 0u; \
  memcpy( &x##N, (char*)&pkt->ip_hdrlen + (N<<2u), 4u )

  STAGE(0);
  STAGE(1);
  STAGE(2);
  STAGE(3);
  STAGE(4);

  uint64_t check0 = (uint64_t)x0 + (uint64_t)x1 + (uint64_t)x2 + (uint64_t)x3 + (uint64_t)x4;
  uint64_t check1 = ( check0 & 0xffffffffu ) + ( check0 >> 32u );
  uint64_t check2 = ( check1 & 0xffffu )     + ( check1 >> 16u );
  uint64_t check3 = ( check2 & 0xffffu )     + ( check2 >> 16u );

  if( check3 != 0xffffu ) {
    printf( "ip checksums don't match\n" );
    exit(1);
  }
}


size_t my_aio_cb_receive( void *            context,
                          fd_aio_buffer_t * batch,
                          size_t            batch_sz );


int
main( int argc, char **argv ) {
  char const * intf = "";
  float f_batch_sz  = 128;

  for( int i = 1; i < argc; ++i ) {
    // --intf
    if( strcmp( argv[i], "--intf" ) == 0 ) {
      if( i+1 < argc ) {
        intf = argv[i+1];
        i++;
        continue;
      } else {
        fprintf( stderr, "--intf requires a value\n" );
        exit(1);
      }
    }
    if( strcmp( argv[i], "--batch-sz" ) == 0 ) {
      if( i+1 < argc ) {
        f_batch_sz = strtof( argv[i+1], NULL );
      } else {
        fprintf( stderr, "--batch-sz requires a value\n" );
        exit(1);
      }
    }
  }

  uint64_t batch_sz = (uint64_t)roundf( f_batch_sz );

  printf( "xdp test parms:\n" );

  printf( "--intf %s\n", intf );
  printf( "--batch-sz %ld\n", batch_sz );

  fd_xdp_config_t config;
  fd_xdp_config_init( &config );

  config.bpf_pin_dir = "/sys/fs/bpf";
  config.bpf_pgm_file = "fd_xdp_bpf_udp.o";
  config.xdp_mode = XDP_FLAGS_SKB_MODE;
  //config.xdp_mode = XDP_FLAGS_DRV_MODE;
  //config.xdp_mode = XDP_FLAGS_HW_MODE;
  config.frame_size = FRAME_SIZE;

  void * xdp_mem = aligned_alloc( fd_xdp_align(), fd_xdp_footprint( &config ) );

  fd_xdp_t * xdp = fd_xdp_new( xdp_mem, intf, &config );

  if( !xdp ) {
    fprintf( stderr, "Failed to create fd_xdp. Aborting\n" );
    exit(1);
  }

  size_t aio_batch_sz = 32;

  void * xdp_aio_mem = aligned_alloc( fd_xdp_aio_align(), fd_xdp_aio_footprint( xdp, aio_batch_sz ) );
  fd_xdp_aio_t * xdp_aio = fd_xdp_aio_new( xdp_aio_mem, xdp, aio_batch_sz );
  if( !xdp_aio ) {
    fprintf( stderr, "Failed to create xdp_aio_mem\n" );
    exit(1);
  }

  // set up aio ingress and egress:
  //   fd_xdp_aio_egress_get
  //   fd_xdp_aio_ingress_set

  fd_aio_t ingress = { my_aio_cb_receive, (void*)xdp_aio };
  fd_xdp_aio_ingress_set( xdp_aio, &ingress );

  fd_xdp_add_key( xdp, 42421 );
  fd_xdp_add_key( xdp, 42423 );
  fd_xdp_add_key( xdp, 42425 );

  // wait for first RX
  printf( "Waiting for first packet\n" );
  printf( "  Don't forget ethtool\n" );
  printf( "    sudo ethtool -L enp59s0f1 combined 1\n" );
  fflush( stdout );

  while(1) {
    // service xdp
    fd_xdp_aio_service( xdp_aio );
  }

  // close down
  fd_xdp_delete( xdp );

  // free memory
  free( xdp_mem );

  return 0;
}


void
echo( fd_xdp_aio_t * xdp_aio, uchar const * raw_pkt, size_t raw_pkt_sz ) {
  packet_t const* rx_pkt = (packet_t const*)raw_pkt;
  packet_t        tx_pkt[1];

  fd_aio_t egress = xdp_aio->egress;

  /* copy packet into buffer */
  memcpy( tx_pkt, rx_pkt, raw_pkt_sz );

  /* switch eth, ip and udp, src and dst */
  memcpy(  tx_pkt->eth_src,  rx_pkt->eth_dst, 6 );
  memcpy(  tx_pkt->eth_dst,  rx_pkt->eth_src, 6 );
  memcpy( &tx_pkt->ip_src,  &rx_pkt->ip_dst,  4 );
  memcpy( &tx_pkt->ip_dst,  &rx_pkt->ip_src,  4 );

  /* make the ports the same */
  uint16_t port = htons( 42425u );
  memcpy( &tx_pkt->udp_dst, &port, 2 );
  memcpy( &tx_pkt->udp_src, &port, 2 );

  uint16_t udp_check = 0u; /* no udp checksum */
  memcpy( &tx_pkt->udp_check, &udp_check, 2 );

  /* calculate checksum */
  tx_pkt->ip_check = 0u;
  calc_check( tx_pkt );
  calc_check2( tx_pkt );

  /* send */
  fd_aio_buffer_t tx_buf[1] = {{ (void*)&tx_pkt, raw_pkt_sz }};
  
  size_t send_cnt = fd_aio_send( &egress, tx_buf, 1u );
  if( send_cnt ) {
    printf( "echo: send successful\n" );
  } else {
    printf( "echo: send failed\n" );
  }
}


/* callback for ingress
   echos back to sender */
size_t
my_aio_cb_receive( void *            context,
                   fd_aio_buffer_t * batch,
                   size_t            batch_sz ) {
  fd_xdp_aio_t * xdp_aio = (fd_xdp_aio_t*)context;

  for( size_t j = 0; j < batch_sz; ++j ) {
    echo( xdp_aio, batch[j].data, batch[j].data_sz );
  }

  return batch_sz;
}

