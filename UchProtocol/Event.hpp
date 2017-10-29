#pragma once

#include "Common.hpp"

#include "FileChunk.hpp"
#include "EventId.hpp"

namespace protocol {
#   define UEV_ID clsv::kPulse
#   define UEV_NAME EvcPulse
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID svcl::kPulse
#   define UEV_NAME EvsPulse
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID clsv::kExit
#   define UEV_NAME EvcExit
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID svcl::kExit
#   define UEV_NAME EvsExit
#   define UEV_MEMBERS UEV_END(String, sReason)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID clsv::kLoginReq
#   define UEV_NAME EvcLoginReq
#   define UEV_MEMBERS UEV_VAL(String, sUser) UEV_END(ShaDigest, vPass)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID svcl::kLoginRes
#   define UEV_NAME EvsLoginRes
#   define UEV_MEMBERS UEV_VAL(bool, bSuccess) UEV_END(String, sResult)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID clsv::kRegisReq
#   define UEV_NAME EvcRegisReq
#   define UEV_MEMBERS UEV_VAL(String, sUser) UEV_VAL(ShaDigest, vPass) \
        UEV_VAL(String, sQues) UEV_END(ShaDigest, vAnsw)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID svcl::kRegisRes
#   define UEV_NAME EvsRegisRes
#   define UEV_MEMBERS UEV_VAL(bool, bSuccess) UEV_END(String, sResult)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID clsv::kRecoUserReq
#   define UEV_NAME EvcRecoUserReq
#   define UEV_MEMBERS UEV_END(String, sUser)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID svcl::kRecoUserRes
#   define UEV_NAME EvsRecoUserRes
#   define UEV_MEMBERS UEV_VAL(bool, bSuccess) UEV_END(String, sResult)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID clsv::kRecoPassReq
#   define UEV_NAME EvcRecoPassReq
#   define UEV_MEMBERS UEV_VAL(String, sAnswer) UEV_END(ShaDigest, vPass)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID svcl::kRecoPassRes
#   define UEV_NAME EvsRecoPassRes
#   define UEV_MEMBERS UEV_VAL(bool, bSuccess) UEV_END(String, sResult)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID clsv::kListReq
#   define UEV_NAME EvcListReq
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID svcl::kListRes
#   define UEV_NAME EvsListRes
#   define UEV_MEMBERS UEV_VAL(std::vector<String>, vecOnline) UEV_END(std::vector<String>, vecOffline)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID clsv::kMessageTo
#   define UEV_NAME EvcMessageTo
#   define UEV_MEMBERS UEV_VAL(String, sWhom) UEV_END(String, sMessage)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID svcl::kMessageFrom
#   define UEV_NAME EvsMessageFrom
#   define UEV_MEMBERS UEV_VAL(String, sWhom) UEV_END(String, sMessage)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID clsv::kP2pTo
#   define UEV_NAME EvcP2pTo
#   define UEV_MEMBERS UEV_VAL(String, sWhom) UEV_VAL(U64, uKey) UEV_END(U16, uPort)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID svcl::kP2pFrom
#   define UEV_NAME EvsP2pFrom
#   define UEV_MEMBERS UEV_VAL(String, sWhom) UEV_VAL(U64, uKey) UEV_END(SockName, vSn)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID p2pchat::kPulse
#   define UEV_NAME EvpPulse
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID p2pchat::kExit
#   define UEV_NAME EvpExit
#   define UEV_MEMBERS UEV_END(String, sReason)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID p2pchat::kMessage
#   define UEV_NAME EvpMessage
#   define UEV_MEMBERS UEV_END(String, sMessage)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID p2pchat::kFileReq
#   define UEV_NAME EvpFileReq
#   define UEV_MEMBERS UEV_VAL(String, sName) UEV_END(U64, uSize)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID p2pchat::kFileRes
#   define UEV_NAME EvpFileRes
#   define UEV_MEMBERS UEV_VAL(bool, bAccepted) UEV_END(U16, uPort)
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID ucpfile::kPulse
#   define UEV_NAME EvuPulse
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID ucpfile::kFin
#   define UEV_NAME EvuFin
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID ucpfile::kFinAck
#   define UEV_NAME EvuFinAck
#   include "../UchCommon/GenEvent.inl"

#   define UEV_ID ucpfile::kFile
#   define UEV_NAME EvuFile
#   define UEV_NOCOPY
#   define UEV_MEMBERS UEV_VAL(U64, uOffset) UEV_END(FileChunkPool::UniquePtr, upChunk)
#   include "../UchCommon/GenEvent.inl"

}
