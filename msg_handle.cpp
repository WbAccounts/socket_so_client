#include "msg_handle.h"
#include <tr1/memory>
#include <tr1/functional>
#include "common/utils/proc_info_utils.h"
#include "common/utils/string_utils.hpp"
#include "common/socket/socket_utils.h"
#include "common/AsFramework/ASBundle.h"
#include "common/AsFramework/ASBundleImpl.hpp"

/////////////////////////////////////////////////////////////////////////////////////////
#if defined(__GNUC__) && ((__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3))
#define VISIBILITY_DEFAULT __attribute__((visibility("default")))
#else
#define VISIBILITY_DEFAULT
#endif

#ifdef __cplusplus
extern "C" {
#endif

VISIBILITY_DEFAULT CMsgHandle* CreateInstance(const char* conf) {
    CMsgHandle* ptr = new (std::nothrow) CMsgHandle;
    if (ptr == NULL) return NULL;
    if (ptr->Init(conf)) {
        ptr->AddRef();
        return ptr;
    }
    delete ptr;
    return NULL;
}

#ifdef __cplusplus
}
#endif
/////////////////////////////////////////////////////////////////////////////////////////

bool CMsgHandle::Init(const char* dst) {
    if (m_inited_ == true) {
        return true;
    }
    m_inited_ = true;
    if (m_local_conf_->Init(dst)) {
        if (!m_local_conf_->IsConfigMethodFile()) {
            LOG_INFO("only set socket addr[%s], need manual registed the functions.", m_local_conf_->GetSockAddr().c_str());
            return true;
        }
        // init log when it is config file
        // m_log_helper_->SetLogPath(m_local_conf_->GetLogPath().c_str());
        // m_log_helper_->SetLogBackupPath(m_local_conf_->GetLogBackupPath().c_str());
        // m_log_helper_->SetLogLevel((ASLogLevel)m_local_conf_->GetLogLevel());
        // m_log_helper_->SetLogMaxSize(m_local_conf_->GetLogMaxSize());
        m_log_helper_->Init();
    } else {
        LOG_ERROR("process local socket engine's conf file failed.");
        return false;
    }
    RecvMap recv_funcs = m_local_conf_->GetRecvFunctions();
    RecvMap::iterator iter1 = recv_funcs.begin();
    while (iter1 != recv_funcs.end()) {
        for (int nIndex = 0; nIndex < iter1->second.size(); nIndex++) {
            RegisterRecvFunc(iter1->first.c_str(), iter1->second[nIndex].c_str());
        }
        iter1++;
    }
    IntrestedMap intrested_funcs = m_local_conf_->GetIntrestedFunc();
    IntrestedMap::iterator iter2 = intrested_funcs.begin();
    while (iter2 != intrested_funcs.end()) {
        for (int nIndex = 0; nIndex < iter2->second.size(); nIndex++) {
            RegisterInterestedFunc(iter2->second[nIndex].first.c_str(), iter2->first.c_str());
        }
        iter2++;
    }
    return true;
}

bool CMsgHandle::ConnectSocket(const char* process_name) {
    // must after other layer regist the functions
    std::map<std::string, std::list<std::string> >::iterator it = m_interestd_list_.begin();
    for (; it != m_interestd_list_.end(); it++) {
        if ((strcmp(it->first.c_str(), PROCESS_UNKNOWN_NAME) == 0) ||
            (strcmp(it->first.c_str(), process_name) == 0)) {
            std::list<std::string>::iterator iter_list = it->second.begin();
            while (iter_list != it->second.end()) {
                m_socket_mgr_->RegFunctionList(iter_list->c_str());
                iter_list++;
            }
        }
    }
    m_socket_mgr_->RegSendCb(std::tr1::bind(&CMsgHandle::RecvDataCBFunc, this, std::tr1::placeholders::_1));
    m_socket_mgr_->RegClientCb(m_cb_bundle_);
    m_socket_mgr_->Init(process_name, m_local_conf_);
    return true;
}

bool CMsgHandle::DestroySocketConnect() {
    if (m_socket_mgr_) {
        m_socket_mgr_->UnInit();
    }
    CMultiThread::AsynStop();
    m_cond_task_list_.BroadCast();
    CMultiThread::SynStop();
    CMultiThread::Release();
    return true;
}

bool CMsgHandle::ReconnectSocket(const char* process_name) {
    return ConnectSocket(process_name);
}

void CMsgHandle::Run() {
    CMultiThread::SetConcurrentSize(m_local_conf_->GetSocketClientThreadNum());
    CMultiThread::Run();
    if (m_socket_mgr_) {
        m_socket_mgr_->Run();
    }
}

void CMsgHandle::RegisterService(const char* service, IEntBase* p_ent) {
    QH_THREAD::CMutexAutoLocker Lck(&m_mutex_service_list_);
    std::map<std::string, IEntBase *>::iterator it = m_service_list_.find(service);
    if (it == m_service_list_.end()) {
        m_service_list_.insert(std::pair<std::string, IEntBase *>(service, p_ent));
    } else {
        LOG_ERROR("you have registed the service[%s] already.", service);
    }
}

void CMsgHandle::RegisterRecvFunc(const char* service, const char* function) {
    QH_THREAD::CMutexAutoLocker Lck(&m_mutex_function_list_);
    std::map<std::string, std::set<std::string> >::iterator it = m_function_list_.find(service);
    if (it == m_function_list_.end()) {
        std::set<std::string> function_data;
        function_data.insert(function);
        m_function_list_.insert(std::pair<std::string, std::set<std::string> >(service, function_data));
    } else {
        it->second.insert(function);
    }
}

void CMsgHandle::RegisterInterestedFunc(const char* function, const char* process_name, unsigned int timeout) {
    QH_THREAD::CMutexAutoLocker Lck(&m_mutex_interested_function_list_);
    if (m_interestd_list_.count(process_name) == 0) {
        std::list<std::string> intereste_list;
        intereste_list.push_back(function);
        m_interestd_list_[process_name] = intereste_list;
    } else {
        m_interestd_list_[process_name].push_back(function);
    }
    if (!m_local_conf_->IsConfigMethodFile()) {
        m_local_conf_->ManualRegistedIntrestedFuncs(function, process_name, timeout);
    }
}

void CMsgHandle::DoRecvDataCBFunc(IASBundle* recv_data) {
    QH_THREAD::CMutexAutoLocker Lck(&m_mutex_task_list_);
    unsigned int nPriority = 0;
    socket_control::GetBundleItemInfo(recv_data, UnixSocketKeyDataPriority, nPriority);
    std::list<IASBundle*>::iterator iter = m_task_list_.begin();
    for (; iter != m_task_list_.end(); iter++) {
        unsigned int nIterPriority = 0;
        socket_control::GetBundleItemInfo(*iter, UnixSocketKeyDataPriority, nIterPriority);
        if (nPriority <= nIterPriority) {
            m_task_list_.insert(iter, 1, recv_data);
            m_cond_task_list_.BroadCast();
            return;
        }
    }
    m_task_list_.push_back(recv_data);
    m_cond_task_list_.BroadCast();
}

void CMsgHandle::SyncSendData(const char* str_send, IASBundle* &recv_data) {
    if (m_socket_mgr_) m_socket_mgr_->SyncSendData(str_send, recv_data);
}

void CMsgHandle::ASyncSendData(const char* str_send) {
    if (m_socket_mgr_) m_socket_mgr_->ASyncSendData(str_send);
}

std::string CMsgHandle::GetServiceInfo(const std::string& str_function) {
    std::map<std::string, std::set<std::string> >::iterator it = m_function_list_.begin();
    for (; it != m_function_list_.end(); it++) {
        if (it->second.find(str_function) != it->second.end()) {
            return it->first;
        }
    }
    return "";
}

void* CMsgHandle::thread_function(void* param) {
    LOG_INFO("started multithread for msg handle, tid[%d].", proc_info_utils::GetTid());
    while (!CMultiThread::IsCancelled()) {
        IASBundle* recv_data;
        QH_THREAD::CMutexManualLocker Lck(&m_mutex_task_list_);
        Lck.lock();
        if (!m_task_list_.empty()) {
            recv_data = m_task_list_.front();
            m_task_list_.pop_front();
            Lck.unlock();
        } else {
            m_cond_task_list_.WaitTimeoutSecond(5);
            Lck.unlock();
            continue;
        }
        // dispatch recv data
        std::string str_function = socket_control::GetBundleStringInfo(recv_data, UnixSocketKeyDataFunction);
        std::string str_service = GetServiceInfo(str_function);
        std::map<std::string, IEntBase *>::iterator it = m_service_list_.find(str_service);
        if (it != m_service_list_.end()) {
            it->second->RecvData(recv_data);
        } else {
            LOG_ERROR("recv the function[%s], but havn't recv the regist event before.", str_function.c_str());
        }
    }
    LOG_INFO("tid[%d] exited multithread for msg handle.", proc_info_utils::GetTid());
    return NULL;
}

void CMsgHandle::RegisterCallBackReceiver(IASBundle* p_cb) {
    if (m_cb_bundle_ == NULL) {
        m_cb_bundle_ = p_cb;
        m_cb_bundle_->AddRef();
    } else {
        LOG_DEBUG("message manager has been registed the client callback info before.");
    }
}
