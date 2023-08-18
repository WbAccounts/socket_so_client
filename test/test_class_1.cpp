#include "test_class_1.h"
#include <unistd.h>
#include "common/socket/socket_process_info.h"
#include "common/socket/socket_utils.h"
#include "common/ASFramework/ASBundle.h"
#include "common/ASFramework/ASBundleImpl.hpp"
#include "common/uuid.h"
#include "common/log/log.h"
#include "socket_test.h"
#include "test_utils.h"

void CTest1::Init(ISocketClientMgr* socket_mgr) {
    m_socket_mgr_ = socket_mgr;
    m_socket_mgr_->RegisterService(TEST_SERVICE_1, this);
    m_socket_mgr_->AddRef();
    ISocketCallBackReceiver* cb = this;
    m_cb_bundle_ = CASBundle::CreateInstance();
    m_cb_bundle_->putBinary(SOCKET_CLIENT_CALLBACK, (const unsigned char*)(&cb), sizeof(this));
    m_socket_mgr_->RegisterCallBackReceiver(m_cb_bundle_);
}

void CTest1::UnInit() {
    m_socket_mgr_->Release();
    m_login_status_ = true;
    m_cb_bundle_->Release();
}

void CTest1::DoLogin() {
    std::string str_recv_response;
    while (!m_login_status_ && false == SyncSendDataToOtherProcess(ReServerFunctionLogin, TEST_1_NAME, AK_REGISTER_FUNCTION_CLIENT_LOGIN, str_recv_response)) {
        printf("client %s login failed, wait 1 second for retry...\n", TEST_1_NAME);
        usleep(1000 * 1000);
    }
    m_login_status_ = true;
    printf("client login success, recv data[%s].\n", str_recv_response.c_str());
}

bool CTest1::ASyncSendDataToOtherProcess(const std::string& str_content, const char *lpRecver, const char *lpFunction, int nPriority) {
    return test_utils::ASyncSendDataToOtherProcess(m_socket_mgr_, str_content, TEST_1_NAME, lpRecver, lpFunction, TEST_SERVICE_1, nPriority);
}

bool CTest1::SyncSendDataToOtherProcess(const std::string& str_content, const char *lpRecver, const char *lpFunction, std::string& recv_content, int nPriority) {
    return test_utils::SyncSendDataToOtherProcess(m_socket_mgr_, str_content, TEST_1_NAME, lpRecver, lpFunction, TEST_SERVICE_1, recv_content, nPriority);
}

void CTest1::RecvData(IASBundle* p_bundle) {
    struct UnixSocketData recvData;
    if (-1 != socket_control::ParseRecvBundleData(p_bundle, recvData)) {
        if (strcmp(recvData.strFunction.c_str(), TEST_REGISTER_FUNCTION_TEST_SYNC_10) == 0) {
            std::string response = std::string((char*)recvData.lpContent, recvData.nContLen);
            test_utils::ResponseSyncEvents(m_socket_mgr_, recvData, response.c_str(), TEST_SERVICE_1, "CTest1");
        } else if (strcmp(recvData.strFunction.c_str(), TEST_REGISTER_FUNCTION_TEST_SYNC_11) == 0) {
            std::string response = std::string((char*)recvData.lpContent, recvData.nContLen);
            test_utils::ResponseSyncEvents(m_socket_mgr_, recvData, response.c_str(), TEST_SERVICE_1, "CTest1");
        } else if (strcmp(recvData.strFunction.c_str(), TEST_REGISTER_FUNCTION_TEST_SYNC_12) == 0) {
            std::string response = std::string((char*)recvData.lpContent, recvData.nContLen);
            test_utils::ResponseSyncEvents(m_socket_mgr_, recvData, response.c_str(), TEST_SERVICE_1, "CTest1");
        }
    } else {
        printf("[%s] parse recive data failed.\n", TEST_SERVICE_1);
    }
    recvData.clear();
    if (p_bundle != NULL) { p_bundle->clear(); delete p_bundle; }
}

void CTest1::OnCallBack(IASBundle* p_bundle) {
    if (NULL != p_bundle) {
        std::string strError = ASBundleHelper::getBundleAString(p_bundle, SOCKET_CLIENT_ERROR_MSG, "");
        int nCBType = ASBundleHelper::getBundleInt(p_bundle, SOCKET_CLIENT_CALLBACK_TYPE, SOCKET_CLIENT_CALLBACK_TYPE_UNKNOWN);
        printf("recv client sdk callback, info[%s], type[%d].\n", strError.c_str(), nCBType);
        switch(nCBType) {
            case SOCKET_CLIENT_CALLBACK_TYPE_SERVER_EXIT:
                printf("detective the socket server exit.\n");
                m_login_status_ = false;
                break;
            case SOCKET_CLIENT_CALLBACK_TYPE_CONNECTED:
                DoLogin();
                break;
            case SOCKET_CLIENT_CALLBACK_TYPE_CORE_ERROR:
                printf("detective the socket client's core error.\n");
                break;
            default:
                printf("recv unknown events.\n");
        }
    } else {
        printf("recv bundle data is null.\n");
    }
}