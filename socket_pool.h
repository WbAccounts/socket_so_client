#ifndef SOCKET_POOL_H_
#define SOCKET_POOL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <vector>
#include <map>
// #include "common/qh_thread/multi_thread.h"
#include "common/qh_thread/multi_thread.h"
#include "common/socket/socket_utils.h"
#include "common/socket_client/ISocketClientMgr.h"
#include "common/singleton.hpp"
#include "init_helper.h"

typedef std::tr1::function<int(std::string &)> fn_InsertRecvData;

enum ConnectionStatus {
    kConnectionStatusUnkown = -1,
    kConnectionStatusUnused,
    kConnectionStatusUsed,
    kConnectionStatusInvalid,
    kConnectionStatusAddTemp,
    kConnectionStatusMax
};

struct socket_connection_t {
    ConnectionStatus m_status_;
    int m_socket_fd_;
    fn_InsertRecvData p_cb_recv_data_;
    socket_connection_t() {
        m_status_ = kConnectionStatusUnkown;
        m_socket_fd_ = -1;
        p_cb_recv_data_ = NULL;
    }
    ~socket_connection_t() {
        m_status_ = kConnectionStatusUnkown;
        if (m_socket_fd_ != -1) {
            shutdown(m_socket_fd_, SHUT_RDWR);
            close(m_socket_fd_);
            m_socket_fd_ = -1;
        }
        p_cb_recv_data_ = NULL;
    }
};

class CSocketPool : public QH_THREAD::CMultiThread {
  public:
    CSocketPool(const std::string& process_name, int pool_size)
        : m_process_name_(process_name)
        , m_sock_pos_(0)
        , m_socket_connect_size_(0)
        , m_socket_pool_size_(pool_size)
        , p_recv_data_(NULL)
        , m_cb_bundle_(NULL)
        , m_local_conf_(NULL) {
    }
    ~CSocketPool();

  public:
    bool InitSocketPool(const fn_InsertRecvData &recv_callback, CLocalConf* local_conf);
    bool DestroySocketPool();
    void Run();
    void ClearSocketSource();
    void RegClientCb(IASBundle* cb_bundle);

  public:
    bool PutConnection(struct socket_connection_t* socket_conn);
    struct socket_connection_t* GetConnection(bool bsync);
    int getSocketPoolSize() { return m_socket_pool_size_; }

  private:
    struct socket_connection_t* NewConnection(struct sockaddr_un& sockaddr_info, ConnectionStatus status);
    bool ParseRecivedData(std::string& str_remain_data, std::string& recv_data);
    void* ConnectSocket(void* para);
    void* MonitorSocketStatus(void* para);

  protected:
    void* thread_function(void* param);

  private:
    std::vector<struct socket_connection_t *> m_socket_unused_;
    std::map<int, struct socket_connection_t *> m_socket_used_;
    QH_THREAD::CMutex m_mutex_used_;
    QH_THREAD::CMutex m_mutex_unused_;
    QH_THREAD::CWorkerThread m_socket_worker_;
    QH_THREAD::CWorkerThread m_client_connect_;
    std::string m_process_name_;
    int m_socket_pool_size_;
    int m_sock_pos_;
    int m_socket_connect_size_;
    fn_InsertRecvData p_recv_data_;
    IASBundle *m_cb_bundle_;
    CLocalConf *m_local_conf_;
};

#endif /* SOCKET_POOL_H_ */
