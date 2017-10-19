#pragma once

#include "Common.hpp"

struct LockAcquired {};

template<class tLock>
class LockGuard {
public:
    LockGuard() = delete;
    LockGuard(const LockGuard &) = delete;
    LockGuard(LockGuard &&) = delete;

    inline LockGuard(tLock &vLock) noexcept : x_vLock(vLock) {
        x_vLock.Acquire();
    }

    constexpr LockGuard(tLock &vLock, LockAcquired) noexcept : x_vLock(vLock) {}

    inline ~LockGuard() {
        x_vLock.Release();
    }

    LockGuard &operator =(const LockGuard &) = delete;
    LockGuard &operator =(LockGuard &&) = delete;

private:
    tLock &x_vLock;

};

#define RAII_LOCK(lock_) \
    LockGuard<std::decay_t<decltype(lock_)>> CONCAT(vLg_, __COUNTER__)(lock_)
#define RAII_LOCK_ACQUIRED(lock_) \
    LockGuard<std::decay_t<decltype(lock_)>> CONCAT(vLg_, __COUNTER__)(lock_, LockAcquired {})

class RecursiveMutex {
public:
    inline RecursiveMutex() noexcept {
        InitializeCriticalSection(&x_vCritSec);
    }

    RecursiveMutex(const RecursiveMutex &) = delete;
    RecursiveMutex(RecursiveMutex &&) = delete;

    inline ~RecursiveMutex() {
        assert(!d_atmnCount.load());
        DeleteCriticalSection(&x_vCritSec);
    }

    RecursiveMutex &operator =(const RecursiveMutex &) = delete;
    RecursiveMutex &operator =(RecursiveMutex &&) = delete;

public:
    constexpr CRITICAL_SECTION *GetNative() noexcept {
        return &x_vCritSec;
    }

    inline void Acquire() noexcept {
        EnterCriticalSection(&x_vCritSec);
        assert(d_atmnCount.fetch_add(1) >= 0);
    }

    inline bool TryAcquire() noexcept {
        auto bRes = TryEnterCriticalSection(&x_vCritSec);
        assert(bRes ? d_atmnCount.fetch_add(1) >= 0 : true);
        return bRes;
    }

    inline void Release() noexcept {
        assert(d_atmnCount.fetch_sub(1) > 0);
        LeaveCriticalSection(&x_vCritSec);
    }

private:
    CRITICAL_SECTION x_vCritSec;
#ifndef NDEBUG
    std::atomic<I64> d_atmnCount = 0;
#endif

};

class Mutex {
public:
    constexpr Mutex() noexcept = default;
    Mutex(const Mutex &) = delete;
    Mutex(Mutex &&) = delete;

#ifndef NDEBUG
    inline ~Mutex() {
        assert(!d_atmnCount.load());
    }
#endif

    Mutex &operator =(const Mutex &) = delete;
    Mutex &operator =(Mutex &&) = delete;

public:
    constexpr SRWLOCK *GetNative() noexcept {
        return &x_vSrwl;
    }

    inline void Acquire() noexcept {
        AcquireSRWLockExclusive(&x_vSrwl);
        assert(d_atmnCount.fetch_add(1) >= 0);
    }

    inline bool TryAcquire() noexcept {
        auto bRes = TryAcquireSRWLockExclusive(&x_vSrwl);
        assert(bRes ? d_atmnCount.fetch_add(1) >= 0 : true);
        return bRes;
    }

    inline void Release() noexcept {
        assert(d_atmnCount.fetch_sub(1) > 0);
        ReleaseSRWLockExclusive(&x_vSrwl);
    }

private:
    SRWLOCK x_vSrwl = SRWLOCK_INIT;
#ifndef NDEBUG
    std::atomic<I64> d_atmnCount = 0;
#endif

};

class RWLock {
public:
    class ReadLock {
        friend RWLock;
    private:
        constexpr ReadLock(RWLock &rwl) noexcept : x_pSrwl(&rwl.x_vSrwl) {}

        ReadLock() = delete;
        ReadLock(const ReadLock &) = delete;
        ReadLock(ReadLock &&) = delete;

#ifndef NDEBUG
        inline ~ReadLock() {
            assert(!d_atmnCount.load());
        }
#endif

        ReadLock &operator =(const ReadLock &) = delete;
        ReadLock &operator =(ReadLock &&) = delete;

    public:
        constexpr SRWLOCK *GetNative() noexcept {
            return x_pSrwl;
        }

        inline void Acquire() noexcept {
            AcquireSRWLockShared(x_pSrwl);
            assert(d_atmnCount.fetch_add(1) >= 0);
        }

        inline bool TryAcquire() noexcept {
            auto bRes = TryAcquireSRWLockShared(x_pSrwl);
            assert(bRes ? d_atmnCount.fetch_add(1) >= 0 : true);
            return bRes;
        }

        inline void Release() noexcept {
            assert(d_atmnCount.fetch_sub(1) > 0);
            ReleaseSRWLockShared(x_pSrwl);
        }

    private:
        SRWLOCK *x_pSrwl;
#ifndef NDEBUG
        std::atomic<I64> d_atmnCount = 0;
#endif

    };

    class WriteLock {
        friend RWLock;
    private:
        constexpr WriteLock(RWLock &rwl) noexcept : x_pSrwl(&rwl.x_vSrwl) {}

        WriteLock() = delete;
        WriteLock(const WriteLock &) = delete;
        WriteLock(WriteLock &&) = delete;

#ifndef NDEBUG
        inline ~WriteLock() {
            assert(!d_atmnCount.load());
        }
#endif

        WriteLock &operator =(const WriteLock &) = delete;
        WriteLock &operator =(WriteLock &&) = delete;

    public:
        constexpr SRWLOCK *GetNative() noexcept {
            return x_pSrwl;
        }

        inline void Acquire() noexcept {
            AcquireSRWLockExclusive(x_pSrwl);
            assert(d_atmnCount.fetch_add(1) >= 0);
        }

        inline bool TryAcquire() noexcept {
            auto bRes = TryAcquireSRWLockExclusive(x_pSrwl);
            assert(bRes ? d_atmnCount.fetch_add(1) >= 0 : true);
            return bRes;
        }

        inline void Release() noexcept {
            assert(d_atmnCount.fetch_sub(1) > 0);
            ReleaseSRWLockExclusive(x_pSrwl);
        }

    private:
        SRWLOCK *x_pSrwl;
#ifndef NDEBUG
        std::atomic<I64> d_atmnCount = 0;
#endif

    };

public:
    constexpr RWLock() noexcept = default;
    RWLock(const RWLock&) = delete;
    RWLock(RWLock &&) = delete;

    RWLock &operator =(const RWLock &) = delete;
    RWLock &operator =(RWLock &&) = delete;

public:
    constexpr SRWLOCK *GetNative() noexcept {
        return &x_vSrwl;
    }

    constexpr ReadLock &GetReadLock() noexcept {
        return x_vRLock;
    }

    constexpr WriteLock &GetWriteLock() noexcept {
        return x_vWLock;
    }

private:
    ReadLock x_vRLock {*this};
    WriteLock x_vWLock {*this};
    SRWLOCK x_vSrwl = SRWLOCK_INIT;

};

class ConditionVariable {
public:
    constexpr ConditionVariable() noexcept = default;
    ConditionVariable(const ConditionVariable &) = delete;
    ConditionVariable(ConditionVariable &&) = delete;

    ConditionVariable &operator =(const ConditionVariable &) = delete;
    ConditionVariable &operator =(ConditionVariable &&) = delete;

public:
    constexpr CONDITION_VARIABLE *GetNative() noexcept {
        return &x_vCondVar;
    }

    inline void Wait(Mutex &vMtx) noexcept {
        SleepConditionVariableSRW(&x_vCondVar, vMtx.GetNative(), INFINITE, 0);
    }

    inline void Wait(RecursiveMutex &vMtx) noexcept {
        SleepConditionVariableCS(&x_vCondVar, vMtx.GetNative(), INFINITE);
    }

    inline void WakeOne() noexcept {
        WakeConditionVariable(&x_vCondVar);
    }

    inline void WakeAll() noexcept {
        WakeAllConditionVariable(&x_vCondVar);
    }

private:
    CONDITION_VARIABLE x_vCondVar = CONDITION_VARIABLE_INIT;

};
