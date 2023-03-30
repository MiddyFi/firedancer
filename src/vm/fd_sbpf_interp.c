#include "fd_sbpf_interp.h" 

#include <immintrin.h>

#define FD_MEM_MAP_PROGRAM_REGION_START   (0x100000000UL)
#define FD_MEM_MAP_STACK_REGION_START     (0x200000000UL)
#define FD_MEM_MAP_HEAP_REGION_START      (0x300000000UL)
#define FD_MEM_MAP_INPUT_REGION_START     (0x400000000UL)
#define FD_MEM_MAP_REGION_SZ              (0x0FFFFFFFFUL)
#define FD_MEM_MAP_REGION_MASK            (~FD_MEM_MAP_REGION_SZ)
#define FD_MEM_MAP_REGION_VIRT_ADDR_BITS  (32)

static ulong
fd_vm_sbpf_interp_translate_vm_to_host( fd_vm_sbpf_exec_context_t * ctx,
                                        uchar                       write,
                                        ulong                       vm_addr,
                                        ulong                       sz,
                                        void * *                   host_addr ) { 
  ulong mem_region = vm_addr & FD_MEM_MAP_REGION_MASK;
  ulong start_addr = vm_addr & FD_MEM_MAP_REGION_SZ;
  ulong end_addr = start_addr + sz;

  switch( mem_region ) {
    case FD_MEM_MAP_PROGRAM_REGION_START:
      if( write ) {
        return FD_VM_MEM_MAP_ERR_ACC_VIO;
      }
      if( end_addr >= ctx->read_only_sz ) {
        return FD_VM_MEM_MAP_ERR_ACC_VIO;
      }
      *host_addr = &ctx->read_only[start_addr];
      break;
    case FD_MEM_MAP_STACK_REGION_START:
    case FD_MEM_MAP_HEAP_REGION_START:
      if( end_addr >= FD_VM_HEAP_SZ ) {
        return FD_VM_MEM_MAP_ERR_ACC_VIO;
      }
      *host_addr = &ctx->heap[start_addr];
      break;
    case FD_MEM_MAP_INPUT_REGION_START:
      if( end_addr >= ctx->input_sz ) {
        return FD_VM_MEM_MAP_ERR_ACC_VIO;
      }
      *host_addr = &ctx->input[start_addr];
      break;
    default:
      return FD_VM_MEM_MAP_ERR_ACC_VIO;
  }

  return FD_VM_MEM_MAP_SUCCESS;
}

static ulong 
fd_vm_mem_map_read_uchar( fd_vm_sbpf_exec_context_t * ctx,
                          ulong              vm_addr,
                          uchar *            val ) {
  void * vm_mem;
  ulong translation_res = fd_vm_sbpf_interp_translate_vm_to_host(ctx, 0, vm_addr, sizeof(uchar), &vm_mem);
  if( translation_res != FD_VM_MEM_MAP_SUCCESS ) {
    return translation_res;
  }

  *val = *(uchar *)vm_mem;

  return FD_VM_MEM_MAP_SUCCESS;
}

static ulong 
fd_vm_mem_map_read_ushort( fd_vm_sbpf_exec_context_t * ctx,
                          ulong              vm_addr,
                          ushort *            val ) {
  void * vm_mem;
  ulong translation_res = fd_vm_sbpf_interp_translate_vm_to_host(ctx, 0, vm_addr, sizeof(ushort), &vm_mem);
  if( translation_res != FD_VM_MEM_MAP_SUCCESS ) {
    return translation_res;
  }

  *val = *(ushort *)vm_mem;

  return FD_VM_MEM_MAP_SUCCESS;
}

static ulong 
fd_vm_mem_map_read_uint( fd_vm_sbpf_exec_context_t * ctx,
                          ulong              vm_addr,
                          uint *            val ) {
  void * vm_mem;
  ulong translation_res = fd_vm_sbpf_interp_translate_vm_to_host(ctx, 0, vm_addr, sizeof(uint), &vm_mem);
  if( translation_res != FD_VM_MEM_MAP_SUCCESS ) {
    return translation_res;
  }

  *val = *(uint *)vm_mem;

  return FD_VM_MEM_MAP_SUCCESS;
}

static ulong 
fd_vm_mem_map_read_ulong( fd_vm_sbpf_exec_context_t * ctx,
                          ulong              vm_addr,
                          ulong *            val ) {
  void * vm_mem;
  ulong translation_res = fd_vm_sbpf_interp_translate_vm_to_host(ctx, 0, vm_addr, sizeof(ulong), &vm_mem);
  if( translation_res != FD_VM_MEM_MAP_SUCCESS ) {
    return translation_res;
  }

  *val = *(ulong *)vm_mem;

  return FD_VM_MEM_MAP_SUCCESS;
}

static ulong 
fd_vm_mem_map_write_uchar( fd_vm_sbpf_exec_context_t * ctx,
                          ulong              vm_addr,
                          uchar             val ) {
  void * vm_mem;
  ulong translation_res = fd_vm_sbpf_interp_translate_vm_to_host(ctx, 1, vm_addr, sizeof(uchar), &vm_mem);
  if( translation_res != FD_VM_MEM_MAP_SUCCESS ) {
    return translation_res;
  }

  *(uchar *)vm_mem = val;

  return FD_VM_MEM_MAP_SUCCESS;
}

static ulong 
fd_vm_mem_map_write_ushort( fd_vm_sbpf_exec_context_t * ctx,
                          ulong              vm_addr,
                          ushort             val ) {
  void * vm_mem;
  ulong translation_res = fd_vm_sbpf_interp_translate_vm_to_host(ctx, 1, vm_addr, sizeof(ushort), &vm_mem);
  if( translation_res != FD_VM_MEM_MAP_SUCCESS) {
    return translation_res;
  }

  *(ushort *)vm_mem = val;

  return FD_VM_MEM_MAP_SUCCESS;
}

static ulong 
fd_vm_mem_map_write_uint( fd_vm_sbpf_exec_context_t * ctx,
                          ulong              vm_addr,
                          uint             val ) {
  void * vm_mem;
  ulong translation_res = fd_vm_sbpf_interp_translate_vm_to_host(ctx, 1, vm_addr, sizeof(uint), &vm_mem);
  if( translation_res != FD_VM_MEM_MAP_SUCCESS) {
    return translation_res;
  }

  *(uint *)vm_mem = val;

  return FD_VM_MEM_MAP_SUCCESS;
}

static ulong 
fd_vm_mem_map_write_ulong( fd_vm_sbpf_exec_context_t * ctx,
                          ulong              vm_addr,
                          ulong             val ) {
  void * vm_mem;
  ulong translation_res = fd_vm_sbpf_interp_translate_vm_to_host(ctx, 1, vm_addr, sizeof(ulong), &vm_mem);
  if( translation_res != FD_VM_MEM_MAP_SUCCESS) {
    return translation_res;
  }

  *(ulong *)vm_mem = val;

  return FD_VM_MEM_MAP_SUCCESS;
}

void
fd_vm_sbpf_interp_instrs( fd_vm_sbpf_exec_context_t * ctx ) {
  long pc = ctx->entrypoint;
  ulong ic = ctx->instruction_counter;
  ulong * register_file = ctx->register_file;
  fd_memset(register_file, 0, sizeof(register_file));
  
  ulong cond_fault = 0;

#define ALIGNED_JMP_TAB_ID interp
#define ALIGNED_JMP_TAB_ALIGNMENT 32
#include "fd_aligned_jump_tab.c"
  
  fd_vm_sbpf_instr_t instr;
  ulong dst_reg;
  ulong src_reg;
  ulong imm; 
  static const long locs[256] = {
#include "fd_sbpf_interp_locs.c"  
  };

  FD_LOG_WARNING(( "BASE1 %lx", locs[0] ));
  for( ulong i = 1; i < 256; i++ ) {
    FD_LOG_WARNING(( "DIFF %02lx: %ld %lx %lx", i, locs[i] - locs[i-1], locs[i-1], locs[i]));
  }
  
    instr = ctx->instrs[pc];
    dst_reg = instr.dst_reg;
    src_reg = instr.src_reg;
    imm = instr.imm;

    goto *(&&AJT_BASE_LOC + locs[instr.opcode.raw]);
  /*for( ;; ) {
    if( pc >= (long) ctx->instrs_sz ) {
      break;
    }

    instr = ctx->instrs[pc];
    dst_reg = instr.dst_reg;
    src_reg = instr.src_reg;
    imm = instr.imm;

    goto *locs[instr.opcode.raw];
    //AJT_GOTO(instr.opcode.raw);
AJT_BREAK_LOC:
    ic++;
    pc++;
    
    continue;
  }*/

AJT_START;
#include "fd_sbpf_interp_dispatch_tab.c"
AJT_END;
  ctx->program_counter = (ulong) pc;
  ctx->instruction_counter = ic;

#include "fd_aligned_jump_tab_teardown.c"
#undef ALIGNED_JMP_TAB_ALIGNMENT

//  goto *locs[0];
//done:
  return;
}


#define FD_VALID        (0)
#define FD_CHECK_JMP    (1)
#define FD_CHECK_END    (2)
#define FD_CHECK_ST     (3)
#define FD_CHECK_LDQ    (4)
#define FD_CHECK_CALL   (5)
#define FD_INVALID      (6)

uchar const FD_OPCODE_VALIDATION_MAP[256] = {
  /* 0x00 */ FD_INVALID,    /* 0x01 */ FD_INVALID,    /* 0x02 */ FD_INVALID,    /* 0x03 */ FD_INVALID,
  /* 0x04 */ FD_VALID,      /* 0x05 */ FD_CHECK_JMP,  /* 0x06 */ FD_INVALID,    /* 0x07 */ FD_VALID,
  /* 0x08 */ FD_INVALID,    /* 0x09 */ FD_INVALID,    /* 0x0a */ FD_INVALID,    /* 0x0b */ FD_INVALID,
  /* 0x0c */ FD_VALID,      /* 0x0d */ FD_INVALID,    /* 0x0e */ FD_INVALID,    /* 0x0f */ FD_VALID,
  /* 0x10 */ FD_INVALID,    /* 0x11 */ FD_INVALID,    /* 0x12 */ FD_INVALID,    /* 0x13 */ FD_INVALID,   
  /* 0x14 */ FD_VALID,      /* 0x15 */ FD_CHECK_JMP,  /* 0x16 */ FD_CHECK_JMP,  /* 0x17 */ FD_VALID,   
  /* 0x18 */ FD_CHECK_LDQ,  /* 0x19 */ FD_INVALID,    /* 0x1a */ FD_INVALID,    /* 0x1b */ FD_INVALID,   
  /* 0x1c */ FD_VALID,      /* 0x1d */ FD_CHECK_JMP,  /* 0x1e */ FD_CHECK_JMP,  /* 0x1f */ FD_VALID,   
  /* 0x20 */ FD_INVALID,    /* 0x21 */ FD_INVALID,    /* 0x22 */ FD_INVALID,    /* 0x23 */ FD_INVALID,   
  /* 0x24 */ FD_VALID,      /* 0x25 */ FD_CHECK_JMP,  /* 0x26 */ FD_CHECK_JMP,  /* 0x27 */ FD_VALID,   
  /* 0x28 */ FD_INVALID,    /* 0x29 */ FD_INVALID,    /* 0x2a */ FD_INVALID,    /* 0x2b */ FD_INVALID,   
  /* 0x2c */ FD_VALID,      /* 0x2d */ FD_CHECK_JMP,  /* 0x2e */ FD_CHECK_JMP,  /* 0x2f */ FD_VALID,   
  /* 0x30 */ FD_INVALID,    /* 0x31 */ FD_INVALID,    /* 0x32 */ FD_INVALID,    /* 0x33 */ FD_INVALID, 
  /* 0x34 */ FD_VALID,      /* 0x35 */ FD_CHECK_JMP,  /* 0x36 */ FD_CHECK_JMP,  /* 0x37 */ FD_VALID,   
  /* 0x38 */ FD_INVALID,    /* 0x39 */ FD_INVALID,    /* 0x3a */ FD_INVALID,    /* 0x3b */ FD_INVALID,   
  /* 0x3c */ FD_VALID,      /* 0x3d */ FD_CHECK_JMP,  /* 0x3e */ FD_CHECK_JMP,  /* 0x3f */ FD_VALID,   
  /* 0x40 */ FD_INVALID,    /* 0x41 */ FD_INVALID,    /* 0x42 */ FD_INVALID,    /* 0x43 */ FD_INVALID,    
  /* 0x44 */ FD_VALID,      /* 0x45 */ FD_CHECK_JMP,  /* 0x46 */ FD_CHECK_JMP,  /* 0x47 */ FD_VALID,   
  /* 0x48 */ FD_INVALID,    /* 0x49 */ FD_INVALID,    /* 0x4a */ FD_INVALID,    /* 0x4b */ FD_INVALID,   
  /* 0x4c */ FD_VALID,      /* 0x4d */ FD_CHECK_JMP,  /* 0x4e */ FD_CHECK_JMP,  /* 0x4f */ FD_VALID,   
  /* 0x50 */ FD_INVALID,    /* 0x51 */ FD_INVALID,    /* 0x52 */ FD_INVALID,    /* 0x53 */ FD_INVALID,   
  /* 0x54 */ FD_VALID,      /* 0x55 */ FD_CHECK_JMP,  /* 0x56 */ FD_CHECK_JMP,  /* 0x57 */ FD_VALID,   
  /* 0x58 */ FD_INVALID,    /* 0x59 */ FD_INVALID,    /* 0x5a */ FD_INVALID,    /* 0x5b */ FD_INVALID,   
  /* 0x5c */ FD_VALID,      /* 0x5d */ FD_CHECK_JMP,  /* 0x5e */ FD_CHECK_JMP,  /* 0x5f */ FD_VALID,   
  /* 0x60 */ FD_INVALID,    /* 0x61 */ FD_VALID,      /* 0x62 */ FD_CHECK_ST,   /* 0x63 */ FD_CHECK_ST,   
  /* 0x64 */ FD_VALID,      /* 0x65 */ FD_CHECK_JMP,  /* 0x66 */ FD_CHECK_JMP,  /* 0x67 */ FD_VALID,   
  /* 0x68 */ FD_INVALID,    /* 0x69 */ FD_VALID,      /* 0x6a */ FD_CHECK_ST,   /* 0x6b */ FD_CHECK_ST,   
  /* 0x6c */ FD_VALID,      /* 0x6d */ FD_CHECK_JMP,  /* 0x6e */ FD_CHECK_JMP,  /* 0x6f */ FD_VALID,   
  /* 0x70 */ FD_INVALID,    /* 0x71 */ FD_VALID,      /* 0x72 */ FD_CHECK_ST,   /* 0x73 */ FD_CHECK_ST,   
  /* 0x74 */ FD_VALID,      /* 0x75 */ FD_CHECK_JMP,  /* 0x76 */ FD_CHECK_JMP,  /* 0x77 */ FD_VALID,   
  /* 0x78 */ FD_INVALID,    /* 0x79 */ FD_VALID,      /* 0x7a */ FD_CHECK_ST,   /* 0x7b */ FD_CHECK_ST,   
  /* 0x7c */ FD_VALID,      /* 0x7d */ FD_CHECK_JMP,  /* 0x7e */ FD_CHECK_JMP,  /* 0x7f */ FD_VALID,   
  /* 0x80 */ FD_INVALID,    /* 0x81 */ FD_INVALID,    /* 0x82 */ FD_INVALID,    /* 0x83 */ FD_INVALID,   
  /* 0x84 */ FD_VALID,      /* 0x85 */ FD_CHECK_CALL, /* 0x86 */ FD_INVALID,    /* 0x87 */ FD_VALID,   
  /* 0x88 */ FD_INVALID,    /* 0x89 */ FD_INVALID,    /* 0x8a */ FD_INVALID,    /* 0x8b */ FD_INVALID,   
  /* 0x8c */ FD_INVALID,    /* 0x8d */ FD_INVALID,    /* 0x8e */ FD_INVALID,    /* 0x8f */ FD_INVALID,   
  /* 0x90 */ FD_INVALID,    /* 0x91 */ FD_INVALID,    /* 0x92 */ FD_INVALID,    /* 0x93 */ FD_INVALID,   
  /* 0x94 */ FD_VALID,      /* 0x95 */ FD_VALID,      /* 0x96 */ FD_INVALID,    /* 0x97 */ FD_VALID,   
  /* 0x98 */ FD_INVALID,    /* 0x99 */ FD_INVALID,    /* 0x9a */ FD_INVALID,    /* 0x9b */ FD_INVALID,   
  /* 0x9c */ FD_VALID,      /* 0x9d */ FD_INVALID,    /* 0x9e */ FD_INVALID,    /* 0x9f */ FD_VALID,   
  /* 0xa0 */ FD_INVALID,    /* 0xa1 */ FD_INVALID,    /* 0xa2 */ FD_INVALID,    /* 0xa3 */ FD_INVALID,   
  /* 0xa4 */ FD_VALID,      /* 0xa5 */ FD_CHECK_JMP,  /* 0xa6 */ FD_CHECK_JMP,  /* 0xa7 */ FD_VALID,   
  /* 0xa8 */ FD_INVALID,    /* 0xa9 */ FD_INVALID,    /* 0xaa */ FD_INVALID,    /* 0xab */ FD_INVALID,   
  /* 0xac */ FD_VALID,      /* 0xad */ FD_CHECK_JMP,  /* 0xae */ FD_CHECK_JMP,  /* 0xaf */ FD_VALID,   
  /* 0xb0 */ FD_INVALID,    /* 0xb1 */ FD_INVALID,    /* 0xb2 */ FD_INVALID,    /* 0xb3 */ FD_INVALID,   
  /* 0xb4 */ FD_VALID,      /* 0xb5 */ FD_CHECK_JMP,  /* 0xb6 */ FD_CHECK_JMP,  /* 0xb7 */ FD_VALID,   
  /* 0xb8 */ FD_INVALID,    /* 0xb9 */ FD_INVALID,    /* 0xba */ FD_INVALID,    /* 0xbb */ FD_INVALID,   
  /* 0xbc */ FD_VALID,      /* 0xbd */ FD_CHECK_JMP,  /* 0xbe */ FD_CHECK_JMP,  /* 0xbf */ FD_VALID,   
  /* 0xc0 */ FD_INVALID,    /* 0xc1 */ FD_INVALID,    /* 0xc2 */ FD_INVALID,    /* 0xc3 */ FD_INVALID,   
  /* 0xc4 */ FD_VALID,      /* 0xc5 */ FD_CHECK_JMP,  /* 0xc6 */ FD_CHECK_JMP,  /* 0xc7 */ FD_VALID,   
  /* 0xc8 */ FD_INVALID,    /* 0xc9 */ FD_INVALID,    /* 0xca */ FD_INVALID,    /* 0xcb */ FD_INVALID,   
  /* 0xcc */ FD_VALID,      /* 0xcd */ FD_CHECK_JMP,  /* 0xce */ FD_CHECK_JMP,  /* 0xcf */ FD_VALID,   
  /* 0xd0 */ FD_INVALID,    /* 0xd1 */ FD_INVALID,    /* 0xd2 */ FD_INVALID,    /* 0xd3 */ FD_INVALID,   
  /* 0xd4 */ FD_CHECK_END,  /* 0xd5 */ FD_CHECK_JMP,  /* 0xd6 */ FD_CHECK_JMP,  /* 0xd7 */ FD_INVALID,   
  /* 0xd8 */ FD_INVALID,    /* 0xd9 */ FD_INVALID,    /* 0xda */ FD_INVALID,    /* 0xdb */ FD_INVALID,   
  /* 0xdc */ FD_CHECK_END,  /* 0xdd */ FD_CHECK_JMP,  /* 0xde */ FD_CHECK_JMP,  /* 0xdf */ FD_INVALID,   
  /* 0xe0 */ FD_INVALID,    /* 0xe1 */ FD_INVALID,    /* 0xe2 */ FD_INVALID,    /* 0xe3 */ FD_INVALID,   
  /* 0xe4 */ FD_INVALID,    /* 0xe5 */ FD_INVALID,    /* 0xe6 */ FD_INVALID,    /* 0xe7 */ FD_INVALID,   
  /* 0xec */ FD_INVALID,    /* 0xed */ FD_INVALID,    /* 0xee */ FD_INVALID,    /* 0xef */ FD_INVALID,   
  /* 0xf0 */ FD_INVALID,    /* 0xf1 */ FD_INVALID,    /* 0xf2 */ FD_INVALID,    /* 0xf3 */ FD_INVALID,   
  /* 0xf4 */ FD_INVALID,    /* 0xf5 */ FD_INVALID,    /* 0xf6 */ FD_INVALID,    /* 0xf7 */ FD_INVALID,   
  /* 0xf8 */ FD_INVALID,    /* 0xf9 */ FD_INVALID,    /* 0xfa */ FD_INVALID,    /* 0xfb */ FD_INVALID,   
  /* 0xfc */ FD_INVALID,    /* 0xfd */ FD_INVALID,    /* 0xfe */ FD_INVALID,    /* 0xff */ FD_INVALID,   
};

ulong 
fd_vm_sbpf_interp_validate( fd_vm_sbpf_exec_context_t const * ctx ) {
  for( ulong i = 0; i < ctx->instrs_sz; ++i ) {
    fd_vm_sbpf_instr_t instr = ctx->instrs[i];
    uchar validation_code = FD_OPCODE_VALIDATION_MAP[instr.opcode.raw];

    switch (validation_code) {
      case FD_VALID: 
        break;
      case FD_CHECK_JMP:
        if (instr.offset == -1) {
          return FD_VM_SBPF_VALIDATE_ERR_INF_LOOP;
        }
        long jmp_dst = (long)i + instr.offset + 1;
        if (jmp_dst < 0 || jmp_dst >= (long)ctx->instrs_sz) {
          return FD_VM_SBPF_VALIDATE_ERR_JMP_OUT_OF_BOUNDS;
        } else if (ctx->instrs[jmp_dst].opcode.raw == FD_BPF_OP_ADDL_IMM) {
          return FD_VM_SBPF_VALIDATE_ERR_JMP_TO_ADDL_IMM;
        }
        break;
      case FD_CHECK_END:
        if (instr.imm != 16 && instr.imm != 32 && instr.imm != 64) {
          return FD_VM_SBPF_VALIDATE_ERR_INVALID_END_IMM;
        }
        break;
      case FD_CHECK_ST:
        break;
      case FD_CHECK_LDQ:
        if (instr.src_reg != 0) {
          return FD_VM_SBPF_VALIDATE_ERR_INVALID_SRC_REG;
        }
        if ((i+1) >= ctx->instrs_sz) {
          return FD_VM_SBPF_VALIDATE_ERR_INCOMPLETE_LDQ;
        }
        if (ctx->instrs[i + 1].opcode.raw != FD_BPF_OP_ADDL_IMM) {
          return FD_VM_SBPF_VALIDATE_ERR_LDQ_NO_ADDL_IMM; 
        }
        ++i;
        break;
      case FD_CHECK_CALL:
        // TODO: CHECK CALL!
        if (instr.imm >= ctx->num_ext_funcs) {
          return FD_VM_SBPF_VALIDATE_ERR_NO_SUCH_EXT_CALL;
        }
        break;
      case FD_INVALID:
        // FD_LOG_WARNING(( "INVALID OPCODE: %u", instr.opcode.raw)); 
        return FD_VM_SBPF_VALIDATE_ERR_INVALID_OPCODE;
    }
    
    if (instr.src_reg > 10) {
      return FD_VM_SBPF_VALIDATE_ERR_INVALID_SRC_REG; 
    }
    
    int is_invalid_dst_reg = instr.dst_reg > ((validation_code == FD_CHECK_ST) ? 10 : 9);
    if (is_invalid_dst_reg) {
      return FD_VM_SBPF_VALIDATE_ERR_INVALID_DST_REG; 
    }
  }

  return FD_VM_SBPF_VALIDATE_SUCCESS;
}