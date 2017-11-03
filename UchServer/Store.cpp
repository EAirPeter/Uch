#include "Common.hpp"

#include "../UchCommon/ByteBuffer.hpp"
#include "../UchCommon/ByteChunk.hpp"
#include "../UchCommon/System.hpp"

#include "Event.hpp"
#include "Store.hpp"
#include "Usv.hpp"

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>

void Store::Load() {
    RAII_LOCK(x_mtx);
    x_mapUsr.clear();
    x_mapUon.clear();
    x_setUff.clear();
    Buffer vBuf;
    X_ReadAll(vBuf);
    if (vBuf.IsEmpty())
        return;
    U16 uUsrs;
    vBuf >> uUsrs;
    while (uUsrs--) {
        X_Auth vAuth;
        vBuf >> vAuth;
        x_setUff.emplace(vAuth.sUser);
        auto sUser = vAuth.sUser;
        if (!x_mapUsr.emplace(std::move(sUser), std::move(vAuth)).second)
            throw ExnIllegalArg {};
    }
}

void Store::Save() {
    RAII_LOCK(x_mtx);
    Buffer vBuf;
    vBuf << static_cast<U16>(x_mapUsr.size());
    for (auto &p : x_mapUsr)
        vBuf << p.second;
    X_WriteAll(vBuf);
}

void Store::X_ReadAll(Buffer &vBuf) {
    auto uhFile = CreateFileHandle(kszFile, GENERIC_READ, OPEN_EXISTING, 0);
    LARGE_INTEGER vLi;
    if (!GetFileSizeEx(uhFile.get(), &vLi))
        throw ExnSys();
    auto uSize = static_cast<U32>(vLi.QuadPart);
    CryptoPP::CFB_Mode<CryptoPP::AES>::Decryption vDec {
        reinterpret_cast<const CryptoPP::byte *>(x_kAesKey), 32,
        reinterpret_cast<const CryptoPP::byte *>(x_kAesIv)
    };
    using UpByte = std::unique_ptr<Byte, decltype(&_aligned_free)>;
    UpByte upCipher(
        reinterpret_cast<Byte *>(_aligned_malloc(uSize, vDec.OptimalDataAlignment())),
        &_aligned_free
    );
    UpByte upPlaint(
        reinterpret_cast<Byte *>(_aligned_malloc(uSize, vDec.OptimalDataAlignment())),
        &_aligned_free
    );
    if (!ReadFile(uhFile.get(), upCipher.get(), static_cast<DWORD>(uSize), nullptr, nullptr))
        throw ExnSys();
    vDec.ProcessData(
        reinterpret_cast<CryptoPP::byte *>(upPlaint.get()),
        reinterpret_cast<CryptoPP::byte *>(upCipher.get()),
        uSize
    );
    vBuf.WriteBytes(upPlaint.get(), uSize);
}

void Store::X_WriteAll(Buffer &vBuf) {
    auto uSize = vBuf.GetSize();
    CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption vEnc {
        reinterpret_cast<const CryptoPP::byte *>(x_kAesKey), 32,
        reinterpret_cast<const CryptoPP::byte *>(x_kAesIv)
    };
    using UpByte = std::unique_ptr<Byte, decltype(&_aligned_free)>;
    UpByte upPlaint(
        reinterpret_cast<Byte *>(_aligned_malloc(uSize, vEnc.OptimalDataAlignment())),
        &_aligned_free
    );
    UpByte upCipher(
        reinterpret_cast<Byte *>(_aligned_malloc(uSize, vEnc.OptimalDataAlignment())),
        &_aligned_free
    );
    vBuf.ReadBytes(upPlaint.get(), uSize);
    vEnc.ProcessData(
        reinterpret_cast<CryptoPP::byte *>(upCipher.get()),
        reinterpret_cast<CryptoPP::byte *>(upPlaint.get()),
        uSize
    );
    auto uhFile = CreateFileHandle(kszFile, GENERIC_WRITE, CREATE_ALWAYS, 0);
    if (!WriteFile(uhFile.get(), upCipher.get(), static_cast<DWORD>(uSize), nullptr, nullptr))
        throw ExnSys();
}

protocol::EvsRegisRes Store::Register(const protocol::EvcRegisReq &e) {
    {
        RAII_LOCK(x_mtx);
        X_Auth vAuth {e.sUser, e.vPass, e.sQues, e.vAnsw, std::vector<protocol::ChatMessage> {}};
        if (!x_mapUsr.emplace(e.sUser, std::move(vAuth)).second)
            return {false, L"Duplicated user name"};
        x_setUff.emplace(e.sUser);
    }
    Usv::Bus().PostEvent(protocol::EvsNewUser {e.sUser});
    Usv::Bus().PostEvent(event::EvLog {
        String {L"["} + e.sUser + L"] registered."
    });
    return {true, L"Operation success"};
}

protocol::EvsRecoUserRes Store::RecoUser(const protocol::EvcRecoUserReq &e) {
    RAII_LOCK(x_mtx);
    auto it = x_mapUsr.find(e.sUser);
    if (it == x_mapUsr.end())
        return {false, L"No such user"};
    Usv::Bus().PostEvent(event::EvLog {
        String {L"["} + e.sUser + L"] is attempting to recover his password."
    });
    return {true, it->second.sQues};
}

protocol::EvsRecoPassRes Store::RecoPass(const protocol::EvcRecoPassReq &e) {
    RAII_LOCK(x_mtx);
    auto it = x_mapUsr.find(e.sUser);
    if (it == x_mapUsr.end())
        return {false, L"No such user"};
    auto &vAuth = it->second;
    if (std::memcmp(&vAuth.vAnsw, &e.vAnsw, sizeof(ShaDigest)))
        return {false, L"Wrong answer"};
    vAuth.vPass = e.vPass;
    Usv::Bus().PostEvent(event::EvLog {
        String {L"["} + e.sUser + L"] reset his password."
    });
    return protocol::EvsRecoPassRes {true, L"Operation success"};
}

protocol::EvsLoginRes Store::Login(const protocol::EvcLoginReq &e) {
    RAII_LOCK(x_mtx);
    auto it = x_mapUsr.find(e.vUser.sName);
    if (it == x_mapUsr.end()) {
        return {
            false, L"No such user",
            std::vector<protocol::OnlineUser> {},
            std::vector<String> {},
            std::vector<protocol::ChatMessage> {}
        };
    }
    auto &vAuth = it->second;
    if (std::memcmp(&vAuth.vPass, &e.vPass, sizeof(ShaDigest))) {
        return {
            false, L"Wrong password",
            std::vector<protocol::OnlineUser> {},
            std::vector<String> {},
            std::vector<protocol::ChatMessage> {}
        };
    }
    if (!x_mapUon.emplace(e.vUser.sName, e.vUser).second) {
        return {
            false, L"You have already logged in",
            std::vector<protocol::OnlineUser> {},
            std::vector<String> {},
            std::vector<protocol::ChatMessage> {}
        };
    }
    x_setUff.erase(e.vUser.sName);
    std::vector<protocol::OnlineUser> vecUon;
    for (auto &p : x_mapUon)
        vecUon.emplace_back(p.second);
    std::vector<String> vecUff(x_setUff.begin(), x_setUff.end());
    std::vector<protocol::ChatMessage> vecMsgs {};
    vecMsgs.swap(vAuth.vecMsgs);
    Usv::Bus().PostEvent(event::EvLog {
        String {L"["} + e.vUser.sName + L"] logged in, listening on "
            + static_cast<String>(e.vUser.vAddr) + L'.'
    });
    return protocol::EvsLoginRes {
        true, L"Operation success",
        std::move(vecUon), std::move(vecUff), std::move(vecMsgs)
    };
}

void Store::MessageTo(const String &sUser, const protocol::EvcMessageTo &e) {
    RAII_LOCK(x_mtx);
    auto it = x_mapUsr.find(e.sWhom);
    if (it == x_mapUsr.end())
        return;
    auto &vAuth = it->second;
    vAuth.vecMsgs.emplace_back(sUser, e.sMessage);
    Usv::Bus().PostEvent(event::EvLog {
        String {L"["} + sUser + L"] sent an offline message to [" + e.sWhom + L"]."
    });
}

void Store::Exit(const String &sUser) {
    RAII_LOCK(x_mtx);
    if (x_mapUon.erase(sUser))
        x_setUff.emplace(sUser);
    Usv::Bus().PostEvent(event::EvLog {
        String {L"["} + sUser + L"] logged out."
    });
}

std::vector<protocol::OnlineUser> Store::GetUon() {
    std::vector<protocol::OnlineUser> vecUon;
    RAII_LOCK(x_mtx);
    vecUon.reserve(x_mapUon.size());
    for (auto &p : x_mapUon)
        vecUon.emplace_back(p.second);
    return std::move(vecUon);
}

std::vector<String> Store::GetUff() {
    RAII_LOCK(x_mtx);
    return {x_setUff.begin(), x_setUff.end()};
}
