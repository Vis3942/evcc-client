/**************************************************************************************************
 *                                                                                                *
 *  Kitu Systems, Inc.     Copyright(C) 2016.  All rights reserved.                               *
 *                                                                                                *
 *  This source code and any compilation or derivative thereof is the proprietary information of  *
 *  Kitu Systems, Inc. and is confidential in nature.                                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  psal_failfile.h                                                                               *
 *                                                                                                *
 *  This file contains the public interface to a module used for reading and writing a special    *
 *  "failure file" which is used to track aberrant behavior of Kitu software (e.g., crashes).  It *
 *  contains symbolic constants, type definitions, and function prototypes for reading the        *
 *  failure file and updating the failure file                                                    *
 *                                                                                                *
 *  The failure file may be written to within the context of a signal handler.  As such, any C    *
 *  library system call used must be AS-Safe (a POSIX safety concept, or Async-Signal-Safe),      *
 *  limiting the available functions.  Because none of the "formatted output" functions are safe, *
 *  the failure file will use a binary format as a list of entries where each entry contains:     *
 *                                                                                                *
 *      +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+                         *
 *      |Time of Failure| Failure Code  |Secondary Code | Tertiary Code |                         *
 *      +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+                         *
 *      |<-- 4 bytes -->|<-- 4 bytes -->|<-- 4 bytes -->|<-- 4 bytes -->|                         *
 *                                                                                                *
 *  The Time of Failure will be in UNIX time (number of seconds since January 1, 1970).  The set  *
 *  of valid Failure Codes include the following causes:                                          *
 *                                                                                                *
 *      - Illegal Instruction                                                                     *
 *      - Floating-Point Exception                                                                *
 *      - Abort                                                                                   *
 *      - Segmentation Violation                                                                  *
 *      - Software Watchdog                                                                       *
 *      - Hazard Lights                                                                           *
 *      - Unknown (other)                                                                         *
 *                                                                                                *
 *  For Illegal Instructions, Floating-Point Exceptions, and Segmentation Violations, the         *
 *  Secondary Code will be the cause of the signal generation (e.g., FPE_INTDIV for integer       *
 *  division be zero, FPE_FLTDIV for floating-point division by zero, ILL_ILLOPC for illegal      *
 *  opcode, etc.), and the Tertiary Code will be the address of the fault.  For Hazard Lights,    *
 *  the Secondary Code will be the line number of the caller and the Tertiary Code will be the    *
 *  address of a string containing the caller's filename.  The Secondary and Tertiary Codes       *
 *  should be set to zero for other failure types.                                                *
 *                                                                                                *
 *  This module is for use on Linux systems exclusively.                                          *
 *                                                                                                *
 **************************************************************************************************/


#ifndef PSAL_FAILFILE_H
#define PSAL_FAILFILE_H


/**************************************************************************************************
 *                                                                                                *
 *  Header File Inclusions                                                                        *
 *                                                                                                *
 **************************************************************************************************/
#include "psal_types.h"








/**************************************************************************************************
 *                                                                                                *
 *  Symbolic Constants                                                                            *
 *                                                                                                *
 **************************************************************************************************/
#define PSAL_FAILFILE_MAX_ENTRIES                     (10)
#ifdef OVERRIDE_PSAL_FAILFILE_PATHNAME
#define PSAL_FAILFILE_PATHNAME                        OVERRIDE_PSAL_FAILFILE_PATHNAME
#else
#define PSAL_FAILFILE_PATHNAME                        "./recentFailures.dat"
#endif








/**************************************************************************************************
 *                                                                                                *
 *  Enumerations                                                                                  *
 *                                                                                                *
 **************************************************************************************************/
enum
{
    PSAL_FAILFILE_FAILURE_CODE_UNKNOWN = 0,
    PSAL_FAILFILE_FAILURE_CODE_SIGILL  = 1,
    PSAL_FAILFILE_FAILURE_CODE_SIGFPE  = 2,
    PSAL_FAILFILE_FAILURE_CODE_SIGABRT = 3,
    PSAL_FAILFILE_FAILURE_CODE_SIGSEGV = 4,
    PSAL_FAILFILE_FAILURE_CODE_SWD     = 16,
    PSAL_FAILFILE_FAILURE_CODE_PSALHAZ = 17
};








/**************************************************************************************************
 *                                                                                                *
 *  Type Definitions                                                                              *
 *                                                                                                *
 **************************************************************************************************/
typedef struct psal_FailfileEntry_s
{
    i32_t  timeOfFailure;
    i32_t  failureCode;
    i32_t  signalCode;
    void  *faultAddress;
}  psal_FailfileEntry_t;








/**************************************************************************************************
 *                                                                                                *
 *  Public Function Prototypes                                                                    *
 *                                                                                                *
 **************************************************************************************************/


/**************************************************************************************************
 *                                                                                                *
 *  PSAL_FailfileRead() - this public routine is used to read the contents of the failure file    *
 *                        into a supplied buffer.  This routine may be executed from signal       *
 *                        context, so only AS-Safe routines may be used.                          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      psal_FailFileEntry_t  fileBuffer[PSAL_FAILFILE_MAX_ENTRIES] - a buffer where the contents *
 *                                                                    of the failure file will be *
 *                                                                    placed.                     *
 *      i32_t*                const p_numInFile                     - a pointer to where the      *
 *                                                                    number of entries in the    *
 *                                                                    file (and, hence, buffer)   *
 *                                                                    will be placed.             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
PSAL_FailfileRead(psal_FailfileEntry_t  fileBuffer[PSAL_FAILFILE_MAX_ENTRIES],
                  i32_t*                const p_numInFile);




/**************************************************************************************************
 *                                                                                                *
 *  PSAL_FailfileUpdate() - this public routine is used to add a new failure code in the failure  *
 *                          file.  The timestamp for the entry is generated within this routine.  *
 *                          The failure file contains a list of failure entries with the most     *
 *                          recent first.  As such, the file needs to be read in, then            *
 *                          re-written.  At most, PSAL_FAILFILE_MAX_ENTRIES will be written.      *
 *                          This routine may be executed from the context of a signal handler,    *
 *                          so only AS-Safe routines are used.                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      const i32_t  failureCode  - the failure code to be placed in the most recent entry.       *
 *      const i32_t  signalCode   - the cause of the signal.  This is only used if the failure    *
 *                                  code is for an Illegal Instruction, Floating-Point Exception, *
 *                                  or Segmentation Violation.                                    *
 *      const void*  faultAddress - the address of the fault.  This is only used if the failure   *
 *                                  code is for an Illegal Instruction, Floating-Point Exception, *
 *                                  or Segmentation Violation.                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Altered:                                                                              *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Globals Used:                                                                                 *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
extern void
PSAL_FailfileUpdate(const i32_t failureCode,
                    const i32_t signalCode,
                    void *faultAddress);








#endif  /* matches #ifndef PSAL_FAILFILE_H  */
