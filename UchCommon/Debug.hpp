#pragma once

#ifdef NDEBUG

#include "Common.hpp"

#include "Sync.hpp"

#include <iostream>

namespace ImplDbg {
    extern Mutex mtxConsole;

    inline void X_Println() noexcept {
        std::wcout << std::endl;
    }

    template<class tOstream, class tElem>
    inline tOstream &operator <<(tOstream &vOs, const std::vector<tElem> &vec) noexcept {
        vOs << L'{';
        if (!vec.empty()) {
            vOs << vec[0];
            for (USize i = 1; i != vec.size(); ++i)
                vOs << L',' << vec[i];
        }
        vOs << L'}';
        return vOs;
    }

    template<class tFirst, class ...tvArgs>
    inline void X_Println(tFirst &&vFirst, tvArgs &&...vArgs) noexcept {
        std::wcout << std::forward<tFirst>(vFirst);
        X_Println(std::forward<tvArgs>(vArgs)...);
    }

    template<class ...tvArgs>
    inline void Println(tvArgs &&...vArgs) noexcept {
        RAII_LOCK(mtxConsole);
        X_Println(std::forward<tvArgs>(vArgs)...);
    }

    inline void X_Scan() noexcept {}

    template<class tFirst, class ...tvArgs>
    inline void X_Scan(tFirst &&vFirst, tvArgs &&...vArgs) noexcept {
        std::wcin >> std::forward<tFirst>(vFirst);
        X_Scan(std::forward<tvArgs>(vArgs)...);
    }

    template<class ...tvArgs>
    inline void Scan(tvArgs &&...vArgs) noexcept {
        RAII_LOCK(mtxConsole);
        X_Scan(std::forward<tvArgs>(vArgs)...);
    }

}

#define DBG_PRINTLN(...) ImplDbg::Println(__VA_ARGS__)

#else

#define DBG_PRINTLN(...) ((void) 0)

#endif
