/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2010 Grid2Home, Inc. All rights reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          fsal.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \brief         API for the File System Abstraction Layer (FSAL).
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef FSAL_H
#define FSAL_H


/* --------------------------------------------------------------------------------------------------
 *                                           Includes
 * --------------------------------------------------------------------------------------------------
 */
#include "psal_types.h"


/* --------------------------------------------------------------------------------------------------
 *                                            Defines
 * --------------------------------------------------------------------------------------------------
 */

#define FSAL_FILENAME_MAX       63

#define FSAL_MODE_READ          1
#define FSAL_MODE_WRITE         2

#define FSAL_SEEK_SET           1
#define FSAL_SEEK_END           2
#define FSAL_SEEK_CUR           3

#define FSAL_SUCCESS                  0
#define FSAL_FAIL                   (-1)
#define FSAL_MEM_ALLOC_FAIL         (-2)
#define FSAL_FILE_NOT_FOUND         (-3)
#define FSAL_TOO_MANY_FILES_OPEN    (-4)
#define FSAL_FILE_IS_OPEN           (-5)
#define FSAL_INTEGRITY_FAIL         (-6)
#define FSAL_FILE_NOT_OPEN          (-7)
#define FSAL_NO_DIR_ENTRY           (-8)
#define FSAL_DIRECTORY_FULL         (-9)
#define FSAL_NO_SPACE               (-10)
#define FSAL_NVM_FAIL               (-11)
#define FSAL_WRITE_ONLY             (-12)
#define FSAL_READ_ONLY              (-13)
#define FSAL_INVALID_SEEK           (-14)
#define FSAL_DIR_IS_OPEN            (-15)
#define FSAL_DIR_IS_CLOSED          (-16)
#define FSAL_FILE_EXISTS            (-17)
#define FSAL_CANNOT_CREATE_FD       (-18)


/* --------------------------------------------------------------------------------------------------
 *                                           Typedefs
 * --------------------------------------------------------------------------------------------------
 */
typedef struct fsalStat_s
{
  i16_t  filesize;
  u32_t  timedate;
} fsalStat_t;

typedef struct fsalDirEnt_s
{
  char filename[ FSAL_FILENAME_MAX + 1 ];  /* +1 so a null termination can be provided in all cases */
  fsalStat_t stat;
} fsalDirEnt_t;

typedef i8_t fsalFd_t;


/****************************************************************************************************
 |   FSAL_About() - Provide a string that briefly describes installed file system.
 |
 |   Parameters:
 |      none
 |
 |   Return value:
 |      Pointer to description string.
 ****************************************************************************************************
 */
const char *  FSAL_About( void );


/****************************************************************************************************
 |   FSAL_Init() - Initialize the file system.  Call before using any file system functionality.
 |
 |   Parameters:
 |      none
 |
 |   Return value:
 |      zero     - success
 |      non-zero - fail
 ****************************************************************************************************
 */
i8_t  FSAL_Init( void );


/****************************************************************************************************
 |   FSAL_Format() - Format the file system.  NOTE! Destroys all previous data.
 |
 |   Parameters:
 |      none
 |
 |   Return value:
 |      zero     - success
 |      non-zero - fail
 ****************************************************************************************************
 */
i8_t  FSAL_Format( void );


/****************************************************************************************************
 |   FSAL_FreeSpace() - Return number of free bytes available in the file system.
 |
 |   Parameters:
 |      none
 |
 |   Return value:
 |      number of bytes available
 |      <0 - error
 ****************************************************************************************************
 */
i32_t  FSAL_FreeSpace( void );


/****************************************************************************************************
 |   FSAL_Open() - Open a file for reading or writing.
 |
 |   Parameters:
 |      filename - name of the file
 |      mode     - access mode to open file with
 |                    FSAL_MODE_READ                   - read only mode
 |                    FSAL_MODE_WRITE                  - write only mode (erases any existing data)
 |                    FSAL_MODE_READ + FSAL_MODE_WRITE - modify mode
 |
 |   Return value:
 |      file descriptor
 |      <0 - fail
 ****************************************************************************************************
 */
fsalFd_t  FSAL_Open( char * filename, u8_t mode );


/****************************************************************************************************
 |   FSAL_Close() - Close a file.
 |
 |   Parameters:
 |      fd - file descriptor
 |
 |   Return value:
 |      none  (function calls assert if there is an error)
 ****************************************************************************************************
 */
i8_t  FSAL_Close( fsalFd_t fd );


/****************************************************************************************************
 |   FSAL_Read() - Read bytes from an open a file.
 |
 |   Parameters:
 |      fd - file descriptor
 |      pBuf - pointer to buffer where read bytes are stored
 |      numBytes - number of bytes to read from the file
 |
 |   Return value:
 |      number of bytes read
 |      <0 - fail
 ****************************************************************************************************
 */
i16_t  FSAL_Read( fsalFd_t fd, void * pBuf, i16_t numBytes );


/****************************************************************************************************
 |   FSAL_Write() - Write bytes to an open a file.
 |
 |   Parameters:
 |      fd - file descriptor as returned from open
 |      pBuf - pointer to buffer to write
 |      numBytes - number of bytes to write to file
 |
 |   Return value:
 |      number of byte written
 |      <0 - fail
 ****************************************************************************************************
 */
i16_t  FSAL_Write( fsalFd_t fd, void * pBuf, i16_t numBytes );


/****************************************************************************************************
 |   FSAL_Seek() - Set position within an open file.
 |
 |   Parameters:
 |      fd - file descriptor
 |      offset - relative offset to new position within file, offset can be negative
 |      whence - starting point within file from whence to apply position offset
 |                  FSAL_SEEK_SET - offset is from start of file
 |                  FSAL_SEEK_END - offset is from end of file
 |                  FSAL_SEEK_CUR - offset is from current position
 |
 |   Return value:
 |      zero     - success
 |      non-zero - fail
 ****************************************************************************************************
 */
i8_t  FSAL_Seek( fsalFd_t fd, i16_t offset, u8_t whence );


/****************************************************************************************************
 |   FSAL_Remove() - Delete a file.
 |
 |   Parameters:
 |      filename - name of file to delete
 |
 |   Return value:
 |      zero     - success
 |      non-zero - fail
 ****************************************************************************************************
 */
i8_t  FSAL_Remove( char * filename );


/****************************************************************************************************
 |   FSAL_Rename() - Rename a file.
 |
 |   Parameters:
 |      filename - name of file to rename
 |      newFilename - new name of file
 |
 |   Return value:
 |      zero     - success
 |      non-zero - fail
 ****************************************************************************************************
 */
i8_t  FSAL_Rename( char * filename, char * newFilename );


/****************************************************************************************************
 |   FSAL_Stat() - Get information about a file.
 |
 |   Parameters:
 |      filename - name of file
 |      pStat - pointer to where stat information gets stored
 |
 |   Return value:
 |      zero     - success
 |      non-zero - fail
 ****************************************************************************************************
 */
i8_t  FSAL_Stat( char * filename, fsalStat_t * pStat );


/****************************************************************************************************
 |   FSAL_OpenDir()  - Open directory for read.
 |
 |   Parameters:
 |      filter - text that filenames must start with to be listed
 |
 |   Return value:
 |      zero     - success
 |      non-zero - fail
 ****************************************************************************************************
 */
i8_t  FSAL_OpenDir( char * filter );


/****************************************************************************************************
 |   FSAL_CloseDir() - Close directory after done reading.
 |
 |   Parameters:
 |      none
 |
 |   Return value:
 |      zero     - success
 |      non-zero - fail
 ****************************************************************************************************
 */
i8_t  FSAL_CloseDir( void );


/****************************************************************************************************
 |   FSAL_ReadDir() - Read directory entry.
 |
 |   Parameters:
 |      pDirEnt - point to structure that is filled by function
 |
 |   Return value:
 |      zero     - success
 |      non-zero - no more entries (or failure)
 ****************************************************************************************************
 */
i8_t  FSAL_ReadDir( fsalDirEnt_t * pFsalDirEnt );


/****************************************************************************************************
 */
#endif  /* FSAL_H */
