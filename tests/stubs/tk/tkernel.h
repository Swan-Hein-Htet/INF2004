/* Minimal host-only RTOS API for executing the production startup path. */
#ifndef TEST_TKERNEL_H
#define TEST_TKERNEL_H
#include <stdint.h>
#define TK_SUPPORT_SMP 0
typedef int ID;
typedef int ER;
typedef int INT;
typedef int PRI;
typedef unsigned int UINT;
typedef uint32_t UW;
typedef unsigned char UB;
typedef void (*FP)(void);
typedef struct { UINT mtxatr; } T_CMTX;
typedef struct { FP task; PRI itskpri; UW stksz; UINT tskatr; } T_CTSK;
typedef struct { UINT cycatr; FP cychdr; UW cyctim; UW cycphs; } T_CCYC;
typedef struct { UINT intatr; FP inthdr; } T_DINT;
typedef struct { UW hi; UW lo; } SYSTIM;
#define E_OK 0
#define E_SYS (-5)
#define TMO_FEVR (-1)
#define TA_INHERIT 1U
#define TA_HLNG 1U
#define TA_RNG0 0U
#define DI(status) ((status) = 0U)
#define EI(status) ((void)(status))
ER tk_dly_tsk(UW delay);
ER tk_loc_mtx(ID id, INT timeout);
ER tk_unl_mtx(ID id);
ER tk_wup_tsk(ID id);
ER tk_can_wup(ID id);
ER tk_slp_tsk(INT timeout);
ER tk_get_tim(SYSTIM *time);
ID tk_cre_mtx(T_CMTX const *definition);
ID tk_cre_tsk(T_CTSK const *definition);
ER tk_sta_tsk(ID id, INT code);
ID tk_cre_cyc(T_CCYC const *definition);
ER tk_sta_cyc(ID id);
ER tk_def_int(UINT number, T_DINT const *definition);
#endif
