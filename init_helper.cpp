#include "init_helper.h"
#include "common/utils/file_utils.h"
#include "common/utils/proc_info_utils.h"
#include "common/AsFramework/util/ASJsonWrapper.h"
// #include "common/ASFramework/util/ASLogImpl.h"


bool CLocalConf::Init(const char* conf) {
    Json::Value jvRoot;
    if (false == InnerLoadConfFile(conf, jvRoot)) {
        return false;
    }
    if (m_config_method_ == kConfigOnlyAddr) {
        m_str_soc_addr_ = conf;
        return true;
    }
    GetLogConfig(jvRoot);
    GetSocketConfig(jvRoot);
    if (false == GetIntrestedFunctionsConfig(jvRoot)) {
        return false;
    }
    if (false == GetRecvFunctionsConfig(jvRoot)) {
        return false;
    }
    return true;
}

bool CLocalConf::InnerLoadConfFile(const std::string &conf, Json::Value &jvRoot) {
    // 若文件不存在，传入的配置参数为所设置的socket addr
    if (conf.empty() || !file_utils::IsExist(conf)) {
        m_config_method_ = kConfigOnlyAddr;
        return true;
    }

    // 若文件存在，解析文件相关配置。请按照配置进行配置，不做细致的容错处理
    if (!CASJsonWrapper::LoadJsonFile(conf.c_str(), jvRoot)) {
        return false;
    }

    return true;
}

bool CLocalConf::GetLogConfig(const Json::Value &jvRoot) {
    m_log_level_ = CASJsonWrapper::GetJsonValueInt("log_level", jvRoot, 2);
    m_log_max_size_ = CASJsonWrapper::GetJsonValueInt("log_size", jvRoot, 10 * 1024 * 1024);
    m_log_path_ = CASJsonWrapper::GetJsonValueString("log_path", jvRoot);
    // 支持相对路径设置，以达更好的兼容性
    if (m_log_path_.length() >= 2 && m_log_path_[0] == '.' && m_log_path_[1] == '/') {
        m_log_path_ = proc_info_utils::GetInstallPath() + "/" + m_log_path_;
    }
    m_log_backup_path_ = CASJsonWrapper::GetJsonValueString("log_backup_path", jvRoot);
    // 支持相对路径设置，以达更好的兼容性
    if (m_log_backup_path_.length() >= 2 && m_log_backup_path_[0] == '.' && m_log_backup_path_[1] == '/') {
        m_log_backup_path_ = proc_info_utils::GetInstallPath() + "/" + m_log_backup_path_;
    }
    return true;
}

bool CLocalConf::GetSocketConfig(const Json::Value &jvRoot) {
    if (!jvRoot["socket_addr"].isNull()) {
        if (jvRoot["socket_addr"].isString()) {
            m_str_soc_addr_ = CASJsonWrapper::GetJsonValueString("socket_addr", jvRoot);
        } else {
            Json::Value jvSocketAddrs;
            if (CASJsonWrapper::GetJsonValueObject("socket_addr", jvRoot, jvSocketAddrs)) {
                std::vector<std::string> process_name = jvSocketAddrs.getMemberNames();
                for (int nIndex = 0; nIndex < process_name.size(); nIndex++) {
                    std::string str_soc_addr = CASJsonWrapper::GetJsonValueString(process_name[nIndex].c_str(), jvSocketAddrs, "");
                    m_soc_addr_map_[process_name[nIndex]] = str_soc_addr;
                }
            }
            if (m_soc_addr_map_.size()) {
                m_str_soc_addr_ = m_soc_addr_map_.begin()->second;
            }
        }
    } else {
        LOG_ERROR("conf file has not config socket addr, unknown.");
        return false;
    }
    m_thread_nums_ = CASJsonWrapper::GetJsonValueInt("thread_nums", jvRoot, 3);
    return true;
}

bool CLocalConf::GetRecvFunctionsConfig(const Json::Value &jvRoot) {
    Json::Value jvFunc;
    if (!CASJsonWrapper::GetJsonValueObject("recv_funcs", jvRoot, jvFunc)) {
        return false;
    }
    std::vector<std::string> service = jvFunc.getMemberNames();
    for (int nIndex = 0; nIndex < service.size(); nIndex++) {
        Json::Value item;
        if (!CASJsonWrapper::GetJsonValueArray(service[nIndex].c_str(), jvFunc, item)) {
            continue;
        }
        std::vector<std::string> funcs;
        for (int mIndex = 0; mIndex < item.size(); mIndex++) {
            Json::Value sub_item = item[mIndex];
            if (sub_item.isNull() || !sub_item.isString()) {
                continue;
            }
            funcs.push_back(sub_item.asCString());
        }
        m_recv_func_[service[nIndex]] = funcs;
    }
    return true;
}

bool CLocalConf::GetIntrestedFunctionsConfig(const Json::Value &jvRoot) {
    Json::Value jvFunc;
    if (!CASJsonWrapper::GetJsonValueObject("intrested_funcs", jvRoot, jvFunc)) {
        return false;
    }
    std::vector<std::string> service = jvFunc.getMemberNames();
    for (int nIndex = 0; nIndex < service.size(); nIndex++) {
        Json::Value item;
        if (!CASJsonWrapper::GetJsonValueArray(service[nIndex].c_str(), jvFunc, item)) {
            continue;
        }
        std::vector<std::pair<std::string, int> > funcs;
        for (int mIndex = 0; mIndex < item.size(); mIndex++) {
            Json::Value sub_item = item[mIndex];
            if (sub_item.isNull() || !sub_item.isObject()) {
                continue;
            }
            int timeout = CASJsonWrapper::GetJsonValueInt("timeout", sub_item, 15 * 1000);
            std::string functions = CASJsonWrapper::GetJsonValueString("funcs", sub_item);
            funcs.push_back(make_pair(functions, timeout));
        }
        m_intrested_func_[service[nIndex]] = funcs;
    }
    return true;
}

bool CLocalConf::ManualRegistedIntrestedFuncs(const char* function, const char* process_name, unsigned int timeout) {
    IntrestedMap::iterator iter = m_intrested_func_.find(process_name);
    if (iter != m_intrested_func_.end()) {
        iter->second.push_back(make_pair(function, timeout));
    } else {
        std::vector<std::pair<std::string, int> > funcs;
        funcs.push_back(make_pair(function, timeout));
        m_intrested_func_[process_name] = funcs;
    }
    return true;
}

long CLocalConf::GetFunctionTimeOut(const std::string& user, const std::string& func) {
    // 初始化时需确定好map的大小，此处使用不再上锁
    IntrestedMap::iterator iter = m_intrested_func_.find(user);
    if (iter != m_intrested_func_.end()) {
        for (int nIndex = 0; nIndex < iter->second.size(); nIndex++) {
            if (func == iter->second[nIndex].first) {
                return iter->second[nIndex].second;
            }
        }
    }
    return 10 * 1000;
}

CLogHelper::CLogHelper() {
    // m_log_level_ = ASLog_Level_Trace;
    // m_log_size_ = 10 * 1024 * 1024;
    // m_log_ptr_ = NULL;
}

CLogHelper::~CLogHelper()  {
    Uninit();
}

bool CLogHelper::Init() {
    // TODO：这里要做日志的设置
    // m_log_ptr_ = new (std::nothrow) CASLogImpl();
    // m_log_ptr_->AddRef();
    // m_log_ptr_->SetLogFilePath(m_log_path_.c_str());
    // m_log_ptr_->SetBackupFilePath(m_log_backup_path_.c_str());
    // m_log_ptr_->SetLogMaxSize(m_log_size_);
    // m_log_ptr_->SetLogLevel(m_log_level_);
    // m_log_ptr_->Open();
    // CEntModuleDebug::SetModuleDebugger(m_log_ptr_);
    return true;
}

bool CLogHelper::Uninit() {
    // TODO：要做日志设置类的析构

    // 场景： 针对同一个进程不同插件业务创建多个连接，如果各业务插件存在插拔的场景；
    // 原因： -Wl,-Bsymbolic无法做到这个级别的全局&静态变量的隔离；
    // 分析： 会导致多个连接的日志指针公用m_pDebugger，但是引用计数却被最后赋值为2；
    // TODO: 1. 其他业务的m_log_ptr_存在内存泄露，优先级：低 （2021.04.09）
    // TODO: 2. 考虑这种业务场景下涉及的多线程性操作非安全，优先级：中 （2021.04.09）
    // if ((NULL != CEntModuleDebug::GetModuleDebugger()) && (0 == CEntModuleDebug::GetModuleDebugger()->Release())) {
    //     m_log_ptr_ = NULL;
    // }
    return true;
}
