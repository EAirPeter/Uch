#include "Common.hpp"

#include "IpcData.hpp"

class X_Receiver {
public:
    X_Receiver(HANDLE hFile, SOCKET hSocket, U64 uSize, HANDLE hMapping, HANDLE hMutex) :
        x_vFio(x_vProxy, hFile), x_vUcp(*this, hSocket), x_uzFileSize(uSize), x_hMapping(hMapping), x_hMutex(hMutex)
    {
        x_vIogUcp.Start();
        x_vIogOth.Start();
        SetFileSize(
            hFile,
            (x_uzFileSize + FileChunk::kCapacity - 1) & ~static_cast<U64>(FileChunk::kCapacity - 1)
        );
        x_vUcp.AssignToIoGroup(x_vIogUcp);
        x_vFio.AssignToIoGroup(x_vIogOth);
        x_vIogOth.RegisterTick(*this);
    }

    ~X_Receiver() {
        x_vIogUcp.Shutdown();
        x_vIogOth.Shutdown();
    }

public:
    void Wait() {
        RAII_LOCK(x_mtx);
        while (!(x_bTikDone && x_bUcpDone && x_bFioDone))
            x_cv.Wait(x_mtx);
    }

    bool OnTick(U64 usNow) noexcept {
        UNREFERENCED_PARAMETER(usNow);
        {
            RAII_LOCK(x_mtx);
            if (x_bUcpDone) {
                x_bTikDone = true;
                x_cv.WakeAll();
                return false;
            }
        }
        auto uzRcvd = x_vUcp.GetReceivedSize();
        auto uzSent = x_vUcp.GetSentSize();
        uzRcvd -= std::exchange(x_uzRcvd, uzRcvd);
        uzSent -= std::exchange(x_uzSent, uzSent);
        auto uzWritten = x_atmuzFileWritten.load();
        x_vUcp.PostPacket(protocol::EvuProgress {uzWritten});
        WaitForSingleObject(x_hMutex, INFINITE);
        auto pIpc = reinterpret_cast<uchfile::IpcData *>(
            MapViewOfFile(x_hMapping, FILE_MAP_WRITE, 0, 0, sizeof(uchfile::IpcData))
        );
        pIpc->uzRcvdSec = static_cast<U32>(uzRcvd);
        pIpc->uzSentSec = static_cast<U32>(uzSent);
        pIpc->uzFile = uzWritten;
        UnmapViewOfFile(pIpc);
        ReleaseMutex(x_hMutex);
        return true;
    }

    void OnFinalize() noexcept {
        RAII_LOCK(x_mtx);
        x_bUcpDone = true;
        x_cv.WakeAll();
    }

    void OnForciblyClose() noexcept {}

    void OnPacket(U32 uSize, UcpBuffer &vBuf) noexcept {
        using namespace protocol;
        auto byId = vBuf.Read<Byte>();
        switch (byId) {
        case ucpfile::kFile:
            {
                EvuFile e;
                e.upChunk = x_vFcp.MakeUnique();
                vBuf >> e;
                e.upChunk->uzOnPacket = uSize;
                e.upChunk->uzPayload = e.upChunk->GetReadable();
                e.upChunk->IncWriter(e.upChunk->GetWritable());
                e.upChunk->Offset = static_cast<DWORD>(e.uOffset);
                e.upChunk->OffsetHigh = static_cast<DWORD>(e.uOffset >> 32);
                x_vFio.Write(e.upChunk.release());
            }
            break;
        case ucpfile::kFin:
            x_vUcp.PostPacket(EvuFinAck {});
            x_vUcp.EndOnPacket(uSize);
            x_vUcp.Shutdown();
            break;
        case ucpfile::kFinAck:
            x_vUcp.EndOnPacket(uSize);
            x_vUcp.Shutdown();
            break;
        default:
            assert(false);
        }
    }

private:
    struct X_Proxy {
        void OnFinalize() noexcept {
            auto hFile = p->x_vFio.GetNative();
            auto dwRes = GetFinalPathNameByHandleW(hFile, g_szWideBuf, STRCVT_BUFSIZE, 0);
            CloseHandle(hFile);
            auto uhFile = CreateFileHandle(String {g_szWideBuf, dwRes}, GENERIC_WRITE, OPEN_EXISTING, 0);
            SetFileSize(uhFile.get(), p->x_uzFileSize);
            uhFile.reset();
            RAII_LOCK(p->x_mtx);
            p->x_bFioDone = true;
            p->x_cv.WakeAll();
        }

        void OnForciblyClose() noexcept {}

        void OnWrite(DWORD dwError, U32 uDone, FileChunk *pChunk) noexcept {
            auto upChunk = p->x_vFcp.Wrap(pChunk);
            p->x_vUcp.EndOnPacket(upChunk->uzOnPacket);
            auto uzWritten = p->x_atmuzFileWritten.fetch_add(upChunk->uzPayload);
            if (uzWritten + upChunk->uzPayload >= p->x_uzFileSize) {
                p->x_vUcp.PostPacket(protocol::EvuFin {});
                p->x_vFio.Shutdown();
            }
        }

        X_Receiver *p;
    };

private:
    IoGroup x_vIogUcp {TP_CALLBACK_PRIORITY_HIGH, 8, GetProcessors()};
    IoGroup x_vIogOth {TP_CALLBACK_PRIORITY_NORMAL, 1000, 1};

    FileChunkPool x_vFcp;
    X_Proxy x_vProxy {this};

    FileIo<X_Proxy> x_vFio;
    Ucp<X_Receiver> x_vUcp;

    U64 x_uzFileSize = 0;
    std::atomic<U64> x_atmuzFileWritten = 0;
    
    U64 x_uzRcvd = 0;
    U64 x_uzSent = 0;

    bool x_bTikDone = false;
    bool x_bUcpDone = false;
    bool x_bFioDone = false;
    Mutex x_mtx;
    ConditionVariable x_cv;
    
    HANDLE x_hMapping;
    HANDLE x_hMutex;

};

int RecvMain(PCWSTR pszCmdLine) {
    UPtr uMapping, uMutex, uFile, uSocket, uEvent;
    U64 uSize;
    auto nRes = swscanf_s(
        pszCmdLine,
        L"%*s%*s%" CONCAT(L, SCNuPTR) L"%" CONCAT(L, SCNuPTR) L"%" CONCAT(L, SCNuPTR)
            L"%" CONCAT(L, SCNuPTR) L"%" CONCAT(L, SCNuPTR) L"%" CONCAT(L, SCNu64),
        &uMapping, &uMutex, &uFile, &uSocket, &uEvent, &uSize
    );
    if (nRes != 6)
        throw ExnIllegalArg {};
    auto hMapping = HandleCast(uMapping);
    auto hMutex = HandleCast(uMutex);
    auto hFile = HandleCast(uFile);
    auto hSocket = reinterpret_cast<SOCKET>(HandleCast(uSocket));
    auto hEvent = HandleCast(uEvent);
    X_Receiver vRecv {hFile, hSocket, uSize, hMapping, hMutex};
    SetEvent(hEvent);
    vRecv.Wait();
    CloseHandle(hEvent);
    constexpr static uchfile::IpcData vDone {~0, ~0, ~0};
    WaitForSingleObject(hMutex, INFINITE);
    auto pIpc = reinterpret_cast<uchfile::IpcData *>(
        MapViewOfFile(hMapping, FILE_MAP_WRITE, 0, 0, sizeof(uchfile::IpcData))
    );
    memcpy(pIpc, &vDone, sizeof(uchfile::IpcData));
    UnmapViewOfFile(pIpc);
    ReleaseMutex(hMutex);
    return 0;
}
