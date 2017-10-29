#pragma once

#include "Common.hpp"

#include "AlignedStorage.hpp"

// AES256-CFB referred to as Aes
// SHA256 referred to as Sha

constexpr static USize kuCryptoAlign = 16;

using AesIv = AlignedStorage<16, kuCryptoAlign>;
using AesKey = AlignedStorage<32, kuCryptoAlign>;
using ShaDigest = AlignedStorage<32, kuCryptoAlign>;
