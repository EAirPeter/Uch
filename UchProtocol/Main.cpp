#include "Common.hpp"

#include "../UchCommon/ByteBuffer.hpp"
#include "../UchCommon/ByteChunk.hpp"
#include "../UchCommon/EventBus.hpp"
#include "../UchCommon/IoGroup.hpp"
#include "../UchCommon/System.hpp"

#include "Event.hpp"

#include <iostream>

#include <cryptopp/hex.h>

using namespace protocol;
using namespace std;

struct Handlers : HandlerBase<EvcPulse, EvcLoginReq> {
    Handlers(EventBus &bus) : vBus(bus) {
        vBus.Register(*this);
    }

    ~Handlers() {
        vBus.Unregister(*this);
    }

    void OnEvent(EvcPulse &e) noexcept override {
    }

    void OnEvent(EvcLoginReq &e) noexcept override {
        wcout << e.sUser;
    }

    EventBus &vBus;

};

int wmain() {
    System::GlobalStartup();
    IoGroup iog;
    iog.Start();
    EventBus bus {iog};
    Handlers h {bus};
    ByteBuffer<LinkedChunk<256>> b;
    EvcLoginReq e {L"233", ShaDigest {}};
    b << e;
    Byte by;
    b >> by;
    assert(by == EvcLoginReq::kEventId);
    bus.PostEvent(b.Read<EvcLoginReq>());
    FileChunkPool fcp;
    auto pch = fcp.MakeUnique();
    U64 vvx[] {123123, 98123891, 172323495827394572};
    pch->Write(vvx, (U32) sizeof(vvx));
    EvuFile ef {100, std::move(pch)};
    b << ef;
    b >> by;
    assert(by == EvuFile::kEventId);
    EvuFile eg {233, fcp.MakeUnique()};
    b >> eg;
    assert(eg.upChunk->GetReadable() == sizeof(vvx));
    assert(!memcmp(vvx, eg.upChunk->GetReader(), eg.upChunk->GetReadable()));
    iog.Shutdown();
    return 0;
}
