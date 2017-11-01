#pragma once

#include "Common.hpp"

class Store {
    friend class Usv;

    using Buffer = ByteBuffer<LinkedChunk<4096>>;

private:
#   define UEV_NAME X_Auth
#   define UEV_NOTEV
#   define UEV_MEMBERS UEV_VAL(String, sUser) UEV_VAL(ShaDigest, vPass) \
    UEV_VAL(String, sQues) UEV_VAL(ShaDigest, vAnsw) \
    UEV_END(std::vector<protocol::ChatMessage>, vecMsgs)
#   include "../UchCommon/GenEvent.inl"

public:
    void Load();
    void Save();

private:
    void X_ReadAll(Buffer &vBuf);
    void X_WriteAll(Buffer &vBuf);

public:
    protocol::EvsRegisRes Register(const protocol::EvcRegisReq &e);
    protocol::EvsRecoUserRes RecoUser(const protocol::EvcRecoUserReq &e);
    protocol::EvsRecoPassRes RecoPass(const protocol::EvcRecoPassReq &e);
    protocol::EvsLoginRes Login(const protocol::EvcLoginReq &e);
    void MessageTo(const String &sUser, const protocol::EvcMessageTo &e);
    void Exit(const String &sUser);

    std::vector<protocol::OnlineUser> GetUon();
    std::vector<String> GetUff();

private:
    std::unordered_map<String, X_Auth> x_mapUsr;
    std::unordered_set<String> x_setUff;
    std::unordered_map<String, protocol::OnlineUser> x_mapUon;
    
    Mutex x_mtx;

private:
    constexpr static Byte x_kAesKey[32] {
        0xcb, 0x1e, 0x63, 0x21, 0xec, 0x74, 0x44, 0xe3,
        0x9b, 0x58, 0x18, 0x56, 0x60, 0x21, 0xfc, 0xd7,
        0xba, 0x1c, 0x49, 0xe7, 0x7a, 0x9d, 0x4a, 0x08,
        0x9f, 0xc0, 0x3d, 0x84, 0xbf, 0x6a, 0x1e, 0xd0,
    };
    constexpr static Byte x_kAesIv[16] {
        0x4a, 0x9e, 0x3e, 0x3d, 0xf0, 0x75, 0x45, 0xdd,
        0xb9, 0xd6, 0xe9, 0x41, 0xb3, 0xad, 0xb6, 0x93,
    };

public:
    constexpr static decltype(auto) kszFile = L"UchStore.dat";

};
