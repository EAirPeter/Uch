#pragma once

#include "Common.hpp"

// weakened AES 256-CTR
class Aes256 {
public:
    Aes256() = delete;
    constexpr Aes256(const Aes256 &) noexcept = default;
    constexpr Aes256(Aes256 &&) noexcept = default;

    Aes256(const void *pKey, U64 uInitialCounter) noexcept;

    constexpr Aes256 &operator =(const Aes256 &) noexcept = default;
    constexpr Aes256 &operator =(Aes256 &&) noexcept = default;

public:
    constexpr const void *GetExpandedKey() const noexcept {
        return x_abyExpandedKey;
    }

    constexpr U64 GetInitialCounter() const noexcept {
        return x_uInitialCounter;
    }

    void Encrypt(Byte *pCipher, Byte *pPlain, USize uSize) noexcept;

    inline void Decrypt(Byte *pPlain, Byte *pCipher, USize uSize) noexcept {
        Encrypt(pPlain, pCipher, uSize);
    }

private:
    alignas(16) Byte x_abyExpandedKey[256];
    U64 x_uInitialCounter;

};
