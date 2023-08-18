#ifndef LOCAL_SOCKET_TEST_H_
#define LOCAL_SOCKET_TEST_H_

#define TEST_SERVICE_1 "test_service_1"
#define TEST_SERVICE_2 "test_service_2"

namespace SocketProcessNameID {
    // 测试 1000~1050
    const long TEST_1_ID = 1000;
    const long TEST_2_ID = 1001;
};

namespace SocketProcessNameStr {
    const char* const TEST_1_NAME = "socket.test.name.test_1";
    const char* const TEST_2_NAME = "socket.test.name.test_2";
};

namespace RegisterFunctionStr {
    const char* const TEST_REGISTER_FUNCTION_TEST_SYNC_10 = "socket.test.events.sync_10";
    const char* const TEST_REGISTER_FUNCTION_TEST_SYNC_11 = "socket.test.events.sync_11";
    const char* const TEST_REGISTER_FUNCTION_TEST_SYNC_12 = "socket.test.events.sync_12";
    const char* const TEST_REGISTER_FUNCTION_TEST_SYNC_20 = "socket.test.events.sync_20";
    const char* const TEST_REGISTER_FUNCTION_TEST_SYNC_21 = "socket.test.events.sync_21";
    const char* const TEST_REGISTER_FUNCTION_TEST_SYNC_22 = "socket.test.events.sync_22";
    const char* const TEST_REGISTER_FUNCTION_TEST_ASYNC_10 = "socket.test.events.async_10";
    const char* const TEST_REGISTER_FUNCTION_TEST_ASYNC_11 = "socket.test.events.async_11";
    const char* const TEST_REGISTER_FUNCTION_TEST_ASYNC_12 = "socket.test.events.async_12";
    const char* const TEST_REGISTER_FUNCTION_TEST_ASYNC_20 = "socket.test.events.async_20";
    const char* const TEST_REGISTER_FUNCTION_TEST_ASYNC_21 = "socket.test.events.async_21";
    const char* const TEST_REGISTER_FUNCTION_TEST_ASYNC_22 = "socket.test.events.async_22";
};

#endif /* LOCAL_SOCKET_TEST_H_ */
