#pragma once

#include "Common.hpp"

namespace protocol {
    constexpr static Byte kBegin = 0;

    namespace clsv {
        enum Id : Byte {
            kPulse = kBegin,
            kExit,
            kLoginReq,
            kRegisReq,
            kRecoUserReq,
            kRecoPassReq,
            kListReq,
            kMessageTo,
            kP2pTo,
            x_kEnd,
        };
    }

    namespace svcl {
        enum Id : Byte {
            kPulse = clsv::x_kEnd,
            kExit,
            kLoginRes,
            kRegisRes,
            kRecoUserRes,
            kRecoPassRes,
            kListRes,
            kMessageFrom,
            kP2pFrom,
            x_kEnd,
        };
    }

    namespace p2pchat {
        enum Id : Byte {
            kPulse = svcl::x_kEnd,
            kExit,
            kMessage,
            kFileReq,
            kFileRes,
            x_kEnd,
        };
    }

    namespace ucpfile {
        enum Id : Byte {
            kPulse = p2pchat::x_kEnd,
            kFile,
            kFin,
            kFinAck,
            x_kEnd,
        };
    }

    constexpr static Byte kEnd = ucpfile::x_kEnd;

}
