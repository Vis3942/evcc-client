/**************************************************************************************************
 *                                                                                                *
 *  Kitu Systems, Inc.     Copyright(C) 2016.  All rights reserved.                               *
 *                                                                                                *
 *  This source code and any compilation or derivative thereof is the proprietary information of  *
 *  Kitu Systems, Inc. and is confidential in nature.                                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  psal_failfile.c                                                                               *
 *                                                                                                *
 *  This file contains the implementation of a module used to read and write a special "failure   *
 *  file" which is used to track aberrant behavior of Kitu software.  Aberrant behavior includes: *
 *                                                                                                *
 *      - Software Watchdog faults                                                                *
 *      - Crashes (e.g., segmentation violations)                                                 *
 *      - Calls to PSAL_HazardLights() (trapped, but unhandlable error)                           *
 *                                                                                                *
 *  The failure file contains a list of failure entries, most recent first.  Each entry is 8      *
 *  bytes long:                                                                                   *
 *                                                                                                *
 *      +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+                         *
 *      |Time of Failure| Failure Code  |Secondary Code | Tertiary Code |                         *
 *      +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+                         *
 *      |<-- 4 bytes -->|<-- 4 bytes -->|<-- 4 bytes -->|<-- 4 bytes -->|                         *
 *                                                                                                *
 *  The failure file may be written to from the context of a signal handler.  Therefore, all C    *
 *  library routines used must be AS-Safe (a POSIX safety concept, also called                    *
 *  Async-Signal-Safe).  Also, in order to write the failure file, the contents of the file need  *
 *  to be read first.  Therefore, both routines to read and write the file are restricted to      *
 *  using AS-Safe routines only.                                                                  *
 *                                                                                                *
 **************************************************************************************************/


/**************************************************************************************************
 *                                                                                                *
 *  Header File Inclusions                                                                        *
 *                                                                                                *
 **************************************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include "psal_failfile.h"
#include "kal_api.h"








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
void
PSAL_FailfileRead(psal_FailfileEntry_t  fileBuffer[PSAL_FAILFILE_MAX_ENTRIES],
                  i32_t*                const p_numInFile)
{
    i32_t    fd;

    KAL_Memset(fileBuffer, 0, PSAL_FAILFILE_MAX_ENTRIES*sizeof(psal_FailfileEntry_t));
    *p_numInFile = 0;

    /* Try to read in the contents of the file, if extant.  */
    fd = open(PSAL_FAILFILE_PATHNAME, O_RDONLY);
    if (fd != -1)
    {
        /* Read in file one record at a time.  */
        while ((*p_numInFile < PSAL_FAILFILE_MAX_ENTRIES) && 
               (read(fd, &(fileBuffer[*p_numInFile]), sizeof(psal_FailfileEntry_t)) == 
                sizeof(psal_FailfileEntry_t)))
        {
            (*p_numInFile)++;
        }
        close(fd);
    }
}




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
void
PSAL_FailfileUpdate(const i32_t failureCode,
                    const i32_t signalCode,
                    void *faultAddress)
{
    psal_FailfileEntry_t  fileBuffer[PSAL_FAILFILE_MAX_ENTRIES+1];
    i32_t                 numInFile;
    i32_t                 fd;

    fileBuffer[0].timeOfFailure = (int) time(NULL);
    fileBuffer[0].failureCode = failureCode;
    if ((failureCode == PSAL_FAILFILE_FAILURE_CODE_SIGILL) ||
        (failureCode == PSAL_FAILFILE_FAILURE_CODE_SIGFPE) ||
        (failureCode == PSAL_FAILFILE_FAILURE_CODE_SIGSEGV) ||
        (failureCode == PSAL_FAILFILE_FAILURE_CODE_PSALHAZ))
    {
        fileBuffer[0].signalCode = signalCode;
        fileBuffer[0].faultAddress = faultAddress;
    }
    else
    {
        fileBuffer[0].signalCode = 0;
        fileBuffer[0].faultAddress = NULL;
    }

    PSAL_FailfileRead(&(fileBuffer[1]), &numInFile);

    /* Write the failure file.  */
    fd = open(PSAL_FAILFILE_PATHNAME, O_WRONLY | O_CREAT | O_TRUNC, 
              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd != -1)
    {
        if (numInFile < PSAL_FAILFILE_MAX_ENTRIES)
        {
            int stat = write(fd, fileBuffer, sizeof(psal_FailfileEntry_t)*(numInFile+1));
            assert(stat == sizeof(psal_FailfileEntry_t)*(numInFile+1));
        }
        else
        {
            int stat = write(fd, fileBuffer, sizeof(psal_FailfileEntry_t)*PSAL_FAILFILE_MAX_ENTRIES);
            assert(stat == sizeof(psal_FailfileEntry_t)*PSAL_FAILFILE_MAX_ENTRIES);
        }
        close(fd);
    }
}





