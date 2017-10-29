#pragma once

#include "Common.hpp"

template<USize kuSize, USize kuAlign>
struct AlignedStorage {
    alignas(kuAlign) Byte x_aby[kuSize];
};
