/*
*********************************************************************************************************
*                                                uC/OS-III
*                                          The Real-Time Kernel
*
*
*                           (c) Copyright 2009-2010; Micrium, Inc.; Weston, FL
*                    All rights reserved.  Protected by international copyright laws.
*
*                                                x86  Port
*
* File      : OS_CPU_C.C
* Version   : V3.01.2
* By        : JJL
*             BAN
*
* LICENSING TERMS:
* ---------------
*             uC/OS-III is provided in source form to registered licensees ONLY.  It is 
*             illegal to distribute this source code to any third party unless you receive 
*             written permission by an authorized Micrium representative.  Knowledge of 
*             the source code may NOT be used to develop a similar product.
*
*             Please help us continue to provide the Embedded community with the finest
*             software available.  Your honesty is greatly appreciated.
*
*             You can contact us at www.micrium.com.
*
* For       : x86
* Toolchain : BC45
* Modify    : 591881218@qq.com
*********************************************************************************************************
*/

#if 1
#define   OS_CPU_GLOBALS
#endif

#ifdef VSC_INCLUDE_SOURCE_FILE_NAMES
const  CPU_CHAR  *os_cpu_c__c = "$Id: $";
#endif

#include <os.h>
#include <dos.h>
#include <pc.h>


/*$PAGE*/
/*
*********************************************************************************************************
*                                             LOCAL CONSTANTS
*
* Note(s) : 1) OS_NTASKS_FP  establishes the number of tasks capable of supporting floating-point.  One
*              task is removed for the idle task because it doesn't do floating-point at all.
*           2) OS_FP_STORAGE_SIZE  currently allocates 128 bytes of storage even though the 80x86 FPU 
*              only require 108 bytes to save the FPU context.  I decided to allocate 128 bytes for 
*              future expansion.  
*********************************************************************************************************
*/

#define  OS_MAX_TASKS         64u      /* #define  OS_CFG_PRIO_MAX      64u                            */
#define  OS_N_SYS_TASKS       5u
#define  OS_NTASKS_FP         OS_MAX_TASKS + OS_N_SYS_TASKS - 1u
#define  OS_FP_STORAGE_SIZE   128u

#ifndef  OS_TASK_OPT_SAVE_FP
#define  OS_TASK_OPT_SAVE_FP  0x0004   /* Save the contents of any floating-point registers            */    
#endif

/*
*********************************************************************************************************
*                                             LOCAL VARIABLES
*********************************************************************************************************
*/

static  OS_MEM  *OSFPPartPtr;          /* Pointer to memory partition holding FPU storage areas        */

                                       /* I used CPU_INT32U to ensure that storage is aligned on a ... */
                                       /* ... 32-bit boundary.                                         */
static  CPU_INT32U   OSFPPart[OS_NTASKS_FP][OS_FP_STORAGE_SIZE / sizeof(CPU_INT32U)];


/*$PAGE*/
/*
*********************************************************************************************************
*                                        INITIALIZE FP SUPPORT
*
* Description: This function is called to initialize the memory partition needed to support context
*              switching the Floating-Point registers.  This function MUST be called AFTER calling
*              OSInit().
*
* Arguments  : none
*
* Returns    : none
*
* Note(s)    : 1) Tasks that are to use FP support MUST be created with OSTaskCreateExt().
*              2) For the 80x86 FPU, 108 bytes are required to save the FPU context.  I decided to
*                 allocate 128 bytes for future expansion.  Also, I used CPU_INT32U to ensure that storage 
*                 is aligned on a 32-bit boundary.
*              3) I decided to 'change' the 'Options' attribute for the statistic task in case you
*                 use OSTaskStatHook() and need to perform floating-point operations in this function.
*                 This only applies if OS_TaskStat() was created with OSTaskCreateExt().
*********************************************************************************************************
*/

void  OSFPInit (void)
{
    OS_ERR   err;
#if OS_CFG_STAT_TASK_EN > 0u
    OS_TCB  *ptcb;
    void    *pblk;
#endif


    OSMemCreate(OSFPPartPtr, "OS FP Part", &OSFPPart[0][0], OS_NTASKS_FP, OS_FP_STORAGE_SIZE, &err);
    
#if OS_CFG_STAT_TASK_EN > 0u                       /* CHANGE 'OPTIONS' for OS_TaskStat()                */
    ptcb            = (OS_TCB  *)&OSStatTaskTCB;
    ptcb->Opt      |= OS_TASK_OPT_SAVE_FP;         /* Allow floating-point support for Statistic task   */
    pblk            = OSMemGet(OSFPPartPtr, &err); /* Get storage for FPU registers                     */
    if (pblk != (void *)0) {                       /* Did we get a memory block?                        */
        ptcb->ExtPtr      = pblk;                  /* Yes, Link to task's TCB                           */
        OSFPSave(pblk);                            /*      Save the FPU registers in block              */
    }
#endif
}


/*$PAGE*/
/*
*********************************************************************************************************
*                                        INITIALIZE A TASK'S STACK
*
* Description: This function is called by either OSTaskCreate() or OSTaskCreateExt() to initialize the
*              stack frame of the task being created.  This function is highly processor specific.
*
* Arguments  : task          is a pointer to the task code
*
*              pdata         is a pointer to a user supplied data area that will be passed to the task
*                            when the task first executes.
*
*              ptos          is a pointer to the top of stack.  It is assumed that 'ptos' points to
*                            a 'free' entry on the task stack.  If OS_STK_GROWTH is set to 1 then 
*                            'ptos' will contain the HIGHEST valid address of the stack.  Similarly, if
*                            OS_STK_GROWTH is set to 0, the 'ptos' will contains the LOWEST valid address
*                            of the stack.
*
*              opt           specifies options that can be used to alter the behavior of OSTaskStkInit().
*                            (see uCOS_II.H for OS_TASK_OPT_???).
*
* Returns    : Always returns the location of the new top-of-stack' once the processor registers have
*              been placed on the stack in the proper order.
*
* Note(s)    : Interrupts are enabled when your task starts executing. You can change this by setting the
*              PSW to 0x0002 instead.  In this case, interrupts would be disabled upon task startup.  The
*              application code would be responsible for enabling interrupts at the beginning of the task
*              code.  You will need to modify OSTaskIdle() and OSTaskStat() so that they enable 
*              interrupts.  Failure to do this will make your system crash!
*********************************************************************************************************
*/

CPU_STK  *OSTaskStkInit (OS_TASK_PTR    p_task,
                         void          *p_arg,
                         CPU_STK       *p_stk_base,
                         CPU_STK       *p_stk_limit,
                         CPU_STK_SIZE   stk_size,
                         OS_OPT         opt)
{
    CPU_INT16U *stk;
    CPU_INT32U  ptos = (CPU_INT32U)(p_stk_base + stk_size - 1);
    

    opt    = opt;                               /* 'opt' is not used, prevent warning                      */
    stk    = (CPU_INT16U *)ptos;                /* Load stack pointer                                      */
    *stk-- = (CPU_INT16U)FP_SEG(p_arg);         /* Simulate call to function with argument                 */
    *stk-- = (CPU_INT16U)FP_OFF(p_arg);         
    *stk-- = (CPU_INT16U)FP_SEG(p_task);
    *stk-- = (CPU_INT16U)FP_OFF(p_task);
    *stk-- = (CPU_INT16U)0x0202;                /* SW = Interrupts enabled                                 */
    *stk-- = (CPU_INT16U)FP_SEG(p_task);        /* Put pointer to task   on top of stack                   */
    *stk-- = (CPU_INT16U)FP_OFF(p_task);
    *stk-- = (CPU_INT16U)0xAAAA;                /* AX = 0xAAAA                                             */
    *stk-- = (CPU_INT16U)0xCCCC;                /* CX = 0xCCCC                                             */
    *stk-- = (CPU_INT16U)0xDDDD;                /* DX = 0xDDDD                                             */
    *stk-- = (CPU_INT16U)0xBBBB;                /* BX = 0xBBBB                                             */
    *stk-- = (CPU_INT16U)0x0000;                /* SP = 0x0000                                             */
    *stk-- = (CPU_INT16U)0x1111;                /* BP = 0x1111                                             */
    *stk-- = (CPU_INT16U)0x2222;                /* SI = 0x2222                                             */
    *stk-- = (CPU_INT16U)0x3333;                /* DI = 0x3333                                             */
    *stk-- = (CPU_INT16U)0x4444;                /* ES = 0x4444                                             */
    *stk   = _DS;                               /* DS = Current value of DS                                */
    return ((CPU_STK *)stk);
}


/*$PAGE*/
/*
*********************************************************************************************************
*                        INITIALIZE A TASK'S STACK FOR FLOATING POINT EMULATION
*
* Description: This function MUST be called BEFORE calling either OSTaskCreate() or OSTaskCreateExt() in
*              order to initialize the task's stack to allow the task to use the Borland floating-point 
*              emulation.  The returned pointer MUST be used in the task creation call.
*
*              Ex.:   OS_STK TaskStk[1000];
*
*
*                     void main (void)
*                     {
*                         OS_STK *ptos;
*                         OS_STK *pbos;
*                         INT32U  size;
*
*
*                         OSInit();
*                         .
*                         .
*                         ptos  = &TaskStk[999];
*                         pbos  = &TaskStk[0];
*                         psize = 1000;
*                         OSTaskStkInit_FPE_x86(&ptos, &pbos, &size);
*                         OSTaskCreate(Task, (void *)0, ptos, 10);
*                         .
*                         .
*                         OSStart();
*                     }
*
* Arguments  : pptos         is the pointer to the task's top-of-stack pointer which would be passed to 
*                            OSTaskCreate() or OSTaskCreateExt().
*
*              ppbos         is the pointer to the new bottom of stack pointer which would be passed to
*                            OSTaskCreateExt().
*
*              psize         is a pointer to the size of the stack (in number of stack elements).  You 
*                            MUST allocate sufficient stack space to leave at least 384 bytes for the 
*                            floating-point emulation.
*
* Returns    : The new size of the stack once memory is allocated to the floating-point emulation.
*
* Note(s)    : 1) _SS  is a Borland 'pseudo-register' and returns the contents of the Stack Segment (SS)
*              2) The pointer to the top-of-stack (pptos) will be modified so that it points to the new
*                 top-of-stack.
*              3) The pointer to the bottom-of-stack (ppbos) will be modified so that it points to the new
*                 bottom-of-stack.
*              4) The new size of the stack is adjusted to reflect the fact that memory was reserved on
*                 the stack for the floating-point emulation.
*********************************************************************************************************
*/

void  OSTaskStkInit_FPE_x86 (CPU_STK **pptos, CPU_STK **ppbos, CPU_INT32U *psize)
{
    CPU_INT32U   lin_tos;                                 /* 'Linear' version of top-of-stack    address   */
    CPU_INT32U   lin_bos;                                 /* 'Linear' version of bottom-of-stack address   */
    CPU_INT16U   seg;
    CPU_INT16U   off;
    CPU_INT32U   bytes;


    seg      = FP_SEG(*pptos);                            /* Decompose top-of-stack pointer into seg:off   */
    off      = FP_OFF(*pptos);
    lin_tos  = ((CPU_INT32U)seg << 4) + (CPU_INT32U)off;  /* Convert seg:off to linear address             */
    bytes    = *psize * sizeof(CPU_STK);                  /* Determine how many bytes for the stack        */
    lin_bos  = (lin_tos - bytes + 15) & 0xFFFFFFF0L;      /* Ensure paragraph alignment for BOS            */
    
    seg      = (CPU_INT16U)(lin_bos >> 4);                /* Get new 'normalized' segment                  */
    *ppbos   = (CPU_STK *)MK_FP(seg, 0x0000);             /* Create 'normalized' BOS pointer               */
    memcpy(*ppbos, MK_FP(_SS, 0), 384);                   /* Copy FP emulation memory to task's stack      */
    bytes    = bytes - 16;                                /* Loose 16 bytes because of alignment           */
    *pptos   = (CPU_STK *)MK_FP(seg, (CPU_INT16U)bytes);  /* Determine new top-of-stack                    */
    *ppbos   = (CPU_STK *)MK_FP(seg, 384);                /* Determine new bottom-of-stack                 */
    bytes    = bytes - 384;
    *psize   = bytes / sizeof(CPU_STK);                   /* Determine new stack size                      */
}


/*
*********************************************************************************************************
*                                           IDLE TASK HOOK
*
* Description: This function is called by the idle task.  This hook has been added to allow you to do
*              such things as STOP the CPU to conserve power.
*
* Arguments  : None.
*
* Note(s)    : None.
*********************************************************************************************************
*/

void  OSIdleTaskHook (void)
{
#if OS_CFG_APP_HOOKS_EN > 0u

#endif
}


/*$PAGE*/
/*
*********************************************************************************************************
*                                       OS INITIALIZATION HOOK
*
* Description: This function is called by OSInit() at the beginning of OSInit().
*
* Arguments  : None.
*
* Note(s)    : None.
*********************************************************************************************************
*/

void  OSInitHook (void)
{
#if OS_CFG_APP_HOOKS_EN > 0u
    OSFPInit();
#endif
}


/*$PAGE*/
/*
*********************************************************************************************************
*                                         STATISTIC TASK HOOK
*
* Description: This function is called every second by uC/OS-III's statistics task.  This allows your
*              application to add functionality to the statistics task.
*
* Arguments  : None.
*
* Note(s)    : None.
*********************************************************************************************************
*/

void  OSStatTaskHook (void)
{
#if OS_CFG_APP_HOOKS_EN > 0u

#endif
}


/*$PAGE*/
/*
*********************************************************************************************************
*                                          TASK CREATION HOOK
*
* Description: This function is called when a task is created.
*
* Arguments  : p_tcb        Pointer to the task control block of the task being created.
*
* Note(s)    : None.
*********************************************************************************************************
*/

void  OSTaskCreateHook (OS_TCB  *p_tcb)
{
#if OS_CFG_APP_HOOKS_EN > 0u
    OS_ERR  err;
    void  *pblk;
    
    
    if (p_tcb->Opt & OS_TASK_OPT_SAVE_FP) {      /* See if task needs FP support                      */
        pblk = OSMemGet(OSFPPartPtr, &err);      /* Yes, Get storage for FPU registers                */
        if (pblk != (void *)0) {                 /*      Did we get a memory block?                   */
            p_tcb->ExtPtr = pblk;                /*      Yes, Link to task's TCB                      */
            OSFPSave(pblk);                      /*           Save the FPU registers in block         */
        }
    }
#else
    (void)p_tcb;                                 /* Prevent compiler warning                          */
#endif
}


/*$PAGE*/
/*
*********************************************************************************************************
*                                           TASK DELETION HOOK
*
* Description: This function is called when a task is deleted.
*
* Arguments  : p_tcb        Pointer to the task control block of the task being deleted.
*
* Note(s)    : None.
*********************************************************************************************************
*/

void  OSTaskDelHook (OS_TCB  *p_tcb)
{
#if OS_CFG_APP_HOOKS_EN > 0u
    OS_ERR  err;
    
    
    if (p_tcb->Opt & OS_TASK_OPT_SAVE_FP) {                /* See if task had FP support               */
        if (p_tcb->ExtPtr != (void *)0) {                  /* Yes, OSTCBExtPtr must not be NULL        */
            OSMemPut(OSFPPartPtr, p_tcb->ExtPtr,&err);     /*      Return memory block to free pool    */
        }
    }
#else
    (void)p_tcb;                                           /* Prevent compiler warning                 */
#endif
}


/*$PAGE*/
/*
*********************************************************************************************************
*                                            TASK RETURN HOOK
*
* Description: This function is called if a task accidentally returns.  In other words, a task should
*              either be an infinite loop or delete itself when done.
*
* Arguments  : p_tcb        Pointer to the task control block of the task that is returning.
*
* Note(s)    : None.
*********************************************************************************************************
*/

void  OSTaskReturnHook (OS_TCB  *p_tcb)
{
#if OS_CFG_APP_HOOKS_EN > 0u
    
#else
    (void)p_tcb;                                            /* Prevent compiler warning                               */
#endif
}


/*$PAGE*/
/*
*********************************************************************************************************
*                                           TASK SWITCH HOOK
*
* Description: This function is called when a task switch is performed.  This allows you to perform other
*              operations during a context switch.
*
* Arguments  : None.
*
* Note(s)    : 1) Interrupts are disabled during this call.
*              2) It is assumed that the global pointer 'OSTCBHighRdyPtr' points to the TCB of the task
*                 that will be 'switched in' (i.e. the highest priority task) and, 'OSTCBCurPtr' points
*                 to the task being switched out (i.e. the preempted task).
*********************************************************************************************************
*/

void  OSTaskSwHook (void)
{
#if OS_CFG_TASK_PROFILE_EN > 0u
    CPU_TS  ts;
#endif
#ifdef  CPU_CFG_INT_DIS_MEAS_EN
    CPU_TS  int_dis_time;
#endif
#if OS_CFG_APP_HOOKS_EN > 0u
    OS_ERR  err;
    void  *pblk;
#endif



#if OS_CFG_APP_HOOKS_EN > 0u
                                                           /* Save FPU context of preempted task       */
    if (OSRunning == TRUE) {                               /* Don't save on OSStart()!                 */
        if (OSTCBCurPtr->Opt & OS_TASK_OPT_SAVE_FP) {      /* See if task used FP                      */
            pblk = OSTCBCurPtr->ExtPtr;                    /* Yes, Get pointer to FP storage area      */
            if (pblk != (void *)0) {                       /*      Make sure we have storage           */
                OSFPSave(pblk);                            /*      Save the FPU registers in block     */
            }
        }
    }
                                                           /* Restore FPU context of new task          */
    if (OSTCBHighRdyPtr->Opt & OS_TASK_OPT_SAVE_FP) {      /* See if new task uses FP                  */
        pblk = OSTCBHighRdyPtr->ExtPtr;                    /* Yes, Get pointer to FP storage area      */
        if (pblk != (void *)0) {                           /*      Make sure we have storage           */
            OSFPRestore(pblk);                             /*      Get contents of FPU registers       */
        }
    }
#endif

#if OS_CFG_TASK_PROFILE_EN > 0u
    ts = OS_TS_GET();
    if (OSTCBCurPtr != OSTCBHighRdyPtr) {
        OSTCBCurPtr->CyclesDelta  = ts - OSTCBCurPtr->CyclesStart;
        OSTCBCurPtr->CyclesTotal += (OS_CYCLES)OSTCBCurPtr->CyclesDelta;
    }

    OSTCBHighRdyPtr->CyclesStart = ts;
#endif

#ifdef  CPU_CFG_INT_DIS_MEAS_EN
    int_dis_time = CPU_IntDisMeasMaxCurReset();            /* Keep track of per-task interrupt disable time */
    if (OSTCBCurPtr->IntDisTimeMax < int_dis_time) {
        OSTCBCurPtr->IntDisTimeMax = int_dis_time;
    }
#endif

#if OS_CFG_SCHED_LOCK_TIME_MEAS_EN > 0u
                                                           /* Keep track of per-task scheduler lock time    */
    if (OSTCBCurPtr->SchedLockTimeMax < OSSchedLockTimeMaxCur) {
        OSTCBCurPtr->SchedLockTimeMax = OSSchedLockTimeMaxCur;
    }
    OSSchedLockTimeMaxCur = (CPU_TS)0;                      /* Reset the per-task value                               */
#endif
}


/*$PAGE*/
/*
*********************************************************************************************************
*                                              TICK HOOK
*
* Description: This function is called every tick.
*
* Arguments  : None.
*
* Note(s)    : 1) This function is assumed to be called from the Tick ISR.
*********************************************************************************************************
*/

void  OSTimeTickHook (void)
{
#if OS_CFG_APP_HOOKS_EN > 0u

#endif
}


/*$PAGE*/
/*
*********************************************************************************************************
*                                          SYS TICK HANDLER
*
* Description: Handle the system tick (SysTick) interrupt, which is used to generate the uC/OS-II tick
*              interrupt.
*
* Arguments  : None.
*
* Note(s)    : 1) This function MUST be placed on entry 15 of the Cortex-M3 vector table.
*********************************************************************************************************
*/

void  OS_CPU_SysTickHandler (void)
{

}



/*$PAGE*/
/*
*********************************************************************************************************
*                                         INITIALIZE SYS TICK
*
* Description: Initialize the SysTick.
*
* Arguments  : cnts         Number of SysTick counts between two OS tick interrupts.
*
* Note(s)    : 1) This function MUST be called after OSStart() & after processor initialization.
*********************************************************************************************************
*/

void  OS_CPU_SysTickInit (CPU_INT32U  cnts)
{

}
