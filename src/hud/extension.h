#pragma once
#include <cstddef>
#include <cstdint>
namespace Atrium::Hud
{
struct Name
{
    uint32_t Index;
    uint32_t Number;
};
struct Extension
{
    uint8_t Slot;
    uint8_t Padding[7];
    uint8_t WeakPointer[16];
    Name Path;
    void *SubPath;
    int SubPathNum;
    int SubPathMax;
};
struct Array
{
    Extension *Data;
    int Num;
    int Max;
};
static_assert(sizeof(Extension) == 0x30 && offsetof(Extension, Path) == 0x18);
static_assert(sizeof(Array) == 0x10);
enum class Result
{
    Added,
    Present,
    Invalid,
    AllocationFailed
};
template <class Reallocate> Result Append(Array &array, Name path, Reallocate reallocate)
{
    if (!path.Index || array.Num < 0 || array.Max < array.Num || array.Max > 128 ||
        (array.Max && !array.Data))
        return Result::Invalid;
    for (int i = 0; i < array.Num; ++i)
        if (array.Data[i].Path.Index == path.Index && array.Data[i].Path.Number == path.Number)
            return Result::Present;
    if (array.Num == array.Max)
    {
        if (array.Max == 128)
            return Result::Invalid;
        const auto capacity = array.Num + 1;
        const auto data = static_cast<Extension *>(reallocate(array.Data, capacity * sizeof(Extension)));
        if (!data)
            return Result::AllocationFailed;
        array.Data = data;
        array.Max = capacity;
    }
    Extension extension{};
    extension.Path = path;
    array.Data[array.Num++] = extension;
    return Result::Added;
}
} 
