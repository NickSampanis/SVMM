#ifndef PARALLEL_H
#define PARALLEL_H
#include <Windows.h>


/* defines for accessing the register bits */
#define LPT_STATUS_BUSY                0x80
#define LPT_STATUS_ACK                 0x40
#define LPT_STATUS_PAPER_OUT           0x20
#define LPT_STATUS_SELECT_IN           0x10
#define LPT_STATUS_ERROR               0x08
#define LPT_STATUS_IRQ                 0x04
#define LPT_STATUS_BIT1                0x02 /* reserved (only for completeness) */
#define LPT_STATUS_EPP_TIMEOUT         0x01

#define LPT_CONTROL_BIT7               0x80 /* reserved (only for completeness) */
#define LPT_CONTROL_BIT6               0x40 /* reserved (only for completeness) */
#define LPT_CONTROL_ENABLE_BIDIRECT    0x20
#define LPT_CONTROL_ENABLE_IRQ_VIA_ACK 0x10
#define LPT_CONTROL_SELECT_PRINTER     0x08
#define LPT_CONTROL_RESET              0x04
#define LPT_CONTROL_AUTO_LINEFEED      0x02
#define LPT_CONTROL_STROBE             0x01

/** mode defines for the extended control register */
#define LPT_ECP_ECR_CHIPMODE_MASK      0xe0
#define LPT_ECP_ECR_CHIPMODE_GET_BITS(reg) ((reg) >> 5)
#define LPT_ECP_ECR_CHIPMODE_SET_BITS(val) ((val) << 5)
#define LPT_ECP_ECR_CHIPMODE_CONFIGURATION 0x07
#define LPT_ECP_ECR_CHIPMODE_FIFO_TEST 0x06
#define LPT_ECP_ECR_CHIPMODE_RESERVED  0x05
#define LPT_ECP_ECR_CHIPMODE_EPP       0x04
#define LPT_ECP_ECR_CHIPMODE_ECP_FIFO  0x03
#define LPT_ECP_ECR_CHIPMODE_PP_FIFO   0x02
#define LPT_ECP_ECR_CHIPMODE_BYTE      0x01
#define LPT_ECP_ECR_CHIPMODE_COMPAT    0x00

/** FIFO status bits in extended control register */
#define LPT_ECP_ECR_FIFO_MASK          0x03
#define LPT_ECP_ECR_FIFO_SOME_DATA     0x00
#define LPT_ECP_ECR_FIFO_FULL          0x02
#define LPT_ECP_ECR_FIFO_EMPTY         0x01

#define LPT_ECP_CONFIGA_FIFO_WITDH_MASK 0x70
#define LPT_ECP_CONFIGA_FIFO_WIDTH_GET_BITS(reg) ((reg) >> 4)
#define LPT_ECP_CONFIGA_FIFO_WIDTH_SET_BITS(val) ((val) << 4)
#define LPT_ECP_CONFIGA_FIFO_WIDTH_16   0x00
#define LPT_ECP_CONFIGA_FIFO_WIDTH_32   0x20
#define LPT_ECP_CONFIGA_FIFO_WIDTH_8    0x10


#define LPT_ECP_FIFO_DEPTH 2

#define BX_PAR_DATA  0
#define BX_PAR_STAT  1
#define BX_PAR_CTRL  2


typedef struct _PARALLEL {
	BYTE	Data;
	BYTE	Status;
	BYTE	Control;
} PARALLEL;

VOID ParallelInitialize();
ULONG ParallelPortIoReadHandler(ULONG64 Address, ULONG Length);
VOID ParallelPortIoWriteHandler(ULONG64 Address, ULONG Value, ULONG Length);


#endif
