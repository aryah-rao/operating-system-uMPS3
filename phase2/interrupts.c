/* interrupts.c */

/******************************************************************
 * Module Level: Implements the device/timer interrupt exception handler.
 * Processes all device/timer interrupts, converting them into V operations
 * on the appropriate semaphores.
 ******************************************************************/

 #include "../h/interrupts.h"
 
 void interruptHandler() {
   /**************************************************************
    * Functional Level: Interrupt Exception Handling
    *************************************************************/
 
   /* **Determine the line number of the interrupting device** */
 
   /* **Perform a V operation on the Nucleus-maintained semaphore
    * associated with the (sub)device** */
 
   /* **Place the stored-off status code in the newly unblocked pcb's
    * v0 register** */
 
   /* **Insert the newly unblocked pcb on the Ready Queue** */
 
   /**************************************************************
    * Functional Level: Local Timer Interrupt
    *************************************************************/
 
   /* **Acknowledge the PLT interrupt by loading the timer with a new value** */
 
   /* **Copy the processor state at the time of the exception into the
    * Current Process's pcb** */
 
   /* **Update the accumulated CPU time for the Current Process** */
 
   /* **Place the Current Process on the Ready Queue** */
 
   /* **Call the Scheduler** */
 }

 