#if !defined(CREUSE_STACKALLOCATOR_H)
#define CREUSE_STACKALLOCATOR_H

#include "Creuse/Core/Types.h"
#include "Creuse/Core/Config.h"
#include "Creuse/Core/Assert.h"

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

bool StackAllocatorInit(StackAllocator* allocator, usize size);
void StackAllocatorShutdown(StackAllocator* allocator);

#if CREUSE_PLATFORM_WINDOWS
    #include "Platform/Windows/Win32StackAllocator.h"
#else
    #error "Creuse: Unsupported platform for StackAllocator."
#endif

inline void* StackAllocatorAllocate(StackAllocator* allocator, Heap heap, usize size, usize alignment)
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

inline usize StackAllocatorGetAvailableMemory(StackAllocator* allocator)
{
    if(!allocator)
    {
        return(0);
    }

    return(allocator->UpperHeap - allocator->LowerHeap);
}

inline usize StackAllocatorGetUsedMemory(StackAllocator* allocator)
{
    if(!allocator)
    {
        return(0);
    }

    return((allocator->LowerHeap - allocator->Base) + (allocator->Cap - allocator->UpperHeap));
}

inline usize StackAllocatorGetHeapUsedMemory(StackAllocator* allocator, Heap heap)
{
    if(!allocator)
    {
        return(0);
    }

    return((heap == Heap::Upper) ? (usize)(allocator->Cap - allocator->UpperHeap) : (usize)(allocator->LowerHeap - allocator->Base));
}

inline usize StackAllocatorGetHeapPeakMemory(StackAllocator* allocator, Heap heap)
{
    if(!allocator)
    {
        return(0);
    }

    return((heap == Heap::Upper) ? allocator->UpperPeak : allocator->LowerPeak);
}

// NOTE(saeb): Brings both peaks down to what's in use now, so a peak can cover a window of time, such as the last second, instead of the whole run.
inline void StackAllocatorResetPeakMemory(StackAllocator* allocator)
{
    if(!allocator)
    {
        return;
    }

    allocator->LowerPeak = StackAllocatorGetHeapUsedMemory(allocator, Heap::Lower);
    allocator->UpperPeak = StackAllocatorGetHeapUsedMemory(allocator, Heap::Upper);
}

inline Frame StackAllocatorGetFrame(StackAllocator* allocator, Heap heap)
{
    Frame frame;
    frame.Heap = heap;

    if(!allocator)
    {
        frame.Mark = nullptr;
        return(frame);
    }

    frame.Mark = (heap == Heap::Upper) ? allocator->UpperHeap : allocator->LowerHeap;

    return(frame);
}

inline void StackAllocatorReleaseFrame(StackAllocator* allocator, Frame frame)
{
    if(!allocator || !frame.Mark)
    {
        return;
    }

    // NOTE(saeb): Frames must be released newest first; releasing an older frame and then a newer one would move the heap back over memory that was already given back.
    if(frame.Heap == Heap::Upper)
    {
        CREUSE_ASSERT_MSG(frame.Mark >= allocator->UpperHeap && frame.Mark <= allocator->Cap, "Upper heap frame released out of order");
        allocator->UpperHeap = frame.Mark;
    }
    else
    {
        CREUSE_ASSERT_MSG(frame.Mark <= allocator->LowerHeap && frame.Mark >= allocator->Base, "Lower heap frame released out of order");
        allocator->LowerHeap = frame.Mark;
    }
}

#endif
