#pragma once

#include "Common.hpp"

#include "EventBase.hpp"
#include "IoGroup.hpp"
#include "Sync.hpp"

// TODO: exceptions?
class EventBus {
public:
    EventBus() = delete;
    EventBus(const EventBus &) = delete;
    EventBus(EventBus &&) = delete;

    inline EventBus(IoGroup &vIoGroup) : x_vIoGroup(vIoGroup) {}

    inline ~EventBus() {
        RAII_LOCK(x_mtxPending);
        while (x_uPending)
            x_cvPending.Wait(x_mtxPending);
    }

    EventBus &operator =(const EventBus &) = delete;
    EventBus &operator =(EventBus &&) = delete;

private:
    struct X_HandlerContext {
        std::unordered_set<void *> set;
        RWLock rwl;
    };

private:
    template<class tHandler>
    constexpr void X_Register(tHandler &) noexcept {}

    template<class tHandler, class tEvent, class ...tvEvents>
    inline void X_Register(tHandler &vHandler) noexcept {
        auto &vCtx = x_aCtx[tEvent::kEventId];
        RAII_LOCK(vCtx.rwl.GetWriteLock());
        vCtx.set.emplace(reinterpret_cast<void *>(static_cast<tEvent::Handler *>(&vHandler)));
        X_Register<tHandler, tvEvents...>(vHandler);
    }
    
    template<class tHandler>
    constexpr void X_Unregister(tHandler &) noexcept {}

    template<class tHandler, class tEvent, class ...tvEvents>
    inline void X_Unregister(tHandler &vHandler) noexcept {
        auto &vCtx = x_aCtx[tEvent::kEventId];
        if (vCtx.rwl.GetWriteLock().TryAcquire()) {
            vCtx.set.erase(reinterpret_cast<void *>(static_cast<tEvent::Handler *>(&vHandler)));
            vCtx.rwl.GetWriteLock().Release();
        }
        else {
            auto &&fnJob = [this, &vCtx, &vHandler] {
                {
                    RAII_LOCK(vCtx.rwl.GetWriteLock());
                    vCtx.set.erase(reinterpret_cast<void *>(&vHandler));
                }
                RAII_LOCK(x_mtxPending);
                --x_uPending;
                x_cvPending.WakeOne();
            };
            RAII_LOCK(x_mtxPending);
            ++x_uPending;
            x_vIoGroup.PostJob(std::move(fnJob));
        }
        X_Unregister<tHandler, tvEvents...>(vHandler);
    }
    
public:
    template<class ...tvEvents>
    inline EventBus &Register(HandlerBase<tvEvents...> &vHandler) noexcept {
        X_Register<HandlerBase<tvEvents...>, tvEvents...>(vHandler);
        return *this;
    }

    template<class ...tvEvents>
    inline EventBus &Unregister(HandlerBase<tvEvents...> &vHandler) noexcept {
        X_Unregister<HandlerBase<tvEvents...>, tvEvents...>(vHandler);
        return *this;
    }

    template<
        class tEvent,
        REQUIRES(std::is_base_of_v<EventBase<tEvent::kEventId, tEvent>, tEvent>)
    >
    inline EventBus &PostEvent(tEvent &e) noexcept {
        auto &vCtx = x_aCtx[tEvent::kEventId];
        RAII_LOCK(vCtx.rwl.GetReadLock());
        for (auto pHandler : vCtx.set)
            reinterpret_cast<typename tEvent::Handler *>(pHandler)->OnEvent(e);
        return *this;
    }

    template<
        class tEvent,
        REQUIRES(std::is_base_of_v<EventBase<tEvent::kEventId, tEvent>, tEvent>)
    >
    inline EventBus &PostEvent(tEvent &&e) noexcept {
        return PostEvent(e);
    }

private:
    IoGroup &x_vIoGroup;
    X_HandlerContext x_aCtx[256];
    Mutex x_mtxPending;
    ConditionVariable x_cvPending;
    U32 x_uPending = 0;

};
