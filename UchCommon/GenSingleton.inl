#if !defined(USG_NAME) || !defined(USG_MEMBERS)
#   error singleton generation
#endif

#ifdef USG_VAL
#   undef USG_VAL
#endif

class USG_NAME {
public:
#define USG_VAL(type_, getter_, name_, ...) static inline type_ &getter_() { return x_vInstance.name_; }
    USG_MEMBERS
#undef USG_VAL

private:
#define USG_VAL(type_, getter_, name_, ...) type_ name_ __VA_ARGS__;
    USG_MEMBERS
#undef USG_VAL

private:
    USG_NAME() = default;
    USG_NAME(const USG_NAME &) = delete;
    USG_NAME(USG_NAME &&) = delete;

    USG_NAME &operator =(const USG_NAME &) = delete;
    USG_NAME &operator =(USG_NAME &&) = delete;

private:
    static USG_NAME x_vInstance;

};

#ifdef USG_NAME
#   undef USG_NAME
#endif
#ifdef USG_MEMBERS
#   undef USG_MEMBERS
#endif
