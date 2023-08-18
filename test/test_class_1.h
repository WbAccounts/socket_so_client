#ifndef TEST_CLASS_1_H_
#define TEST_CLASS_1_H_

#include <string>
#include "common/socket_client/ISocketClientMgr.h"

class CTest1 : public ISocketCallBackReceiver, public IEntBase {
  public:
    CTest1()
        : m_socket_mgr_(NULL)
        , m_cb_bundle_(NULL)
        , m_login_status_(false) {
    }
    ~CTest1() {
        m_login_status_ = true;
    }

  public:
    void Init(ISocketClientMgr* socket_mgr);
    void UnInit();
    void DoLogin();
    bool GetLoginStatus() { return m_login_status_; }

  public:
    void RecvData(IASBundle* p_bundle);
    void OnCallBack(IASBundle* p_bundle);

  public:
    bool ASyncSendDataToOtherProcess(const std::string& str_content, const char *lpRecver, const char *lpFunction, int nPriority = 0);
    bool SyncSendDataToOtherProcess(const std::string& str_content, const char *lpRecver, const char *lpFunction, std::string& recv_content, int nPriority = 0);

  private:
    ISocketClientMgr* m_socket_mgr_;
    IASBundle* m_cb_bundle_;
    volatile bool m_login_status_;
};

#endif /* TEST_CLASS_1_H_ */