# unix domain socket engine server

描述：多路数据转发器客户端实现；

---

导出接口：`typedef ISocketClientMgr* (*FCreateInstance)(const char* config_path);`
传入参数：`const char* config_path`
说明：
（1）config_path如果是文件，请参照下述的“配置文件配置方法示例”进行配置；
（2）config_path如果不是文件，传入参数为合法的“通信地址”，且需要手动注册所有接收和同步发送的事件
（3）针对托盘这种一连多server的情况，通信地址是为json object，格式如下示例：
```
"socket_addr" : {
    "socket.test.name.test_1" : "test_1",
    "socket.test.name.test_2" : "test_2"
}
```

---

配置文件配置方法示例：
```
{
    "log_level" : 4,                                     // 日志记录级别，0~5
    "log_size" : 10485760,                               // 日志记录大小，单位字节
    "log_path" : "/home/huweiping/test/test_client.log", // 日志记录路径
    "log_backup_path" : "/home/huweiping/test/backup",   // 日志备份路径
    "socket_addr" : "/home/huweiping/hwp_test",          // 通信地址
    "recv_funcs" : {                                     // 接收事件列表
        "test_service_1" : [                             // 接收事件者1
            "socket.test.events.async_10",               // 接收事件者1的接收事件1
            "socket.test.events.async_11",
            "socket.test.events.async_12",
            "socket.test.events.sync_10",
            "socket.test.events.sync_11",
            "socket.test.events.sync_12"
        ],
        "test_service_2" : [                             // 接收事件者2
            "socket.test.events.async_20",               // 接收事件者2的接收事件1
            "socket.test.events.async_21",
            "socket.test.events.async_22",
            "socket.test.events.sync_20",
            "socket.test.events.sync_21",
            "socket.test.events.sync_22"
        ]
    },
    "intrested_funcs" : {                                // 同步发送事件列表
        "socket.test.name.test_1" : [                    // 同步发送者1
            // 同步发送者1发送的注册事件，超时时间2秒[可选，默认10秒]
            { "funcs" : "socket.*.cmd.login", "timeout" : 2000 },
            { "funcs" : "socket.test.events.sync_10", "timeout" : 5000 },
            { "funcs" : "socket.test.events.sync_11", "timeout" : 10000 },
            { "funcs" : "socket.test.events.sync_12" },
            { "funcs" : "socket.test.events.sync_20", "timeout" : 5000 },
            { "funcs" : "socket.test.events.sync_21" },
            { "funcs" : "socket.test.events.sync_22", "timeout" : 10000 }
        ]
    }
}
```