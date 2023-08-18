#ifndef TEST_CLASS_2_H_
#define TEST_CLASS_2_H_

#include <string>
#include "common/socket_client/ISocketClientMgr.h"

class CTest2 : public IEntBase {
  public:
    CTest2() {};
    ~CTest2(){};

  public:
    void Init(ISocketClientMgr* socket_mgr);
    void UnInit();

  public:
    void RecvData(IASBundle* p_bundle);

  public:
    bool ASyncSendDataToOtherProcess(const std::string& str_content, const char *lpRecver, const char *lpFunction, int nPriority = 0);
    bool SyncSendDataToOtherProcess(const std::string& str_content, const char *lpRecver, const char *lpFunction, std::string& recv_content, int nPriority = 0);

  private:
    ISocketClientMgr* m_socket_mgr_;
};

#endif /* TEST_CLASS_2_H_ */