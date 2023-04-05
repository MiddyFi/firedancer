#include "fd_frank.h"

#include "../../disco/quic/fd_quic.h"
#include "../../tango/xdp/fd_xdp.h"
#include "../../util/net/fd_eth.h"
#include "../../util/net/fd_ip4.h"

int
fd_frank_quic_task( int     argc,
                    char ** argv ) {
  (void)argc;
  fd_log_thread_set( argv[0] );
  char const * quic_name = argv[0];
  FD_LOG_INFO(( "quic.%s init", quic_name ));

  /* Parse "command line" arguments */

  char const * pod_gaddr = argv[1];
  char const * cfg_path  = argv[2];

  /* Load up the configuration for this frank instance */

  FD_LOG_INFO(( "using configuration in pod %s at path %s", pod_gaddr, cfg_path ));
  uchar const * pod     = fd_wksp_pod_attach( pod_gaddr );
  uchar const * cfg_pod = fd_pod_query_subpod( pod, cfg_path );
  if( FD_UNLIKELY( !cfg_pod ) ) FD_LOG_ERR(( "path %s not found", cfg_path ));

  uchar const * quic_pods = fd_pod_query_subpod( cfg_pod, "quic" );
  if( FD_UNLIKELY( !quic_pods ) ) FD_LOG_ERR(( "%s.quic path not found", cfg_path ));

  uchar const * quic_pod = fd_pod_query_subpod( quic_pods, quic_name );
  if( FD_UNLIKELY( !quic_pod ) ) FD_LOG_ERR(( "%s.quic.%s path not found", cfg_path, quic_name ));

  uchar const * quic_cfg_pod = fd_pod_query_subpod( cfg_pod, "quic_cfg" );
  if( FD_UNLIKELY( !quic_cfg_pod ) ) FD_LOG_ERR(( "%s.quic_cfg path not found", cfg_path ));

  /* Join the IPC objects needed by this tile instance */

  FD_LOG_INFO(( "joining %s.quic.%s.cnc", cfg_path, quic_name ));
  fd_cnc_t * cnc = fd_cnc_join( fd_wksp_pod_map( quic_pod, "cnc" ) );
  if( FD_UNLIKELY( !cnc ) ) FD_LOG_ERR(( "fd_cnc_join failed" ));
  if( FD_UNLIKELY( fd_cnc_signal_query( cnc )!=FD_CNC_SIGNAL_BOOT ) ) FD_LOG_ERR(( "cnc not in boot state" ));

  FD_LOG_INFO(( "joining %s.quic.%s.mcache", cfg_path, quic_name ));
  fd_frag_meta_t * mcache = fd_mcache_join( fd_wksp_pod_map( quic_pod, "mcache" ) );
  if( FD_UNLIKELY( !mcache ) ) FD_LOG_ERR(( "fd_mcache_join failed" ));

  FD_LOG_INFO(( "joining %s.quic.%s.dcache", cfg_path, quic_name ));
  uchar * dcache = fd_dcache_join( fd_wksp_pod_map( quic_pod, "dcache" ) );
  if( FD_UNLIKELY( !dcache ) ) FD_LOG_ERR(( "fd_dcache_join failed" ));

  FD_LOG_INFO(( "loading %s.quic.%s.quic", cfg_path, quic_name ));
  fd_quic_t * quic = (fd_quic_t *)fd_wksp_pod_map( quic_pod, "quic" );
  if( FD_UNLIKELY( !quic ) ) FD_LOG_ERR(( "fd_wksp_pod_map quic failed" ));

  FD_LOG_INFO(( "loading %s.quic.%s.xsk", cfg_path, quic_name ));
  fd_xsk_t * xsk = fd_xsk_join( fd_wksp_pod_map( quic_pod, "xsk") );
  if( FD_UNLIKELY( !xsk ) ) FD_LOG_ERR(( "fd_xsk_join failed" ));

  FD_LOG_INFO(( "loading %s.quic.%s.xsk_aio", cfg_path, quic_name ));
  fd_xsk_aio_t * xsk_aio = fd_xsk_aio_join( fd_wksp_pod_map( quic_pod, "xsk_aio" ), xsk );
  if( FD_UNLIKELY( !xsk_aio ) ) FD_LOG_ERR(( "fd_xsk_aio_join failed" ));

  /* Setup local objects used by this tile */

  FD_LOG_INFO(( "configuring flow control" ));
  ulong cr_max    = fd_pod_query_ulong( quic_pod, "cr_max",    0UL );
  ulong cr_resume = fd_pod_query_ulong( quic_pod, "cr_resume", 0UL );
  ulong cr_refill = fd_pod_query_ulong( quic_pod, "cr_refill", 0UL );
  long  lazy      = fd_pod_query_long ( quic_pod, "lazy",      0L  );
  FD_LOG_INFO(( "%s.quic.%s.cr_max    %lu", cfg_path, quic_name, cr_max    ));
  FD_LOG_INFO(( "%s.quic.%s.cr_resume %lu", cfg_path, quic_name, cr_resume ));
  FD_LOG_INFO(( "%s.quic.%s.cr_refill %lu", cfg_path, quic_name, cr_refill ));
  FD_LOG_INFO(( "%s.quic.%s.lazy      %li", cfg_path, quic_name, lazy      ));

  uint seed = fd_pod_query_uint( cfg_pod, "dedup.seed", (uint)fd_tile_id() ); /* use app tile_id as default */
  FD_LOG_INFO(( "creating rng (%s.dedup.seed %u)", cfg_path, seed ));
  fd_rng_t _rng[ 1 ];
  fd_rng_t * rng = fd_rng_join( fd_rng_new( _rng, seed, 0UL ) );
  if( FD_UNLIKELY( !rng ) ) FD_LOG_ERR(( "fd_rng_join failed" ));

  FD_LOG_INFO(( "creating scratch" ));
  ulong footprint = fd_quic_tile_scratch_footprint( fd_mcache_depth( mcache ) );
  if( FD_UNLIKELY( !footprint ) ) FD_LOG_ERR(( "fd_quic_tile_scratch_footprint failed" ));
  void * scratch = fd_alloca( FD_QUIC_TILE_SCRATCH_ALIGN, footprint );
  if( FD_UNLIKELY( !scratch ) ) FD_LOG_ERR(( "fd_alloca failed" ));

  /* Configure QUIC server */

  fd_quic_config_t * quic_cfg = fd_quic_get_config( quic );
  if( FD_UNLIKELY( !quic_cfg ) ) FD_LOG_ERR(( "failed to configure QUIC" ));

  char const * cert_file = fd_pod_query_cstr( quic_cfg_pod, "cert_file", NULL );
  if( FD_UNLIKELY( !cert_file ) ) FD_LOG_ERR(( "%s.quic_cfg.cert_file not set", cfg_path ));
  char const * key_file  = fd_pod_query_cstr( quic_cfg_pod, "key_file",  NULL );
  if( FD_UNLIKELY( !key_file  ) ) FD_LOG_ERR(( "%s.quic_cfg.key_file not set", cfg_path ));
  char const * keylog_file = fd_pod_query_cstr( quic_cfg_pod, "keylog_file", NULL ); /* optional */

  strncpy( quic_cfg->cert_file, cert_file, FD_QUIC_CERT_PATH_LEN );
  strncpy( quic_cfg->key_file,  key_file,  FD_QUIC_CERT_PATH_LEN );
  strncpy( quic_cfg->keylog_file, keylog_file ? keylog_file : "", FD_QUIC_CERT_PATH_LEN );

  /* TODO read IP addresses from interface instead? */
  char const * ip_addr_cstr = fd_pod_query_cstr( quic_cfg_pod, "ip_addr", NULL );
  if( FD_UNLIKELY( !ip_addr_cstr ) ) FD_LOG_ERR(( "%s.quic_cfg.ip_addr not set", cfg_path ));
  if( FD_UNLIKELY( !fd_cstr_to_ip4_addr( ip_addr_cstr, &quic_cfg->net.ip_addr ) ) )
    FD_LOG_ERR(( "%s.quic_cfg.ip_addr is invalid (\"%s\")", cfg_path, ip_addr_cstr ));

  /* TODO read MAC address from interface instead? */
  char const * src_mac_addr_cstr = fd_pod_query_cstr( quic_cfg_pod, "src_mac_addr", NULL );
  if( FD_UNLIKELY( !src_mac_addr_cstr ) ) FD_LOG_ERR(( "%s.quic_cfg.src_mac_addr not set", cfg_path ));
  if( FD_UNLIKELY( !fd_cstr_to_mac_addr( src_mac_addr_cstr, quic_cfg->link.src_mac_addr ) ) )
    FD_LOG_ERR(( "%s.quic_cfg.src_mac_addr is invalid (\"%s\")", cfg_path, src_mac_addr_cstr ));

  /* TODO implement ARP */
  char const * dst_mac_addr_cstr = fd_pod_query_cstr( quic_cfg_pod, "dst_mac_addr", NULL );
  if( FD_UNLIKELY( !dst_mac_addr_cstr ) ) FD_LOG_ERR(( "%s.quic_cfg.dst_mac_addr not set", cfg_path ));
  if( FD_UNLIKELY( !fd_cstr_to_mac_addr( dst_mac_addr_cstr, quic_cfg->link.dst_mac_addr ) ) )
    FD_LOG_ERR(( "%s.quic_cfg.dst_mac_addr is invalid (\"%s\")", cfg_path, dst_mac_addr_cstr ));

  ushort listen_port = fd_pod_query_ushort( quic_cfg_pod, "listen_port", 0 );
  if( FD_UNLIKELY( !listen_port ) ) FD_LOG_ERR(( "%s.quic_cfg.listen_port not set", cfg_path ));
  quic_cfg->net.listen_udp_port = listen_port;

  ulong idle_timeout_ms = fd_pod_query_ulong( quic_cfg_pod, "idle_timeout_ms", 0 );
  if( FD_UNLIKELY( !idle_timeout_ms ) ) FD_LOG_ERR(( "%s.quic_cfg.idle_timeout_ms not set", cfg_path ));
  quic_cfg->idle_timeout = idle_timeout_ms * 1000000UL;

  /* Attach to XSK */

  fd_aio_t _rx[1];
  fd_quic_get_aio_net_rx( quic, _rx );
  fd_xsk_aio_set_rx( xsk_aio, _rx );
  fd_quic_set_aio_net_tx( quic, fd_xsk_aio_get_tx( xsk_aio ) );

  /* Start serving */

  FD_LOG_INFO(( "%s run", quic_name ));
  int err = fd_quic_tile( cnc, quic, xsk_aio, mcache, dcache, lazy, rng, scratch );
  if( FD_UNLIKELY( err ) ) FD_LOG_ERR(( "fd_quic_tile failed (%i)", err ));

  /* Clean up */

  FD_LOG_INFO(( "%s fini", quic_name ));
  fd_rng_delete    ( fd_rng_leave    ( rng     ) );
  fd_wksp_pod_unmap(                   quic      );
  fd_wksp_pod_unmap( fd_xsk_aio_leave( xsk_aio ) );
  fd_wksp_pod_unmap( fd_xsk_leave    ( xsk     ) );
  fd_wksp_pod_unmap( fd_mcache_leave ( mcache  ) );
  fd_wksp_pod_unmap( fd_dcache_leave ( dcache  ) );
  fd_wksp_pod_unmap( fd_cnc_leave    ( cnc     ) );
  fd_wksp_pod_detach( pod );
  return 0;
}