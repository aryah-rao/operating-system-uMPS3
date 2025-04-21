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
#include "../h/initial.h"
#include "../h/vmSupport.h"
#include "../h/initProc.h"
#include "../h/sysSupport.h"

/* Function Declarations */
extern void diskPutHandler(support_PTR supportStruct);              /* SYS14: Disk Put */
extern void diskGetHandler(support_PTR supportStruct);              /* SYS15: Disk Get */
extern void flashPutHandler(support_PTR supportStruct);             /* SYS16: Flash Put */
extern void flashGetHandler(support_PTR supportStruct);             /* SYS17: Flash Get */

/***************************************************************/

#endif /* DEVICESUPPORT_H */