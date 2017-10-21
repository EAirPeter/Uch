#pragma once

#include "Common.hpp"

template<class tIntegral>
class Flags {
public:
    using State = tIntegral;

    static_assert(std::is_integral_v<State>, "integral");

public:
    inline Flags() noexcept = default;
    constexpr Flags(const Flags &) noexcept = default;
    constexpr Flags(Flags &&) noexcept = default;

    constexpr Flags(State vState) : x_vState(vState) {}

    constexpr Flags &operator =(const Flags &) noexcept = default;
    constexpr Flags &operator =(Flags &&) noexcept = default;

    constexpr Flags &operator =(State vState) {
        x_vState = vState;
        return *this;
    }

    constexpr explicit operator State() const noexcept {
        return x_vState;
    }

    constexpr bool operator ()(State vOptions) const noexcept {
        return Test(vOptions);
    }

    constexpr void operator +=(State vOptions) noexcept {
        Set(vOptions);
    }

    constexpr void operator -=(State vOptions) noexcept {
        Unset(vOptions);
    }

public:
    constexpr bool Test(State vOptions) const noexcept {
        return x_vState & vOptions;
    }

    constexpr void Set(State vOptions) noexcept {
        x_vState |= vOptions;
    }

    constexpr void Unset(State vOptions) noexcept {
        x_vState &= ~vOptions;
    }

private:
    State x_vState;

};
