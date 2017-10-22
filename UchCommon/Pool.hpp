#pragma once

#include "Common.hpp"

#ifndef NDEBUG
#define POOL_ATMGUARD
#endif

template<class tPool>
class PoolDeleter {
public:
    constexpr PoolDeleter() noexcept = default;
    constexpr PoolDeleter(const PoolDeleter &) noexcept = default;
    constexpr PoolDeleter(PoolDeleter &&) noexcept = default;
    
    constexpr PoolDeleter(tPool &vPool) noexcept : x_pPool(&vPool) {}

    PoolDeleter &operator =(const PoolDeleter &) = default;
    PoolDeleter &operator =(PoolDeleter &&) = default;

public:
    inline void operator ()(typename tPool::Obj *pObj) noexcept {
        assert(x_pPool);
        x_pPool->Delete(pObj);
    }

private:
    tPool *x_pPool = nullptr;
};

template<class tObj>
struct SysPool {
    using Obj = tObj;
    using Deleter = PoolDeleter<SysPool>;
    using UniquePtr = std::unique_ptr<Obj, Deleter>;
    
    inline Obj *Alloc() noexcept {
        return reinterpret_cast<Obj *>(::operator new(sizeof(tObj)));
    }

    inline void Dealloc(Obj *pObj) noexcept {
        ::operator delete(pObj);
    }

    template<class ...tvArgs>
    inline Obj *New(tvArgs &&...vArgs) noexcept(std::is_nothrow_constructible_v<Obj, tvArgs...>) {
        return ::new Obj(std::forward<tvArgs>(vArgs)...);
    }

    inline void Delete(Obj *pObj) noexcept {
        ::delete pObj;
    }

    template<class ...tvArgs>
    inline UniquePtr MakeUnique(tvArgs &&...vArgs) noexcept(std::is_nothrow_constructible_v<Obj, tvArgs...>) {
        auto pObj = New(std::forward<tvArgs>(vArgs)...);
        return UniquePtr(pObj, Deleter(*this));
    }

    inline UniquePtr Wrap(Obj *pObj) noexcept {
        return UniquePtr(pObj, Deleter(*this));
    }

    constexpr void Shrink() noexcept {}

};

template<class tObj, USize kuAlignOff = 0, USize kuAlign = MEMORY_ALLOCATION_ALIGNMENT>
class Pool {
public:
    using Obj = tObj;
    using Deleter = PoolDeleter<Pool>;
    using UniquePtr = std::unique_ptr<Obj, Deleter>;

private:
    struct alignas(MEMORY_ALLOCATION_ALIGNMENT) X_Block {
        Obj vObj;
        SLIST_ENTRY vEntry;
    };

public:
    inline Pool(U32 uPrepared = 0) noexcept {
        InitializeSListHead(&x_vHeader);
        while (uPrepared--) {
            auto pBlock = reinterpret_cast<X_Block *>(
                ::_aligned_offset_malloc(sizeof(X_Block), kuAlign, kuAlignOff)
            );
            assert(pBlock);
            InterlockedPushEntrySList(&x_vHeader, &pBlock->vEntry);
        }
    }

    Pool(const Pool &) = delete;
    Pool(Pool &&) = delete;

    inline ~Pool() {
#ifdef POOL_ATMGUARD
        while (x_atmuAlloc.load())
#endif
            Shrink();
    }

    Pool &operator =(const Pool &) = delete;
    Pool &operator =(Pool &&) = delete;

public:
    inline Obj *Alloc() noexcept {
        auto pEntry = InterlockedPopEntrySList(&x_vHeader);
        if (pEntry == nullptr) {
#ifdef POOL_ATMGUARD
            x_atmuAlloc.fetch_add(1);
#endif
            auto pBlock = reinterpret_cast<X_Block *>(
                ::_aligned_malloc(sizeof(X_Block), MEMORY_ALLOCATION_ALIGNMENT)
            );
            assert(pBlock);
            return &pBlock->vObj;
        }
        auto pBlock = CONTAINING_RECORD(pEntry, X_Block, vEntry);
        return &pBlock->vObj;
    }

    inline void Dealloc(Obj *pObj) noexcept {
        auto pBlock = CONTAINING_RECORD(pObj, X_Block, vObj);
        InterlockedPushEntrySList(&x_vHeader, &pBlock->vEntry);
    }

    template<class ...tvArgs>
    inline Obj *New(tvArgs &&...vArgs) noexcept(std::is_nothrow_constructible_v<Obj, tvArgs...>) {
        return ::new(Alloc()) Obj(std::forward<tvArgs>(vArgs)...);
    }

    inline void Delete(Obj *pObj) noexcept {
        pObj->~Obj();
        Dealloc(pObj);
    }

    template<class ...tvArgs>
    inline UniquePtr MakeUnique(tvArgs &&...vArgs) noexcept(std::is_nothrow_constructible_v<Obj, tvArgs...>) {
        auto pObj = New(std::forward<tvArgs>(vArgs)...);
        return UniquePtr(pObj, Deleter(*this));
    }

    inline UniquePtr Wrap(Obj *pObj) noexcept {
        return UniquePtr(pObj, Deleter(*this));
    }

    inline void Shrink() noexcept {
        auto pEntry = InterlockedPopEntrySList(&x_vHeader);
        while (pEntry) {
            auto pBlock = CONTAINING_RECORD(pEntry, X_Block, vEntry);
            ::_aligned_free(pBlock);
#ifdef POOL_ATMGUARD
            x_atmuAlloc.fetch_sub(1);
#endif
            pEntry = InterlockedPopEntrySList(&x_vHeader);
        }
    }

private:
    alignas(MEMORY_ALLOCATION_ALIGNMENT) SLIST_HEADER x_vHeader;
#ifdef POOL_ATMGUARD
    std::atomic<U32> x_atmuAlloc = 0;
#endif

};
