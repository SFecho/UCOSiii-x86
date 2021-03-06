;********************************************************************************************************
;                                              uC/OS-III
;                                         The Real-Time Kernel
;
;                          (c) Copyright 2009-2011, Jean J. Labrosse, Weston, FL
;                                          All Rights Reserved
;
;
;                                       80x86/80x88 Specific code
;                                 LARGE MEMORY MODEL WITH FLOATING-POINT
;                                          Borland C/C++ V4.5
;
; File         : OS_CPU_A.ASM
; By           : Jean J. Labrosse
; Modify       : 591881218@qq.com
;********************************************************************************************************
;
            PUBLIC _OSStartHighRdy
            PUBLIC _OSCtxSw
            PUBLIC _OSIntCtxSw
            PUBLIC _OSFPRestore
            PUBLIC _OSFPSave
            PUBLIC _OSTickISR
;
            EXTRN  _OSIntExit:FAR
            EXTRN  _OSTimeTick:FAR
            EXTRN  _OSTaskSwHook:FAR
;            
            EXTRN  _OSIntNestingCtr:BYTE ; Modify
            EXTRN  _OSTickDOSCtr:BYTE
            EXTRN  _OSPrioHighRdy:BYTE
            EXTRN  _OSPrioCur:BYTE
            EXTRN  _OSRunning:BYTE
            EXTRN  _OSTCBCurPtr:DWORD ; Modify
            EXTRN  _OSTCBHighRdyPtr:DWORD ; Modify
;
.MODEL      LARGE
.CODE
.186
            PAGE                                    ; /*$PAGE*/
;*********************************************************************************************************
;                                          START MULTITASKING
;                                       void OSStartHighRdy(void)
;
; The stack frame is assumed to look as follows:
;
; OSTCBHighRdyPtr->OSTCBStkPtr --> DS                               (Low memory)
;                               ES
;                               DI
;                               SI
;                               BP
;                               SP
;                               BX
;                               DX
;                               CX
;                               AX
;                               OFFSET  of task code address
;                               SEGMENT of task code address
;                               Flags to load in PSW             
;                               OFFSET  of task code address
;                               SEGMENT of task code address
;                               OFFSET  of 'pdata'
;                               SEGMENT of 'pdata'               (High memory)
;
; Note : OSStartHighRdy() MUST:
;           a) Call OSTaskSwHook() then,
;           b) Set OSRunning to TRUE,
;           c) Switch to the highest priority task.
;*********************************************************************************************************
;
_OSStartHighRdy  PROC FAR
;
            CALL   FAR PTR _OSTaskSwHook            ; Call user defined task switch hook
;
            MOV    AX, SEG _OSTCBHighRdyPtr            ; Reload DS
            MOV    DS, AX                           ; 
            MOV    AL, 1                            ; OSRunning = TRUE;
            MOV    BYTE PTR DS:_OSRunning, AL       ;   (Indicates that multitasking has started)
;
            LES    BX, DWORD PTR DS:_OSTCBHighRdyPtr   ; SS:SP = OSTCBHighRdyPtr->OSTCBStkPtr
            MOV    SS, ES:[BX+2]                    ; 
            MOV    SP, ES:[BX+0]                    ; 
;
            POP    DS                               ; Load task's context
            POP    ES                               ; 
            POPA                                    ; 
;
            IRET                                    ; Run task
;
_OSStartHighRdy  ENDP
;
            PAGE                                    ; /*$PAGE*/
;*********************************************************************************************************
;                                PERFORM A CONTEXT SWITCH (From task level)
;                                           void OSCtxSw(void)
;
; Note(s): 1) Upon entry, 
;             OSTCBCurPtr     points to the OS_TCB of the task to suspend
;             OSTCBHighRdyPtr points to the OS_TCB of the task to resume
;
;          2) The stack frame of the task to suspend looks as follows:
;
;                 SP -> OFFSET  of task to suspend    (Low memory)
;                       SEGMENT of task to suspend      
;                       PSW     of task to suspend    (High memory)  
;
;          3) The stack frame of the task to resume looks as follows:
; 
;                 OSTCBHighRdyPtr->OSTCBStkPtr --> DS                               (Low memory)
;                                               ES
;                                               DI
;                                               SI
;                                               BP
;                                               SP
;                                               BX
;                                               DX
;                                               CX
;                                               AX
;                                               OFFSET  of task code address
;                                               SEGMENT of task code address
;                                               Flags to load in PSW             (High memory)
;*********************************************************************************************************
;
_OSCtxSw    PROC   FAR
;
            PUSHA                                   ; Save current task's context
            PUSH   ES                               ; 
            PUSH   DS                               ; 
;
            MOV    AX, SEG _OSTCBCurPtr                ; Reload DS in case it was altered
            MOV    DS, AX                           ; 
;
            LES    BX, DWORD PTR DS:_OSTCBCurPtr       ; OSTCBCurPtr->OSTCBStkPtr = SS:SP
            MOV    ES:[BX+2], SS                    ; 
            MOV    ES:[BX+0], SP                    ; 
;
            CALL   FAR PTR _OSTaskSwHook            ; Call user defined task switch hook
;
            MOV    AX, WORD PTR DS:_OSTCBHighRdyPtr+2  ; OSTCBCurPtr = OSTCBHighRdyPtr
            MOV    DX, WORD PTR DS:_OSTCBHighRdyPtr    ; 
            MOV    WORD PTR DS:_OSTCBCurPtr+2, AX      ; 
            MOV    WORD PTR DS:_OSTCBCurPtr, DX        ; 
;
            MOV    AL, BYTE PTR DS:_OSPrioHighRdy   ; OSPrioCur = OSPrioHighRdy
            MOV    BYTE PTR DS:_OSPrioCur, AL       ;     
;
            LES    BX, DWORD PTR DS:_OSTCBHighRdyPtr   ; SS:SP = OSTCBHighRdyPtr->OSTCBStkPtr
            MOV    SS, ES:[BX+2]                    ; 
            MOV    SP, ES:[BX]                      ; 
;
            POP    DS                               ; Load new task's context
            POP    ES                               ; 
            POPA                                    ; 
;
            IRET                                    ; Return to new task
;
_OSCtxSw    ENDP
;
            PAGE                                    ; /*$PAGE*/
;*********************************************************************************************************
;                                PERFORM A CONTEXT SWITCH (From an ISR)
;                                        void OSIntCtxSw(void)
;
; Note(s): 1) Upon entry, 
;             OSTCBCurPtr     points to the OS_TCB of the task to suspend
;             OSTCBHighRdyPtr points to the OS_TCB of the task to resume
;
;          2) The stack frame of the task to suspend looks as follows:
;
;             OSTCBCurPtr->OSTCBStkPtr ------>  DS                              (Low memory)
;                                            ES
;                                            DI
;                                            SI
;                                            BP
;                                            SP
;                                            BX
;                                            DX
;                                            CX
;                                            AX
;                                            OFFSET  of task code address
;                                            SEGMENT of task code address
;                                            Flags to load in PSW            (High memory)
;
;          3) The stack frame of the task to resume looks as follows:
; 
;             OSTCBHighRdyPtr->OSTCBStkPtr --> DS                               (Low memory)
;                                           ES
;                                           DI
;                                           SI
;                                           BP
;                                           SP
;                                           BX
;                                           DX
;                                           CX
;                                           AX
;                                           OFFSET  of task code address
;                                           SEGMENT of task code address
;                                           Flags to load in PSW             (High memory)
;*********************************************************************************************************
;
_OSIntCtxSw PROC   FAR
;
            CALL   FAR PTR _OSTaskSwHook            ; Call user defined task switch hook
;
            MOV    AX, SEG _OSTCBCurPtr                ; Reload DS in case it was altered
            MOV    DS, AX                           ; 
;
            MOV    AX, WORD PTR DS:_OSTCBHighRdyPtr+2  ; OSTCBCurPtr = OSTCBHighRdyPtr
            MOV    DX, WORD PTR DS:_OSTCBHighRdyPtr    ; 
            MOV    WORD PTR DS:_OSTCBCurPtr+2, AX      ; 
            MOV    WORD PTR DS:_OSTCBCurPtr, DX        ; 
;
            MOV    AL, BYTE PTR DS:_OSPrioHighRdy   ; OSPrioCur = OSPrioHighRdy
            MOV    BYTE PTR DS:_OSPrioCur, AL         
;
            LES    BX, DWORD PTR DS:_OSTCBHighRdyPtr   ; SS:SP = OSTCBHighRdyPtr->OSTCBStkPtr
            MOV    SS, ES:[BX+2]                    ; 
            MOV    SP, ES:[BX]                      ; 
;
            POP    DS                               ; Load new task's context
            POP    ES                               ; 
            POPA                                    ; 
;
            IRET                                    ; Return to new task
;
_OSIntCtxSw ENDP
;
            PAGE                                    ; /*$PAGE*/
;*********************************************************************************************************
;                                           RESTORE FPU REGISTERS
;                                       void OSFPRestore(void *pblk)
;
; Description : This function is called to restore the contents of the FPU registers during a context
;               switch.  It is assumed that a pointer to a storage area for the FPU registers is placed
;               in the task's TCB (i.e. .OSTCBExtPtr).
; Arguments   : pblk    is passed to this function when called.
; Note(s)     : 1) The stack frame upon entry looks as follows:
;
;                      SP + 0 -> OFFSET  of caller             (Low memory)
;                         + 2    SEGMENT of caller
;                         + 4    OFFSET  of pblk
;                         + 6    SEGMENT of pblk               (High memory)  
;*********************************************************************************************************
;
_OSFPRestore PROC   FAR
;
             PUSH   BP                              ; Save work registers
             MOV    BP,SP
             PUSH   ES                              
             PUSH   BX
;
             LES    BX, DWORD PTR [BP+6]            ; Point to 'pblk'
;
             FRSTOR ES:[BX]                         ; Restore FPU context
;
             POP    BX                              ; Restore work registers
             POP    ES
             POP    BP
;
             RET                                    ; Return to caller
;
_OSFPRestore ENDP
;
             PAGE                                   ; /*$PAGE*/
;*********************************************************************************************************
;                                           SAVE FPU REGISTERS
;                                        void OSFPSave(void *pblk)
;
; Description : This function is called to save the contents of the FPU registers during a context
;               switch.  It is assumed that a pointer to a storage area for the FPU registers is placed
;               in the task's TCB (i.e. .OSTCBExtPtr).
; Arguments   : pblk    is passed to this function when called.
; Note(s)     : 1) The stack frame upon entry looks as follows:
;
;                      SP + 0 -> OFFSET  of caller             (Low memory)
;                         + 2    SEGMENT of caller
;                         + 4    OFFSET  of pblk
;                         + 6    SEGMENT of pblk               (High memory)  
;*********************************************************************************************************
;
_OSFPSave    PROC   FAR
;
             PUSH   BP                              ; Save work registers
             MOV    BP,SP
             PUSH   ES                              
             PUSH   BX
;
             LES    BX, DWORD PTR [BP+6]            ; Point to 'pblk'
;
             FSAVE  ES:[BX]                         ; Save FPU context
;
             POP    BX                              ; Restore work registers
             POP    ES
             POP    BP
;
             RET                                    ; Return to caller
;
_OSFPSave    ENDP
;
             PAGE                                   ; /*$PAGE*/
;*********************************************************************************************************
;                                            HANDLE TICK ISR
;
; Description: This function is called 199.99 times per second or, 11 times faster than the normal DOS
;              tick rate of 18.20648 Hz.  Thus every 11th time, the normal DOS tick handler is called.
;              This is called chaining.  10 times out of 11, however, the interrupt controller on the PC
;              must be cleared to allow for the next interrupt.
;
; Arguments  : none
;
; Returns    : none
;
; Note(s)    : The following C-like pseudo-code describe the operation being performed in the code below.
;
;              Save all registers on the current task's stack;
;              OSIntNestingCtr++;
;              if (OSIntNestingCtr == 1) {
;                  OSTCBCurPtr->OSTCBStkPtr = SS:SP
;              }
;              OSTickDOSCtr--;
;              if (OSTickDOSCtr == 0) {
;                  OSTickDOSCtr = 11;
;                  INT 81H;               Chain into DOS every 54.925 mS 
;                                         (Interrupt will be cleared by DOS)
;              } else {
;                  Send EOI to PIC;       Clear tick interrupt by sending an End-Of-Interrupt to the 8259
;                                         PIC (Priority Interrupt Controller)
;              }
;              OSTimeTick();              Notify uC/OS-II that a tick has occured       
;              OSIntExit();               Notify uC/OS-II about end of ISR
;              Restore all registers that were save on the current task's stack;
;              Return from Interrupt;
;*********************************************************************************************************
;
_OSTickISR  PROC   FAR
;
            PUSHA                                   ; Save interrupted task's context
            PUSH   ES
            PUSH   DS
;
            MOV    AX, SEG(_OSIntNestingCtr)           ; Reload DS 
            MOV    DS, AX
            INC    BYTE PTR DS:_OSIntNestingCtr        ; Notify uC/OS-II of ISR
;
            CMP    BYTE PTR DS:_OSIntNestingCtr, 1	    ; if (OSIntNestingCtr == 1)
            JNE    SHORT _OSTickISR1             
            MOV    AX, SEG(_OSTCBCurPtr)               ;     Reload DS
            MOV    DS, AX
            LES    BX, DWORD PTR DS:_OSTCBCurPtr       ;     OSTCBCurPtr->OSTCBStkPtr = SS:SP
            MOV    ES:[BX+2], SS                    ;
            MOV    ES:[BX+0], SP                    ;
;
_OSTickISR1:
            MOV    AX, SEG(_OSTickDOSCtr)           ; Reload DS
            MOV    DS, AX
            DEC    BYTE PTR DS:_OSTickDOSCtr
            CMP    BYTE PTR DS:_OSTickDOSCtr, 0
            JNE    SHORT _OSTickISR2                ; Every 11 ticks (~199.99 Hz), chain into DOS
;
            MOV    BYTE PTR DS:_OSTickDOSCtr, 11
            INT    081H                             ; Chain into DOS's tick ISR
            JMP    SHORT _OSTickISR3
;
_OSTickISR2:
            MOV    AL, 20H                          ; Move EOI code into AL.
            MOV    DX, 20H                          ; Address of 8259 PIC in DX.
            OUT    DX, AL                           ; Send EOI to PIC if not processing DOS timer.
;
_OSTickISR3:
            CALL   FAR PTR _OSTimeTick              ; Process system tick
;
            CALL   FAR PTR _OSIntExit               ; Notify uC/OS-II of end of ISR
;
            POP    DS                               ; Restore interrupted task's context
            POP    ES
            POPA
;
            IRET                                    ; Return to interrupted task
;
_OSTickISR  ENDP
;
            END
