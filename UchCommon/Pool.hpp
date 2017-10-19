#pragma once

#include "Common.hpp"

template<class tObj>
class Pool {
public:
    using Obj = tObj;

private:
    struct X_Block {
        SLIST_ENTRY vEntry;
        Obj vObj;
    };

public:
    static inline Pool &Instance() noexcept {
        static Pool vInstance;
        return vInstance;
    }

private:
    inline Pool(U32 uPrepared = 0) noexcept {
        InitializeSListHead(&x_vHeader);
        while (uPrepared--) {
            auto pBlock = reinterpret_cast<X_Block *>(
                ::_aligned_malloc(sizeof(X_Block), MEMORY_ALLOCATION_ALIGNMENT)
            );
            assert(pBlock);
            InterlockedPushEntrySList(&x_vHeader, &pBlock->vEntry);
        }
    }

    Pool(const Pool &) = delete;
    Pool(Pool &&) = delete;

    inline ~Pool() {
        auto pEntry = InterlockedPopEntrySList(&x_vHeader);
        while (pEntry) {
            auto pBlock = CONTAINING_RECORD(pEntry, X_Block, vEntry);
            ::_aligned_free(pBlock);
            pEntry = InterlockedPopEntrySList(&x_vHeader);
        }
    }

    Pool &operator =(const Pool &) = delete;
    Pool &operator =(Pool &&) = delete;

public:
    inline Obj *Alloc() noexcept {
        auto pEntry = InterlockedPopEntrySList(&x_vHeader);
        if (pEntry == nullptr) {
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

private:
    alignas(MEMORY_ALLOCATION_ALIGNMENT) SLIST_HEADER x_vHeader;

};
