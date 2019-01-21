/******************
MemChunk.h
******************/

/* @file MemChunk.h
 * @brief Contains the "SMemoryChunk" Type-definition.
 */

#ifndef MemChunk_H
#define MemChunk_H

#include "MemBlock.h"

namespace MemPool
{

/* @class SMemoryChunk
 * @brief Memory Chunk Struct
 *
 * This struct will hold (and manage) the actual allocated Memory.
 * Every MemoryChunk will point to a MemoryBlock, and another SMemoryChunk,
 * thus creating a linked-list of MemoryChunks.
 */
typedef struct SMemoryChunk
{
  TByte *Data;				// The actual Data
  std::size_t DataSize;	// Size of the "Data"-Block
  std::size_t UsedSize;	// actual used Size
  bool IsAllocationChunk;	// true, when this MemoryChunks Points to a "Data"-Block which can be deallocated via "free()"
  SMemoryChunk *Next;		// Pointer to the Next MemoryChunk in the List (may be NULL)

} SMemoryChunk;

}

#endif
