/*******************
MemBlock.h
******************/

/* @file MemBlock.h
 * @brief Contains the "IMemoryBlock" Class-defintion.
 *        This is the (abstract) interface for the actual MemoryPool-Class.
 */

#ifndef MEMBLOCK_H
#define MEMBLOCK_H

#include "BasicIncludes.h"

namespace MemPool
{

// a char refers to a memory of 1 byte
typedef unsigned char TByte;

// interface of mem block
// implemented by the mem pool
class IMemoryBlock
{
  public :
    virtual ~IMemoryBlock(){};

    virtual void *GetMemory(const std::size_t &sMemorySize) = 0;
    virtual void FreeMemory(void *ptrMemoryBlock, const std::size_t &sMemoryBlockSize) = 0;
} ;

}

#endif