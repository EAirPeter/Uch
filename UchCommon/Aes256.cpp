#include "Common.hpp"

#include "Aes256.hpp"

#pragma comment(lib, "intel_aes.lib")

#include <iaes_asm_interface.h>

Aes256::Aes256(const void *pKey, U64 uInitialCounter) noexcept : x_uInitialCounter(uInitialCounter) {
    alignas(16) Byte abyKey[32];
    std::memcpy(abyKey, pKey, 32);
    iEncExpandKey256(abyKey, x_abyExpandedKey);
}

void Aes256::Encrypt(Byte *pCipher, Byte *pPlain, USize uSize) noexcept {
    assert(!(uSize & 0x0f));
    alignas(16) U64 uCounter = x_uInitialCounter;
    sAesData vAesData {pPlain, pCipher, x_abyExpandedKey, reinterpret_cast<Byte *>(&uCounter), uSize >> 4};
    iEnc256_CTR(&vAesData);
}
