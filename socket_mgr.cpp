#include "socket_mgr.h"
#include <errno.h>
#include <stdlib.h>
#include <vector>
#include "common/utils/proc_info_utils.h"
#include "common/utils/string_utils.hpp"
// #include "common/log/log.h"
#include "common/log/log.hpp"
// #include "common/ASFramework/ASBundle.h"
#include "common/AsFramework/ASBundle.h"
// #include "common/ASFramework/ASBundleImpl.hpp"
#include "common/AsFramework/ASBundleImpl.hpp"

bool CSocketMgr::Init(const char* process_name, CLocalConf* local_conf) {
    if (process_name == NULL || local_conf == NULL) {
        LOG_ERROR("init socket mgr failed, input params format error.");
        return false;
    } else {
        m_process_name_ = process_name;
        m_local_conf_ = local_conf;
    }
    m_socket_pool_[m_process_name_] = new (std::nothrow) CSocketPool(m_process_name_, 1);
    if (!m_socket_pool_[m_process_name_]) {
        LOG_ERROR("create socket pool failed, out of memory, pool size(%d).", 1);
        return false;
    }
    m_socket_pool_[m_process_name_]->RegClientCb(m_cb_bundle_);
    if (false == m_socket_pool_[m_process_name_]->InitSocketPool(std::tr1::bind(&CSocketMgr::RecvDataCBFunc, this, std::tr1::placeholders::_1), m_local_conf_)) {
        LOG_ERROR("init socket pool failed.");
        return false;
    }
    return true;
}

void CSocketMgr::RegClientCb(IASBundle* cb_bundle) {
    if (m_cb_bundle_ == NULL) {
        m_cb_bundle_ = cb_bundle;
        m_cb_bundle_->AddRef();
    } else {
        LOG_DEBUG("socket pool manager has been registed the client callback info before.");
    }
}

void CSocketMgr::Run() {
    std::map<std::string, CSocketPool*>::iterator iter = m_socket_pool_.begin();
    while (iter != m_socket_pool_.end()) {
        if (iter->second) iter->second->Run();
        iter++;
    }
    CMultiThread::SetConcurrentSize(m_local_conf_->GetSocketClientThreadNum());
    CMultiThread::Run();
}

void CSocketMgr::UnInit() {
    CMultiThread::AsynStop();
    m_async_send_list_cond_.BroadCast();
    CMultiThread::SynStop();
    CMultiThread::Release();
    std::map<std::string, CSocketPool*>::iterator iter = m_socket_pool_.begin();
    while (iter != m_socket_pool_.end()) {
        if (iter->second) delete iter->second;
        iter++;
    }
    m_socket_pool_.clear();
    std::map<std::string, sync_send_info_t *>::iterator it_map = m_function_list_signal_.begin();
    for(; it_map != m_function_list_signal_.end(); it_map++) {
        delete it_map->second;
    }
    m_function_list_signal_.clear();
    m_async_send_list_.clear();
    if ((m_cb_bundle_ != NULL) && (0 == m_cb_bundle_->Release())) {
        m_cb_bundle_ = NULL;
    }
}

void CSocketMgr::RegFunctionList(const char * function_list) {
    // must completed regist before begin to send data
    if (m_function_list_signal_.find(function_list) == m_function_list_signal_.end()) {
        sync_send_info_t * p_send_signal = new (std::nothrow) sync_send_info_t;
        if (p_send_signal == NULL) {
            LOG_ERROR("process[%s] create sync send info failed. out of memory.", m_process_name_.c_str());
            return;
        }
        m_function_list_signal_.insert(std::pair<std::string, sync_send_info_t *>(function_list, p_send_signal));
    }
}

int CSocketMgr::DoRecvDataCBFunc(std::string& str_recv_data) {
    if (str_recv_data.find_first_of("{") == std::string::npos ||
        str_recv_data.find_last_of("}") == std::string::npos) {
        return -1;
    }
    str_recv_data = str_recv_data.substr(str_recv_data.find_first_of("{")).substr(0, str_recv_data.find_last_of("}"));
    std::vector<std::string> json_vector;
    string_utils::Split(json_vector, str_recv_data, "}");
    for (int i = 0; i < json_vector.size(); i++) {
        IASBundle* recv_bundle = NULL;
        std::string recv_data = json_vector[i] + "}";
        int rtn = socket_control::ConvertRecvStrToBundle(&recv_bundle, recv_data);
        if (-1 == rtn || NULL == recv_bundle) {
            LOG_ERROR("process[%s] recieved data parsed failed.", m_process_name_.c_str());
            return rtn;
        }
        if (0 != DoASyncRecvData(recv_bundle) && recv_bundle) {
            recv_bundle->clear();
            delete recv_bundle;
        }
    }
    return 0;
}

int CSocketMgr::ASyncSendData(const std::string& send_data) {
    // async send data if sync flag is set 0
    QH_THREAD::CMutexAutoLocker Lck(&m_async_send_list_mutex_);
    m_async_send_list_.push_back(send_data);
    m_async_send_list_cond_.BroadCast();
    return 0;
}

int CSocketMgr::SyncSendData(const std::string& send_data, IASBundle* &recv_data) {
    int rtn = -1;
    std::string lpFunction = socket_control::GetJsonStringInfo(send_data, UnixSocketKeyDataFunction);
    std::map<std::string, sync_send_info_t *>::iterator it = m_function_list_signal_.find(lpFunction);
    if (it == m_function_list_signal_.end()) {
        LOG_ERROR("function[%s] has not been register.", lpFunction.c_str());
        return rtn;
    } else {
        std::string lpUUID = socket_control::GetJsonStringInfo(send_data, UnixSocketKeyDataUniqueID);
        long timeout = m_local_conf_->GetFunctionTimeOut(m_process_name_, lpFunction);
        // sync send data if sync flag is set 1
        if (DoASyncSendData(send_data) < 0) {
            LOG_ERROR("async send data[%s] failed.", send_data.c_str());
            return rtn;
        }
        event_cond_info_t *pdata = new (std::nothrow) event_cond_info_t;
        if (NULL == pdata)  {
            LOG_ERROR("async send data failed, out of memory.");
            return rtn;
        }
        pdata->m_uuid_ = lpUUID;
        {
            QH_THREAD::CMutexAutoLocker Lck(&(it->second->m_mutex_));
            it->second->m_info_[lpUUID] = pdata;
        }
        if (timeout >= 1000) {
            QH_THREAD::CMutexAutoLocker Lck(&(pdata->m_mutex_));
            pdata->m_cond_.WaitTimeoutSecond(timeout/1000);
        } else {
            QH_THREAD::CMutexAutoLocker Lck(&(pdata->m_mutex_));
            pdata->m_cond_.WaitTimeoutMilliSecond(timeout);
        }
        {
            QH_THREAD::CMutexAutoLocker Lck(&(it->second->m_mutex_));
            it->second->m_info_.erase(lpUUID);
        }
        if (pdata->p_bundle_ != NULL) {
            recv_data = pdata->p_bundle_;
            rtn = 0;
        } else {
            LOG_ERROR("recv sync data[%s] response failed, recv data is null.", lpUUID.c_str());
        }
        if (pdata) delete pdata;
    }
    return rtn;
}

void* CSocketMgr::thread_function(void* param) {
    LOG_INFO("async send data thread[%d] started.", proc_info_utils::GetTid());
    while(!CMultiThread::IsCancelled()) {
        std::string str_send_data;
        QH_THREAD::CMutexManualLocker lck(&m_async_send_list_mutex_);
        lck.lock();
        if (!m_async_send_list_.empty()) {
            str_send_data = m_async_send_list_.front();
            m_async_send_list_.pop_front();
            lck.unlock();
        } else {
            m_async_send_list_cond_.WaitTimeoutSecond(5);
            lck.unlock();
            continue;
        }
        int rtn = DoASyncSendData(str_send_data);
        if (rtn < 0) {
            {
                lck.lock();
                m_async_send_list_.push_back(str_send_data);
                m_async_send_list_cond_.BroadCast();
                lck.unlock();
            }
            usleep(1000 * 1000);
        }
    }
    LOG_INFO("async send data thread[%d] exited.", proc_info_utils::GetTid());
    return NULL;
}

int CSocketMgr::DoASyncRecvData(IASBundle* recv_data) {
    std::string str_function = socket_control::GetBundleStringInfo(recv_data, UnixSocketKeyDataFunction);
    int bResponse = 0;
    recv_data->getInt(UnixSocketKeyDataResponed, &bResponse);
    std::string str_uuid = socket_control::GetBundleStringInfo(recv_data, UnixSocketKeyDataUniqueID);
    if (1 == bResponse && !str_function.empty() && !str_uuid.empty()) {
        std::map<std::string, sync_send_info_t *>::iterator it = m_function_list_signal_.find(str_function);
        if (it == m_function_list_signal_.end()) {
            LOG_ERROR("function[%s] has not been register, recv call back response is meaningless.", str_function.c_str());
            return -1;
        } else {
            for (int retry_count = 0; retry_count < 10; retry_count++) {
                QH_THREAD::CMutexManualLocker Lck(&(it->second->m_mutex_));
                Lck.lock();
                if (it->second->m_info_.find(str_uuid) != it->second->m_info_.end()) {
                    event_cond_info_t *pdata = it->second->m_info_.find(str_uuid)->second;
                    pdata->p_bundle_ = recv_data;
                    pdata->m_cond_.Signal();
                    Lck.unlock();
                    LOG_DEBUG("recv %s uuid[%s] responsed.", str_function.c_str(), str_uuid.c_str());
                    return 0;
                } else {
                    // 说明1：此处处理逻辑针对于未wait之前已经收到回复的情况
                    // 说明2：以目前的通信时效来看：
                    // （1）对端不在（未运行、异常等）的情况下，应答一个来回在150微妙内
                    // （2）正常通信（对端立马回复等）模式下，应答一个来回时间在5000微妙内
                    Lck.unlock();
                    usleep(5 * 1000);
                }
            }
            LOG_ERROR("recv expired response data, uuid[%s].", str_uuid.c_str());
            return -1;
        }
    } else if (p_cb_recv_) {
        LOG_DEBUG("recv new data from other process, send to business layer.");
        p_cb_recv_(recv_data);
    }
    return 0;
}

int CSocketMgr::DoASyncSendData(const std::string& send_data) {
    LOG_DEBUG("async send data[%s]", send_data.c_str());
    std::string strSender = socket_control::GetJsonStringInfo(send_data, UnixSocketKeyDataSender);
    if (m_socket_pool_.count(strSender) == 0) {
        LOG_ERROR("cannot get process_name[%s] connection.", strSender.c_str());
        return -1;
    }
    struct socket_connection_t *socket_conn = m_socket_pool_[strSender]->GetConnection(true);
    if (socket_conn == NULL) {
        LOG_DEBUG("current socket pool has no free connect, please wait.");
        return -1;
    } else {
        size_t send_left = send_data.length();
        size_t send_pos = 0;
        int send_size = 0;
        while((send_size = write(socket_conn->m_socket_fd_, send_data.data() + send_pos, send_left)) > 0) {
            send_pos += send_size;
            send_left -= send_pos;
        }
        m_socket_pool_[strSender]->PutConnection(socket_conn);
        if (send_left == 0) {
            LOG_DEBUG("process[%s] send data success.", m_process_name_.c_str());
            return 0;
        }
        if (send_size <= 0) {
            LOG_ERROR("proeess[%s] send data failed, errno[%s], data[%s].", m_process_name_.c_str(), strerror(errno), send_data.c_str());
            return -1;
        }
    }
    return 0;
}
