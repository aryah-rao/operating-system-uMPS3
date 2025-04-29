#ifndef DEVICESUPPORT_H
#define DEVICESUPPORT_H

/******************************* deviceSupport.h *******************************
 *
 * This header file contains declarations for the device support functions
 * 
 * Written by Aryah Rao and Anish Reddy
 *
 ****************************************************************************/

/* Included Header Files */

#include "../h/types.h" /*for memaddr definition*/ 
#include "../h/initial.h"
#include "../h/vmSupport.h"
#include "../h/initProc.h"
#include "../h/sysSupport.h"

/*--- Public function declarations ---*/

/*General Flash Read/Write*/ 
int flashRW(int operation, int flashNum, int blockNum, memaddr ramAddr);

/* Disk syscall handlers*/
int diskPutSyscallHandler(void);
int diskGetSyscallHandler(void);

/*Flash syscall handlers*/
int flashPutSyscallHandler(void);
int flashGetSyscallHandler(void);

#endif /* DEVICESUPPORT_H */
