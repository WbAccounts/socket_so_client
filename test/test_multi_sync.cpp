#include <string>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <signal.h>
#include "common/singleton.hpp"
#include "common/qh_thread/thread.h"
#include "common/socket/socket_process_info.h"
#include "common/socket_client/ISocketClientMgr.h"
#include "test_class_1.h"
#include "test_class_2.h"
#include "socket_test.h"

#define WORKERNUM 10

static void* dl_handler = NULL;
static ISocketClientMgr* p_socket_client = NULL;
static bool runloop = true;
CTest1 *p_test1 = NULL;
CTest2 *p_test2 = NULL;

void CreateInstance(const char* dl_path, const char* config) {
    dl_handler = dlopen(dl_path, RTLD_LAZY);
    if (!dl_handler) { 
        fprintf(stderr,"dlopen %s failed: %s.\n", dl_path,dlerror());
        return;
    }

    dlerror();
    FCreateInstance fn = (FCreateInstance)dlsym(dl_handler, "CreateInstance");
    if(!fn) {
        fprintf(stderr,"dlsym CreateInstance failed: %s.\n", dlerror());
        dlclose(dl_handler);
        return;
    }
    p_socket_client = fn(config);
}

static void signal_exit_handler(int sig) {
    fprintf(stderr,"recv sig = [%d], main process exit.\n", sig);
    runloop = false;
}

static void* SendData(void* para) {
    QH_THREAD::CWorkerThread* cur_thread_p = static_cast<QH_THREAD::CWorkerThread*>(para);
    while (!cur_thread_p->IsQuit()) {
        std::string str_recv;
        p_test1->SyncSendDataToOtherProcess("test_sync10", TEST_1_NAME, TEST_REGISTER_FUNCTION_TEST_SYNC_10 , str_recv);
        // while (runloop) sleep(1);
        sleep(1);
    }
    return NULL;
}

int main(int argc,char* argv[])
{
    signal(SIGINT,  signal_exit_handler); /* Interactive attention signal.  */
    signal(SIGILL,  signal_exit_handler); /* Illegal instruction.  */
    signal(SIGABRT, signal_exit_handler); /* Abnormal termination.  */
    signal(SIGTERM, signal_exit_handler); /* Termination request.  */
    signal(SIGKILL, signal_exit_handler); /* Termination request.  */

    CreateInstance(argv[1], argv[2]);

    if (!p_socket_client) { return -1; }
    p_test1 = &Singleton<CTest1>::Instance();
    p_test2 = &Singleton<CTest2>::Instance();
    p_test1->Init(p_socket_client);
    p_test2->Init(p_socket_client);
    p_socket_client->ConnectSocket(TEST_1_NAME);
    p_socket_client->Run();
    while (!p_test1->GetLoginStatus()) {
        printf("waitting login success...\n");
        sleep(1);
    }
    QH_THREAD::CWorkerThread worker[WORKERNUM];
    for (int i = 0; i < WORKERNUM; i++) {
        worker[i].SetThreadFunc(std::tr1::bind(&SendData, std::tr1::placeholders::_1));
        worker[i].Run(&worker[i]);
    }
    while(runloop) {
        sleep(1);
    }

    p_test1->UnInit();
    p_test2->UnInit();
    for (int i = 0 ; i < WORKERNUM; i++) {
        worker[i].Quit();
        worker[i].Join();
    }
    p_socket_client->Release();
    dlclose(dl_handler);
    return 0;
}
