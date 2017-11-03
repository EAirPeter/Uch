#include "Common.hpp"

class X_Sender {
public:
    X_Sender(HANDLE hFile, SOCKET hSocket) :
        x_vFio(x_vProxy, hFile), x_vUcp(*this, hSocket), x_uzFileSize(x_vFio.GetSize())
    {
        x_vIogUcp.Start();
        x_vIogOth.Start();
        x_atmucFileChunks.store(x_kucFileChunks);
        x_vUcp.AssignToIoGroup(x_vIogUcp);
        x_vFio.AssignToIoGroup(x_vIogOth);
        x_vIogOth.RegisterTick(*this);
    }

    ~X_Sender() {
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
        {
            RAII_LOCK(x_mtx);
            if (x_bUcpDone) {
                x_bTikDone = true;
                x_cv.WakeAll();
                return false;
            }
        }
        if (x_vUcp.GetQueueSize() < x_kuzUcpQueThresh) {
            while (x_atmucFileChunks.fetch_sub(1)) {
                auto uzOffset = x_atmuzFileOffset.fetch_add(FileChunk::kCapacity);
                if (uzOffset < x_uzFileSize) {
                    auto upChunk = x_vFcp.MakeUnique();
                    upChunk->Offset = static_cast<DWORD>(uzOffset);
                    upChunk->OffsetHigh = static_cast<DWORD>(uzOffset >> 32);
                    x_vFio.PostRead(upChunk.release());
                }
                else {
                    x_vFio.Shutdown();
                }
            }
            x_atmucFileChunks.fetch_add(1);
        }
        if (StampDue(usNow, x_usNextSec)) {
            x_usNextSec = usNow + 1'000'000;
            auto uzRcvd = x_vUcp.GetReceivedSize();
            auto uzSent = x_vUcp.GetSentSize();
            uzRcvd -= std::exchange(x_uzRcvd, uzRcvd);
            uzSent -= std::exchange(x_uzSent, uzSent);
            auto uzRead = x_atmuzFileRead.load();
            Printf(
                L"recv-sec=%-10I64u send-sec=%-10I64u file-total=%I64u file-read=%I64u\n",
                uzRcvd, uzSent, x_uzFileSize, uzRead
            );
        }
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
            CloseHandle(p->x_vFio.GetNative());
            RAII_LOCK(p->x_mtx);
            p->x_bFioDone = true;
            p->x_cv.WakeAll();
        }

        void OnForciblyClose() noexcept {}

        void OnRead(DWORD dwError, U32 uDone, FileChunk *pChunk) noexcept {
            auto upChunk = p->x_vFcp.Wrap(pChunk);
            p->x_atmuzFileRead.fetch_add(uDone);
            auto uzOffset = static_cast<U64>(upChunk->OffsetHigh) << 32 | upChunk->Offset;
            p->x_vUcp.PostPacket(protocol::EvuFile {
                uzOffset, std::move(upChunk)
            });
            p->x_atmucFileChunks.fetch_add(1);
        }

        X_Sender *p;
    };

private:
    IoGroup x_vIogUcp {TP_CALLBACK_PRIORITY_HIGH, 10, GetProcessors()};
    IoGroup x_vIogOth {TP_CALLBACK_PRIORITY_NORMAL, 200, 1};

    FileChunkPool x_vFcp;
    X_Proxy x_vProxy {this};

    FileIo<X_Proxy> x_vFio;
    Ucp<X_Sender> x_vUcp;

    std::atomic<U32> x_atmucFileChunks = 0;

    U64 x_uzFileSize = 0;
    std::atomic<U64> x_atmuzFileRead = 0;
    std::atomic<U64> x_atmuzFileOffset = 0;
    
    U64 x_uzRcvd = 0;
    U64 x_uzSent = 0;
    U64 x_usNextSec = 0;

    bool x_bTikDone = false;
    bool x_bUcpDone = false;
    bool x_bFioDone = false;
    Mutex x_mtx;
    ConditionVariable x_cv;

private:
    constexpr static U32 x_kucFileChunks = 4;
    constexpr static U32 x_kuzUcpQueThresh = FileChunk::kCapacity * x_kucFileChunks;

};

int SendMain(int nArgs, wchar_t *apszArgs[]) {
    if (nArgs != 4)
        return EXIT_FAILURE;
    void *pFile, *pSocket;
    if (swscanf_s(apszArgs[2], L"%p", &pFile) != 1)
        return EXIT_FAILURE;
    if (swscanf_s(apszArgs[3], L"%p", &pSocket) != 1)
        return EXIT_FAILURE;
    auto hFile = reinterpret_cast<HANDLE>(pFile);
    auto hSocket = reinterpret_cast<SOCKET>(pSocket);
    DWORD dw;
    if (!GetHandleInformation(hFile, &dw))
        return EXIT_FAILURE;
    if (!GetHandleInformation(reinterpret_cast<HANDLE>(hSocket), &dw))
        return EXIT_FAILURE;
    X_Sender vSend {hFile, hSocket};
    vSend.Wait();
    return 0;
}
