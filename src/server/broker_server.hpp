// ======================= broker_server.hpp =======================
#pragma once

#include <memory>
#include <string>

#include "../common/msg.pb.h"          // 各种请求 / 响应消息定义
#include "../common/protocol.pb.h"

#include "connection.hpp"              // connection / connection_manager (前向声明已在头内)

// -------------------- Muduo 前向声明 ------------------------------
namespace muduo {
namespace net {
class EventLoop;
class TcpServer;
class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
class InetAddress;
} // namespace net
class Timestamp;          // muduo::Timestamp
} // namespace muduo

// Protobuf Codec / Dispatcher 前向声明 -----------------------------
class ProtobufDispatcher;
class ProtobufCodec;

namespace hz_mq {

// 其余模块前向声明，减少耦合 ---------------------------------------
class virtual_host;
class consumer_manager;
class connection_manager;
class thread_pool;

// 便捷别名（pb 指针） ---------------------------------------------
using ProtobufCodecPtr         = std::shared_ptr<ProtobufCodec>;
using openChannelRequestPtr    = std::shared_ptr<openChannelRequest>;
using closeChannelRequestPtr   = std::shared_ptr<closeChannelRequest>;
using declareExchangeRequestPtr= std::shared_ptr<declareExchangeRequest>;
using deleteExchangeRequestPtr = std::shared_ptr<deleteExchangeRequest>;
using declareQueueRequestPtr   = std::shared_ptr<declareQueueRequest>;
using deleteQueueRequestPtr    = std::shared_ptr<deleteQueueRequest>;
using bindRequestPtr           = std::shared_ptr<bindRequest>;
using unbindRequestPtr         = std::shared_ptr<unbindRequest>;
using basicPublishRequestPtr   = std::shared_ptr<basicPublishRequest>;
using basicAckRequestPtr       = std::shared_ptr<basicAckRequest>;
using basicConsumeRequestPtr   = std::shared_ptr<basicConsumeRequest>;
using basicCancelRequestPtr    = std::shared_ptr<basicCancelRequest>;
using basicQueryRequestPtr     = std::shared_ptr<basicQueryRequest>;
using heartbeatRequestPtr      = std::shared_ptr<heartbeatRequest>;
using MessagePtr               = std::shared_ptr<::google::protobuf::Message>;
using queueStatusRequestPtr    = std::shared_ptr<queueStatusRequest>;
using queueStatusResponsePtr   = std::shared_ptr<queueStatusResponse>;
using declareQueueWithDLQRequestPtr = std::shared_ptr<declareQueueWithDLQRequest>;
using basicNackRequestPtr = std::shared_ptr<basicNackRequest>;

// 常量 -------------------------------------------------------------
inline constexpr const char* DBFILE_PATH = "/meta.db";
inline constexpr const char* HOST_NAME   = "MyVirtualHost";

// ================================================================
// BrokerServer : 启动 TCP 服务、分发 Protobuf 消息、维护核心管理器
// ================================================================
class BrokerServer {
public:
    BrokerServer(int port, const std::string& base_dir);
    void start();   // 启动事件循环

private:
    // 内部辅助 -----------------------------------------------------
    void printServerInfo();
    void printConnectionInfo(const muduo::net::TcpConnectionPtr& conn);

    // 网络事件回调 -------------------------------------------------
    void onConnection(const muduo::net::TcpConnectionPtr& conn);

    // 未知消息
    void onUnknownMessage(const muduo::net::TcpConnectionPtr& conn,
                          const MessagePtr& message,
                          muduo::Timestamp ts);

    // -- 各类请求回调 --------------------------------------------
    void on_openChannel   (const muduo::net::TcpConnectionPtr&, const openChannelRequestPtr&,    muduo::Timestamp);
    void on_closeChannel  (const muduo::net::TcpConnectionPtr&, const closeChannelRequestPtr&,   muduo::Timestamp);
    void on_declareExchange(const muduo::net::TcpConnectionPtr&, const declareExchangeRequestPtr&,muduo::Timestamp);
    void on_deleteExchange(const muduo::net::TcpConnectionPtr&, const deleteExchangeRequestPtr&, muduo::Timestamp);
    void on_declareQueue  (const muduo::net::TcpConnectionPtr&, const declareQueueRequestPtr&,   muduo::Timestamp);
    void on_deleteQueue   (const muduo::net::TcpConnectionPtr&, const deleteQueueRequestPtr&,    muduo::Timestamp);
    void on_bind          (const muduo::net::TcpConnectionPtr&, const bindRequestPtr&,           muduo::Timestamp);
    void on_unbind        (const muduo::net::TcpConnectionPtr&, const unbindRequestPtr&,         muduo::Timestamp);
    void on_basicPublish  (const muduo::net::TcpConnectionPtr&, const basicPublishRequestPtr&,   muduo::Timestamp);
    void on_basicAck      (const muduo::net::TcpConnectionPtr&, const basicAckRequestPtr&,       muduo::Timestamp);
    void on_basicConsume  (const muduo::net::TcpConnectionPtr&, const basicConsumeRequestPtr&,   muduo::Timestamp);
    void on_basicCancel   (const muduo::net::TcpConnectionPtr&, const basicCancelRequestPtr&,    muduo::Timestamp);
    void on_basicQuery    (const muduo::net::TcpConnectionPtr&, const basicQueryRequestPtr&,     muduo::Timestamp);
    void on_heartbeat     (const muduo::net::TcpConnectionPtr&, const heartbeatRequestPtr&,      muduo::Timestamp);
    void on_declareQueueWithDLQ(const muduo::net::TcpConnectionPtr&, const declareQueueWithDLQRequestPtr&, muduo::Timestamp);
    void on_basicNack     (const muduo::net::TcpConnectionPtr&, const basicNackRequestPtr&,      muduo::Timestamp);
    void on_queueStatusRequest(const muduo::net::TcpConnectionPtr&, const queueStatusRequestPtr&, muduo::Timestamp);

    virtual_host::ptr get_virtual_host() const { return __virtual_host; }
private:
    std::unique_ptr<muduo::net::EventLoop>   __loop;
    std::unique_ptr<muduo::net::TcpServer>   __server;

    std::unique_ptr<ProtobufDispatcher>      __dispatcher;
    ProtobufCodecPtr                         __codec;

    virtual_host::ptr                        __virtual_host;
    consumer_manager::ptr                    __consumer_manager;
    connection_manager::ptr                  __connection_manager;
    thread_pool::ptr                         __thread_pool;
};

} 