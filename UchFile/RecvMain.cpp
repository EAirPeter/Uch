#include "Common.hpp"

class X_Receiver {
public:
    X_Receiver(HANDLE hFile, SOCKET hSocket, U64 uSize) :
        x_vFio(x_vProxy, hFile), x_vUcp(*this, hSocket), x_uzFileSize(uSize)
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
        Printf(
            L"recv-sec=%-10I64u send-sec=%-10I64u file-total=%I64u file-done=%I64u\n",
            uzRcvd, uzSent, x_uzFileSize, uzWritten
        );
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
    IoGroup x_vIogUcp {TP_CALLBACK_PRIORITY_HIGH, 10, GetProcessors()};
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

};

int RecvMain(int nArgs, wchar_t *apszArgs[]) {
    if (nArgs != 6)
        return EXIT_FAILURE;
    void *pFile, *pSocket, *pEvent;
    if (swscanf_s(apszArgs[2], L"%p", &pFile) != 1)
        return EXIT_FAILURE;
    if (swscanf_s(apszArgs[3], L"%p", &pSocket) != 1)
        return EXIT_FAILURE;
    if (swscanf_s(apszArgs[4], L"%p", &pEvent) != 1)
        return EXIT_FAILURE;
    U64 uSize;
    if (swscanf_s(apszArgs[5], L"%I64u", &uSize) != 1)
        return EXIT_FAILURE;
    auto hFile = reinterpret_cast<HANDLE>(pFile);
    auto hSocket = reinterpret_cast<SOCKET>(pSocket);
    auto hEvent = reinterpret_cast<HANDLE>(pEvent);
    DWORD dw;
    if (!GetHandleInformation(hFile, &dw))
        return EXIT_FAILURE;
    if (!GetHandleInformation(reinterpret_cast<HANDLE>(hSocket), &dw))
        return EXIT_FAILURE;
    if (!GetHandleInformation(hEvent, &dw))
        return EXIT_FAILURE;
    X_Receiver vRecv {hFile, hSocket, uSize};
    SetEvent(hEvent);
    vRecv.Wait();
    CloseHandle(hEvent);
    return 0;
}
