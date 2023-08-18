#ifndef INIT_HELPER_H_
#define INIT_HELPER_H_

#include <map>
#include <vector>
#include <string>
#include <string.h>
#include "common/dependlibs/CentOS_jsoncpp/include/jsoncpp/json/value.h"
// #include "json/value.h"
// #include "common/log/log.h"
#include "common/log/log.hpp"

class CLocalConf {

  typedef std::map<std::string, std::vector<std::string> > RecvMap;
  typedef std::map<std::string, std::vector<std::pair<std::string, int> > > IntrestedMap;
  enum ConfigMethod { kConfigFile, kConfigOnlyAddr };

  public:
    CLocalConf() {
        m_log_level_ = 2;
        m_log_max_size_ = 10 * 1024 * 1024;
        m_thread_nums_ = 3;
        m_str_soc_addr_ = "not_config_socket_addr";
        m_config_method_ = kConfigFile;
    }
    ~CLocalConf() {
    }

  public:
    bool Init(const char* conf);

  public:
    int GetLogLevel() { return m_log_level_; }
    int GetLogMaxSize() { return m_log_max_size_; }
    int GetSocketClientThreadNum() { return m_thread_nums_; }
    std::string GetSockAddr() { return m_str_soc_addr_; }
    std::string GetSockAddr(const std::string &str_process_name) {
        if (m_soc_addr_map_.find(str_process_name) == m_soc_addr_map_.end()) {
            return m_str_soc_addr_;
        } else {
            return m_soc_addr_map_[str_process_name];
        }
    }
    std::string GetLogPath() { return m_log_path_; }
    std::string GetLogBackupPath() { return m_log_backup_path_; }
    RecvMap GetRecvFunctions() { return m_recv_func_; }
    IntrestedMap GetIntrestedFunc() { return m_intrested_func_; }
    bool IsConfigMethodFile() { return m_config_method_ == kConfigFile; }
    bool ManualRegistedIntrestedFuncs(const char* function, const char* process_name, unsigned int timeout);
    long GetFunctionTimeOut(const std::string& user, const std::string& func);

  private:
    bool InnerLoadConfFile(const std::string &conf, Json::Value &jvRoot);
    bool GetLogConfig(const Json::Value &jvRoot);
    bool GetSocketConfig(const Json::Value &jvRoot);
    bool GetRecvFunctionsConfig(const Json::Value &jvRoot);
    bool GetIntrestedFunctionsConfig(const Json::Value &jvRoot);

  private:
    int m_log_level_;
    int m_log_max_size_;
    int m_thread_nums_;
    std::string m_str_soc_addr_;
    std::map<std::string, std::string> m_soc_addr_map_;
    std::string m_log_path_;
    std::string m_log_backup_path_;
    ConfigMethod m_config_method_;
  
  private:
    RecvMap m_recv_func_;
    IntrestedMap m_intrested_func_;
};

class CASLogImpl;

class CLogHelper {
  public:
    CLogHelper();
    ~CLogHelper();

  public:
    bool Init();
    bool Uninit();

  public:
    // void SetLogLevel(ASLogLevel log_level) {
    //     m_log_level_ = log_level;
    // }
    // void SetLogMaxSize(int log_size) {
    //     m_log_size_ = log_size;
    // }
    // void SetLogPath(const char* log_path) {
    //     m_log_path_ = log_path;
    // }
    // void SetLogBackupPath(const char* log_backup_path) {
    //     m_log_backup_path_ = log_backup_path;
    // }

  private:
    // ASLogLevel m_log_level_;
    int m_log_size_;
    std::string m_log_path_;
    std::string m_log_backup_path_;
    // CASLogImpl* m_log_ptr_;
};

#endif /* INIT_HELPER_H_ */
