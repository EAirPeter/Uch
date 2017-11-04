#pragma once

#include "Common.hpp"

namespace protocol {
    constexpr static Byte kBegin = 0;

    namespace clsv {
        enum Id : Byte {
            kExit = kBegin,
            kLoginReq,
            kRegisReq,
            kRecoUserReq,
            kRecoPassReq,
            kMessageTo,
            x_kEnd,
        };
    }

    namespace svcl {
        enum Id : Byte {
            kExit = clsv::x_kEnd,
            kLoginRes,
            kRegisRes,
            kNewUser,
            kRecoUserRes,
            kRecoPassRes,
            x_kEnd,
        };
    }

    namespace p2pchat {
        enum Id : Byte {
            kExit = svcl::x_kEnd,
            kAuth,
            kMessage,
            kFileReq,
            kFileRes,
            kFileCancel,
            x_kEnd,
        };
    }

    namespace ucpfile {
        enum Id : Byte {
            kFile = p2pchat::x_kEnd,
            kProgress,
            kFin,
            kFinAck,
            x_kEnd,
        };
    }

    constexpr static Byte kEnd = ucpfile::x_kEnd;

}
