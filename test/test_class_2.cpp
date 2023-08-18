#include "test_class_2.h"
#include <unistd.h>
#include "common/socket/socket_process_info.h"
#include "common/socket/socket_utils.h"
#include "common/uuid.h"
#include "common/log/log.h"
#include "socket_test.h"
#include "test_utils.h"

void CTest2::Init(ISocketClientMgr* socket_mgr) {
    m_socket_mgr_ = socket_mgr;
    m_socket_mgr_->RegisterService(TEST_SERVICE_2, this);
    m_socket_mgr_->AddRef();
}

void CTest2::UnInit() {
    m_socket_mgr_->Release();
}

bool CTest2::ASyncSendDataToOtherProcess(const std::string& str_content, const char *lpRecver, const char *lpFunction, int nPriority) {
    return test_utils::ASyncSendDataToOtherProcess(m_socket_mgr_, str_content, TEST_1_NAME, lpRecver, lpFunction, TEST_SERVICE_2, nPriority);
}

bool CTest2::SyncSendDataToOtherProcess(const std::string& str_content, const char *lpRecver, const char *lpFunction, std::string& recv_content, int nPriority) {
    return test_utils::SyncSendDataToOtherProcess(m_socket_mgr_, str_content, TEST_1_NAME, lpRecver, lpFunction, TEST_SERVICE_2, recv_content, nPriority);
}

void CTest2::RecvData(IASBundle* p_bundle) {
    struct UnixSocketData recvData;
    if (-1 != socket_control::ParseRecvBundleData(p_bundle, recvData)) {
        if (strcmp(recvData.strFunction.c_str(), TEST_REGISTER_FUNCTION_TEST_SYNC_20) == 0) {
            std::string response = std::string((char*)recvData.lpContent, recvData.nContLen);
            test_utils::ResponseSyncEvents(m_socket_mgr_, recvData, response.c_str(), TEST_SERVICE_2, "CTest2");
        } else if (strcmp(recvData.strFunction.c_str(), TEST_REGISTER_FUNCTION_TEST_SYNC_21) == 0) {
            std::string response = std::string((char*)recvData.lpContent, recvData.nContLen);
            test_utils::ResponseSyncEvents(m_socket_mgr_, recvData, response.c_str(), TEST_SERVICE_2, "CTest2");
        } else if (strcmp(recvData.strFunction.c_str(), TEST_REGISTER_FUNCTION_TEST_SYNC_22) == 0) {
            std::string response = std::string((char*)recvData.lpContent, recvData.nContLen);
            test_utils::ResponseSyncEvents(m_socket_mgr_, recvData, response.c_str(), TEST_SERVICE_2, "CTest2");
        }
    } else {
        printf("[%s] parse recive data failed.\n", TEST_SERVICE_2);
    }
    recvData.clear();
    if (p_bundle != NULL) { p_bundle->clear(); delete p_bundle; }
}