/* exceptions.c */

/******************************************************************
 * Module Level: Implements the TLB, Program Trap, and SYSCALL
 * exception handlers. It also contains the skeleton TLB-Refill event handler.
 ******************************************************************/

#include "../h/exceptions.h"

void exceptionHandler() {

/* **Switch based on the exception code** */

/* **Call the device interrupt handler** */

/* **Pass processing to the TLB exception handler** */

/* **Pass processing to the Program Trap exception handler** */

/* **Pass processing to the SYSCALL exception handler** */
  state_t *oldState = (state_t *) BIOSDATAPAGE;
  int excCode = (oldState->cause & 0x0000007C) >> 2;

  if (excCode == EXC_INT) {
      interruptHandler();
  } else if (excCode >= 1 && excCode <= 3) {
      passUpOrDie(oldState);
  } else if (excCode == 8) {
      syscallHandler();
  } else {
      passUpOrDie(oldState);
  }
}

void syscallHandler() {
  state_t *oldState = (state_t *) BIOSDATAPAGE;
  int sysNum = oldState->reg_a0;

  switch (sysNum) {
      case CREATEPROCESS:
          /* Handle process creation */
          break;
      case TERMINATEPROCESS:
          /* Handle process termination */
          break;
      case PASSERN:
      case VERHOGEN:
          /* Semaphore operations */
          break;
      case WAITIO:
          /* Handle device waiting */
          break;
      case GETCPUTIME:
          oldState->reg_v0 = currentProcess->p_time;
          break;
      default:
          passUpOrDie(oldState);
  }

  /* Move to next instruction */
  oldState->pc += 4;
  LDST(oldState);
}

void uTLB_RefillHandler() {
/* Skeleton TLB-Refill event handler */
setENTRYHI(KUSEG);
setENTRYLO(KSEG0);
TLBWR();
LDST((state_PTR)BIOSDATAPAGE);
}