// Machine information registers
#define MVENDORID   0xf11
#define MARCHID     0xf12
#define MIMPID      0xf13
#define MHARTID     0xf14
#define MCONFIGPTR  0xf15

// Machine trap setup registers
#define MSTATUS     0x300
#define MSTATUSH    0x310
#define MISA        0x301
#define MEDELEG     0x302
#define MIDELEG     0x303
#define MIE         0x304
#define MTVEC       0x305

// Machine trap handling
#define MSCRATCH    0x340
#define MEPC        0x341
#define MCAUSE      0x342
#define MTVAL       0x343
#define MIP         0x344

// MSTATUS bits
#define MSTATUS_SIE_BIT     1
#define MSTATUS_MIE_BIT     3
#define MSTATUS_SPIE_BIT    5
#define MSTATUS_MPIE_BIT    7
#define MSTATUS_SPP_BIT     8
#define MSTATUS_MPP_BIT     11
#define MSTATUS_MPRV_BIT    17
#define MSTATUS_SUM_BIT     18
#define MSTATUS_MXR_BIT     19

// MIP bits
#define SSIP_BIT 1
#define MSIP_BIT 3
#define STIP_BIT 5
#define MTIP_BIT 7
#define SEIP_BIT 9
#define MEIP_BIT 11

// Bit Masks
#define MSTATUS_MASK    0x000ff9aa
#define SSTATUS_MASK    0x000de122
#define MEDELEG_MASK    0x0000b3ff
#define EPC_MASK        0xfffffffe
#define TVEC_MASK       0xfffffffd
#define SIE_SIP_MASK    0x00000222
#define MIE_MIP_MASK    0x00000aaa
#define SIP_SSIP_MASK   0x00000002

// Default values
#define MISA_DEFAULT        0x40141101
#define COUNTEREN_DEFAULT   0xffffffff

// Supervisor trap setup
#define SSTATUS     0x100
#define SIE         0x104
#define STVEC       0x105
#define SCOUNTEREN  0x106

// Supervisor trap handling
#define SSCRATCH    0x140
#define SEPC        0x141
#define SCAUSE      0x142
#define STVAL       0x143
#define SIP         0x144

// Supervisor protection and translation
#define SATP 0x180