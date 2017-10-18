#pragma once

#include "Common.hpp"

#include "ByteChunk.hpp"
#include "IoGroup.hpp"

struct ExnFileRead {
    DWORD dwError;
    ByteChunk *pChunk;
};

struct ExnFileWrite {
    DWORD dwError;
    std::unique_ptr<ByteChunk> upChunk;
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
        RAII_LOCK(x_mtx);
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
        RAII_LOCK(x_mtx);
        if (x_pIoGroup)
            throw ExnIllegalState();
        x_pTpIo = vIoGroup.RegisterIo(x_hFile, this);
        x_pIoGroup = &vIoGroup;
    }

    // does not cancel pending send requests
    inline void Shutdown() noexcept {
        RAII_LOCK(x_mtx);
        X_Shutdown();
    }

    // cancels all pending requests
    inline void Close() noexcept {
        RAII_LOCK(x_mtx);
        X_Close();
    }

    inline void PostRead(ByteChunk *pChunk) {
        pChunk->pfnIoCallback = X_FwdOnRead;
        RAII_LOCK(x_mtx);
        if (!x_pIoGroup || x_bStopping)
            throw ExnIllegalState {};
        DWORD dwFlags = 0;
        StartThreadpoolIo(x_pTpIo);
        auto dwToRead = static_cast<DWORD>(pChunk->GetWritable()) & 0xfffff000U;
        auto dwbRes = ReadFile(x_hFile, pChunk->GetWriter(), dwToRead, nullptr, pChunk);
        ++x_uRead;
        if (!dwbRes) {
            auto dwError = GetLastError();
            if (dwError != ERROR_IO_PENDING) {
                CancelThreadpoolIo(x_pTpIo);
                X_EndRead();
                throw ExnFileRead {dwError, pChunk};
            }
        }
    }

    inline void Write(std::unique_ptr<ByteChunk> upChunk) {
        upChunk->pfnIoCallback = X_FwdOnWrite;
        RAII_LOCK(x_mtx);
        if (!x_pIoGroup || x_bStopping)
            throw ExnIllegalState {};
        auto pChunk = upChunk.release();
        StartThreadpoolIo(x_pTpIo);
        auto dwToWrite = static_cast<DWORD>(pChunk->GetReadable()) & 0xfffff000U;
        auto dwbRes = WriteFile(x_hFile, pChunk->GetReader(), dwToWrite, nullptr, pChunk);
        ++x_uWrite;
        if (!dwbRes) {
            auto dwError = GetLastError();
            if (dwError != ERROR_IO_PENDING) {
                CancelThreadpoolIo(x_pTpIo);
                X_EndWrite();
                throw ExnFileWrite {dwError, std::unique_ptr<ByteChunk>(pChunk)};
            }
        }
    }

private:
    void X_Shutdown() noexcept {
        if (x_bStopping)
            return;
        x_bStopping = true;
        if (!x_uWrite)
            X_Close();
    }

    void X_Close() noexcept {
        x_bStopping = true;
        X_CloseFile();
        if (!x_uRead)
            X_Finalize();
    }

    inline void X_Finalize() noexcept {
        if (x_pIoGroup) {
            x_pIoGroup->UnregisterIo(x_pTpIo);
            x_pIoGroup = nullptr;
        }
        if (!x_bFinalized) {
            x_bFinalized = true;
            x_vUpper.OnFinalize();
        }
    }

    inline void X_EndRead() noexcept {
        if (!--x_uRead && x_bStopping) {
            X_CloseFile();
            X_Finalize();
        }
    }

    inline void X_EndWrite() noexcept {
        if (!--x_uWrite && x_bStopping)
            X_Close();
    }

    inline void X_CloseFile() noexcept {
        if (x_hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(x_hFile);
            x_hFile = INVALID_HANDLE_VALUE;
        }
    }

private:
    inline void X_IocbOnRead(DWORD dwRes, USize uDone, ByteChunk *pChunk) noexcept {
        if (dwRes)
            x_vUpper.OnRead(dwRes, 0, pChunk);
        else {
            pChunk->Skip(uDone);
            x_vUpper.OnRead(0, uDone, pChunk);
        }
        RAII_LOCK(x_mtx);
        X_EndRead();
    }

    inline void X_IocbOnWrite(DWORD dwRes, USize uDone, ByteChunk *pChunk) noexcept {
        if (dwRes)
            x_vUpper.OnWrite(dwRes, 0, std::unique_ptr<ByteChunk>(pChunk));
        else {
            pChunk->Discard(uDone);
            x_vUpper.OnWrite(0, uDone, std::unique_ptr<ByteChunk>(pChunk));
        }
        RAII_LOCK(x_mtx);
        X_EndWrite();
    }

    static void X_FwdOnRead(
        void *pParam, DWORD dwRes, USize uDone, ByteChunk *pChunk
    ) noexcept {
        reinterpret_cast<FileIo *>(pParam)->X_IocbOnRead(dwRes, uDone, pChunk);
    }

    static void X_FwdOnWrite(
        void *pParam, DWORD dwRes, USize uDone, ByteChunk *pChunk
    ) noexcept {
        reinterpret_cast<FileIo *>(pParam)->X_IocbOnWrite(dwRes, uDone, pChunk);
    }

private:
    Upper &x_vUpper;

    RecursiveMutex x_mtx;
    bool x_bStopping = false;
    bool x_bFinalized = false;

    IoGroup *x_pIoGroup = nullptr;
    PTP_IO x_pTpIo = nullptr;

    USize x_uRead = 0;
    USize x_uWrite = 0;

    HANDLE x_hFile = INVALID_HANDLE_VALUE;
    U64 x_uSize;

};
