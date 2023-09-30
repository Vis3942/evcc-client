/**************************************************************************************************
 *                                                                                                *
 *  Grid2Home, Inc.     Copyright(C) 2011.  All rights reserved.                                  *
 *  This source code and any compilation or derivative thereof is the proprietary                 *
 *  information of Grid2Home, Inc and is confidential in nature.                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  kal_memory.c                                                                                  *
 *                                                                                                *
 *  This file contains the Linux implementation of the memory manager for the Kernel Abstraction  *
 *  Layer.  Instead of implementing a complete memory management system, this implementation uses *
 *  the underlying C standard library malloc() and free() calls available to the system.          *
 *                                                                                                *
 **************************************************************************************************/


#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include "platform_test_config.h"
#include "klnx_internal.h"
#include "psal_assert.h"






#if defined(KAL_DEBUG_MEMORY_LINUX)


/**************************************************************************************************
 *                                                                                                *
 *  Symbolic Constants                                                                            *
 *                                                                                                *
 **************************************************************************************************/
#ifdef OVERRIDE_KAL_MEMORY_MAX_ALLOCATE
#define KAL_MEMORY_MAX_ALLOCATE                  OVERRIDE_KAL_MEMORY_MAX_ALLOCATE
#else
#define KAL_MEMORY_MAX_ALLOCATE                  (16*1024)  /* Maximum size for dynamic allocation.  */
#endif
#define KAL_MEMORY_DEBUG_TRAIL_GUARDBAND_LENGTH  (0)        /* Zero for same size as allocation.  */
#define KAL_MEMORY_DEBUG_HEAD_GUARDBAND_VALUE    (0xBEDABABE)
#define KAL_MEMORY_DEBUG_TRAIL_START_VALUE       (0xFE)
#define KAL_MEMORY_DEBUG_STATUS_STRING_LENGTH    (80)







/**************************************************************************************************
 *                                                                                                *
 *  Type Definitions                                                                              *
 *                                                                                                *
 **************************************************************************************************/
typedef struct kalMemory_HeadGuardband_s
{
    u32_t  hdrGuardbandValue;
    u16_t  userAllocSize;
    u16_t  chksum;
}  kalMemory_HeadGuardband_t;


typedef struct kalMemory_DebugStatus_s
{
    bool  statusRequested;
    char  statusString[KAL_MEMORY_DEBUG_STATUS_STRING_LENGTH];
    char  endOfLineGuard;
}  kalMemory_DebugStatus_t;








/**************************************************************************************************
 *                                                                                                *
 *  Local Functions                                                                               *
 *                                                                                                *
 **************************************************************************************************/
static void
KAL_DebugCreateHeader(void*   p_rawBlock,
                      size_t  userLength);


static void
KAL_DebugCreateTrailer(void*   p_rawBlock,
                       size_t  trailerLength);


static void
KAL_DebugVerifyHeader(void*  p_userData);


static void
KAL_DebugVerifyTrailer(void*  p_userData);


static void
KAL_DebugHandleStatus(void);


static void
KAL_MemDbgGetLock(void);


static void
KAL_MemDbgReleaseLock(void);









/**************************************************************************************************
 *                                                                                                *
 *  Global and File Scope Variables                                                               *
 *                                                                                                *
 **************************************************************************************************/
u32_t                           kalMemoryMaxAllocate=KAL_MEMORY_MAX_ALLOCATE;
static FILE*                    p_dynMemFile=NULL;
static struct timespec          kalMemoryCurrentTime;
static kalMemory_DebugStatus_t  kalMemoryDebugStatus;
static sem_t                    kalMemorySemaphoreLock;








/**************************************************************************************************
 *                                                                                                *
 *  KAL_MemoryInit() - this routine is used to initialize the Kernel Abstraction Layer's dynamic  *
 *                     memory package.                                                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
KAL_MemoryInit(void)
{
    p_dynMemFile = fopen(KAL_DEBUG_MEMORY_LINUX_FILENAME, "w");
    kalMemoryDebugStatus.statusRequested = false;
    kalMemoryDebugStatus.endOfLineGuard = '\0';

    /* Create a binary semaphore to lock accesses to the dynamic memory log file.  */
    sem_init(&kalMemorySemaphoreLock, 0, 1);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MallocDebug() - this routine is used to dynamically obtain memory from the heap.  This    *
 *                      routine is part of the Linux Memory Debug library.  The memory block is   *
 *                      broken down into three contiguous segments: a header, the user's buffer,  *
 *                      and a trailer.  The header is comprised of a header guard band, the       *
 *                      length of the user's buffer, and a checksum over the guard band and       *
 *                      length.  The user's buffer is not initialized.  The trailer is simply an  *
 *                      ascending byte count, starting at 0xFE and rolling over after 0xFF.  A    *
 *                      limit of the size is imposed for a single allocation using this function. *
 *                                                                                                *
 *                      The following steps will be performed:                                    *
 *                                                                                                *
 *                          1) Verify the parameter.                                              *
 *                          2) Allocate a buffer which is the sum of the header length, user      *
 *                             length, and trailer length.                                        *
 *                          3) Write an entry to the memory log file.                             *
 *                          4) Populate the header with the correct header guard band value, the  *
 *                             length of the user buffer, and a checksum which covers the guard   *
 *                             band and length.                                                   *
 *                          5) Populate the trailer guard band.  The length of this may be fixed  *
 *                             or variable (equal to the user's buffer length).                   *
 *                          6) Return a pointer to the user's buffer.                             *
 *                                                                                                *
 *                      Because this is a debug routine, all errors will be trapped here; the     *
 *                      caller will never receive an error.                                       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      size_t  size         - the size of the contiguous memory chunk desired.                   *
 *      char*   p_callerFile - a pointer to a string containing the filename of the calling       *
 *                             routine.                                                           *
 *      u32_t   callerLine   - the line number of the calling routine.                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void*: non-NULL - a pointer to the memory block, upon success.                            *
 *                                                                                                *
 **************************************************************************************************/
void*
KAL_MallocDebug(size_t  size,
                char*   p_callerFile,
                u32_t   callerLine)
{
    void*   pointer;
    void*   p_userBuffer;
    size_t  trailerLength;

    PSAL_ASSERT(size <= kalMemoryMaxAllocate);

    /* Check and handle status requests.  */
    KAL_DebugHandleStatus();

    /* Compute the size of the trailer guard band.  */
    if (KAL_MEMORY_DEBUG_TRAIL_GUARDBAND_LENGTH == 0)
    {
        trailerLength = size;
    }
    else
    {
        trailerLength = KAL_MEMORY_DEBUG_TRAIL_GUARDBAND_LENGTH;
    }

    /* Allocate a buffer containing the header, buffer, and trailer.  */
    pointer = malloc(sizeof(kalMemory_HeadGuardband_t) + size + trailerLength);
    p_userBuffer = ((u8_t*) pointer) + sizeof(kalMemory_HeadGuardband_t);

    if (p_dynMemFile != NULL)
    {
        clock_gettime(CLOCK_MONOTONIC, &kalMemoryCurrentTime);
        KAL_MemDbgGetLock();
        fprintf(p_dynMemFile, "malloc: %lu %p %s %lu %lu %lu\n", (u32_t) size, p_userBuffer,
                p_callerFile, callerLine, kalMemoryCurrentTime.tv_sec,
                kalMemoryCurrentTime.tv_nsec);
        fflush(p_dynMemFile);
        KAL_MemDbgReleaseLock();
    }

    /* Populate the header.  */
    KAL_DebugCreateHeader(pointer, size);

    /* Populate the trailer.  */
    KAL_DebugCreateTrailer(pointer, trailerLength);

    /* Return a pointer to the user's buffer.  */
    return p_userBuffer;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_CallocDebug() - this routine is used to dynamically allocate an array of "num" elements   *
 *                      where each element is "size" bytes.  The memory is initialized to zeroes. *
 *                      There is a limit on the size of a single allocation                       *
 *                      (kalMemoryMaxAllocate), but since KAL_CallocDebug() is allocating an      *
 *                      array of elements, this size limitation will be placed on the individual  *
 *                      element, not on the array of elements.                                    *
 *                                                                                                *
 *                      This routine is part of the Linux Memory Debug library.  Because this     *
 *                      library uses header and trailer guard bands to ensure integrity, the      *
 *                      underlying calloc() routine *cannot* be used.  Instead, the functionality *
 *                      of calloc() will be reproduced within this routine which will use         *
 *                      malloc().  Specifically, this routine will:                               *
 *                                                                                                *
 *                          1) Verify the parameters.                                             *
 *                          2) Allocate a buffer which is the sum of the header length, user      *
 *                             length, and trailer length.  The user length is "num" times        *
 *                             "size".                                                            *
 *                          3) Write an entry to the memory log file.                             *
 *                          4) Zero out the user buffer.                                          *
 *                          5) Populate the header with the correct values.                       *
 *                          6) Populate the trailer with the correct values.                      *
 *                          7) Return a pointer to the user's buffer.                             *
 *                                                                                                *
 *                      Because this is a debug routine, all errors will be trapped here; the     *
 *                      caller will never receive an error.                                       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      size_t  num          - the number of elements in the array.                               *
 *      size_t  size         - the size of each element, in bytes.                                *
 *      char*   p_callerFile - a pointer to a string containing the filename of the calling       *
 *                             routine.                                                           *
 *      u32_t   callerLine   - the line number of the calling routine.                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void*: non-NULL - a pointer to the memory block, upon success.                            *
 *                                                                                                *
 **************************************************************************************************/
void*
KAL_CallocDebug(size_t  num,
                size_t  size,
                char*   p_callerFile,
                u32_t   callerLine)
{
    void*   pointer;
    void*   p_userBuffer;
    size_t  userLength;
    size_t  trailerLength;

    /* Verify the parameters.  */
    PSAL_ASSERT(size <= kalMemoryMaxAllocate);
    PSAL_ASSERT((size != 0) || (num != 0));

    /* Check and handle status requests.  */
    KAL_DebugHandleStatus();

    /* Compute the length of the user's buffer.  */
    userLength = num * size;

    /* Compute the size of the trailer guard band.  */
    if (KAL_MEMORY_DEBUG_TRAIL_GUARDBAND_LENGTH == 0)
    {
        trailerLength = userLength;
    }
    else
    {
        trailerLength = KAL_MEMORY_DEBUG_TRAIL_GUARDBAND_LENGTH;
    }

    /* Allocate a buffer containing the header, user buffer, and trailer.  */
    pointer = malloc(sizeof(kalMemory_HeadGuardband_t) + userLength + trailerLength);
    p_userBuffer = ((u8_t*) pointer) + sizeof(kalMemory_HeadGuardband_t);

    if (p_dynMemFile != NULL)
    {
        clock_gettime(CLOCK_MONOTONIC, &kalMemoryCurrentTime);
        KAL_MemDbgGetLock();
        fprintf(p_dynMemFile, "calloc: %lu %lu %p %s %lu %lu %lu\n", (u32_t) num, (u32_t) size,
                p_userBuffer, p_callerFile, callerLine, kalMemoryCurrentTime.tv_sec,
                kalMemoryCurrentTime.tv_nsec);
        fflush(p_dynMemFile);
        KAL_MemDbgReleaseLock();
    }

    /* Zero out the user's buffer.  */
    memset(p_userBuffer, 0, userLength);

    /* Populate the header.  */
    KAL_DebugCreateHeader(pointer, userLength);

    /* Populate the trailer.  */
    KAL_DebugCreateTrailer(pointer, trailerLength);

    /* Return a pointer to the user's buffer.  */
    return p_userBuffer;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_ReallocDebug() - this routine is used to change the size of a dynamically allocated       *
 *                       memory block.  This routine is part of the Linux Memory Debug library.   *
 *                       If "ptr" is NULL, then the call will be equivalent to                    *
 *                       KAL_MallocDebug(size).  If "size" is zero and "ptr" is not NULL, then    *
 *                       the call will be equivalent to KAL_FreeDebug(ptr).  Note: the calling    *
 *                       file and line are not shown here.                                        *
 *                                                                                                *
 *                       If the "size" is not zero and "ptr" is not NULL, then the allocation     *
 *                       will be limited in size to kalMemoryMaxAllocate bytes.  This internal    *
 *                       limitation is initialized to KAL_MEMORY_MAX_ALLOCATE, defined in this    *
 *                       file.  Because this is a debug version, there is information which       *
 *                       cannot simply be copied.  Therefore, in the case that "size" is not zero *
 *                       and "ptr" is not NULL, this routine will perform the following:          *
 *                                                                                                *
 *                           1) Verifies the integrity of the header (guard band value, and       *
 *                              checksum of the guard band and user length).                      *
 *                           2) Verifies the integrity of the trailer guard band.                 *
 *                           3) Calls realloc() to allocate a new chunk of memory.  Note: this    *
 *                              version will not have the correct header or trailer, yet.         *
 *                           4) Writes an entry to the memory log file.                           *
 *                           5) Overwrite the header with the correct version.                    *
 *                           6) Overwrite the trailer with the correct version.                   *
 *                           7) Return a pointer to the user buffer.                              *
 *                                                                                                *
 *                       Because this is a debug routine, all errors will be trapped here; the    *
 *                       caller will never receive an error.                                      *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*   ptr          - a pointer to dynamically allocated memory (or NULL).               *
 *      size_t  userSize     - the desired new size for the memory block.                         *
 *      char*   p_callerFile - a pointer to a string containing the filename of the calling       *
 *                             routine.                                                           *
 *      u32_t   callerLine   - the line number of the calling routine.                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void*: non-NULL - a pointer to the newly allocated  memory block, upon success.           *
 *                                                                                                *
 **************************************************************************************************/
void*
KAL_ReallocDebug(void*   ptr,
                 size_t  size,
                 char*   p_callerFile,
                 u32_t   callerLine)
{
    void*   p_newHeader;
    void*   p_userBuffer;
    size_t  trailerLength;

    /* Check for invalid combination of "ptr" and "size" parameters.  */
    PSAL_ASSERT(((ptr != NULL) && (size <= kalMemoryMaxAllocate)) ||
           ((ptr == NULL) && (size > 0)));

    /* Check and handle status requests.  */
    KAL_DebugHandleStatus();

    /* Compute the size of the trailer guard band.  */
    if (KAL_MEMORY_DEBUG_TRAIL_GUARDBAND_LENGTH == 0)
    {
        trailerLength = size;
    }
    else
    {
        trailerLength = KAL_MEMORY_DEBUG_TRAIL_GUARDBAND_LENGTH;
    }

    if ((ptr != NULL) && (size != 0))
    {
        /*** Case where a full reallocation takes place.  ***/

        /* Verify the integrity of the header.  */
        KAL_DebugVerifyHeader(ptr);

        /* Verify the integrity of the trailer.  */
        KAL_DebugVerifyTrailer(ptr);

        /* Reallocate the buffer from the old size (which includes a header and trailer) to the
             new size (which contains a different header and trailer).  */
        p_newHeader = realloc(((u8_t*) ptr)-sizeof(kalMemory_HeadGuardband_t),
                              sizeof(kalMemory_HeadGuardband_t) + size + trailerLength);
        p_userBuffer = ((u8_t*) p_newHeader) + sizeof(kalMemory_HeadGuardband_t);

        /* Create a memory log file entry.  */
        if (p_dynMemFile != NULL)
        {
            clock_gettime(CLOCK_MONOTONIC, &kalMemoryCurrentTime);
            KAL_MemDbgGetLock();
            fprintf(p_dynMemFile, "realloc: %p %lu %p %s %lu %lu %lu\n", ptr, (u32_t) size,
                    p_userBuffer, p_callerFile, callerLine, kalMemoryCurrentTime.tv_sec,
                    kalMemoryCurrentTime.tv_nsec);
            fflush(p_dynMemFile);
            KAL_MemDbgReleaseLock();
        }

        /* Repopulate the header with the correct information.  */
        KAL_DebugCreateHeader(p_newHeader, size);

        /* Repopulate the trailer with the correct information.  */
        KAL_DebugCreateTrailer(p_newHeader, trailerLength);

        /* Return a pointer to the user's buffer.  */
        return p_userBuffer;
    }
    else if ((ptr == NULL) && (size != 0))
    {
        /*** Case where it acts like KAL_MallocDebug().  ***/

        /* Allocate a buffer containing the header, user buffer, and trailer.  */
        p_newHeader = realloc(ptr, sizeof(kalMemory_HeadGuardband_t) + size + trailerLength);
        p_userBuffer = ((u8_t*) p_newHeader) + sizeof(kalMemory_HeadGuardband_t);

        /* Create a memory log file entry.  */
        if (p_dynMemFile != NULL)
        {
            clock_gettime(CLOCK_MONOTONIC, &kalMemoryCurrentTime);
            KAL_MemDbgGetLock();
            fprintf(p_dynMemFile, "realloc: %p %lu %p %s %lu %lu %lu\n", ptr, (u32_t) size,
                    p_userBuffer, p_callerFile, callerLine, kalMemoryCurrentTime.tv_sec,
                    kalMemoryCurrentTime.tv_nsec);
            fflush(p_dynMemFile);
            KAL_MemDbgReleaseLock();
        }

        /* Populate the header.  */
        KAL_DebugCreateHeader(p_newHeader, size);

        /* Populate the trailer.  */
        KAL_DebugCreateTrailer(p_newHeader, trailerLength);

        /* Return a pointer to the user's buffer.  */
        return p_userBuffer;
    }
    else if ((ptr != NULL) && (size == 0))
    {
        /*** Case where it acts like KAL_FreeDebug().  ***/

        /* Verify the pointer value.  */
        PSAL_ASSERT(((u32_t) ptr) >= sizeof(kalMemory_HeadGuardband_t));

        /* Verify the header.  */
        KAL_DebugVerifyHeader(ptr);

        /* Verify the trailer.  */
        KAL_DebugVerifyTrailer(ptr);

        /* Free the memory block.  The address needs to be adjusted from the user's buffer to the
             start of the header.  */
        p_newHeader = realloc(((u8_t*) ptr)-sizeof(kalMemory_HeadGuardband_t), size);

        /* Create a memory log file entry.  */
        if (p_dynMemFile != NULL)
        {
            clock_gettime(CLOCK_MONOTONIC, &kalMemoryCurrentTime);
            KAL_MemDbgGetLock();
            fprintf(p_dynMemFile, "realloc: %p %lu %p %s %lu %lu %lu\n", ptr, (u32_t) size,
                    p_newHeader, p_callerFile, callerLine, kalMemoryCurrentTime.tv_sec,
                    kalMemoryCurrentTime.tv_nsec);
            fflush(p_dynMemFile);
            KAL_MemDbgReleaseLock();
        }

        return p_newHeader;
    }

    return NULL;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_FreeDebug() - this routine is used to return dynamically allocated memory back to the     *
 *                    heap.  This is part of the Linux Memory Debug library, and it performs the  *
 *                    following:                                                                  *
 *                                                                                                *
 *                        1) Writes an entry to the memory log file.                              *
 *                        2) Asserts if a NULL pointer was passed.                                *
 *                        3) Verifies the integrity of the header guard band by checking the      *
 *                           header guard band value and the checksum over the entire header.     *
 *                        4) Verifies the integrity of the trailer guard band.                    *
 *                        5) Frees the memory block.                                              *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*  pointer      - a pointer to a previously allocated block of memory (from a prior   *
 *                            call to KAL_Malloc(), KAL_Calloc(), or KAL_Realloc()).              *
 *      char*  p_callerFile - a pointer to a string containing the filename of the calling        *
 *                            routine.                                                            *
 *      u32_t  callerLine   - the line number of the calling routine.                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
KAL_FreeDebug(void*  pointer,
              char*  p_callerFile,
              u32_t  callerLine)
{
    /* Check and handle status requests.  */
    KAL_DebugHandleStatus();

    /* Write an entry to the memory log file, if extant.  */
    if (p_dynMemFile != NULL)
    {
        clock_gettime(CLOCK_MONOTONIC, &kalMemoryCurrentTime);
        KAL_MemDbgGetLock();
        fprintf(p_dynMemFile, "free: %p %s %lu %lu %lu\n", pointer, p_callerFile, callerLine,
                kalMemoryCurrentTime.tv_sec, kalMemoryCurrentTime.tv_nsec);
        fflush(p_dynMemFile);
        KAL_MemDbgReleaseLock();
    }

    /* Verify the pointer value.  */
    PSAL_ASSERT(pointer != NULL);
    PSAL_ASSERT(((u32_t) pointer) >= sizeof(kalMemory_HeadGuardband_t));

    /* Verify the header.  This includes the header guard band, the user length, and the checksum
         over the rest of the header.  */
    KAL_DebugVerifyHeader(pointer);

    /* Verify the trailer.  */
    KAL_DebugVerifyTrailer(pointer);

    /* Free the memory block.  The address needs to be adjusted from the user's buffer to the start
         of the header. */
    free(((u8_t*) pointer) - sizeof(kalMemory_HeadGuardband_t));
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_FreePtrDebug() - this routine is used to return dynamically allocated memory back to the  *
 *                       heap.  It is different from KAL_FreeDebug() in that the parameter is the *
 *                       address of a pointer, and that the pointer is set to NULL within this    *
 *                       routine.  Thus, this routine:                                            *
 *                                                                                                *
 *                           1) Asserts if NULL was passed as the address.                        *
 *                           2) Writes an entry to the memory log file.                           *
 *                           3) Asserts if NULL was passed as the pointer value.                  *
 *                           4) Verifies the integrity of the header guard band by checking the   *
 *                              header guard band value and the checksum over the entire header.  *
 *                           5) Verifies the integrity of the trailer guard band.                 *
 *                           6) Frees the memory block.                                           *
 *                           7) Sets the pointer to NULL.                                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void**  a_ptr       - the address of a pointer to a previously allocated block of memory  *
 *                            (from a prior call to KAL_Malloc(), KAL_Calloc(), or                *
 *                            KAL_Realloc()).                                                     *
 *      char*  p_callerFile - a pointer to a string containing the filename of the calling        *
 *                            routine.                                                            *
 *      u32_t  callerLine   - the line number of the calling routine.                             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
KAL_FreePtrDebug(void**  a_ptr,
                 char*   p_callerFile,
                 u32_t   callerLine)
{
    PSAL_ASSERT(a_ptr != NULL);
    KAL_FreeDebug(*a_ptr, p_callerFile, callerLine);
    *a_ptr = NULL;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_AllocateStaticDebug() - this routine is used to perform a special one-time allocation of  *
 *                              memory which will never be returned to the system.  This is part  *
 *                              of the Linux Memory Debug library, and it performs the following: *
 *                                                                                                *
 *                                  1) Allocates a buffer of a size which is the sum of the       *
 *                                     user-specified length, a header, and a trailer.            *
 *                                  2) Writes an entry to the memory log file.                    *
 *                                  3) Populates the header with a header guard band value, the   *
 *                                     length of the user-specified length, and a checksum of the *
 *                                     header bytes.                                              *
 *                                  4) Populates the trailer guard band which may be a fixed      *
 *                                     length or a variable length (equal to the user-specified   *
 *                                     length).                                                   *
 *                                  5) Returns a pointer to the user's buffer.                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      size_t  size         - the size of the contiguous memory chunk desired.                   *
 *      char*   p_callerFile - a pointer to a string containing the filename of the calling       *
 *                             routine.                                                           *
 *      u32_t   callerLine   - the line number of the calling routine.                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void*: non-NULL - a pointer to the memory block, upon success.                            *
 *             NULL     - upon failure.                                                           *
 *                                                                                                *
 **************************************************************************************************/
void*
KAL_AllocateStaticDebug(size_t  length,
                        char*   p_callerFile,
                        u32_t   callerLine)
{
    void*   pointer;
    void*   p_userBuffer;
    size_t  trailerLength;

    /* Check and handle status requests.  */
    KAL_DebugHandleStatus();

    /* Compute the size of the trailer guard band.  */
    if (KAL_MEMORY_DEBUG_TRAIL_GUARDBAND_LENGTH == 0)
    {
        trailerLength = length;
    }
    else
    {
        trailerLength = KAL_MEMORY_DEBUG_TRAIL_GUARDBAND_LENGTH;
    }

    /* Allocate a buffer containing the header, buffer, and trailer.  */
    pointer = malloc(sizeof(kalMemory_HeadGuardband_t) + length + trailerLength);
    p_userBuffer = ((u8_t*) pointer) + sizeof(kalMemory_HeadGuardband_t);

    /* Create a memory log file entry.  */
    if (p_dynMemFile != NULL)
    {
        clock_gettime(CLOCK_MONOTONIC, &kalMemoryCurrentTime);
        KAL_MemDbgGetLock();
        fprintf(p_dynMemFile, "static: %lu %p %s %lu %lu %lu\n", (u32_t) length, p_userBuffer,
                p_callerFile, callerLine, kalMemoryCurrentTime.tv_sec,
                kalMemoryCurrentTime.tv_nsec);
        fflush(p_dynMemFile);
        KAL_MemDbgReleaseLock();
    }

    /* Populate the header.  */
    KAL_DebugCreateHeader(pointer, length);

    /* Populate the trailer.  */
    KAL_DebugCreateTrailer(pointer, trailerLength);

    /* Return a pointer to the user's buffer.  */
    return p_userBuffer;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MemoryStatus() - this routine is used to put a special entry into the memory debug log    *
 *                       upon the next invocation of a dynamic memory allocation/deallocation     *
 *                       routine.  The entry has the format:                                      *
 *                                                                                                *
 *                           STATUS: <string>                                                     *
 *                                                                                                *
 *                       It is up to the debug log post-processing tool(s) to dump their status   *
 *                       when this tag is parsed.                                                 *
 *                                                                                                *
 *                       Please note that the entry is not immediately placed within the debug    *
 *                       log file.  Therefore, it is possible for multiple requests to have been  *
 *                       made before any has been handled.  Also, a collision may occur where two *
 *                       tasks are simultaneously requesting status.  In this case, the <string>  *
 *                       may become corrupted, but the entry will be made in the debug log.       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      char*  p_string - a pointer to a character string.                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
KAL_MemoryStatus(char*  p_string)
{
    strncpy(kalMemoryDebugStatus.statusString, p_string, KAL_MEMORY_DEBUG_STATUS_STRING_LENGTH);
    kalMemoryDebugStatus.statusRequested = true;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_DebugCreateHeader() - this routine is used to populate a memory block header as part of   *
 *                            the Linux Memory Debug library.  Whenever a block of memory is      *
 *                            obtained, a header and trailer are added.  The header consists of a *
 *                            four-byte guard band, a two-byte length containing the user buffer  *
 *                            length, and a two-byte checksum of the guard band and length.       *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*   p_rawBlock - a pointer to the beginning of the raw memory block.  This is where   *
 *                           the header will be placed.                                           *
 *      size_t  userLength - the length of the user's buffer.                                     *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
static void
KAL_DebugCreateHeader(void*   p_rawBlock,
                      size_t  userLength)
{
    kalMemory_HeadGuardband_t*  p_headGuard;
    u8_t*                       p_byte;

    p_headGuard = (kalMemory_HeadGuardband_t*) p_rawBlock;
    p_headGuard->hdrGuardbandValue = KAL_MEMORY_DEBUG_HEAD_GUARDBAND_VALUE;
    p_headGuard->userAllocSize = (u16_t) userLength;
    p_headGuard->chksum = 0;
    p_byte = (u8_t*) p_headGuard;
    while (p_byte < (u8_t*) (&p_headGuard->chksum))
    {
        p_headGuard->chksum += *p_byte++;
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_DebugCreateTrailer() - this routine is used to populate a memory block trailer as part of *
 *                             the Linux Memory Debug library.  Whenever a block of memory is     *
 *                             obtained, a header and trailer are added.  The trailer consists of *
 *                             a sequence of one-byte numbers starting with 0xFE and rolling over *
 *                             after 0xFF.                                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*   p_rawBlock    - a pointer to the beginning of the raw memory block.  This is      *
 *                              where the header will be placed.                                  *
 *      size_t  trailerLength - the length of the trailer.                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
static void
KAL_DebugCreateTrailer(void*   p_rawBlock,
                       size_t  trailerLength)
{
    kalMemory_HeadGuardband_t*  p_headGuard;
    u8_t*                       p_byte;
    u8_t*                       p_trailer;
    u16_t                       index;
    u8_t                        trailerValue;

    /* Compute location of the start of the trailer.  */
    p_headGuard = (kalMemory_HeadGuardband_t*) p_rawBlock;
    p_byte = (u8_t*) p_headGuard;
    p_trailer = p_byte + sizeof(kalMemory_HeadGuardband_t) + p_headGuard->userAllocSize;

    /* Populate the trailer.  */
    trailerValue = KAL_MEMORY_DEBUG_TRAIL_START_VALUE;
    for (index=0; index<trailerLength; index++)
    {
        *p_trailer++ = trailerValue++;
    }
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_DebugVerifyHeader() - this routine is used to verify the integrity of a header guard of a *
 *                            memory block (as part of the Linux Memory Debug library).  The      *
 *                            header consists of a four-byte guard band, a two-byte length        *
 *                            containing the user buffer length, and a two-byte checksum of the   *
 *                            guard band and length.  This routine verifies the value of the      *
 *                            guard band, then verifies that the computed checksum matches the    *
 *                            stored value.  An assert is used for each check, so this routine    *
 *                            will never exit if an error is discovered.                          *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*   p_userData - a pointer to the user buffer within a memory block.  The header to   *
 *                           be verified exists prior to this pointer.                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
static void
KAL_DebugVerifyHeader(void*  p_userData)
{
    kalMemory_HeadGuardband_t*  p_headGuard;
    u8_t*                       p_byte;
    u16_t                       computedChksum=0;

    PSAL_ASSERT((u32_t) p_userData >= sizeof(kalMemory_HeadGuardband_t));

    p_headGuard = (kalMemory_HeadGuardband_t*) (((u8_t*) p_userData) -
                                                sizeof(kalMemory_HeadGuardband_t));
    PSAL_ASSERT(p_headGuard->hdrGuardbandValue == KAL_MEMORY_DEBUG_HEAD_GUARDBAND_VALUE);
    p_byte = (u8_t*) p_headGuard;
    while (p_byte < (u8_t*) (&p_headGuard->chksum))
    {
        computedChksum += *p_byte++;
    }
    PSAL_ASSERT(p_headGuard->chksum == computedChksum);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_DebugVerifyTrailer() - this routine is used to verify the integrity of a trailer guard of *
 *                             a memory block (as part of the Linux Memory Debug library).  The   *
 *                             trailer is simply a sequence of byte values starting at 0xFE and   *
 *                             wrapping after 0xFF.  In order to compute the start of the trailer *
 *                             guard, the header must be used.  It is assumed that the header has *
 *                             been verified (by the calling routine) prior to the invocation of  *
 *                             this routine.                                                      *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*   p_userData - a pointer to the user buffer within a memory block.  The header to   *
 *                           be verified exists prior to this pointer.                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
static void
KAL_DebugVerifyTrailer(void*  p_userData)
{
    kalMemory_HeadGuardband_t*  p_headGuard;
    u8_t*                       p_byte;
    u8_t*                       p_trailer;
    u16_t                       trailerLength;
    u16_t                       index;
    u8_t                        trailerValue;

    PSAL_ASSERT((u32_t) p_userData >= sizeof(kalMemory_HeadGuardband_t));

    /* Compute location of the start of the trailer.  */
    p_headGuard = (kalMemory_HeadGuardband_t*) (((u8_t*) p_userData) -
                                                sizeof(kalMemory_HeadGuardband_t));
    p_byte = (u8_t*) p_headGuard;
    p_trailer = p_byte + sizeof(kalMemory_HeadGuardband_t) + p_headGuard->userAllocSize;

    /* Compute the size of the trailer guard band.  */
    if (KAL_MEMORY_DEBUG_TRAIL_GUARDBAND_LENGTH == 0)
    {
        trailerLength = p_headGuard->userAllocSize;
    }
    else
    {
        trailerLength = KAL_MEMORY_DEBUG_TRAIL_GUARDBAND_LENGTH;
    }

    /* Populate the trailer.  */
    trailerValue = KAL_MEMORY_DEBUG_TRAIL_START_VALUE;
    for (index=0; index<trailerLength; index++)
    {
        PSAL_ASSERT(*p_trailer++ == trailerValue++);
    }
}


/**************************************************************************************************
 *                                                                                                *
 *  KAL_DebugHandleStatus() - this routine is used to handle status requests in the debug log.    *
 *                            If the current status has been requested, the log contains a        *
 *                            special tag "STATUS: <string>" so the post-processing tool(s) knows *
 *                            when the status is requested.  The <string> is passed in during the *
 *                            invocation of KAL_MemoryStatus().                                   *
 *                                                                                                *
 *                            Please note that a status request (i.e., an invocation of           *
 *                            KAL_MemoryStatus()) does not immediately create a debug log entry.  *
 *                            Instead, it marks that a status request is pending and will be      *
 *                            handled upon the next invocation of a dynamic memory                *
 *                            allocation/deallocation routine.  Therefore, it is possible for     *
 *                            multiple requests to have been made before any has been handled.    *
 *                            In this case, only the last request will be marked in the dxebug    *
 *                            log file.                                                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
static void
KAL_DebugHandleStatus(void)
{
    if (kalMemoryDebugStatus.statusRequested)
    {
        if (p_dynMemFile != NULL)
        {
            KAL_MemDbgGetLock();
            fprintf(p_dynMemFile, "STATUS: %s\n", kalMemoryDebugStatus.statusString);
            fflush(p_dynMemFile);
            KAL_MemDbgReleaseLock();
        }
        kalMemoryDebugStatus.statusRequested = false;
    }
}




static void
KAL_MemDbgGetLock(void)
{
    int  oldErrno=errno;

    while ((sem_wait(&kalMemorySemaphoreLock) == -1) && (errno == EINTR))
    {
        /* Do nothing and continue to wait for the semaphore.  */
    }

    errno = oldErrno;
}




static void
KAL_MemDbgReleaseLock(void)
{
    sem_post(&kalMemorySemaphoreLock);
}


#else  /* matches #if defined(KAL_DEBUG_MEMORY_LINUX)  */


/**************************************************************************************************
 *                                                                                                *
 *  Symbolic Constants                                                                            *
 *                                                                                                *
 **************************************************************************************************/
// If SERVER is enabled, then make the memory size limit bigger.
#if OVERRIDE_SL_OVERRIDE_SERVER_ENABLE
 #define KAL_MEMORY_MAX_ALLOCATE                (64*1024*1024)  /* 64M  */
#else
 #ifdef OVERRIDE_KAL_MEMORY_MAX_ALLOCATE
  #define KAL_MEMORY_MAX_ALLOCATE               OVERRIDE_KAL_MEMORY_MAX_ALLOCATE
 #else
  #define KAL_MEMORY_MAX_ALLOCATE               (16*1024)  /* Maximum size for dynamic allocation.  */
 #endif
#endif  // OVERRIDE_SL_OVERRIDE_SERVER_ENABLE


/**************************************************************************************************
 *                                                                                                *
 *  Global and File Scope Variables                                                               *
 *                                                                                                *
 **************************************************************************************************/
u32_t  kalMemoryMaxAllocate=KAL_MEMORY_MAX_ALLOCATE;








/**************************************************************************************************
 *                                                                                                *
 *  KAL_MemoryInit() - this routine is used to initialize the Kernel Abstraction Layer's dynamic  *
 *                     memory package.                                                            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
KAL_MemoryInit(void)
{
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_Malloc() - this routine is used to dynamically obtain memory from the heap.  The memory   *
 *                 block is not initialized.  A limit of the size is imposed for a single         *
 *                 allocation using this function.                                                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      size_t  size - the size of the contiguous memory chunk desired.                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void*: non-NULL - a pointer to the memory block, upon success.                            *
 *             NULL     - upon failure.                                                           *
 *                                                                                                *
 **************************************************************************************************/
void*
KAL_Malloc(size_t  size)
{
#ifdef KAL_ENABLE_MALLOC_FAIL_ASSERT
  void *pBuf;

  if (size <= kalMemoryMaxAllocate)
  {
    pBuf = malloc(size);
    PSAL_ASSERT(pBuf != NULL);
    return (pBuf);
  }

  PSAL_ASSERT_FORCE();
  return NULL;

#else

    if (size <= kalMemoryMaxAllocate)
    {
        return malloc(size);
    }

    return NULL;

#endif
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_Calloc() - this routine is used to dynamically allocate an array of "num" elements where  *
 *                 each element is "size" bytes.  The memory is initialized to zeroes.  There is  *
 *                 a limit on the size of a single allocation (kalMemoryMaxAllocate), but         *
 *                 since KAL_Calloc() is allocating an array of elements, this size limitation    *
 *                 will be placed on the individual element, not on the array of elements.        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      size_t  num  - the number of elements in the array.                                       *
 *      size_t  size - the size of each element, in bytes.                                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void*: non-NULL - a pointer to the memory block, upon success.                            *
 *             NULL     - upon failure.                                                           *
 *                                                                                                *
 **************************************************************************************************/
void*
KAL_Calloc(size_t  num,
           size_t  size)
{
    if (size <= kalMemoryMaxAllocate)
    {
        return calloc(num, size);
    }

    return NULL;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_Realloc() - this routine is used to change the size of a dynamically allocated memory     *
 *                  block.  If "ptr" is NULL, then the call will be equivalent to                 *
 *                  KAL_Malloc(size).  If "size" is zero and "ptr" is not NULL, then the call     *
 *                  will be equivalent to KAL_Free(ptr).                                          *
 *                                                                                                *
 *                  If the "size" is not zero and "ptr" is not NULL, then the allocation will be  *
 *                  limited in size to kalMemoryMaxAllocate bytes.  This internal limitation is   *
 *                  initialize to KAL_MEMORY_MAX_ALLOCATE, defined in kal_internal.h.             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*   ptr  - a pointer to dynamically allocated memory (or NULL).                       *
 *      size_t  size - the desired new size for the memory block.                                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void*: non-NULL - a pointer to the newly allocated  memory block, upon success.           *
 *             NULL     - upon failure.  Any block pointed to by "ptr" will remain unchanged in   *
 *                        this case.                                                              *
 *                                                                                                *
 **************************************************************************************************/
void*
KAL_Realloc(void*   ptr,
            size_t  size)
{
    if ((ptr != NULL) && (size > kalMemoryMaxAllocate))
    {
        return NULL;
    }

    return realloc(ptr, size);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_Free() - this routine is used to return dynamically allocated memory back to the heap.    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*  pointer - a pointer to a previously allocated block of memory (from a prior call   *
 *                       to KAL_Malloc(), KAL_Calloc(), or KAL_Realloc()).                        *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
KAL_Free(void*  pointer)
{
    free(pointer);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_FreePtr() - this routine is used to return dynamically allocated memory back to the heap. *
 *                  It is different from KAL_Free() in that the parameter is the address of a     *
 *                  pointer, and that the pointer is set to NULL within this routine.             *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void**  a_ptr - the address of a pointer to a previously allocated block of memory (from  *
 *                      a prior call to KAL_Malloc(), KAL_Calloc(), or KAL_Realloc()).            *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      <none>                                                                                    *
 *                                                                                                *
 **************************************************************************************************/
void
KAL_FreePtr(void**  a_ptr)
{
    free(*a_ptr);
    *a_ptr = NULL;
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_AllocateStatic() - this routine is used to perform a special one-time allocation of       *
 *                         memory which will never be returned to the system.                     *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      size_t  size - the size of the contiguous memory chunk desired.                           *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void*: non-NULL - a pointer to the memory block, upon success.                            *
 *             NULL     - upon failure.                                                           *
 *                                                                                                *
 **************************************************************************************************/
void*
KAL_AllocateStaticDebug(size_t  length)
{
    return malloc(length);
}


#endif  /* matches #else of #if defined(KAL_DEBUG_MEMORY_LINUX)  */


/**************************************************************************************************
 *                                                                                                *
 *  KAL_Memset() - this routine is used to set every byte in a contiguous segment to a single     *
 *                 byte value.                                                                    *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*   dest  - a pointer to where the memory will be set.                                *
 *      int     c     - the value to which memory will be set (when cast to a byte).              *
 *      size_t  count - the number of bytes to set.                                               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void* - a pointer to the destination buffer.                                              *
 *                                                                                                *
 **************************************************************************************************/
void*
KAL_Memset(void*   dest,
           int     c,
           size_t  count)
{
    if (dest == NULL) return NULL;

    return memset(dest, c, count);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_Memcpy() - this routine is used to copy data from one location to another.                *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*        dest  - a pointer to where a copy of the data should be placed.              *
 *      const void*  src   - a pointer where the original exists.                                 *
 *      size_t       count - the number of bytes to copy.                                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void* - a pointer to the destination buffer.                                              *
 *                                                                                                *
 **************************************************************************************************/
void*
KAL_Memcpy(void*        dest,
           const void*  src,
           size_t       count)
{
    if (src == NULL || dest == NULL)    return NULL;

    return memcpy(dest, src, count);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_MemcpyReverse() - this routine copies data from one location to another, but does it in   *
 *                        reverse memory order.  This is useful when the "src" and "dest"         *
 *                        buffers overlap:                                                        *
 *                                                                                                *
 *                            +------------------+-----+-----------------+                        *
 *                            |                  |     |                 |                        *
 *                            +------------------+-----+-----------------+                        *
 *                            |                  |     |                 |                        *
 *                            |                  |     end of src        end of dest              *
 *                            |                  dest                                             *
 *                            src                                                                 *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      void*        dest  - a pointer to where a copy of the data should be placed.              *
 *      const void*  src   - a pointer where the original exists.                                 *
 *      size_t       count - the number of bytes to copy.                                         *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      void* - a pointer to the destination buffer.                                              *
 *                                                                                                *
 **************************************************************************************************/
void*
KAL_MemcpyReverse(void*        dest,
                  const void*  src,
                  size_t       count)
{
    return memmove(dest, src, count);
}




/**************************************************************************************************
 *                                                                                                *
 *  KAL_Memcmp() - this routine compares the contents of two areas of memory.                     *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Parameters:                                                                                   *
 *      const void*  buf1  - a pointer to the first area of memory.                               *
 *      const void*  buf2  - a pointer to the second area of memory.                              *
 *      size_t       count - the number of bytes in the comparison.                               *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  Returns:                                                                                      *
 *      int: positive - if the contents of buf1 is less than buf2.                                *
 *           0        - if the contents of the two memories are equal.                            *
 *           negative - if the contents of buf1 is greater than buf2.                             *
 *                                                                                                *
 **************************************************************************************************/
int
KAL_Memcmp(const void*  buf1,
           const void*  buf2,
           size_t       count)
{
    if (buf1 == NULL || buf2 == NULL) return 1;
    return memcmp(buf1, buf2, count);
}




