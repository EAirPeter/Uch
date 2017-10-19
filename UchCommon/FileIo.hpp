#pragma once

#include "Common.hpp"

#include "ByteChunk.hpp"
#include "IoGroup.hpp"

template<class tChunk>
struct ExnFileRead {
    DWORD dwError;
    tChunk *pChunk;
};

template<class tChunk>
struct ExnFileWrite {
    DWORD dwError;
    std::unique_ptr<tChunk> upChunk;
};

template<class tUpper>
class FileIo {
public:
    using Upper = tUpper;

public:
    inline FileIo(Upper &vUpper, HANDLE hFile) noexcept : x_vUpper(vUpper), x_hFile(hFile) {
        LARGE_INTEGER vLi;
        GetFileSizeEx(x_hFile, &vLi);
        x_uSize = static_cast<U64>(vLi.QuadPart);
    }

    FileIo(const FileIo &) = delete;
    FileIo(FileIo &&) = delete;

    ~FileIo() {
        X_CloseFile();
    }

    FileIo &operator =(const FileIo &) = delete;
    FileIo &operator =(FileIo &&) = delete;

public:
    constexpr Upper &GetUpper() noexcept {
        return x_vUpper;
    }

    constexpr HANDLE GetNative() const noexcept {
        return x_hFile;
    }

    constexpr U64 GetSize() const noexcept {
        return x_uSize;
    }

public:
    inline void AssignToIoGroup(IoGroup &vIoGroup) {
        if (x_atmuState.fetch_or(x_kubAssigned) & x_kubAssigned)
            throw ExnIllegalState();
        x_pTpIo = vIoGroup.RegisterIo(x_hFile, this);
        x_pIoGroup = &vIoGroup;
    }

    inline void Shutdown() noexcept {
        Close();
    }

    inline void Close() noexcept {
        auto uState = x_atmuState.fetch_or(x_kubStopping);
        if (!(uState & x_kumPending))
            X_Finalize();
    }

    template<class tChunk>
    inline void PostRead(tChunk *pChunk) {
        pChunk->pfnIoCallback = X_FwdOnRead<tChunk>;
        auto dwToRead = static_cast<DWORD>(pChunk->GetWritable()) & 0xfffff000U;
        StartThreadpoolIo(x_pTpIo);
        auto uState = x_atmuState.fetch_add(1);
        if (!(uState & x_kubAssigned) || (uState & x_kubStopping)) {
            X_EndIo();
            throw ExnIllegalState {};
        }
        auto dwbRes = ReadFile(x_hFile, pChunk->GetWriter(), dwToRead, nullptr, pChunk);
        if (!dwbRes) {
            auto dwError = GetLastError();
            if (dwError != ERROR_IO_PENDING) {
                CancelThreadpoolIo(x_pTpIo);
                X_EndIo();
                throw ExnFileRead<tChunk> {dwError, pChunk};
            }
        }
    }

    template<class tChunk>
    inline void Write(std::unique_ptr<tChunk> upChunk) {
        upChunk->pfnIoCallback = X_FwdOnWrite<tChunk>;
        auto pChunk = upChunk.release();
        auto dwToWrite = static_cast<DWORD>(pChunk->GetReadable()) & 0xfffff000U;
        StartThreadpoolIo(x_pTpIo);
        auto uState = x_atmuState.fetch_add(1);
        if (!(uState & x_kubAssigned) || (uState & x_kubStopping)) {
            X_EndIo();
            throw ExnIllegalState {};
        }
        auto dwbRes = WriteFile(x_hFile, pChunk->GetReader(), dwToWrite, nullptr, pChunk);
        if (!dwbRes) {
            auto dwError = GetLastError();
            if (dwError != ERROR_IO_PENDING) {
                CancelThreadpoolIo(x_pTpIo);
                X_EndIo();
                throw ExnFileWrite<tChunk> {dwError, std::unique_ptr<tChunk>(pChunk)};
            }
        }
    }

private:
    inline void X_Finalize() noexcept {
        auto uState = x_atmuState.fetch_and(~x_kubAssigned);
        if (uState & x_kubAssigned)
            x_pIoGroup->UnregisterIo(x_pTpIo);
        uState = x_atmuState.fetch_or(x_kubFinalized);
        if (!(uState & x_kubFinalized))
            x_vUpper.OnFinalize();
    }

    inline void X_EndIo() noexcept {
        auto uState = x_atmuState.fetch_sub(1);
        if ((uState & x_kubStopping) && (uState & x_kumPending) == 1) {
            X_CloseFile();
            X_Finalize();
        }
    }

    inline void X_CloseFile() noexcept {
        auto hFile = reinterpret_cast<HANDLE>(InterlockedExchangePointer(&x_hFile, INVALID_HANDLE_VALUE));
        if (hFile != INVALID_HANDLE_VALUE)
            CloseHandle(hFile);
    }

private:
    template<class tChunk>
    inline void X_IocbOnRead(DWORD dwRes, U32 uDone, tChunk *pChunk) noexcept {
        if (dwRes)
            x_vUpper.OnRead(dwRes, 0, pChunk);
        else {
            pChunk->Skip(uDone);
            x_vUpper.OnRead(0, uDone, pChunk);
        }
        X_EndIo();
    }

    template<class tChunk>
    inline void X_IocbOnWrite(DWORD dwRes, U32 uDone, tChunk *pChunk) noexcept {
        if (dwRes)
            x_vUpper.OnWrite(dwRes, 0, std::unique_ptr<tChunk>(pChunk));
        else {
            pChunk->Discard(uDone);
            x_vUpper.OnWrite(0, uDone, std::unique_ptr<tChunk>(pChunk));
        }
        X_EndIo();
    }

    template<class tChunk>
    static void X_FwdOnRead(
        void *pParam, DWORD dwRes, U32 uDone, ChunkIoContext *pCtx
    ) noexcept {
        reinterpret_cast<FileIo *>(pParam)->X_IocbOnRead(dwRes, uDone, static_cast<tChunk *>(pCtx));
    }

    template<class tChunk>
    static void X_FwdOnWrite(
        void *pParam, DWORD dwRes, U32 uDone, ChunkIoContext *pCtx
    ) noexcept {
        reinterpret_cast<FileIo *>(pParam)->X_IocbOnWrite(dwRes, uDone, static_cast<tChunk *>(pCtx));
    }

private:
    constexpr static U32 x_kubStopping = 0x80000000;
    constexpr static U32 x_kubAssigned = 0x40000000;
    constexpr static U32 x_kubFinalized = 0x20000000;
    constexpr static U32 x_kumPending = 0x1fffffff;

private:
    Upper &x_vUpper;

    Mutex x_mtx;
    bool x_bFinalized = false;

    std::atomic<U32> x_atmuState = 0;

    IoGroup *x_pIoGroup = nullptr;
    PTP_IO x_pTpIo = nullptr;

    HANDLE x_hFile = INVALID_HANDLE_VALUE;

    U64 x_uSize;

};
