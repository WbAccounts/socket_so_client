#include "test_utils.h"
#include <unistd.h>
#include <stdio.h>
#include "common/socket_client/ISocketClientMgr.h"
#include "common/socket/socket_utils.h"
#include "common/log/log.h"
#include "common/uuid.h"

bool test_utils::ASyncSendDataToOtherProcess(ISocketClientMgr* ptr, const std::string& str_content, const char* lpSender, const char *lpRecver, const char *lpFunction, const char* lpService, int nPriority) {
    struct UnixSocketData sendData;
    sendData.strSender = lpSender;
    sendData.strReciever = lpRecver;
    sendData.strFunction = lpFunction;
    sendData.lpContent = (unsigned char *)str_content.c_str();
    sendData.nContLen = str_content.length();
    sendData.nPriority = nPriority;
    char uuid[UUID_LEN];
    memset(uuid, 0, UUID_LEN);
    while (uuid::UUID_ESUCCESS != uuid::uuid4_generate(uuid)) {
        printf("[%s] async send data from[%s] to [%s] failed, create uuid failed.\n", lpService, lpSender, lpRecver);
        usleep(100 * 1000);
        continue;
    }
    sendData.strUUID = uuid;
    std::string str_send;
    socket_control::CreateSendData(str_send, sendData);
    if (ptr) {
        ptr->ASyncSendData(str_send.c_str());
    }
    return true;
}

bool test_utils::SyncSendDataToOtherProcess(ISocketClientMgr* ptr, const std::string& str_content, const char* lpSender, const char *lpRecver, const char *lpFunction, const char* lpService, std::string& recv_content, int nPriority) {
    struct UnixSocketData sendData;
    sendData.strSender = lpSender;
    sendData.strReciever = lpRecver;
    sendData.strFunction = lpFunction;
    sendData.lpContent = (unsigned char *)str_content.c_str();
    sendData.nContLen = str_content.length();
    sendData.nPriority = nPriority;
    char uuid[UUID_LEN];
    memset(uuid, 0, UUID_LEN);
    while (uuid::UUID_ESUCCESS != uuid::uuid4_generate(uuid)) {
        printf("[%s] async send data from[%s] to [%s] failed, create uuid failed.\n", lpService, lpSender, lpRecver);
        usleep(100 * 1000);
        continue;
    }
    sendData.strUUID = uuid;
    std::string str_send;
    socket_control::CreateSendData(str_send, sendData);
    IASBundle *recv_data = NULL;
    if (ptr) {
        ptr->SyncSendData(str_send.c_str(), recv_data);
    }
    if (recv_data == NULL) {
        printf("havn't recv the response, retry...\n");
        return false;
    } else {
        recv_content = socket_control::getBundleBinaryInfo(recv_data, UnixSocketKeyDataContent);
        if (recv_data != NULL) {
            recv_data->clear();
            delete recv_data;
            recv_data = NULL;
        }
    }
    return true;
}

bool test_utils::ResponseSyncEvents(ISocketClientMgr* ptr, const struct UnixSocketData& recvData, const char* lpResponse, const char* lpService, const char* lpEvent) {
    struct UnixSocketData responseData;
    std::string str_response_send;
    responseData.lpContent = (unsigned char *)lpResponse;
    responseData.nContLen = strlen(lpResponse);
    responseData.strReciever = recvData.strSender;
    responseData.strSender = recvData.strReciever;
    responseData.strUUID = recvData.strUUID;
    responseData.strFunction = recvData.strFunction;
    responseData.nPriority = recvData.nPriority;
    responseData.bResponse = true;
    socket_control::CreateSendData(str_response_send, responseData);
    ptr->ASyncSendData(str_response_send.c_str());
    return true;
}
