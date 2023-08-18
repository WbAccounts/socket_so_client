#ifndef TEST_UTILS_H_
#define TEST_UTILS_H_

#include <string>
#include "common/socket/socket_process_info.h"

class ISocketClientMgr;

namespace test_utils {
    bool ASyncSendDataToOtherProcess(ISocketClientMgr* ptr, const std::string& str_content, const char* lpSender, const char *lpRecver, const char *lpFunction, const char* lpService, int nPriority = 0);
    bool SyncSendDataToOtherProcess(ISocketClientMgr* ptr, const std::string& str_content, const char* lpSender, const char *lpRecver, const char *lpFunction, const char* lpService, std::string& recv_content, int nPriority = 0);
    bool ResponseSyncEvents(ISocketClientMgr* ptr, const struct UnixSocketData& recvData, const char* lpResponse, const char* lpService, const char* lpEvent);
}

#endif /* TEST_UTILS_H_ */