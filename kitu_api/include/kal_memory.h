/******************************************************************************
 * GRID2HOME SMART ENERGY 2
 *****************************************************************************/
/**
 * Copyright (C) 2009 Grid2Home, Inc. All rights reserved. 
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Grid2Home, Inc and is confidential in nature.
 *
 * \file          kal_memory.h
 *
 * \version       $Revision: $
 *
 * \date          $Date: $
 *
 * \brief         Defines and structures for kal_memory.c and os_message.c
 *
 * \section info  Change Information
 *
 * \verbatim
   $History: $
 */

#ifndef KAL_MEMORY_H
#define KAL_MEMORY_H

/*******************************************************************************
 * Structures used by dynamic memory allocation
 ******************************************************************************/

typedef union Chunk
{
    u8_t            Buffer[MIN_BLOCK_SIZE];
    union Chunk     *Next;
} Chunk_t;

extern Chunk_t      *Heap;

/*******************************************************************************
 * Internal interface
 ******************************************************************************/

u8_t KAL_FreeChunk( u8_t iQ, Chunk_t *pBuff );


#endif // KAL_MEMORY_H
