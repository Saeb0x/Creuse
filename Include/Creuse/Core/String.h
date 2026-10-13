#if !defined(CREUSE_STRING_H)
#define CREUSE_STRING_H

#include "Types.h"
#include "Creuse/Memory/StackAllocator.h"

using char8 = char8_t;
using char16 = char16_t;

struct StringView8
{
    const char8* Data;
    usize Length;
};

struct String8
{
    char8* Data;
    usize Length;
    usize Capacity;
};

struct StringView16
{
    const char16* Data;
    usize Length;
};

struct String16
{
    char16* Data;
    usize Length;
    usize Capacity;
};

struct String8Decoded
{
    uint32 Codepoint;
    usize Length;
};

struct String16Decoded
{
    uint32 Codepoint;
    usize Length;
};

inline StringView8 SV8(const char8* data)
{
    usize length = 0;
    while(data[length] != '\0')
    {
        length++;
    }

    return(StringView8{ data, length });
}

inline StringView16 SV16(const char16* data)
{
    usize length = 0;
    while(data[length] != '\0')
    {
        length++;
    }

    return(StringView16{ data, length });
}

inline String8Decoded String8DecodeNext(StringView8 view, usize index)
{
    // NOTE(saeb): Invalid input decodes to U+FFFD and consumes one byte, so a bad byte never swallows the valid bytes after it.
    const uint32 Replacement = 0xFFFD;
    char8 lead = view.Data[index];

    // 1-byte (ASCII).
    if((lead & 0x80) == 0)
    {
        return(String8Decoded{ lead, 1 });
    }

    usize length;
    uint32 codepoint;
    uint32 minimum;

    if((lead & 0xE0) == 0xC0)
    {
        length = 2;
        codepoint = lead & 0x1F;
        minimum = 0x80;
    }
    else if((lead & 0xF0) == 0xE0)
    {
        length = 3;
        codepoint = lead & 0x0F;
        minimum = 0x800;
    }
    else if((lead & 0xF8) == 0xF0)
    {
        length = 4;
        codepoint = lead & 0x07;
        minimum = 0x10000;
    }
    else
    {
        // Continuation byte without a lead, or 0xF8..0xFF.
        return(String8Decoded{ Replacement, 1 });
    }

    // Truncated at the end of the view.
    if(length > view.Length - index)
    {
        return(String8Decoded{ Replacement, 1 });
    }

    for(usize i = 1; i < length; ++i)
    {
        char8 next = view.Data[index + i];
        if((next & 0xC0) != 0x80)
        {
            return(String8Decoded{ Replacement, 1 });
        }

        codepoint = (codepoint << 6) | (next & 0x3F);
    }

    // Overlong, UTF-16 surrogate, or beyond Unicode.
    if(codepoint < minimum || (codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint > 0x10FFFF)
    {
        return(String8Decoded{ Replacement, 1 });
    }

    return(String8Decoded{ codepoint, length });
}

inline String16Decoded String16DecodeNext(StringView16 view, usize index)
{
    // NOTE(saeb): An unpaired surrogate decodes to U+FFFD and consumes one unit, matching String8DecodeNext; Windows paths and filenames can contain them.
    const uint32 Replacement = 0xFFFD;
    char16 lead = view.Data[index];

    // Not a surrogate (BMP).
    if(lead < 0xD800 || lead > 0xDFFF)
    {
        return(String16Decoded{ lead, 1 });
    }

    // Low surrogate without a high one before it.
    if(lead >= 0xDC00)
    {
        return(String16Decoded{ Replacement, 1 });
    }

    // High surrogate at the end of the view, or not followed by a low one.
    if(index + 1 >= view.Length)
    {
        return(String16Decoded{ Replacement, 1 });
    }

    char16 trail = view.Data[index + 1];
    if(trail < 0xDC00 || trail > 0xDFFF)
    {
        return(String16Decoded{ Replacement, 1 });
    }

    uint32 codepoint = 0x10000 + (((uint32)lead - 0xD800) << 10) + ((uint32)trail - 0xDC00);

    return(String16Decoded{ codepoint, 2 });
}

inline usize SV8ToSV16Length(StringView8 view)
{
    usize length = 0;

    for(usize i = 0; i < view.Length;)
    {
        String8Decoded decoded = String8DecodeNext(view, i);

        // Above the BMP needs a surrogate pair.
        length += (decoded.Codepoint >= 0x10000) ? 2 : 1;
        i += decoded.Length;
    }

    return(length);
}

inline usize String8EncodedLength(uint32 codepoint)
{
    if(codepoint < 0x80)
    {
        return(1);
    }

    if(codepoint < 0x800)
    {
        return(2);
    }

    if(codepoint < 0x10000)
    {
        return(3);
    }

    return(4);
}

inline usize SV16ToSV8Length(StringView16 view)
{
    usize length = 0;

    for(usize i = 0; i < view.Length;)
    {
        String16Decoded decoded = String16DecodeNext(view, i);

        length += String8EncodedLength(decoded.Codepoint);
        i += decoded.Length;
    }

    return(length);
}

inline StringView16 SV8ToSV16(StackAllocator* allocator, StringView8 view)
{
    if(!allocator)
    {
        return(StringView16{ nullptr, 0 });
    }

    usize bufferCapacity = SV8ToSV16Length(view) + 1;
    char16* buffer = (char16*)Allocate(allocator, Heap::Upper, bufferCapacity * sizeof(char16), alignof(char16));
    if(!buffer)
    {
        return(StringView16{ nullptr, 0 });
    }

    usize bufferIndex = 0;

    for(usize i = 0; i < view.Length;)
    {
        String8Decoded decoded = String8DecodeNext(view, i);

        if(decoded.Codepoint >= 0x10000)
        {
            // Surrogate pair.
            uint32 offset = decoded.Codepoint - 0x10000;
            buffer[bufferIndex++] = (char16)(0xD800 + (offset >> 10));
            buffer[bufferIndex++] = (char16)(0xDC00 + (offset & 0x3FF));
        }
        else
        {
            buffer[bufferIndex++] = (char16)decoded.Codepoint;
        }

        i += decoded.Length;
    }

    buffer[bufferIndex] = u'\0';

    return(StringView16{ buffer, bufferIndex });
}

inline StringView8 SV16ToSV8(StackAllocator* allocator, StringView16 view)
{
    if(!allocator)
    {
        return(StringView8{ nullptr, 0 });
    }

    usize bufferCapacity = SV16ToSV8Length(view) + 1;
    char8* buffer = (char8*)Allocate(allocator, Heap::Upper, bufferCapacity * sizeof(char8), alignof(char8));
    if(!buffer)
    {
        return(StringView8{ nullptr, 0 });
    }

    usize bufferIndex = 0;

    for(usize i = 0; i < view.Length;)
    {
        String16Decoded decoded = String16DecodeNext(view, i);
        uint32 codepoint = decoded.Codepoint;

        if(codepoint < 0x80)
        {
            buffer[bufferIndex++] = (char8)codepoint;
        }
        else if(codepoint < 0x800)
        {
            buffer[bufferIndex++] = (char8)(0xC0 | (codepoint >> 6));
            buffer[bufferIndex++] = (char8)(0x80 | (codepoint & 0x3F));
        }
        else if(codepoint < 0x10000)
        {
            buffer[bufferIndex++] = (char8)(0xE0 | (codepoint >> 12));
            buffer[bufferIndex++] = (char8)(0x80 | ((codepoint >> 6) & 0x3F));
            buffer[bufferIndex++] = (char8)(0x80 | (codepoint & 0x3F));
        }
        else
        {
            buffer[bufferIndex++] = (char8)(0xF0 | (codepoint >> 18));
            buffer[bufferIndex++] = (char8)(0x80 | ((codepoint >> 12) & 0x3F));
            buffer[bufferIndex++] = (char8)(0x80 | ((codepoint >> 6) & 0x3F));
            buffer[bufferIndex++] = (char8)(0x80 | (codepoint & 0x3F));
        }

        i += decoded.Length;
    }

    buffer[bufferIndex] = u8'\0';

    return(StringView8{ buffer, bufferIndex });
}

inline String8 String8FromView(StackAllocator* allocator, StringView8 view)
{
    if(!allocator || view.Length == 0)
    {
        return(String8{ nullptr, 0, 0 });
    }

    // Allocate with null terminator.
    char8* data = (char8*)Allocate(allocator, Heap::Lower, (view.Length + 1) * sizeof(char8), alignof(char8));
    if(!data)
    {
        return(String8{ nullptr, 0, 0 });
    }

    // Copy data.
    for(usize i = 0; i < view.Length; i++)
    {
        data[i] = view.Data[i];
    }
    data[view.Length] = '\0';

    return(String8{ data, view.Length, view.Length + 1 });
}

inline String16 String16FromView(StackAllocator* allocator, StringView16 view)
{
    if(!allocator || view.Length == 0)
    {
        return(String16{ nullptr, 0, 0 });
    }

    // Allocate with null terminator.
    char16* data = (char16*)Allocate(allocator, Heap::Lower, (view.Length + 1) * sizeof(char16), alignof(char16));
    if(!data)
    {
        return(String16{ nullptr, 0, 0 });
    }

    // Copy data.
    for(usize i = 0; i < view.Length; i++)
    {
        data[i] = view.Data[i];
    }
    data[view.Length] = '\0';

    return(String16{ data, view.Length, view.Length + 1 });
}

inline String8 String8FromLiteral(StackAllocator* allocator, const char8* literal)
{
    return(String8FromView(allocator, SV8(literal)));
}

inline String16 String16FromLiteral(StackAllocator* allocator, const char16* literal)
{
    return(String16FromView(allocator, SV16(literal)));
}

inline String8 String8Reserve(StackAllocator* allocator, usize capacity)
{
    if(!allocator || capacity == 0)
    {
        return(String8{ nullptr, 0, 0 });
    }

    char8* data = (char8*)Allocate(allocator, Heap::Lower, capacity * sizeof(char8), alignof(char8));
    if(!data)
    {
        return(String8{ nullptr, 0, 0 });
    }

    return(String8{ data, 0, capacity });
}

inline bool String8Append(String8* string, StringView8 view)
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
inline char8* String8WriteDigitsBackward(char8* end, uint64 value)
{
    char8* start = end;
    do
    {
        *--start = (char8)(u8'0' + (value % 10));
        value /= 10;
    } while(value > 0);

    return(start);
}

inline bool String8AppendUInt(String8* string, uint64 value)
{
    char8 text[20];
    char8* start = String8WriteDigitsBackward(text + sizeof(text), value);

    return(String8Append(string, StringView8{ start, (usize)(text + sizeof(text) - start) }));
}

inline bool String8AppendInt(String8* string, int64 value)
{
    // NOTE(saeb): Negated as unsigned, so the most negative int64, which has no positive counterpart, still prints correctly.
    uint64 magnitude = (value < 0) ? (0 - (uint64)value) : (uint64)value;

    char8 text[21]; // 20 digits and a sign
    char8* start = String8WriteDigitsBackward(text + sizeof(text), magnitude);
    if(value < 0)
    {
        *--start = u8'-';
    }

    return(String8Append(string, StringView8{ start, (usize)(text + sizeof(text) - start) }));
}

// NOTE(saeb): Fixed-point, rounded to the nearest: (2.345, 2) is "2.35" (up to double's own precision), (-0.001, 2) is "0.00", never "-0.00". decimals is capped at 9. Prints "nan", "inf" or "-inf" for those values, and "big" or "-big" for finite values too large to show exactly (1e19 or more once scaled by the decimals).
inline bool String8AppendReal(String8* string, real64 value, uint32 decimals)
{
    if(value != value)
    {
        return(String8Append(string, StringView8{ u8"nan", 3 }));
    }

    bool negative = value < 0.0;
    real64 magnitude = negative ? -value : value;

    // NOTE(saeb): x - x is 0 for every finite x and NaN for infinity.
    if(magnitude - magnitude != 0.0)
    {
        return(negative ? String8Append(string, StringView8{ u8"-inf", 4 }) : String8Append(string, StringView8{ u8"inf", 3 }));
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
        return(negative ? String8Append(string, StringView8{ u8"-big", 4 }) : String8Append(string, StringView8{ u8"big", 3 }));
    }

    uint64 fixed = (uint64)scaled;

    char8 text[32]; // Sign, 20 whole digits, the point and 9 decimals, with room to spare
    char8* start = text + sizeof(text);

    // Fraction first, since everything is written backwards; keeps leading zeros, so 0.05 is "05", not "5".
    if(decimals > 0)
    {
        uint64 fraction = fixed % power;
        for(uint32 i = 0; i < decimals; i++)
        {
            *--start = (char8)(u8'0' + (fraction % 10));
            fraction /= 10;
        }

        *--start = u8'.';
    }

    start = String8WriteDigitsBackward(start, fixed / power);
    if(negative && fixed > 0)
    {
        *--start = u8'-';
    }

    return(String8Append(string, StringView8{ start, (usize)(text + sizeof(text) - start) }));
}

#endif
