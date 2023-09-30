/**************************************************************************************************
 *                                                                                                *
 *  Grid2Home, Inc.     Copyright(C) 2011.  All rights reserved.                                  *
 *  This source code and any compilation or derivative thereof is the proprietary                 *
 *  information of Grid2Home, Inc and is confidential in nature.                                  *
 *                                                                                                *
 **************************************************************************************************
 *                                                                                                *
 *  fsal_rfs.c                                                                                    *
 *                                                                                                *
 *  This file contains the implementation of the File Abstraction Layer for the RAM-based         *
 *  file system (RFS).  It contains symbolic constants, type definitions, and function            *
 *  implementations.                                                                              *
 *                                                                                                *                                                                                                *
 **************************************************************************************************/


/* --------------------------------------------------------------------------------------------------
 *                                           Includes
 * --------------------------------------------------------------------------------------------------
 */
#include "fsal.h"
#include "psal_types.h"
#include "psal_assert.h"
#include "kal_api.h"
#include "string.h"


/* --------------------------------------------------------------------------------------------------
 *                                           Defines
 * --------------------------------------------------------------------------------------------------
 */
#define FD_CLOSED         (-1)
#define KAL_MALLOC_FSAL(size)                         KAL_Malloc(size)


/* --------------------------------------------------------------------------------------------------
 *                                             Typedefs
 * --------------------------------------------------------------------------------------------------
 */
typedef struct ramFile_s
{
  struct ramFile_s * pNext;
  i8_t fd;      /* fd < 0 means file is closed */
  u8_t mode;
  char filename[ FSAL_FILENAME_MAX ];
  u16_t iSeek;
  u16_t size;
  u8_t * pRam;
} ramFile_t;


/* --------------------------------------------------------------------------------------------------
 *                                           Local Functions
 * --------------------------------------------------------------------------------------------------
 */
static ramFile_t * FindFileFromFilename( char * filename );
static ramFile_t * FindFileFromFd( i8_t fd );
static i8_t GetUniqueFileDesc( void );
static u8_t FileDescInUse( fsalFd_t fd );


/* --------------------------------------------------------------------------------------------------
 *                                           Local Variables
 * --------------------------------------------------------------------------------------------------
 */

/* head of linked list of all existing files */
ramFile_t * fsal_pFirstFile = NULL;

/* current file of listing */
ramFile_t * fsal_pListCurrFile;

/* position in current read of directory */
u8_t  fsal_DirIsOpen;
ramFile_t  * fsal_pDirFile;

char fsal_DirFilter[ FSAL_FILENAME_MAX ];
u8_t fsal_DirFilterLen;


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
const char *  FSAL_About( void )
{
  static const char about[] = "FSAL/RFS - v0.5";

  return( about );
}


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
i8_t  FSAL_Init( void )
{
    ramFile_t * pFile;
    ramFile_t * pFileNext;

    /* designed to be re-initialized at any given moment */

    fsal_pListCurrFile = NULL;

    pFile = fsal_pFirstFile;
    fsal_pFirstFile = NULL;

    while( pFile != NULL )
    {
        pFileNext = pFile->pNext;
        KAL_Free( pFile->pRam );
        KAL_Free( pFile );
        pFile = pFileNext;
    }


    fsal_DirIsOpen = 0;

    return( FSAL_SUCCESS );
}


/****************************************************************************************************
 |   FSAL_Format() - Format the file system.
 |
 |   Parameters:
 |      none
 |
 |   Return value:
 |      zero     - success
 |      non-zero - fail
 ****************************************************************************************************
 */
i8_t  FSAL_Format( void )
{
  /* nothing to do, RAM based file system is always empty on start */
  return( FSAL_SUCCESS );
}


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
i32_t  FSAL_FreeSpace( void )
{
    i32_t freeSpace;

    /* space left is actually tied to heap memory left, give a dummy amount */
    freeSpace = 10000;

    return( freeSpace );
}


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
fsalFd_t  FSAL_Open( char * filename, u8_t mode )
{
    ramFile_t * pFile;
    fsalFd_t fd;

    PSAL_ASSERT(  ( mode == FSAL_MODE_READ )   ||
                  ( mode == FSAL_MODE_WRITE )  ||
                  ( mode == ( FSAL_MODE_READ + FSAL_MODE_WRITE ) )
                );

    /* see if file exists */
    pFile = FindFileFromFilename( filename );

    /* if file exists, make sure file is not already open */
    if( ( pFile != NULL ) && ( pFile->fd != FD_CLOSED ) )
    {
        return( FSAL_FILE_IS_OPEN );
    }

    /* if opening for read or read/write, the file must exist */
    if( ( mode & FSAL_MODE_READ ) && ( pFile == NULL ) )
    {
        return( FSAL_FILE_NOT_FOUND );
    }

    /* get a unique ID *before* adding to filedescriptor list */
    fd = GetUniqueFileDesc();

    /* if opening for just write, create a new file or overwrite existing one */
    if( mode == FSAL_MODE_WRITE )
    {
        /* if file does not exit, create it */
        if( pFile == NULL )
        {
            /* allocate memory for the new file */
            pFile = KAL_MALLOC_FSAL( sizeof( ramFile_t ) );
            if( pFile == NULL )
            {
                return( FSAL_MEM_ALLOC_FAIL );
            }

            /* populate file parameters */
            strncpy( pFile->filename, filename, FSAL_FILENAME_MAX );

            /* put newly created file into linked list of existing files */
            pFile->pNext    = fsal_pFirstFile;
            fsal_pFirstFile = pFile;
        }
        else
        {
            /* file exists, free previous data */
            if ( pFile->pRam != NULL )
            {
              KAL_Free( pFile->pRam );
            }
        }

        pFile->pRam = NULL;
        pFile->size = 0;
    }

    /* get a unique number for the file descriptor */
    if (pFile == NULL)
        return FSAL_MEM_ALLOC_FAIL;
        
    pFile->fd    = fd;
    pFile->mode  = mode;
    pFile->iSeek = 0;


    return( pFile->fd );
}


/****************************************************************************************************
 |   FSAL_Close() - Close a file.
 |
 |   Parameters:
 |      fd - file descriptor
 |
 |   Return value:
 |      zero     - success
 |      non-zero - fail
 ****************************************************************************************************
 */
i8_t  FSAL_Close( fsalFd_t fd )
{
    ramFile_t * pFile;

    PSAL_ASSERT( fd >= 0 );

    pFile = FindFileFromFd( fd );

    /* see if file exists */
    if( pFile == NULL )
    {
        return( FSAL_FILE_NOT_FOUND );
    }

    /* indicate file is closed */
    pFile->fd = FD_CLOSED;

    return( FSAL_SUCCESS );
}


/****************************************************************************************************
 |   FSAL_Read() - Read bytes from an open a file.
 |
 |   Parameters:
 |      fd - file descriptor
 |      pBuf - pointer to buffer where read bytes get stored
 |      numBytes - number of bytes to read from the file
 |
 |   Return value:
 |      number of bytes read
 |      <0 - fail
 ****************************************************************************************************
 */
i16_t  FSAL_Read( fsalFd_t fd, void * pBuf, i16_t numBytes )
{
    u16_t  i;
    i16_t  bytesReadable;
    u8_t * pFrom;
    u8_t * pTo;
    ramFile_t * pFile;

    /* find the file */
    pFile = FindFileFromFd( fd );
    if( pFile == NULL )
    {
        return( FSAL_FAIL );
    }

    /* make sure file is open for read */
    if( !( pFile->mode & FSAL_MODE_READ ) )
    {
        return( FSAL_FAIL );
    }

    /* determine bytes available to read from the file */
    bytesReadable = pFile->size - pFile->iSeek;

    /* if there are fewer bytes available to read than requested, adjust number of bytes to read */
    if( bytesReadable < numBytes )
    {
        numBytes = bytesReadable;
    }

    /* set memory read *from* seek offset, set memory write *to* user buffer */
    pFrom = pFile->pRam + pFile->iSeek;
    pTo   = (u8_t *)pBuf;

    /* copy number of bytes requested */
    for( i=0; i<numBytes; i++ )
    {
        *pTo++ = *pFrom++;
    }

    /* adjust seek offset */
    pFile->iSeek += numBytes;

    /* return actual number of bytes read (may be less that requested) */
    return( numBytes );
}


/****************************************************************************************************
 |   FSAL_Write() - Write bytes to an open a file.
 |
 |   Parameters:
 |      fd - file descriptor
 |      pBuf - pointer to buffer to write
 |      numBytes - number of bytes to write to file
 |
 |   Return value:
 |      number of byte written
 |      <0 - fail
 ****************************************************************************************************
 */
i16_t  FSAL_Write( fsalFd_t fd, void * pBuf, i16_t numBytes )
{
    u16_t  i;
    u8_t * pRamNew;
    u8_t * pTo;
    u8_t * pFrom;
    ramFile_t * pFile;
    u16_t newSize;

    /* cannot write to a file if directory is open */
    if( fsal_DirIsOpen )
    {
        return( FSAL_FAIL );
    }

    /* find the file */
    pFile = FindFileFromFd( fd );
    if( pFile == NULL )
    {
        return( FSAL_FILE_NOT_FOUND );
    }

    /* make sure file is open for writing */
    if( !( pFile->mode & FSAL_MODE_WRITE ) )
    {
        return( FSAL_FAIL );
    }

    /* see if write does not extend beyond end-of-file */
    if( ( numBytes + pFile->iSeek ) >  pFile->size )
    {
        /* calculate new file size */
        newSize = numBytes + pFile->iSeek;

        /* write goes beyond end of file's allocated memory so must allocated new memory */
        pRamNew = KAL_MALLOC_FSAL( newSize );
        if( pRamNew == NULL )
        {
            return( FSAL_MEM_ALLOC_FAIL );
        }

        /* copy of old ram to new ram */
        pFrom = pFile->pRam;
        pTo   = pRamNew;

        for( i=0; i < pFile->size; i++ )
        {
            *pTo++ = *pFrom++;
        }
        /* free the original memory and point to new memory */
        if (pFile->pRam != NULL)
          KAL_Free( pFile->pRam );
        pFile->pRam = pRamNew;
        pFile->size = newSize;
    }

    /* copy new data into file */
    pTo = pFile->pRam + pFile->iSeek;
    pFrom = (u8_t *)pBuf;
    for( i=0; i<numBytes; i++ )
    {
        *pTo++ = *pFrom++;
    }

    pFile->iSeek += numBytes;

    /* returns number of bytes written, for this file system will always be number requested to write */
    return( numBytes );
}


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
i8_t  FSAL_Seek( fsalFd_t fd, i16_t offset, u8_t whence )
{
    i16_t iSeek = -1;       // SPRK-1018
    ramFile_t * pFile;

    /* find the file */
    pFile = FindFileFromFd( fd );
    if( pFile == NULL )
    {
        return( FSAL_FAIL );
    }

    /* set seek index based on offset and whence */
    if( whence == FSAL_SEEK_SET )
    {
        /* offset is from beginning of file */
        iSeek = offset;
    }
    else if( whence == FSAL_SEEK_CUR )
    {
        /* offset from the current position in the file */
        iSeek = pFile->iSeek + offset;
    }
    else if( whence == FSAL_SEEK_END )
    {
        /* offset is from the end of the file (offset must be negative to be meaningful) */
        iSeek = pFile->size + offset;
    }
    else
    {
        /* invalid "whence" */
        PSAL_ASSERT_FORCE();
    }

    /* the new seek position must be within the size of the file */
    if( ( iSeek < 0 ) || ( iSeek > pFile->size ) )
    {
        return( FSAL_FAIL );
    }

    /* everything is good update the seek position */
    pFile->iSeek = iSeek;

    return( FSAL_SUCCESS );
}


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
i8_t  FSAL_Remove( char * filename )
{
    ramFile_t * pFilePrevious;
    ramFile_t * pFileCurrent;
    ramFile_t * pFileToRemove;

    /* cannot remove a file if directory is open */
    if( fsal_DirIsOpen )
    {
        return( FSAL_FAIL );
    }

    /* see if file exists */
    pFileToRemove = FindFileFromFilename( filename );
    if( pFileToRemove == NULL )
    {
        return( FSAL_FILE_NOT_FOUND );
    }

    /* make sure file is not open */
    if( pFileToRemove->fd != FD_CLOSED )
    {
        return( FSAL_FILE_IS_OPEN );
    }

    /* set up pointers for traversing list */
    pFilePrevious = NULL;
    pFileCurrent  = fsal_pFirstFile;

    /* find file in linked list and remove it */
    while( pFileCurrent != NULL )
    {
        /* see if this is the file to remove */
        if( pFileCurrent == pFileToRemove )
        {
            /* remove file from linked list chain */
            if( pFilePrevious != NULL )
            {
                pFilePrevious->pNext = pFileCurrent->pNext;
            }
            else
            {
                fsal_pFirstFile = pFileCurrent->pNext;
            }

            /* free up file's memory */
            if ( pFileCurrent->pRam != NULL)
            {
              KAL_Free( pFileCurrent->pRam );
            }
            KAL_Free( pFileCurrent );

            /* removed the file, return from here */
            return( FSAL_SUCCESS );
        }

        /* advance to next file in chain */
        pFilePrevious = pFileCurrent;
        pFileCurrent  = pFileCurrent->pNext;
    }

    /* if execution reaches this point, file was not found */
    return( FSAL_FILE_NOT_FOUND );
}


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
i8_t  FSAL_Rename( char * filename, char * newfilename )
{
    ramFile_t * pFile;

    /* cannot rename a file if directory is open */
    if( fsal_DirIsOpen )
    {
        return( FSAL_FAIL );
    }

    /* see if file exists */
    pFile = FindFileFromFilename( filename );
    if( pFile == NULL )
    {
        return( FSAL_FILE_NOT_FOUND );
    }

    /* make sure file is not open */
    if( pFile->fd != FD_CLOSED )
    {
        return( FSAL_FILE_IS_OPEN );
    }

    /* replace the filename */
    strncpy( pFile->filename, newfilename, FSAL_FILENAME_MAX );

    return( FSAL_SUCCESS );
}


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
i8_t  FSAL_Stat( char * filename, fsalStat_t * pStat )
{
    ramFile_t * pFile;

    /* find the file */
    pFile = FindFileFromFilename( filename );
    if( pFile == NULL )
    {
        return( FSAL_FILE_NOT_FOUND );
    }

    /* get statistics */
    pStat->filesize = pFile->size;
    pStat->timedate = 0;

    return( FSAL_SUCCESS );
}


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
i8_t  FSAL_OpenDir( char * filter )
{
    u8_t i;

    /* make sure directory is not already open */
    if( fsal_DirIsOpen )
    {
        return( FSAL_FAIL );
    }

    /* set static var to point at first file descriptor */
    fsal_pDirFile = fsal_pFirstFile;
    fsal_DirIsOpen = 1;

    /* set the filter */
    fsal_DirFilterLen = 0;
    if( filter != NULL )
    {
        /* copy until string termination, or maximum filename length */
        for( i=0; i<FSAL_FILENAME_MAX; i++ )
        {
            fsal_DirFilter[ i ] = filter[ i ];
            if( filter[ i ] == '\0')
            {
                break;
            }
        }
        fsal_DirFilterLen = i;
    }

    return( FSAL_SUCCESS );
}


/****************************************************************************************************
 |   FSAL_CloseDir()  - Open directory for read.
 |
 |   Parameters:
 |      none
 |
 |   Return value:
 |      zero     - success
 |      non-zero - fail
 ****************************************************************************************************
 */
i8_t  FSAL_CloseDir( void )
{
    fsal_DirIsOpen = 0;

    return( FSAL_SUCCESS );
}


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
i8_t  FSAL_ReadDir( fsalDirEnt_t * pFsalDirEnt )
{
    i8_t retVal;

    /* assume no match in remaining entries */
    retVal = FSAL_NO_DIR_ENTRY;

    /* if more entries keep looping */
    while( fsal_pDirFile != NULL )
    {
        /* see if filter matches (note: length of zero will always match ) */
        if( !strncmp( fsal_pDirFile->filename, fsal_DirFilter, fsal_DirFilterLen ) )
        {
            strncpy( pFsalDirEnt->filename, fsal_pDirFile->filename, FSAL_FILENAME_MAX );
            pFsalDirEnt->filename[ FSAL_FILENAME_MAX ] = '\0';
            pFsalDirEnt->stat.filesize = fsal_pDirFile->size;
            pFsalDirEnt->stat.timedate = 0; // for now
            retVal = FSAL_SUCCESS;
        }

         /* advance to next file in list */
         fsal_pDirFile = fsal_pDirFile->pNext;

         if( retVal == FSAL_SUCCESS )
         {
            break;
         }
    }

    /* did not find a match, at end of list */
    return( retVal );
}


/*===================================================================================================
 |   FindFileFromFilename() - Get pointer to file given its filename.
 |
 |   Parameters:
 |      filename - filename of file to find
 |
 |   Return value:
 |       0 - success
 |      <0 - fail
 *===================================================================================================
 */
static ramFile_t * FindFileFromFilename( char * filename )
{
    ramFile_t * pFile;

    pFile = fsal_pFirstFile;
    while( pFile != NULL )
    {
        if( !strncmp( filename, pFile->filename, FSAL_FILENAME_MAX ) )
        {
            break;
        }
        pFile = pFile->pNext;
    }

    return( pFile );
}


/*===================================================================================================
 |   FindFileFromFd() - Get pointer to file given its file descriptor.
 |
 |   Parameters:
 |      fd - file descriptor of file to find
 |
 |   Return value:
 |       0 - success
 |      <0 - fail
 *===================================================================================================
 */
static ramFile_t * FindFileFromFd( i8_t fd )
{
    ramFile_t * pFile;

    pFile = fsal_pFirstFile;
    while( pFile != NULL )
    {
        if( fd == pFile->fd )
        {
            break;
        }
        pFile = pFile->pNext;
    }

    return( pFile );
}


/*===================================================================================================
 |   GetUniqueFileDesc() - Get a unique file descriptor number
 |
 |   Parameters:
 |      none
 |
 |   Return value:
 |       0 - success
 |      <0 - fail
 *===================================================================================================
 */
static i8_t GetUniqueFileDesc( void )
{
    fsalFd_t i;

    /* brute force search each number starting at zero (there will not be *that* many open files) */
    for( i=0; i< 127; i++ )
    {
        if( !(FileDescInUse( i ) ) )
        {
            return( i );
        }
    }

    PSAL_ASSERT_FORCE(); /* something is amiss */
    return( 0xFF );
}


/*===================================================================================================
 |   FileDescInUse() - Determine if a particular file descriptor is currently being used.
 |
 |   Parameters:
 |      fd - file descriptor of file to find
 |
 |   Return value:
 |       0 - false, not in use
 |       1 - true, in use
 *===================================================================================================
 */
static u8_t FileDescInUse( fsalFd_t fd )
{
    ramFile_t * pFile;

    /* start at beginning of linked list */
    pFile = fsal_pFirstFile;

    /* loop through each element of linked list */
    while( pFile != NULL )
    {
        /* if a match was found, return the pointer to the file descriptor structure */
        if( pFile->fd == fd )
        {
            return( 1 ); /* Yes, it is in use */
        }

        /* advance to next element */
        pFile = pFile->pNext;
    }

    /* if execution reaches this point, file descriptor not found */
    return( 0 );  /* No, it is not in use */
}
