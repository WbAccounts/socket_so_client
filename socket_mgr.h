#ifndef MGR_HPP_
#define MGR_HPP_

#include <set>
#include <list>
#include <string>
// #include "common/socket/socket_utils.h"
#include "common/socket/socket_utils.h"
// #include "common/qh_thread/multi_thread.h"
#include "common/qh_thread/multi_thread.h"
#include "socket_pool.h"
#include "init_helper.h"

class IASBundle;

typedef std::tr1::function<void(IASBundle *)> fn_RecvData;

class CSocketMgr : public QH_THREAD::CMultiThread {
  struct event_cond_info_t {
      QH_THREAD::CMutex m_mutex_;
      QH_THREAD::CCond m_cond_;
      std::string m_uuid_;
      IASBundle *p_bundle_;
      event_cond_info_t() {
          p_bundle_ = NULL;
          m_cond_.Init(&m_mutex_);
      }
      ~event_cond_info_t() {
          p_bundle_ = NULL;
          m_cond_.Signal();
      }
  };
  struct sync_send_info_t {
      QH_THREAD::CMutex m_mutex_;
      std::map<std::string, event_cond_info_t*> m_info_;
  };
  public:
    CSocketMgr()
        : p_cb_recv_(NULL)
        , m_cb_bundle_(NULL)
        , m_local_conf_(NULL) {
        m_async_send_list_cond_.Init(&m_async_send_list_mutex_);
        m_socket_pool_.clear();
        m_function_list_signal_.clear();
    }
    ~CSocketMgr() {}

  public:
    bool Init(const char* process_name, CLocalConf* local_conf);
    void UnInit();
    void Run();

  public:
    void RegSendCb(const fn_RecvData &call_back) {
        p_cb_recv_ = (fn_RecvData)call_back;
    }
    void RegClientCb(IASBundle* cb_bundle);
    void RegFunctionList(const char* function_list);
    
    int RecvDataCBFunc(std::string& recv_data) {
        return DoRecvDataCBFunc(recv_data);
    }
    int ASyncSendData(const std::string& send_data);
    // 同步发送对接收到的Bundle负责
    int SyncSendData(const std::string& send_data, IASBundle* &recv_data);

  private:
    int DoRecvDataCBFunc(std::string& recv_data);
    int DoASyncSendData(const std::string& send_data);
    int DoASyncRecvData(IASBundle* recv_data);

  protected:
    void* thread_function(void* param);

  private:
    std::string m_process_name_;
    std::map<std::string, CSocketPool*> m_socket_pool_;
    QH_THREAD::CMutex m_async_send_list_mutex_;
    QH_THREAD::CCond m_async_send_list_cond_;
    std::list<std::string> m_async_send_list_;
    std::map<std::string, sync_send_info_t*> m_function_list_signal_;
    fn_RecvData p_cb_recv_;
    IASBundle *m_cb_bundle_;
    CLocalConf *m_local_conf_;
};

#endif /* MGR_HPP_ */
