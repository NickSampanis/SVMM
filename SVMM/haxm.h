#ifndef BX_HAXM_H
#define BX_HAXM_H
#include <windows.h>
#include <winioctl.h>

#define ALIGNED(x) __declspec(align(x))
#define PACKED ALIGNED(1)




typedef signed char  int8_t;
typedef short int16_t;
typedef int   int32_t;
typedef long long int64_t;
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long long uint64_t;


// Read-only mapping, == HAX_RAM_INFO_ROM in hax_interface.h
#define HAX_MEMSLOT_READONLY (1 << 0)
// Stand-alone mapping, == HAX_RAM_INFO_STANDALONE in hax_interface.h
#define HAX_MEMSLOT_STANDALONE (1 << 6)

// Unmapped, == HAX_RAM_INFO_INVALID in hax_interface.h
// Not to be used by hax_memslot::flags
#define HAX_MEMSLOT_INVALID (1 << 7)

union interruptibility_state_t {
    uint32_t raw;
    struct {
        uint32_t sti_blocking : 1;
        uint32_t movss_blocking : 1;
        uint32_t smi_blocking : 1;
        uint32_t nmi_blocking : 1;
        uint32_t reserved : 28;
    };
    uint64_t pad;
} PACKED;

typedef union interruptibility_state_t interruptibility_state_t;

// Segment descriptor

struct segment_desc_t {
    uint16_t selector;
    uint16_t _dummy;
    uint32_t limit;
    uint64_t base;
    union {
        struct {
            uint32_t type : 4;
            uint32_t desc : 1;
            uint32_t dpl : 2;
            uint32_t present : 1;
            uint32_t : 4;
            uint32_t available : 1;
            uint32_t long_mode : 1;
            uint32_t operand_size : 1;
            uint32_t granularity : 1;
            uint32_t  : 1;
            uint32_t : 15;
        };
        uint32_t ar;
    };
    uint32_t ipad;
} PACKED;
typedef struct segment_desc_t segment_desc_t;

/* fx_layout has 3 formats table 3-56, 512bytes */
struct ALIGNED(16) fx_layout {
    uint16_t  fcw;
    uint16_t  fsw;
    uint8_t   ftw;
    uint8_t   res1;
    uint16_t  fop;
    union {
        struct {
            uint32_t  fip;
            uint16_t  fcs;
            uint16_t  res2;
        };
        uint64_t  fpu_ip;
    };
    union {
        struct {
            uint32_t  fdp;
            uint16_t  fds;
            uint16_t  res3;
        };
        uint64_t  fpu_dp;
    };
    uint32_t  mxcsr;
    uint32_t  mxcsr_mask;
    uint8_t   st_mm[8][16];
    uint8_t   mmx_1[8][16];
    uint8_t   mmx_2[8][16];
    uint8_t   pad[96];
};

struct vcpu_state_t {
    union {
        uint64_t _regs[16];
        struct {
            union {
                struct {
                    uint8_t _al,
                        _ah;
                };
                uint16_t    _ax;
                uint32_t    _eax;
                uint64_t    _rax;
            };
            union {
                struct {
                    uint8_t _cl,
                        _ch;
                };
                uint16_t    _cx;
                uint32_t    _ecx;
                uint64_t    _rcx;
            };
            union {
                struct {
                    uint8_t _dl,
                        _dh;
                };
                uint16_t    _dx;
                uint32_t    _edx;
                uint64_t    _rdx;
            };
            union {
                struct {
                    uint8_t _bl,
                        _bh;
                };
                uint16_t    _bx;
                uint32_t    _ebx;
                uint64_t    _rbx;
            };
            union {
                uint16_t    _sp;
                uint32_t    _esp;
                uint64_t    _rsp;
            };
            union {
                uint16_t    _bp;
                uint32_t    _ebp;
                uint64_t    _rbp;
            };
            union {
                uint16_t    _si;
                uint32_t    _esi;
                uint64_t    _rsi;
            };
            union {
                uint16_t    _di;
                uint32_t    _edi;
                uint64_t    _rdi;
            };

            uint64_t _r8;
            uint64_t _r9;
            uint64_t _r10;
            uint64_t _r11;
            uint64_t _r12;
            uint64_t _r13;
            uint64_t _r14;
            uint64_t _r15;
        };
    };

    union {
        uint32_t _eip;
        uint64_t _rip;
    };

    union {
        uint32_t _eflags;
        uint64_t _rflags;
    };

    segment_desc_t _cs;
    segment_desc_t _ss;
    segment_desc_t _ds;
    segment_desc_t _es;
    segment_desc_t _fs;
    segment_desc_t _gs;
    segment_desc_t _ldt;
    segment_desc_t _tr;

    segment_desc_t _gdt;
    segment_desc_t _idt;

    uint64_t _cr0;
    uint64_t _cr2;
    uint64_t _cr3;
    uint64_t _cr4;

    uint64_t _dr0;
    uint64_t _dr1;
    uint64_t _dr2;
    uint64_t _dr3;
    uint64_t _dr6;
    uint64_t _dr7;
    uint64_t _pde;

    uint32_t _efer;

    uint32_t _sysenter_cs;
    uint64_t _sysenter_eip;
    uint64_t _sysenter_esp;

    uint32_t _activity_state;
    uint32_t pad;
    interruptibility_state_t _interruptibility_state;
} PACKED;

struct hax_debug_t {
    uint32_t control;
    uint32_t reserved;
    uint64_t dr[8];
} PACKED;


struct vmx_msr {
    uint64_t entry;
    uint64_t value;
} PACKED;

struct hax_set_ram_info {
    uint64_t pa_start;
    uint32_t size;
    uint8_t flags;
    uint8_t pad[3];
    uint64_t va;
} PACKED;

struct hax_set_ram_info2 {
    uint64_t pa_start;
    uint64_t size;
    uint64_t va;
    uint32_t flags;
    uint32_t reserved1;
    uint64_t reserved2;
} PACKED;

// No access (R/W/X) is allowed
#define HAX_RAM_PERM_NONE 0x0
// All accesses (R/W/X) are allowed
#define HAX_RAM_PERM_RWX  0x7
#define HAX_RAM_PERM_MASK 0x7
struct hax_protect_ram_info {
    uint64_t pa_start;
    uint64_t size;
    uint32_t flags;
    uint32_t reserved;
} PACKED;

#define HAX_MAX_MSR_ARRAY 0x20
struct hax_msr_data {
    uint16_t nr_msr;
    uint16_t done;
    uint16_t pad[2];
    struct vmx_msr entries[HAX_MAX_MSR_ARRAY];
} PACKED;

struct hax_tunnel_info {
    uint64_t va;
    uint64_t io_va;
    uint16_t size;
    uint16_t pad[3];
} PACKED;

struct hax_alloc_ram_info {
    uint32_t size;
    uint32_t pad;
    uint64_t va;
} PACKED;

/* Common typedef for all platforms */
typedef uint64_t hax_pa_t;
typedef uint64_t hax_pfn_t;
typedef uint64_t hax_paddr_t;
typedef uint64_t hax_vaddr_t;

#define HAX_IO_OUT 0
#define HAX_IO_IN  1

struct Registers {
    struct vcpu_state_t context;
    struct hax_msr_data msrs;
    struct fx_layout fx;
    uint64_t xcr0;
    uint64_t cr8;
    uint64_t smbase;
    uint64_t tsc;
    uint64_t kernelgsbase;
    uint64_t apicbase;
    uint64_t pat;
    uint64_t star;
    uint64_t lstar;
    uint64_t cstar;
    uint64_t fmask;
    uint64_t tsc_aux;
};

/* The area to communicate with device model */
struct hax_tunnel {
    uint32_t _exit_reason;
    uint32_t pad0;
    uint32_t _exit_status;
    uint32_t user_event_pending;
    int ready_for_interrupt_injection;
    int request_interrupt_window;

    union {
        struct {
#define HAX_EXIT_IO_IN  1
#define HAX_EXIT_IO_OUT 0
            uint8_t _direction;
            uint8_t _df;
            uint16_t _size;
            uint16_t _port;
            uint16_t _count;
            /* Followed owned by HAXM, QEMU should not touch them */
            /* bit 1 is 1 means string io */
            uint8_t _flags;
            uint8_t _pad0;
            uint16_t _pad1;
            uint32_t _pad2;
            hax_vaddr_t _vaddr;
        } io;
        struct {
            hax_paddr_t gla;
        } mmio;
        struct {
            hax_paddr_t gpa;
#define HAX_PAGEFAULT_ACC_R  (1 << 0)
#define HAX_PAGEFAULT_ACC_W  (1 << 1)
#define HAX_PAGEFAULT_ACC_X  (1 << 2)
#define HAX_PAGEFAULT_PERM_R (1 << 4)
#define HAX_PAGEFAULT_PERM_W (1 << 5)
#define HAX_PAGEFAULT_PERM_X (1 << 6)
            uint32_t flags;
            uint32_t reserved1;
            uint64_t reserved2;
        } pagefault;
        struct {
            hax_paddr_t dummy;
        } state;
        struct {
            uint64_t rip;
            uint64_t dr6;
            uint64_t dr7;
        } debug;
    };
    uint64_t apic_base;
} PACKED;

struct hax_fastmmio {
    hax_paddr_t gpa;
    union {
        uint64_t value;
        hax_paddr_t gpa2;  /* since API v4 */
    };
    uint8_t size;
    uint8_t direction;
    uint16_t reg_index;  /* obsolete */
    uint32_t pad0;
    uint64_t _cr0;
    uint64_t _cr2;
    uint64_t _cr3;
    uint64_t _cr4;
} PACKED;

enum exit_status {
    HAX_EXIT_IO = 1,
    HAX_EXIT_MMIO,
    HAX_EXIT_REALMODE,
    HAX_EXIT_INTERRUPT,
    HAX_EXIT_UNKNOWN,
    HAX_EXIT_HLT,
    HAX_EXIT_STATECHANGE,
    HAX_EXIT_PAUSED,
    HAX_EXIT_FAST_MMIO,
    HAX_EXIT_PAGEFAULT,
    HAX_EXIT_DEBUG,
    HAX_EXIT_VMX_TIMER,
    HAX_EXIT_MTF,
    HAX_EXIT_RSM
};

enum run_flag {
    HAX_EXIT = 0,
    HAX_RESUME = 1
};

//MSRs
enum {
    IA32_P5_MC_ADDR = 0x0,
    IA32_P5_MC_TYPE = 0x1,
    IA32_TSC = 0x10,
    IA32_PLATFORM_ID = 0x17,
    IA32_APIC_BASE = 0x1b,
    IA32_EBC_HARD_POWERON = 0x2a,
    IA32_EBC_SOFT_POWERON = 0x2b,
    IA32_EBC_FREQUENCY_ID = 0x2c,
    IA32_FEATURE_CONTROL = 0x3a,
    IA32_THERM_DIODE_OFFSET = 0x3f,
    IA32_BIOS_UPDT_TRIG = 0x79,
    IA32_BIOS_SIGN_ID = 0x8b,
    IA32_SMM_MONITOR_CTL = 0x9b,
    IA32_PMC0 = 0xc1,
    IA32_PMC1 = 0xc2,
    IA32_PMC2 = 0xc3,
    IA32_PMC3 = 0xc4,
    IA32_FSB_FREQ = 0xcd,
    IA32_MPERF = 0xe7,
    IA32_APERF = 0xe8,
    IA32_TEMP_TARGET = 0xee,
    IA32_MTRRCAP = 0xfe,
    IA32_BBL_CR_CTL3 = 0x11e,
    IA32_SYSENTER_CS = 0x174,
    IA32_SYSENTER_ESP = 0x175,
    IA32_SYSENTER_EIP = 0x176,
    IA32_MCG_CAP = 0x179,
    IA32_MCG_STATUS = 0x17a,
    IA32_MCG_CTL = 0x17b,
    IA32_PERFEVTSEL0 = 0x186,
    IA32_PERFEVTSEL1 = 0x187,
    IA32_PERFEVTSEL2 = 0x188,
    IA32_PERFEVTSEL3 = 0x189,
    IA32_PERF_CTL = 0x199,
    IA32_MISC_ENABLE = 0x1a0,
    IA32_DEBUGCTL = 0x1d9,
    IA32_MTRR_PHYSBASE0 = 0x200,
    IA32_MTRR_PHYSMASK0 = 0x201,
    IA32_MTRR_PHYSBASE1 = 0x202,
    IA32_MTRR_PHYSMASK1 = 0x203,
    IA32_MTRR_PHYSBASE2 = 0x204,
    IA32_MTRR_PHYSMASK2 = 0x205,
    IA32_MTRR_PHYSBASE3 = 0x206,
    IA32_MTRR_PHYSMASK3 = 0x207,
    IA32_MTRR_PHYSBASE4 = 0x208,
    IA32_MTRR_PHYSMASK4 = 0x209,
    IA32_MTRR_PHYSBASE5 = 0x20a,
    IA32_MTRR_PHYSMASK5 = 0x20b,
    IA32_MTRR_PHYSBASE6 = 0x20c,
    IA32_MTRR_PHYSMASK6 = 0x20d,
    IA32_MTRR_PHYSBASE7 = 0x20e,
    IA32_MTRR_PHYSMASK7 = 0x20f,
    IA32_MTRR_PHYSBASE8 = 0x210,
    IA32_MTRR_PHYSMASK8 = 0x211,
    IA32_MTRR_PHYSBASE9 = 0x212,
    IA32_MTRR_PHYSMASK9 = 0x213,
    MTRRFIX64K_00000 = 0x250,
    MTRRFIX16K_80000 = 0x258,
    MTRRFIX16K_A0000 = 0x259,
    MTRRFIX4K_C0000 = 0x268,
    MTRRFIX4K_F8000 = 0x26f,
    IA32_CR_PAT = 0x277,
    IA32_MC0_CTL2 = 0x280,
    IA32_MC1_CTL2 = 0x281,
    IA32_MC2_CTL2 = 0x282,
    IA32_MC3_CTL2 = 0x283,
    IA32_MC4_CTL2 = 0x284,
    IA32_MC5_CTL2 = 0x285,
    IA32_MC6_CTL2 = 0x286,
    IA32_MC7_CTL2 = 0x287,
    IA32_MC8_CTL2 = 0x288,
    IA32_MTRR_DEF_TYPE = 0x2ff,
    MSR_BPU_COUNTER0 = 0x300,
    IA32_FIXED_CTR0 = 0x309,
    IA32_FIXED_CTR1 = 0x30a,
    IA32_FIXED_CTR2 = 0x30b,
    IA32_PERF_CAPABILITIES = 0x345,
    MSR_PEBS_MATRIX_VERT = 0x3f2,
    IA32_FIXED_CTR_CTRL = 0x38d,
    IA32_PERF_GLOBAL_STATUS = 0x38e,
    IA32_PERF_GLOBAL_CTRL = 0x38f,
    IA32_PERF_GLOBAL_OVF_CTRL = 0x390,
    IA32_MC0_CTL = 0x400,
    IA32_MC0_STATUS = 0x401,
    IA32_MC0_ADDR = 0x402,
    IA32_MC0_MISC = 0x403,
    IA32_CPUID_FEATURE_MASK = 0x478,
    IA32_VMX_BASIC = 0x480,
    IA32_VMX_PINBASED_CTLS = 0x481,
    IA32_VMX_PROCBASED_CTLS = 0x482,
    IA32_VMX_EXIT_CTLS = 0x483,
    IA32_VMX_ENTRY_CTLS = 0x484,
    IA32_VMX_MISC = 0x485,
    IA32_VMX_CR0_FIXED0 = 0x486,
    IA32_VMX_CR0_FIXED1 = 0x487,
    IA32_VMX_CR4_FIXED0 = 0x488,
    IA32_VMX_CR4_FIXED1 = 0x489,
    IA32_VMX_VMCS_ENUM = 0x48a,
    IA32_VMX_SECONDARY_CTLS = 0x48b,
    IA32_VMX_EPT_VPID_CAP = 0x48c,
    IA32_VMX_TRUE_PINBASED_CTLS = 0x48d,
    IA32_VMX_TRUE_PROCBASED_CTLS = 0x48e,
    IA32_VMX_TRUE_EXIT_CTLS = 0x48f,
    IA32_VMX_TRUE_ENTRY_CTLS = 0x490,
    IA32_EFER = 0xc0000080,
    IA32_STAR = 0xc0000081,
    IA32_LSTAR = 0xc0000082,
    IA32_CSTAR = 0xc0000083,
    IA32_SF_MASK = 0xc0000084,
    IA32_FS_BASE = 0xc0000100,
    IA32_GS_BASE = 0xc0000101,
    IA32_KERNEL_GS_BASE = 0xc0000102,
    IA32_TSC_AUX = 0xc0000103
};


// Set helper for bochs segments
#define SET_SEGMENT_FULL(name, bochs_seg) \
  bochs_seg.cache.u.segment.base = context->name.base;\
  bochs_seg.cache.u.segment.limit_scaled = context->name.limit;\
  bochs_seg.selector.value = context->name.selector;\
  bochs_seg.cache.type = context->name.type;\
  bochs_seg.cache.segment  = context->name.desc;\
  bochs_seg.cache.dpl = context->name.dpl;\
  bochs_seg.cache.p = context->name.present;\
  bochs_seg.cache.u.segment.avl = context->name.available;\
  bochs_seg.cache.u.segment.l = context->name.long_mode;\
  bochs_seg.cache.u.segment.d_b =  context->name.operand_size;\
  bochs_seg.cache.u.segment.g = context->name.granularity;\

// Segment getter helper
#define GET_SEGMENT_FULL(name, bochs_seg) \
  context->name.base       = bochs_seg.cache.u.segment.base;\
  context->name.limit      = bochs_seg.cache.u.segment.limit_scaled;\
  context->name.selector   = bochs_seg.selector.value;\
  context->name.type       = bochs_seg.cache.type;\
  context->name.desc       = bochs_seg.cache.segment;\
  context->name.dpl        = bochs_seg.cache.dpl;\
  context->name.present    = bochs_seg.cache.p;\
  context->name.available  = bochs_seg.cache.u.segment.avl;\
  context->name.long_mode  = bochs_seg.cache.u.segment.l;\
  context->name.operand_size  = bochs_seg.cache.u.segment.d_b;\
  context->name.granularity = bochs_seg.cache.u.segment.g;\


// Set helper for floating pointer registers
#define SET_FP_REG(name, bochs_idx) \
  BX_CPU_THIS_PTR the_i387.st_space[bochs_idx].fraction  = context->name.Fp.Mantissa;\
  BX_CPU_THIS_PTR the_i387.st_space[bochs_idx].exp       = context->name.Fp.BiasedExponent;\
  BX_CPU_THIS_PTR the_i387.st_space[bochs_idx].exp      |= (context->name.Fp.Sign << 15);

// Floating point register value getter
#define GET_FP_REG(name, bochs_idx) \
  context->name.Fp.Mantissa       = BX_READ_FPU_REG(bochs_idx).fraction;\
  context->name.Fp.BiasedExponent = BX_READ_FPU_REG(bochs_idx).exp & 0x7fff;\
  context->name.Fp.Sign           = (BX_READ_FPU_REG(bochs_idx).exp >> 15) & 1;

#define gpfn_to_g(gpfn) ((gpfn) >> 18)
#define gpfn_in_g(gpfn) ((gpfn) & 0x3ffff)
#define GPFN_MAP_ARRAY_SIZE (1 << 22)

#define HAX_DEBUG_ENABLE     (1 << 0)
#define HAX_DEBUG_STEP       (1 << 1)
#define HAX_DEBUG_USE_SW_BP  (1 << 2)
#define HAX_DEBUG_USE_HW_BP  (1 << 3)

#define HAX_DEVICE_TYPE 0x4000

#define HAX_IOCTL_VERSION \
        CTL_CODE(HAX_DEVICE_TYPE, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_IOCTL_CREATE_VM \
        CTL_CODE(HAX_DEVICE_TYPE, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_IOCTL_CAPABILITY \
        CTL_CODE(HAX_DEVICE_TYPE, 0x910, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_IOCTL_SET_MEMLIMIT \
        CTL_CODE(HAX_DEVICE_TYPE, 0x911, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HAX_VM_IOCTL_VCPU_CREATE \
        CTL_CODE(HAX_DEVICE_TYPE, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VM_IOCTL_ALLOC_RAM \
        CTL_CODE(HAX_DEVICE_TYPE, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VM_IOCTL_SET_RAM \
        CTL_CODE(HAX_DEVICE_TYPE, 0x904, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VM_IOCTL_VCPU_DESTROY \
        CTL_CODE(HAX_DEVICE_TYPE, 0x905, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VM_IOCTL_ADD_RAMBLOCK \
        CTL_CODE(HAX_DEVICE_TYPE, 0x913, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VM_IOCTL_SET_RAM2 \
        CTL_CODE(HAX_DEVICE_TYPE, 0x914, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VM_IOCTL_PROTECT_RAM \
        CTL_CODE(HAX_DEVICE_TYPE, 0x915, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HAX_VCPU_IOCTL_RUN \
        CTL_CODE(HAX_DEVICE_TYPE, 0x906, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_IOCTL_SET_MSRS \
        CTL_CODE(HAX_DEVICE_TYPE, 0x907, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_IOCTL_GET_MSRS \
        CTL_CODE(HAX_DEVICE_TYPE, 0x908, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HAX_VCPU_IOCTL_SET_FPU \
        CTL_CODE(HAX_DEVICE_TYPE, 0x909, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_IOCTL_GET_FPU \
        CTL_CODE(HAX_DEVICE_TYPE, 0x90a, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HAX_VCPU_IOCTL_SETUP_TUNNEL \
        CTL_CODE(HAX_DEVICE_TYPE, 0x90b, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_IOCTL_INTERRUPT \
        CTL_CODE(HAX_DEVICE_TYPE, 0x90c, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_SET_REGS \
        CTL_CODE(HAX_DEVICE_TYPE, 0x90d, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_GET_REGS \
        CTL_CODE(HAX_DEVICE_TYPE, 0x90e, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_IOCTL_KICKOFF \
        CTL_CODE(HAX_DEVICE_TYPE, 0x90f, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* API version 2.0 */
#define HAX_VM_IOCTL_NOTIFY_QEMU_VERSION \
        CTL_CODE(HAX_DEVICE_TYPE, 0x910, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HAX_VCPU_IOCTL_SMI \
        CTL_CODE(HAX_DEVICE_TYPE, 0x919, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HAX_IOCTL_VCPU_DEBUG \
        CTL_CODE(HAX_DEVICE_TYPE, 0x916, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_IOCTL_SET_CPUID \
        CTL_CODE(HAX_DEVICE_TYPE, 0x917, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define HAX_VCPU_IOCTL_GET_CPUID \
        CTL_CODE(HAX_DEVICE_TYPE, 0x918, METHOD_BUFFERED, FILE_ANY_ACCESS)



void haxm_to_bochs_context(const struct vcpu_state_t* context, const struct fx_layout* fx);
void bochs_to_haxm_context(struct vcpu_state_t* context, struct fx_layout* fx);
int haxm_init(void* ram, size_t ram_size);


#endif