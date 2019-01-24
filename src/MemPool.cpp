/******************
MemPool.cpp
******************/

#include "BasicIncludes.h"
#include "MemChunk.h"
#include "MemPool.h"

namespace MemPool
{

static const int FREEED_MEMORY_CONTENT        = 0xAA ; //!< Value for freed memory  
static const int NEW_ALLOCATED_MEMORY_CONTENT = 0xFF ; //!< Initial Value for new allocated memory 

CMemoryPool::CMemoryPool(const std::size_t &sInitialMemoryPoolSize,
		                 const std::size_t &sMemoryChunkSize,
				         const std::size_t &sMinimalMemorySizeToAllocate,
				         bool bSetMemoryData)
{
    // initialize some parameters
    m_ptrFirstChunk  = NULL;
    m_ptrLastChunk   = NULL;
    m_ptrCursorChunk = NULL;

    m_sTotalMemoryPoolSize = 0;
    m_sUsedMemoryPoolSize  = 0;
    m_sFreeMemoryPoolSize  = 0;

    m_sMemoryChunkSize   = sMemoryChunkSize;
    m_uiMemoryChunkCount = 0;
    m_uiObjectCount      = 0;

    m_bSetMemoryData               = bSetMemoryData;
    m_sMinimalMemorySizeToAllocate = sMinimalMemorySizeToAllocate;

    // pre-allocated memory
    AllocateMemory(sInitialMemoryPoolSize);
}

CMemoryPool::~CMemoryPool()
{
    FreeAllAllocatedMemory();
    DeallocateAllChunks();
    
    // memory leaks?
    assert((m_uiObjectCount == 0) && "WARNING : Memory-Leak : You have not freed all allocated Memory");
}

void *CMemoryPool::GetMemory(const std::size_t &sMemorySize)
{
    std::size_t sBestMemBlockSize = CalculateBestMemoryBlockSize(sMemorySize);  
    SMemoryChunk *ptrChunk = NULL ;
    while(!ptrChunk)
    {
        // Is a Chunks available to hold the requested amount of Memory
        ptrChunk = FindChunkSuitableToHoldMemory(sBestMemBlockSize);
        if(!ptrChunk)
        {
            // No chunk can be found
            // => Memory-Pool is to small. We have to request 
            //    more Memory from the Operating-System....
            sBestMemBlockSize = MaxValue(sBestMemBlockSize, CalculateBestMemoryBlockSize(m_sMinimalMemorySizeToAllocate));
            AllocateMemory(sBestMemBlockSize);
        }
    }

    // Finally, a suitable Chunk was found.
    // Adjust the Values of the internal "TotalSize"/"UsedSize" Members and 
    // the Values of the MemoryChunk itself.
    m_sUsedMemoryPoolSize += sBestMemBlockSize;
    m_sFreeMemoryPoolSize -= sBestMemBlockSize;
    m_uiObjectCount++;
    SetMemoryChunkValues(ptrChunk, sBestMemBlockSize);

    // eventually, return the Pointer to the User
    return ((void *) ptrChunk->Data);
}

void CMemoryPool::FreeMemory(void *ptrMemoryBlock, const std::size_t &sMemoryBlockSize)
{
    // Search all Chunks for the one holding the "ptrMemoryBlock"-Pointer
    // ("SMemoryChunk->Data == ptrMemoryBlock"). Eventually, free that Chunks,
    // so it beecomes available to the Memory-Pool again...
    SMemoryChunk *ptrChunk = FindChunkHoldingPointerTo(ptrMemoryBlock);
    if(ptrChunk)
    {
        //std::cerr << "Freed Chunks OK (Used memPool Size : " << m_sUsedMemoryPoolSize << ")" << std::endl ;
        FreeChunks(ptrChunk);
    }
    else
    {
        assert(false && "ERROR : Requested Pointer not in Memory Pool");
    }
    assert((m_uiObjectCount > 0) && "ERROR : Request to delete more Memory then allocated.");
    m_uiObjectCount--;
}

bool CMemoryPool::AllocateMemory(const std::size_t &sMemorySize)
{
    unsigned int uiNeededChunks = CalculateNeededChunks(sMemorySize);
    std::size_t sBestMemBlockSize = CalculateBestMemoryBlockSize(sMemorySize);

    TByte *ptrNewMemBlock = (TByte *) malloc(sBestMemBlockSize) ; // allocate from Operating System
    SMemoryChunk *ptrNewChunks = (SMemoryChunk *) malloc((uiNeededChunks * sizeof(SMemoryChunk))) ; // allocate Chunk-Array to Manage the Memory
    assert(((ptrNewMemBlock) && (ptrNewChunks)) && "Error : System ran out of Memory");

    // Adjust internal Values (Total/Free Memory, etc.)
    m_sTotalMemoryPoolSize += sBestMemBlockSize;
    m_sFreeMemoryPoolSize += sBestMemBlockSize;
    m_uiMemoryChunkCount += uiNeededChunks;

    // Usefull for Debugging : Set the Memory-Content to a defined Value
    if(m_bSetMemoryData)
    {
        memset(((void *) ptrNewMemBlock), NEW_ALLOCATED_MEMORY_CONTENT, sBestMemBlockSize);
    }

    // Associate the allocated Memory-Block with the Linked-List of MemoryChunks
    return LinkChunksToData(ptrNewChunks, uiNeededChunks, ptrNewMemBlock);
}

unsigned int CMemoryPool::CalculateNeededChunks(const std::size_t &sMemorySize)
{
    float f = (float) (((float)sMemorySize) / ((float)m_sMemoryChunkSize));
    return ((unsigned int) ceil(f));
}

std::size_t CMemoryPool::CalculateBestMemoryBlockSize(const std::size_t &sRequestedMemoryBlockSize)
{
    unsigned int uiNeededChunks = CalculateNeededChunks(sRequestedMemoryBlockSize);
    return std::size_t((uiNeededChunks * m_sMemoryChunkSize));
}

void CMemoryPool::FreeChunks(SMemoryChunk *ptrChunk)
{
    // Make the Used Memory of the given Chunk available
    // to the Memory Pool again.

    SMemoryChunk *ptrCurrentChunk = ptrChunk;
    unsigned int uiChunkCount = CalculateNeededChunks(ptrCurrentChunk->UsedSize);
    for(unsigned int i = 0; i < uiChunkCount; i++)
    {
        if(ptrCurrentChunk)
        {
            // Step 1 : Set the allocated Memory to 'FREEED_MEMORY_CONTENT'
            // Note : This is fully Optional, but usefull for debugging
            if(m_bSetMemoryData)
            {
                memset(((void *) ptrCurrentChunk->Data), FREEED_MEMORY_CONTENT, m_sMemoryChunkSize);
            }

            // Step 2 : Set the Used-Size to Zero
            ptrCurrentChunk->UsedSize = 0;

            // Step 3 : Adjust Memory-Pool Values and goto next Chunk
            m_sUsedMemoryPoolSize -= m_sMemoryChunkSize;
            ptrCurrentChunk = ptrCurrentChunk->Next;
        }
    }
}

SMemoryChunk *CMemoryPool::FindChunkSuitableToHoldMemory(const std::size_t &sMemorySize)
{
    // Find a Chunk to hold *at least* "sMemorySize" Bytes.
    unsigned int uiChunksToSkip = 0;
    bool bContinueSearch = true;
    SMemoryChunk *ptrChunk = m_ptrCursorChunk; // Start search at Cursor Position.
    for(unsigned int i = 0; i < m_uiMemoryChunkCount; i++)
    {
        if(ptrChunk)
        {
            if(ptrChunk == m_ptrLastChunk) // End of List reached : Start over from the beginning
            {
                ptrChunk = m_ptrFirstChunk; // start over
            }

            if(ptrChunk->DataSize >= sMemorySize) // condition: datasize > given size
            {
                // never used
                if(ptrChunk->UsedSize == 0)
                {
                    m_ptrCursorChunk = ptrChunk;
                    return ptrChunk;
                }
            }
            uiChunksToSkip = CalculateNeededChunks(ptrChunk->UsedSize); // skip used memory
            if(uiChunksToSkip == 0)
            {
                uiChunksToSkip = 1;
            }
            ptrChunk = SkipChunks(ptrChunk, uiChunksToSkip);
        }
        else
        {
            bContinueSearch = false;
        }
    }
    return NULL;
}

// offset - skip the leading "uiChunksToSkip" chunks
SMemoryChunk *CMemoryPool::SkipChunks(SMemoryChunk *ptrStartChunk, unsigned int uiChunksToSkip)
{
	SMemoryChunk *ptrCurrentChunk = ptrStartChunk;
	for(unsigned int i = 0; i < uiChunksToSkip; i++)
	{
		if(ptrCurrentChunk)
		{
		   ptrCurrentChunk = ptrCurrentChunk->Next;
		}
		else
		{
			// Will occur, if you try to Skip more Chunks than actually available
			// from your "ptrStartChunk" 
			assert(false && "Error : Chunk == NULL was not expected.");
			break;
		}
	}
	return ptrCurrentChunk;
}

// set chunk as used
void CMemoryPool::SetMemoryChunkValues(SMemoryChunk *ptrChunk, const std::size_t &sMemBlockSize)
{
    if((ptrChunk)) // && (ptrChunk != m_ptrLastChunk))
    {
        ptrChunk->UsedSize = sMemBlockSize;
    }
    else
    {
        assert(false && "Error : Invalid NULL-Pointer passed");
    }
}

bool CMemoryPool::WriteMemoryDumpToFile(const std::string &strFileName)
{
    bool bWriteSuccesfull = false;
    std::ofstream ofOutputFile;
    ofOutputFile.open(strFileName.c_str(), std::ofstream::out | std::ofstream::binary);

    SMemoryChunk *ptrCurrentChunk = m_ptrFirstChunk;
    while(ptrCurrentChunk)
    {
        if(ofOutputFile.good())
        {
            ofOutputFile.write(((char *)ptrCurrentChunk->Data), ((std::streamsize) m_sMemoryChunkSize));
        bWriteSuccesfull = true;
        }
        ptrCurrentChunk = ptrCurrentChunk->Next;
    }
    ofOutputFile.close();
    return bWriteSuccesfull;
}

bool CMemoryPool::LinkChunksToData(SMemoryChunk *ptrNewChunks, unsigned int uiChunkCount, TByte *ptrNewMemBlock)
{
    SMemoryChunk *ptrNewChunk = NULL ;
    unsigned int uiMemOffSet = 0 ;
    bool bAllocationChunkAssigned = false ;
    for(unsigned int i = 0; i < uiChunkCount; i++)
    {
        if(!m_ptrFirstChunk)
        {
            // set the first chunk
            m_ptrFirstChunk = SetChunkDefaults(&(ptrNewChunks[0]));
            m_ptrLastChunk = m_ptrFirstChunk;
            m_ptrCursorChunk = m_ptrFirstChunk;
        }
        else
        {
            ptrNewChunk = SetChunkDefaults(&(ptrNewChunks[i]));
            m_ptrLastChunk->Next = ptrNewChunk;
            m_ptrLastChunk = ptrNewChunk;
        }
        
        uiMemOffSet = (i * ((unsigned int) m_sMemoryChunkSize));
        // allocate a segment to "Data"
        m_ptrLastChunk->Data = &(ptrNewMemBlock[uiMemOffSet]);

        // The first Chunk assigned to the new Memory-Block will be 
        // a "AllocationChunk". This means, this Chunks stores the
        // "original" Pointer to the MemBlock and is responsible for
        // "free()"ing the Memory later
        if(!bAllocationChunkAssigned)
        {
            m_ptrLastChunk->IsAllocationChunk = true;
            bAllocationChunkAssigned = true;
        }
    }
    return RecalcChunkMemorySize(m_ptrFirstChunk, m_uiMemoryChunkCount);
}
 
bool CMemoryPool::RecalcChunkMemorySize(SMemoryChunk *ptrChunk, unsigned int uiChunkCount)
{
    unsigned int uiMemOffSet = 0 ;
    for(unsigned int i = 0; i < uiChunkCount; i++)
    {
        if(ptrChunk)
        {
            uiMemOffSet = (i * ((unsigned int) m_sMemoryChunkSize));
            ptrChunk->DataSize = (((unsigned int) m_sTotalMemoryPoolSize) - uiMemOffSet);
            ptrChunk = ptrChunk->Next;
        }
        else
        {
            assert(false && "Error : ptrChunk == NULL");
            return false;
        }
    }
    return true;
}

// default values of a chunk
SMemoryChunk *CMemoryPool::SetChunkDefaults(SMemoryChunk *ptrChunk)
{
    if(ptrChunk)
    {
        ptrChunk->Data = NULL;
        ptrChunk->DataSize = 0;
        ptrChunk->UsedSize = 0;
        ptrChunk->IsAllocationChunk = false;
        ptrChunk->Next = NULL;
    }
    return ptrChunk;
}

SMemoryChunk *CMemoryPool::FindChunkHoldingPointerTo(void *ptrMemoryBlock)
{
	SMemoryChunk *ptrTempChunk = m_ptrFirstChunk;
	while(ptrTempChunk)
	{
        // found
		if(ptrTempChunk->Data == ((TByte *) ptrMemoryBlock))
		{
			break;
		}
		ptrTempChunk = ptrTempChunk->Next;
	}
	return ptrTempChunk;
}

// deallocate "Data" of each chunk
void CMemoryPool::FreeAllAllocatedMemory()
{
	SMemoryChunk *ptrChunk = m_ptrFirstChunk;
	while(ptrChunk)
	{
		if(ptrChunk->IsAllocationChunk)
		{
			free(((void *) (ptrChunk->Data)));
		}
		ptrChunk = ptrChunk->Next;
	}
}

void CMemoryPool::DeallocateAllChunks()
{
    SMemoryChunk *ptrChunk = m_ptrFirstChunk;
    SMemoryChunk *ptrChunkToDelete = NULL;
    // deallocate chunks one by one
    while(ptrChunk)
    {
        if(ptrChunk->IsAllocationChunk)
        {	
            if(ptrChunkToDelete)
            {
                free(((void *) ptrChunkToDelete));
            }
            ptrChunkToDelete = ptrChunk;
        }
        ptrChunk = ptrChunk->Next;
    }
}

bool CMemoryPool::IsValidPointer(void *ptrPointer)
{
    SMemoryChunk *ptrChunk = m_ptrFirstChunk;
	while(ptrChunk)
	{
		if(ptrChunk->Data == ((TByte *) ptrPointer))
		{
			return true;
		}
		ptrChunk = ptrChunk->Next;
	}
	return false;
}

std::size_t CMemoryPool::MaxValue(const std::size_t &sValueA, const std::size_t &sValueB) const
{
    if(sValueA > sValueB)
    {
        return sValueA;
    }
    return sValueB;
}

}