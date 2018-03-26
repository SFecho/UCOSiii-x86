/*
************************************************************************************************************************
*                                                      uC/OS-III
*                                                 The Real-Time Kernel
*
*                                  (c) Copyright 2009-2011; Micrium, Inc.; Weston, FL
*                           All rights reserved.  Protected by international copyright laws.
*
*
* File    : Random.c
* Modify  : 591881218@qq.com
************************************************************************************************************************
*/

#include <includes.h>

/*
*********************************************************************************************************
*                                               CONSTANTS
*********************************************************************************************************
*/

#define  TASK_STK_SIZE                1024       /* Size of each task's stacks (# of WORDs)            */
#define  N_TASKS                        10       /* Number of identical tasks                          */

#define  AppTaskPrio                    20       /* Tasks' priorities                                  */
#define  TaskStartPrio         AppTaskPrio
#define  TaskRandomPrio    AppTaskPrio + 5

#define  APP_TYPE_EN                    1u       /* Enable (1) or Disable (0) for App Type             */

/*
*********************************************************************************************************
*                                               VARIABLES
*********************************************************************************************************
*/

#if APP_TYPE_EN > 0u
typedef struct {
#if OS_VERSION == 30200u
    CPU_INT08U  OSCPUUsage;
#elif OS_VERSION == 30300u
    CPU_INT16U  OSCPUUsage;
#endif

    CPU_INT16U  OSTaskCtr;
    CPU_INT32U  OSCtxSwCtr;
} APP_TYPE;

APP_TYPE      *AppType;                                      /* App Type                                      */

#define  OSCPUUsage    AppType->OSCPUUsage
#define  OSTaskCtr      AppType->OSTaskCtr
#define  OSCtxSwCtr    AppType->OSCtxSwCtr
#endif

CPU_STK        TaskRandomStk[N_TASKS][TASK_STK_SIZE];        /* Tasks stacks                                  */
CPU_STK        TaskStartStk[TASK_STK_SIZE];

OS_TCB 	       TaskStartTCB;                                 /* Tasks TCBs                                    */
OS_TCB         TaskRandomTCB[N_TASKS];

CPU_CHAR       TaskRandomData[N_TASKS];                      /* Parameters to pass to each task               */

OS_SEM         RandomSem;

/*
*********************************************************************************************************
*                                           FUNCTION PROTOTYPES
*********************************************************************************************************
*/

        void  TaskRandom(void *p_arg);                       /* Function prototypes of taskradoms             */
        void  TaskStart(void *p_arg);                        /* Function prototypes of Startup task           */
static  void  TaskStartCreateTasks(void);
static  void  TaskStartDispInit(void);
static  void  TaskStartDisp(void);
        

/*$PAGE*/
/*
*********************************************************************************************************
*                                                MAIN
*********************************************************************************************************
*/

void  main (void)
{
    OS_ERR  err;
    
    
    PC_DispClrScr(DISP_FGND_WHITE + DISP_BGND_BLACK);      /* Clear the screen                          */

    OSInit((OS_ERR *)&err);                                /* Initialize uC/OS-III                      */

    PC_DOSSaveReturn();                                    /* Save environment to return to DOS         */
    PC_VectSet(uCOS, OSCtxSw);                             /* Install uC/OS-III's context switch vector */
    
#if APP_TYPE_EN > 0u
    OSCPUUsage = 0;
    OSTaskCtr  = 0;
    OSCtxSwCtr = 0;
#endif
    
    OSSemCreate(&RandomSem,
                "Random Sem",
                1,
                (OS_ERR *)&err);                           /* Random number semaphore                   */

    OSTaskCreate((OS_TCB     *)&TaskStartTCB,              /* Create the App Start Task.                */
                 (CPU_CHAR   *)"Task Start",
                 (OS_TASK_PTR )TaskStart, 
                 (void       *)0,
                 (OS_PRIO     )TaskStartPrio,
                 (CPU_STK    *)&TaskStartStk[0],
                 (CPU_STK_SIZE)TASK_STK_SIZE/10,
                 (CPU_STK_SIZE)TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
                 
    OSStart((OS_ERR *)&err);                               /* Start multitasking                        */
}


/*$PAGE*/
/*
*********************************************************************************************************
*                                              STARTUP TASK
*********************************************************************************************************
*/

void  TaskStart (void *p_arg)
{
    OS_ERR      err;
    CPU_INT16S  key;


    p_arg = p_arg;                                         /* Prevent compiler warning                 */
  
    CPU_CRITICAL_ENTER();
    PC_VectSet(0x08, OSTickISR);                           /* Install uC/OS-III's clock tick ISR       */
    PC_SetTickRate(OS_TICKS_PER_SEC);                      /* Reprogram tick rate                      */
    CPU_CRITICAL_EXIT();
    
#if OS_CFG_STAT_TASK_EN > 0u
    OSStatTaskCPUUsageInit((OS_ERR *)&err);
#endif

    TaskStartDispInit();                                   /* Initialize the display                   */

    TaskStartCreateTasks();                                /* Create all the application tasks         */

    for (;;) {		
        TaskStartDisp();                                   /* Update the display                       */

        if (PC_GetKey(&key) == TRUE) {                     /* See if key has been pressed              */
            if (key == 0x1B) {                             /* Yes, see if it's the ESCAPE key          */
                PC_DOSReturn();                            /* Return to DOS                            */
            }
        }

#if APP_TYPE_EN > 0u  
        CPU_CRITICAL_ENTER();
        OSTaskCtxSwCtr = 0;                                /* Clear context switch counter             */
        CPU_CRITICAL_EXIT();
#endif

        OSTimeDlyHMSM(0, 0, 1, 0, OS_OPT_TIME_DLY, (OS_ERR  *)&err);  /* Wait one second               */
    }
}


/*$PAGE*/
/*
*********************************************************************************************************
*                                        INITIALIZE THE DISPLAY
*********************************************************************************************************
*/

static  void  TaskStartDispInit (void)
{
/*                                1111111111222222222233333333334444444444555555555566666666667777777777 */
/*                      01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
    PC_DispStr( 0,  0, "                         uC/OS-III,The Real-Time Kernel                         ", DISP_FGND_WHITE + DISP_BGND_RED + DISP_BLINK);
    PC_DispStr( 0,  1, "                                Jean J. Labrosse                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  2, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  3, "                             Port: 591881218@qq.com                                  ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  4, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  5, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  6, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  7, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  8, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0,  9, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 10, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 11, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 12, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 13, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 14, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 15, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 16, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 17, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 18, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 19, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 20, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 21, "                                                                                ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);

#if OS_VERSION == 30200u
    PC_DispStr( 0, 22, "#Tasks          :         CPU Usage:     %                                       ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
#elif OS_VERSION == 30300u
    PC_DispStr( 0, 22, "#Tasks          :         CPU Usage:       %                                       ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
#endif  

    PC_DispStr( 0, 23, "#Task switch/sec:                                                               ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
    PC_DispStr( 0, 24, "                            <-PRESS 'ESC' TO QUIT->                             ", DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY + DISP_BLINK);
/*                                1111111111222222222233333333334444444444555555555566666666667777777777 */
/*                      01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
}


/*$PAGE*/
/*
*********************************************************************************************************
*                                           UPDATE THE DISPLAY
*********************************************************************************************************
*/

static  void  TaskStartDisp (void)
{
    OS_ERR          err;
    CPU_CHAR    str[80];
    

#if APP_TYPE_EN > 0u  
    CPU_CRITICAL_ENTER();
#if OS_CFG_STAT_TASK_EN > 0u
    OSCPUUsage = OSStatTaskCPUUsage;
#endif
    OSTaskCtr = OSTaskQty;
    OSCtxSwCtr = OSTaskCtxSwCtr;
    CPU_CRITICAL_EXIT();

    sprintf(str, "%6d", OSTaskCtr);                                          /* Display #tasks running               */
    PC_DispStr(18, 22, str, DISP_FGND_YELLOW + DISP_BGND_BLUE);

#if OS_VERSION == 30200u
    sprintf(str, "%3d", OSCPUUsage);                                         /* Display CPU usage in %               */
    PC_DispStr(37, 22, str, DISP_FGND_YELLOW + DISP_BGND_BLUE);
#elif OS_VERSION == 30300u
    sprintf(str, "%5d", OSCPUUsage);                                         /* Display CPU usage in %               */
    PC_DispStr(37, 22, str, DISP_FGND_YELLOW + DISP_BGND_BLUE);
#endif

    sprintf(str, "%6d", OSCtxSwCtr);                                         /* Display #context switches per second */
    PC_DispStr(18, 23, str, DISP_FGND_YELLOW + DISP_BGND_BLUE);
#endif

    sprintf(str, "V%1d.%02d", OSVersion((OS_ERR  *)&err) / 10000, OSVersion((OS_ERR  *)&err) /100 % 10); /* Display uC/OS-III's version number */
    PC_DispStr(75, 24, str, DISP_FGND_YELLOW + DISP_BGND_BLUE);

    switch (_8087) {                                                         /* Display whether FPU present          */
        case 0:
             PC_DispStr(71, 22, " NO  FPU ", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;

        case 1:
             PC_DispStr(71, 22, " 8087 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;

        case 2:
             PC_DispStr(71, 22, "80287 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;

        case 3:
             PC_DispStr(71, 22, "80387 FPU", DISP_FGND_YELLOW + DISP_BGND_BLUE);
             break;
    }
}


/*$PAGE*/
/*
*********************************************************************************************************
*                                             CREATE TASKS
*********************************************************************************************************
*/

static  void  TaskStartCreateTasks (void)
{
    OS_ERR    err;
    CPU_INT08U  i;


#if OS_CFG_SCHED_ROUND_ROBIN_EN > 0u                       /* ROUND-ROBIN SCHEDULING                   */
    for (i = 0; i < N_TASKS; i++) {                        /* Create N_TASKS identical tasks           */
        TaskRandomData[i] = '0' + i;                       /* Each task will display its own letter    */
        
        OSTaskCreate((OS_TCB     *)&TaskRandomTCB[i],                                           
                     (CPU_CHAR   *)"Task Random",
                     (OS_TASK_PTR )TaskRandom, 
                     (void       *)&TaskRandomData[i],
                     (OS_PRIO     )(TaskRandomPrio),       /* Notice ...                               */
                     (CPU_STK    *)&TaskRandomStk[i][0],
                     (CPU_STK_SIZE)TASK_STK_SIZE/10,
                     (CPU_STK_SIZE)TASK_STK_SIZE,
                     (OS_MSG_QTY  )0,
                     (OS_TICK     )5,
                     (void       *)0,
                     (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                     (OS_ERR     *)&err);
    }
    
    OSSchedRoundRobinCfg(DEF_ENABLED, 10, &err);           /* CONFIGURE ROUND-ROBIN SCHEDULING PARAMETERS */

#else                                                      /* PREEMPTIVE SCHEDULING                    */

    for (i = 0; i < N_TASKS; i++) {                        /* Create N_TASKS identical tasks           */
        TaskRandomData[i] = '0' + i;                       /* Each task will display its own letter    */
        
        OSTaskCreate((OS_TCB     *)&TaskRandomTCB[i],                                           
                     (CPU_CHAR   *)"Task Random",
                     (OS_TASK_PTR )TaskRandom, 
                     (void       *)&TaskRandomData[i],
                     (OS_PRIO     )(i + TaskRandomPrio),   /* Notice ...                               */
                     (CPU_STK    *)&TaskRandomStk[i][0],
                     (CPU_STK_SIZE)TASK_STK_SIZE/10,
                     (CPU_STK_SIZE)TASK_STK_SIZE,
                     (OS_MSG_QTY  )0,
                     (OS_TICK     )0,
                     (void       *)0,
                     (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                     (OS_ERR     *)&err);
    }
#endif
}


/*
*********************************************************************************************************
*                                             TASKRANDOMS
*********************************************************************************************************
*/

void  TaskRandom (void *p_arg)
{
    OS_ERR    err;
    CPU_INT08U  x;
    CPU_INT08U  y;


    for (;;) {

        OSSemPend(&RandomSem,
                  0,
                  OS_OPT_PEND_BLOCKING,
                  0,
                  (OS_ERR *)&err);               /* Acquire semaphore to perform random numbers        */
       
        x = random(80);                          /* Find X position where task number will appear      */
        y = random(16);                          /* Find Y position where task number will appear      */

        OSSemPost(&RandomSem,     
                  OS_OPT_POST_1,
                  (OS_ERR *)&err);               /* Release semaphore                                  */
                                                 /* Display the task number on the screen              */                       
        PC_DispChar(x, y + 5, *(CPU_CHAR *)p_arg, DISP_FGND_BLACK + DISP_BGND_LIGHT_GRAY);
        
#if OS_CFG_SCHED_ROUND_ROBIN_EN <= 0u            /* PREEMPTIVE SCHEDULING                              */
        OSTimeDly(1, OS_OPT_TIME_DLY, (OS_ERR *)&err);   /* Delay 1 clock tick                         */
#endif
    }
}

