#if !defined(CREUSE_STACKALLOCATOR_H)
#define CREUSE_STACKALLOCATOR_H

#include "Creuse/Core/Config.h"
#include "Creuse/Core/Types.h"

#define CREUSE_ALIGNUP(address, alignmentBytes) ((((usize)(address)) + ((usize)(alignmentBytes)) - 1) & (~(((usize)(alignmentBytes)) - 1)))
#define CREUSE_ALIGNDOWN(address, alignmentBytes) ((((usize)(address))) & (~(((usize)(alignmentBytes)) - 1)))

struct StackAllocator
{
    uint8* MemoryBlock;
    uint8* Base;
    uint8* Cap;
    uint8* LowerHeap;
    uint8* UpperHeap;

    usize LowerPeak, UpperPeak; // The most bytes each heap has had in use at once; frames release memory, the peaks remember
};

enum class Heap : uint8
{
    Lower = 0,
    Upper
};

struct Frame
{
    uint8* Mark;
    Heap Heap;
};

bool InitStackAllocator(StackAllocator* allocator, usize size);
void ShutdownStackAllocator(StackAllocator* allocator);

#if CREUSE_PLATFORM_WINDOWS
    #include "Platform/Windows/Win32StackAllocator.h"
#else
    #error "Creuse: Unsupported platform for StackAllocator."
#endif

inline void* Allocate(StackAllocator* allocator, Heap heap, usize size, usize alignment)
{
    if(!allocator || size == 0 || alignment == 0)
    {
        return(nullptr);
    }

    // Validate alignment is power of two.
    if((alignment & (alignment - 1)) != 0)
    {
        return(nullptr);
    }

    usize lower = (usize)allocator->LowerHeap;
    usize upper = (usize)allocator->UpperHeap;

    void* memory;
    if(heap == Heap::Upper)
    {
        // From upper heap (down).
        if(size > upper - lower)
        {
            return(nullptr); // Out of memory
        }

        // NOTE(saeb): Growing down, so align down; aligning up would move back into memory already handed out.
        usize aligned = CREUSE_ALIGNDOWN(upper - size, alignment);
        if(aligned < lower)
        {
            return(nullptr); // Alignment padding collides with lower heap
        }

        allocator->UpperHeap = (uint8*)aligned;
        memory = (void*)aligned;
    }
    else
    {
        // From lower heap (up).
        usize aligned = CREUSE_ALIGNUP(lower, alignment);
        if(aligned > upper || size > upper - aligned)
        {
            return(nullptr); // Out of memory or collision
        }

        allocator->LowerHeap = (uint8*)(aligned + size);
        memory = (void*)aligned;
    }

    // NOTE(saeb): Only allocations can raise a peak, so this is the one place to track them.
    usize lowerUsed = (usize)(allocator->LowerHeap - allocator->Base);
    usize upperUsed = (usize)(allocator->Cap - allocator->UpperHeap);
    if(lowerUsed > allocator->LowerPeak)
    {
        allocator->LowerPeak = lowerUsed;
    }

    if(upperUsed > allocator->UpperPeak)
    {
        allocator->UpperPeak = upperUsed;
    }

    return(memory);
}

inline usize GetAvailableMemory(StackAllocator* allocator)
{
    if(!allocator)
    {
        return(0);
    }

    return(allocator->UpperHeap - allocator->LowerHeap);
}

inline usize GetUsedMemory(StackAllocator* allocator)
{
    if(!allocator)
    {
        return(0);
    }

    return((allocator->LowerHeap - allocator->Base) + (allocator->Cap - allocator->UpperHeap));
}

inline usize GetHeapUsedMemory(StackAllocator* allocator, Heap heap)
{
    if(!allocator)
    {
        return(0);
    }

    return((heap == Heap::Upper) ? (usize)(allocator->Cap - allocator->UpperHeap) : (usize)(allocator->LowerHeap - allocator->Base));
}

inline usize GetHeapPeakMemory(StackAllocator* allocator, Heap heap)
{
    if(!allocator)
    {
        return(0);
    }

    return((heap == Heap::Upper) ? allocator->UpperPeak : allocator->LowerPeak);
}

// NOTE(saeb): Brings both peaks down to what's in use now, so a peak can cover a window of time, such as the last second, instead of the whole run.
inline void ResetPeakMemory(StackAllocator* allocator)
{
    if(!allocator)
    {
        return;
    }

    allocator->LowerPeak = GetHeapUsedMemory(allocator, Heap::Lower);
    allocator->UpperPeak = GetHeapUsedMemory(allocator, Heap::Upper);
}

inline Frame GetFrame(StackAllocator* allocator, Heap heap)
{
    Frame frame;
    frame.Mark = (heap == Heap::Upper) ? allocator->UpperHeap : allocator->LowerHeap;
    frame.Heap = heap;

    return(frame);
}

inline void ReleaseFrame(StackAllocator* allocator, Frame frame)
{
    if(frame.Heap == Heap::Upper)
    {
        allocator->UpperHeap = frame.Mark;
    }
    else
    {
        allocator->LowerHeap = frame.Mark;
    }
}

#endif
