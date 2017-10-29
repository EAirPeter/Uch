#if !(defined(UEV_ID) && defined(UEV_NAME))
#   error packet generation
#endif

#ifdef UEV_VAL
#   undef UEV_VAL
#endif
#ifdef UEV_END
#   undef UEV_END
#endif

struct UEV_NAME : EventBase<UEV_ID, UEV_NAME> {
    inline UEV_NAME() = default;
#ifndef UEV_NOCOPY
    inline UEV_NAME(const UEV_NAME &) = default;
#endif
    inline UEV_NAME(UEV_NAME &&) = default;

#ifndef UEV_NOCOPY
    inline UEV_NAME &operator =(const UEV_NAME &) = default;
#endif
    inline UEV_NAME &operator =(UEV_NAME &&) = default;

#ifndef UEV_NOCOPY
#   ifdef UEV_MEMBERS
#       define UEV_VAL(type_, name_) UEV_END(type_, name_),
#       define UEV_END(type_, name_) const type_ &CONCAT(arg, name_)
    UEV_NAME(UEV_MEMBERS) :
#       undef UEV_END
#       define UEV_END(type_, name_) name_ {CONCAT(arg, name_)}
        UEV_MEMBERS
#       undef UEV_VAL
#       undef UEV_END
    {}
#   endif
#endif

#ifdef UEV_MEMBERS
#   define UEV_VAL(type_, name_) UEV_END(type_, name_),
#   define UEV_END(type_, name_) type_ &&CONCAT(arg, name_)
        UEV_NAME(UEV_MEMBERS) :
#   undef UEV_END
#   define UEV_END(type_, name_) name_ {std::move(CONCAT(arg, name_))}
        UEV_MEMBERS
#   undef UEV_VAL
#   undef UEV_END
    {}
#endif

    template<class tBuffer>
    inline void EmitBuffer(tBuffer &vBuf) const {
#ifdef UEV_MEMBERS
#   define UEV_VAL(type_, name_) UEV_END(type_, name_)
#   define UEV_END(type_, name_) << name_
        vBuf << static_cast<Byte>(kEventId) UEV_MEMBERS;
#   undef UEV_VAL
#   undef UEV_END
#else
        UNREFERENCED_PARAMETER(vBuf);
#endif
    }

    template<class tBuffer>
    inline void FromBuffer(tBuffer &vBuf) {
#ifdef UEV_MEMBERS
#   define UEV_VAL(type_, name_) UEV_END(type_, name_)
#   define UEV_END(type_, name_) >> name_
        vBuf UEV_MEMBERS;
#   undef UEV_VAL
#   undef UEV_END
#else
        UNREFERENCED_PARAMETER(vBuf);
#endif
    }

    template<class tBuffer>
    friend inline tBuffer &operator <<(tBuffer &vBuf, const UEV_NAME &vPak) {
        vPak.EmitBuffer(vBuf);
        return vBuf;
    }

    template<class tBuffer>
    friend inline tBuffer &operator >>(tBuffer &vBuf, UEV_NAME &vPak) {
        vPak.FromBuffer(vBuf);
        return vBuf;
    }

#ifdef UEV_MEMBERS
#   define UEV_VAL(type_, name_) UEV_END(type_, name_)
#   define UEV_END(type_, name_) type_ name_;
    UEV_MEMBERS
#   undef UEV_VAL
#   undef UEV_END
#endif

};

#ifdef UEV_ID
#   undef UEV_ID
#endif
#ifdef UEV_NAME
#   undef UEV_NAME
#endif
#ifdef UEV_NOCOPY
#   undef UEV_NOCOPY
#endif
#ifdef UEV_MEMBERS
#   undef UEV_MEMBERS
#endif
