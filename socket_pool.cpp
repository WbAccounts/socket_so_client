#include "socket_pool.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
// #include "common/log/log.h"
#include "common/log/log.hpp"
#include "common/utils/proc_info_utils.h"
#include "common/utils/file_utils.h"
#include "common/singleton.hpp"
#include "common/AsFramework/ASBundle.h"
#include "common/AsFramework/ASBundleImpl.hpp"

#define MAXUSEDSOCKETSIZE(size) ((size > 50) ? size : 50)
#define EPOLLEVENTS 64
#define MAXLINE 4 * 1024
#define DEFAULTTIMEOUT 2

CSocketPool::~CSocketPool() {
    DestroySocketPool();
    if ((m_cb_bundle_ != NULL) && (0 == m_cb_bundle_->Release())) {
        m_cb_bundle_ = NULL;
    }
}

void CSocketPool::RegClientCb(IASBundle* cb_bundle) {
    if (m_cb_bundle_ == NULL) {
        m_cb_bundle_ = cb_bundle;
        m_cb_bundle_->AddRef();
    } else {
        LOG_DEBUG("socket pool has been registed the client callback info before.");
    }
}

bool CSocketPool::InitSocketPool(const fn_InsertRecvData &recv_callback, CLocalConf* local_conf) {
    if (recv_callback == NULL || local_conf == NULL) {
        LOG_ERROR("init socket pool failed, input params format error.");
        return false;
    } else {
        p_recv_data_ = (fn_InsertRecvData) recv_callback;
        m_local_conf_ = local_conf;
    }
    m_socket_worker_.SetThreadFunc(std::tr1::bind(&CSocketPool::ConnectSocket, this, std::tr1::placeholders::_1));
    m_socket_worker_.Run(&m_socket_worker_);
    m_client_connect_.SetThreadFunc(std::tr1::bind(&CSocketPool::MonitorSocketStatus, this, std::tr1::placeholders::_1));
    m_client_connect_.Run(&m_client_connect_);
    return true;
}

void* CSocketPool::ConnectSocket(void* para) {
    QH_THREAD::CWorkerThread* cur_thread_p = static_cast<QH_THREAD::CWorkerThread*>(para);
    for (int i_count = 0; i_count < m_socket_pool_size_ && !cur_thread_p->IsQuit(); ) {
        struct sockaddr_un un;
        memset(&un, 0, sizeof(un));
        un.sun_family = AF_UNIX;
        snprintf(un.sun_path, sizeof(un.sun_path), "@%s", m_local_conf_->GetSockAddr(m_process_name_).c_str());
        struct socket_connection_t *socket_conn = NewConnection(un, kConnectionStatusUnused);
        if (NULL != socket_conn) {
            i_count += 1;
            socket_conn->p_cb_recv_data_ = p_recv_data_;
            QH_THREAD::CMutexAutoLocker Lck(&m_mutex_unused_);
            m_socket_unused_.push_back(socket_conn);
            m_socket_connect_size_++;
        } else {
            usleep(1000 * 1000);
        }
    }
    // call the client callback function, notify the events
    if (NULL != m_cb_bundle_ && -1 != m_socket_pool_size_) {
        ISocketCallBackReceiver* cb_connected;
        int len = sizeof(cb_connected);
        m_cb_bundle_->getBinary(SOCKET_CLIENT_CALLBACK, (unsigned char *)(&cb_connected), &len);
        IASBundle* p_data = CASBundle::CreateInstance();
        p_data->putInt(SOCKET_CLIENT_CALLBACK_TYPE, SOCKET_CLIENT_CALLBACK_TYPE_CONNECTED);
        p_data->putAString(SOCKET_CLIENT_ERROR_MSG, "detective all clients connected server.");
        cb_connected->OnCallBack(p_data);
        p_data->Release();
    }
    return NULL;
}

void* CSocketPool::MonitorSocketStatus(void* para) {
    QH_THREAD::CWorkerThread* cur_thread_p = static_cast<QH_THREAD::CWorkerThread*>(para);
    // start monitor after work thread started
    usleep(5 * 1000 * 1000);
    while (!cur_thread_p->IsQuit()) {
        if (!CMultiThread::IsRunning()) {
            // clear all socket source
            ClearSocketSource();
            // call the client callback function, notify the events
            if (NULL != m_cb_bundle_) {
                ISocketCallBackReceiver* cb_server_exit;
                int len = sizeof(cb_server_exit);
                m_cb_bundle_->getBinary(SOCKET_CLIENT_CALLBACK, (unsigned char *)(&cb_server_exit), &len);
                IASBundle* p_data = CASBundle::CreateInstance();
                p_data->putInt(SOCKET_CLIENT_CALLBACK_TYPE, SOCKET_CLIENT_CALLBACK_TYPE_SERVER_EXIT);
                p_data->putAString(SOCKET_CLIENT_ERROR_MSG, "detective server exit.");
                cb_server_exit->OnCallBack(p_data);
                p_data->Release();
            }
            // resetting the socket pool config
            m_socket_worker_.SetThreadFunc(std::tr1::bind(&CSocketPool::ConnectSocket, this, std::tr1::placeholders::_1));
            m_socket_worker_.Run(&m_socket_worker_);
            CMultiThread::Run();
        } else {
            usleep(5 * 1000 * 1000);
        }
    }
    return NULL;
}

void CSocketPool::Run() {
    CMultiThread::SetConcurrentSize(m_socket_pool_size_);
    CMultiThread::Run();
}

bool CSocketPool::DestroySocketPool() {
    m_socket_pool_size_ = -1;
    m_socket_worker_.Quit();
    m_socket_worker_.Join();
    m_client_connect_.Quit();
    m_client_connect_.Join();
    if (CMultiThread::IsRunning()) {
        CMultiThread::SynStop();
    }
    CMultiThread::Release();
    ClearSocketSource();
    m_socket_connect_size_ = 0;

    return true;
}

void CSocketPool::ClearSocketSource() {
    {
        QH_THREAD::CMutexAutoLocker Lck(&m_mutex_used_);
        std::map<int, struct socket_connection_t *>::iterator it = m_socket_used_.begin();
        while (it != m_socket_used_.end()) {
            if (it->second != NULL && (-1 != it->second->m_socket_fd_) && (-1 == shutdown(it->second->m_socket_fd_, SHUT_RDWR))) {
                LOG_ERROR("process[%s] close socket failed", m_process_name_.c_str());
            } else {
                LOG_DEBUG("process[%s] close socket success", m_process_name_.c_str());
            }
            if (it->second != NULL && (-1 != it->second->m_socket_fd_)) delete (it->second);
            m_socket_used_.erase(it++);
        }
    }
    {
        QH_THREAD::CMutexAutoLocker Lck(&m_mutex_unused_);
        while (!m_socket_unused_.empty()) {
            struct socket_connection_t *socket_conn = m_socket_unused_.back();
            if (socket_conn != NULL && (-1 != socket_conn->m_socket_fd_) && (-1 == shutdown(socket_conn->m_socket_fd_, SHUT_RDWR))) {
                LOG_ERROR("process[%s] close socket failed", m_process_name_.c_str());
            } else {
                LOG_DEBUG("process[%s] close socket success", m_process_name_.c_str());
            }
            if (socket_conn != NULL && (-1 != socket_conn->m_socket_fd_)) delete socket_conn;
            m_socket_unused_.pop_back();
        }
        m_sock_pos_ = 0;
    }
}

struct socket_connection_t *CSocketPool::NewConnection(struct sockaddr_un& sockaddr_info, ConnectionStatus status) {
    bool rtn = false;
    struct socket_connection_t *socket_conn = new (std::nothrow) struct socket_connection_t;
    if (NULL == socket_conn) {
        LOG_ERROR("process[%s] no more memory to new socket_connection_t.", m_process_name_.c_str());
        return NULL;
    }
    do {
        socket_conn->m_status_ = status;
        socket_conn->m_socket_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (-1 == socket_conn->m_socket_fd_) {
            LOG_ERROR("process[%s] create socket failed.", m_process_name_.c_str());
            break;
        }
        file_utils::SetFDCLOEXEC(socket_conn->m_socket_fd_);
        int len = strlen(sockaddr_info.sun_path) + offsetof(struct sockaddr_un, sun_path);
        sockaddr_info.sun_path[0] = '\0';
        if (-1 == connect(socket_conn->m_socket_fd_, (struct sockaddr *) &sockaddr_info, len)) {
            LOG_ERROR("process[%s] connect to server failed, error[%s]", m_process_name_.c_str(), strerror(errno));
            break;
        }
        LOG_INFO("process[%s] connect to server success.", m_process_name_.c_str());
        rtn = true;
    } while(false);

    if (false == rtn) {
        delete socket_conn;
        return NULL;
    }
    return socket_conn;
}

struct socket_connection_t *CSocketPool::GetConnection(bool bsync) {
    // limit the max pool size
    if ((m_socket_used_.size() >= (MAXUSEDSOCKETSIZE(m_socket_pool_size_))) || (m_sock_pos_ < m_socket_pool_size_)) {
        LOG_ERROR("the socket pool size is full or not finished init, get connection failed.");
        return NULL;
    }
    struct socket_connection_t *socket_conn = NULL;
    struct timespec tv_pre, tv_now;
    clock_gettime(CLOCK_REALTIME, &tv_pre);
    clock_gettime(CLOCK_REALTIME, &tv_now);
    while(tv_now.tv_sec - tv_pre.tv_sec <= DEFAULTTIMEOUT) {
        QH_THREAD::CMutexManualLocker ManualLck(&m_mutex_unused_);
        ManualLck.lock();
        if (m_socket_unused_.empty()) {
            ManualLck.unlock();
            usleep(10 * 1000);
        } else {
            socket_conn = m_socket_unused_.front();
            socket_conn->m_status_ = kConnectionStatusUsed;
            m_socket_unused_.erase(m_socket_unused_.begin());
            ManualLck.unlock();
            {
                QH_THREAD::CMutexAutoLocker Lck(&m_mutex_used_);
                m_socket_used_.insert(std::pair<int, struct socket_connection_t *>(socket_conn->m_socket_fd_, socket_conn));
            }
            return socket_conn;
        }
        clock_gettime(CLOCK_REALTIME, &tv_now);
    }

    if (false == bsync) {
        LOG_DEBUG("get the connect from the socket pool timeout. all busy.");
        return NULL;
    }
    // Timeout
    struct sockaddr_un un;
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;
    snprintf(un.sun_path, sizeof(un.sun_path), "@%s", m_local_conf_->GetSockAddr(m_process_name_).c_str());

    socket_conn = NewConnection(un, kConnectionStatusAddTemp);
    return socket_conn;
}

bool CSocketPool::PutConnection(struct socket_connection_t *socket_conn) {
    if (socket_conn == NULL) return false;
    {
        QH_THREAD::CMutexAutoLocker Lck(&m_mutex_used_);
        m_socket_used_.erase(socket_conn->m_socket_fd_);
    }
    if (socket_conn->m_status_ == kConnectionStatusUsed) {
        socket_conn->m_status_ = kConnectionStatusUnused;
        QH_THREAD::CMutexAutoLocker Lck(&m_mutex_unused_);
        m_socket_unused_.push_back(socket_conn);
    } else if (socket_conn->m_status_ == kConnectionStatusAddTemp) {
        if (-1 == shutdown(socket_conn->m_socket_fd_, SHUT_RDWR)) {
            LOG_ERROR("process[%s] close socket failed", m_process_name_.c_str());
        }
        delete socket_conn;
    }
    return true;
}

void* CSocketPool::thread_function(void* param) {
    while (m_socket_connect_size_ < m_socket_pool_size_ && !CMultiThread::IsCancelled()) {
        LOG_DEBUG("wait all socket connect to server, waiting...");
        usleep(1000 * 1000);
    }
    int tid = proc_info_utils::GetTid();
    LOG_INFO("process[%s] started multithread for socket pool, tid[%d]", m_process_name_.c_str(), tid);
    QH_THREAD::CMutexManualLocker Lck(&m_mutex_unused_);
    Lck.lock();
    struct socket_connection_t *socket_conn = NULL;
    if (m_sock_pos_ < m_socket_unused_.size()) {
        socket_conn = m_socket_unused_[m_sock_pos_++];
    } else {
        Lck.unlock();
        return NULL;
    }
    Lck.unlock();
    int efd = -1;
#ifdef EPOLL_CLOEXEC
    efd = epoll_create1(EPOLL_CLOEXEC);
    if (efd < 0) {
        LOG_ERROR("tid[%d] create epoll failed.", tid);
        return NULL;
    }
#else
    // Since Linux 2.6.8, the size argument is ignored, but must be greater than zero
    efd = epoll_create(32);
    if (efd < 0) {
        LOG_ERROR("tid[%d] create epoll failed.", tid);
        return NULL;
    }
    file_utils::SetFDCLOEXEC(efd);
#endif

    int sockfd = socket_conn->m_socket_fd_;
    struct epoll_event tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.events = EPOLLIN;
    tmp.data.fd = sockfd;
    if (-1 == epoll_ctl(efd, EPOLL_CTL_ADD, sockfd, &tmp)) {
        LOG_ERROR("tid[%d] add epool event failed.", tid);
        return NULL;
    }
    std::string str_recv_data;
    char *recvline = new (std::nothrow) char[MAXLINE];
    if (NULL == recvline) return NULL;
    memset(recvline, 0, MAXLINE);
    while (!CMultiThread::IsCancelled()) {
        struct epoll_event ev[EPOLLEVENTS];
        memset(ev, 0, EPOLLEVENTS * sizeof(struct epoll_event));
        int ret = epoll_wait(efd, ev, EPOLLEVENTS, 500);
        for (int i = 0; i < ret; ++i) {
            int ev_fd = ev[i].data.fd;
            if (ev[i].events & EPOLLIN) {
                if (ev_fd == sockfd) {
                    memset(recvline, 0, MAXLINE);
                    std::string read_str("");
                    int read_count = -1;
                    // read_count = read(ev_fd, recvline, MAXLINE);
                    // if (read_count > 0)
                    //     read_str.append(recvline, read_count);
                    do {
                        read_count = read(ev_fd, recvline, MAXLINE);
                        if (read_count > 0)
                            read_str.append(recvline, read_count);
                        memset(recvline, 0, MAXLINE);
                    } while(read_count >= MAXLINE);
                    if ((read_str.empty() && read_count == 0) || (read_count < 0)) {
                        LOG_DEBUG("process[%s] read data from buffer failed, errno[%s], tid[%d]", m_process_name_.c_str(), strerror(errno), tid);
                        memset(&tmp, 0, sizeof(tmp));
                        tmp.events = EPOLLIN;
                        tmp.data.fd = sockfd;
                        epoll_ctl(efd, EPOLL_CTL_DEL, sockfd, &tmp);
                        if (NULL != socket_conn) {
                            socket_conn->m_status_ = kConnectionStatusUnkown;
                            if (socket_conn->m_socket_fd_ != -1) {
                                shutdown(socket_conn->m_socket_fd_, SHUT_RDWR);
                                close(socket_conn->m_socket_fd_);
                                socket_conn->m_socket_fd_ = -1;
                            }
                            socket_conn->p_cb_recv_data_ = NULL;
                        }
                        m_socket_connect_size_--;
                        break;
                    } else {
                        if (!read_str.empty()) {
                            LOG_DEBUG("process[%s] read data success, tid[%d], data[%s]", m_process_name_.c_str(), tid, read_str.c_str());
                            ParseRecivedData(str_recv_data, read_str);
                            socket_conn->p_cb_recv_data_(read_str);
                        }
                    }
                }
            }
        }
        if (m_socket_connect_size_ == 0) {
            LOG_DEBUG("detective the server exit, process[%s] lost all the connection.", m_process_name_.c_str(), tid);
            break;
        }
    }
    if (recvline) delete [] recvline;
    LOG_INFO("process[%s] exit multithread for socket pool, tid[%d]", m_process_name_.c_str(), tid);
    if (efd != -1) {
        close(efd);
    }
    return NULL;
}

bool CSocketPool::ParseRecivedData(std::string& str_remain_data, std::string& str_recv_data) {
    if (str_recv_data[0] != '{') {
        str_recv_data = str_remain_data + str_recv_data;
        str_remain_data.clear();
    }
    if (str_recv_data.find_first_of("{") == std::string::npos) {
        LOG_ERROR("recv transfer data error, format invalid.");
        return false;
    }
    size_t pos = str_recv_data.find_last_of("}");
    if (pos == std::string::npos) {
        LOG_DEBUG("recv a part of data, wait for other come.");
        str_remain_data = str_recv_data;
        return true;
    }
    if (pos + 1 < str_recv_data.length()) {
        str_remain_data = str_recv_data.substr(pos + 1);
    }
    str_recv_data = str_recv_data.substr(0, pos + 1);
    return true;
}
