#ifndef MSG_HANDLE_H_
#define MSG_HANDLE_H_

#include <set>
#include <map>
#include <list>
#include <vector>
#include <string>
#include "common/singleton.hpp"
// #include "common/log/log.h"
#include "common/log/log.hpp"
#include "common/qh_thread/cond.hpp"
#include "common/qh_thread/thread.h"
#include "common/qh_thread/multi_thread.h"
#include "common/socket/socket_process_info.h"
#include "common/socket_client/ISocketClientMgr.h"
#include "common/AsFramework/ASBundleImpl.hpp"
#include "init_helper.h"
#include "socket_mgr.h"

class CMsgHandle : public ISocketClientMgr, public QH_THREAD::CMultiThread {

  typedef std::map<std::string, std::vector<std::string> > RecvMap;
  typedef std::map<std::string, std::vector<std::pair<std::string, int> > > IntrestedMap;

  ASUNKNOWN_EASY_IMPLEMENT(CMsgHandle)

  public:
    CMsgHandle()
        : m_lRefCount_CMsgHandle(0)
        , m_inited_(false)
        , m_cb_bundle_(NULL) {
        m_cond_task_list_.Init(&m_mutex_task_list_);
        m_service_list_.clear();
        m_function_list_.clear();
        m_interestd_list_.clear();
        m_local_conf_ = new(std::nothrow) CLocalConf;
        m_log_helper_ = new(std::nothrow) CLogHelper;
        m_socket_mgr_ = new(std::nothrow) CSocketMgr;
    }
    ~CMsgHandle() {
        UnInit();
    }

  public:
    bool Init(const char* dst);
    bool UnInit() {
        DestroySocketConnect();
        if ((m_cb_bundle_ != NULL) && (0 == m_cb_bundle_->Release())) {
            m_cb_bundle_ = NULL;
        }
        if (m_socket_mgr_) {
            delete m_socket_mgr_;
            m_socket_mgr_ = NULL;
        }
        if (m_local_conf_) {
            delete m_local_conf_;
            m_local_conf_ = NULL;
        }
        if (m_log_helper_) {
            delete m_log_helper_;
            m_log_helper_ = NULL;
        }
        // CEntModuleDebug::ReleaseModuleDebugger();
        return true;
    }

  public:
    void Run();
    bool ConnectSocket(const char* process_name);
    bool DestroySocketConnect();
    bool ReconnectSocket(const char* process_name);

  public:
    void RegisterService(const char* service, IEntBase* p_ent);
    void RegisterRecvFunc(const char* service, const char* function);
    void RegisterInterestedFunc(const char* function, const char* process_name = PROCESS_UNKNOWN_NAME, unsigned int timeout = 10 * 1000);

  public:
    void SyncSendData(const char* str_send, IASBundle* &recv_data);
    void ASyncSendData(const char* str_send);

  public:
    void RecvDataCBFunc(IASBundle* recv_data) {
        DoRecvDataCBFunc(recv_data);
    }
    void RegisterCallBackReceiver(IASBundle* p_cb);

  private:
    void DoRecvDataCBFunc(IASBundle* recv_data);
    std::string GetServiceInfo(const std::string& str_function);

  protected:
    void* thread_function(void* param);

  private:
    volatile bool m_inited_;
    QH_THREAD::CMutex m_mutex_service_list_;
    std::map<std::string, IEntBase *> m_service_list_;
    QH_THREAD::CMutex m_mutex_function_list_;
    std::map<std::string, std::set<std::string> > m_function_list_;
    QH_THREAD::CMutex m_mutex_interested_function_list_;
    std::map<std::string, std::list<std::string> > m_interestd_list_;
    QH_THREAD::CMutex m_mutex_task_list_;
    QH_THREAD::CCond m_cond_task_list_;
    std::list<IASBundle*> m_task_list_;
    IASBundle *m_cb_bundle_;
    CLocalConf *m_local_conf_;
    CLogHelper *m_log_helper_;
    CSocketMgr *m_socket_mgr_;
};

#endif /* MSG_HANDLE_H_ */
