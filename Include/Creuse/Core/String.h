#if !defined(CREUSE_STRING_H)
#define CREUSE_STRING_H

#include "Types.h"
#include "Creuse/Memory/StackAllocator.h"

struct StringView
{
    const char* Data;
    usize Length;
};

struct String
{
    char* Data;
    usize Length;
    usize Capacity;
};

inline StringView StringViewFromCString(const char* data)
{
    usize length = 0;
    while(data[length] != '\0')
    {
        length++;
    }

    return(StringView{ data, length });
}

inline String StringFromView(StackAllocator* allocator, StringView view)
{
    if(!allocator || view.Length == 0)
    {
        return(String{ nullptr, 0, 0 });
    }

    // Allocate with null terminator.
    char* data = (char*)StackAllocatorAllocate(allocator, Heap::Lower, (view.Length + 1) * sizeof(char), alignof(char));
    if(!data)
    {
        return(String{ nullptr, 0, 0 });
    }

    // Copy data.
    for(usize i = 0; i < view.Length; i++)
    {
        data[i] = view.Data[i];
    }
    data[view.Length] = '\0';

    return(String{ data, view.Length, view.Length + 1 });
}

inline String StringFromCString(StackAllocator* allocator, const char* data)
{
    return(StringFromView(allocator, StringViewFromCString(data)));
}

inline String StringReserve(StackAllocator* allocator, usize capacity)
{
    if(!allocator || capacity == 0)
    {
        return(String{ nullptr, 0, 0 });
    }

    char* data = (char*)StackAllocatorAllocate(allocator, Heap::Lower, capacity * sizeof(char), alignof(char));
    if(!data)
    {
        return(String{ nullptr, 0, 0 });
    }

    return(String{ data, 0, capacity });
}

inline bool StringAppend(String* string, StringView view)
{
    if(!string || view.Length == 0)
    {
        return(true);
    }

    if(string->Length + view.Length >= string->Capacity)
    {
        return(false); // Not enough capacity
    }

    for(usize i = 0; i < view.Length; i++)
    {
        string->Data[string->Length + i] = view.Data[i];
    }

    string->Length += view.Length;
    string->Data[string->Length] = '\0';

    return(true);
}

// NOTE(saeb): Writes value's decimal digits so they end just before end, and returns where they start. Digits come out lowest first, so they're written backwards; a uint64 has at most 20.
inline char* StringWriteDigitsBackward(char* end, uint64 value)
{
    char* start = end;
    do
    {
        *--start = (char)('0' + (value % 10));
        value /= 10;
    } while(value > 0);

    return(start);
}

inline bool StringAppendUInt(String* string, uint64 value)
{
    char text[20];
    char* start = StringWriteDigitsBackward(text + sizeof(text), value);

    return(StringAppend(string, StringView{ start, (usize)(text + sizeof(text) - start) }));
}

inline bool StringAppendInt(String* string, int64 value)
{
    // NOTE(saeb): Negated as unsigned, so the most negative int64, which has no positive counterpart, still prints correctly.
    uint64 magnitude = (value < 0) ? (0 - (uint64)value) : (uint64)value;

    char text[21]; // 20 digits and a sign
    char* start = StringWriteDigitsBackward(text + sizeof(text), magnitude);
    if(value < 0)
    {
        *--start = '-';
    }

    return(StringAppend(string, StringView{ start, (usize)(text + sizeof(text) - start) }));
}

// NOTE(saeb): Fixed-point, rounded to the nearest: (2.345, 2) is "2.35" (up to double's own precision), (-0.001, 2) is "0.00", never "-0.00". decimals is capped at 9. Prints "nan", "inf" or "-inf" for those values, and "big" or "-big" for finite values too large to show exactly (1e19 or more once scaled by the decimals).
inline bool StringAppendReal(String* string, real64 value, uint32 decimals)
{
    if(value != value)
    {
        return(StringAppend(string, StringView{ "nan", 3 }));
    }

    bool negative = value < 0.0;
    real64 magnitude = negative ? -value : value;

    // NOTE(saeb): x - x is 0 for every finite x and NaN for infinity.
    if(magnitude - magnitude != 0.0)
    {
        return(negative ? StringAppend(string, StringView{ "-inf", 4 }) : StringAppend(string, StringView{ "inf", 3 }));
    }

    if(decimals > 9)
    {
        decimals = 9;
    }

    uint64 power = 1;
    for(uint32 i = 0; i < decimals; i++)
    {
        power *= 10;
    }

    real64 scaled = magnitude * (real64)power + 0.5;
    if(scaled >= 1.0e19) // A uint64 tops out near 1.8e19
    {
        return(negative ? StringAppend(string, StringView{ "-big", 4 }) : StringAppend(string, StringView{ "big", 3 }));
    }

    uint64 fixed = (uint64)scaled;

    char text[32]; // Sign, 20 whole digits, the point and 9 decimals, with room to spare
    char* start = text + sizeof(text);

    // Fraction first, since everything is written backwards; keeps leading zeros, so 0.05 is "05", not "5".
    if(decimals > 0)
    {
        uint64 fraction = fixed % power;
        for(uint32 i = 0; i < decimals; i++)
        {
            *--start = (char)('0' + (fraction % 10));
            fraction /= 10;
        }

        *--start = '.';
    }

    start = StringWriteDigitsBackward(start, fixed / power);
    if(negative && fixed > 0)
    {
        *--start = '-';
    }

    return(StringAppend(string, StringView{ start, (usize)(text + sizeof(text) - start) }));
}

#endif
