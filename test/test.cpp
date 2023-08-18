#include <string>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <signal.h>
#include "common/singleton.hpp"
#include "common/socket/socket_process_info.h"
#include "common/socket_client/ISocketClientMgr.h"
#include "test_class_1.h"
#include "test_class_2.h"
#include "socket_test.h"

static void* dl_handler = NULL;
static ISocketClientMgr* p_socket_client = NULL;
static bool runloop = true;

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

int main(int argc,char* argv[])
{
    signal(SIGINT,  signal_exit_handler); /* Interactive attention signal.  */
    signal(SIGILL,  signal_exit_handler); /* Illegal instruction.  */
    signal(SIGABRT, signal_exit_handler); /* Abnormal termination.  */
    signal(SIGTERM, signal_exit_handler); /* Termination request.  */
    signal(SIGKILL, signal_exit_handler); /* Termination request.  */

    CreateInstance(argv[1], argv[2]);

    if (!p_socket_client) { return -1; }
    CTest1 *p_test1 = &Singleton<CTest1>::Instance();
    CTest2 *p_test2 = &Singleton<CTest2>::Instance();
    p_test1->Init(p_socket_client);
    p_test2->Init(p_socket_client);
    p_socket_client->ConnectSocket(TEST_1_NAME);
    p_socket_client->Run();
    while (!p_test1->GetLoginStatus()) {
        printf("waitting login success...\n");
        sleep(1);
    }
    while(runloop) {
        std::string str_recv;
        p_test1->SyncSendDataToOtherProcess("test_sync10", TEST_1_NAME, TEST_REGISTER_FUNCTION_TEST_SYNC_10 , str_recv);
        p_test1->SyncSendDataToOtherProcess("test_sync11", TEST_1_NAME, TEST_REGISTER_FUNCTION_TEST_SYNC_11 , str_recv);
        p_test1->SyncSendDataToOtherProcess("test_sync12", TEST_1_NAME, TEST_REGISTER_FUNCTION_TEST_SYNC_12 , str_recv);
        p_test2->SyncSendDataToOtherProcess("test_sync20", TEST_1_NAME, TEST_REGISTER_FUNCTION_TEST_SYNC_20 , str_recv);
        p_test2->SyncSendDataToOtherProcess("test_sync21", TEST_1_NAME, TEST_REGISTER_FUNCTION_TEST_SYNC_21 , str_recv);
        p_test2->SyncSendDataToOtherProcess("test_sync22", TEST_1_NAME, TEST_REGISTER_FUNCTION_TEST_SYNC_22 , str_recv);
        p_test1->ASyncSendDataToOtherProcess("test_async10", TEST_1_NAME, TEST_REGISTER_FUNCTION_TEST_ASYNC_10);
        p_test1->ASyncSendDataToOtherProcess("test_async11", TEST_1_NAME, TEST_REGISTER_FUNCTION_TEST_ASYNC_11);
        p_test1->ASyncSendDataToOtherProcess("test_async12", TEST_1_NAME, TEST_REGISTER_FUNCTION_TEST_ASYNC_12);
        p_test2->ASyncSendDataToOtherProcess("test_async20", TEST_1_NAME, TEST_REGISTER_FUNCTION_TEST_ASYNC_20);
        p_test2->ASyncSendDataToOtherProcess("test_async21", TEST_1_NAME, TEST_REGISTER_FUNCTION_TEST_ASYNC_21);
        p_test2->ASyncSendDataToOtherProcess("test_async22", TEST_1_NAME, TEST_REGISTER_FUNCTION_TEST_ASYNC_22);
        getchar();
    }

    p_test1->UnInit();
    p_test2->UnInit();
    p_socket_client->Release();
    dlclose(dl_handler);
    return 0;
}
