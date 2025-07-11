#include "broker_server.hpp"

// ---- Muduo 头文件 ------------------------------------------------
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"
#include "muduo/net/InetAddress.h"
#include "muduo/protoc/codec.h"
#include "muduo/protoc/dispatcher.h"

// ---- POSIX -------------------------------------------------------
#include <pwd.h>
#include <unistd.h>
#include <ctime>
#include <sys/types.h>
#include <chrono>

// ---- 本项目 ------------------------------------------------------
#include "../common/logger.hpp"
#include "virtual_host.hpp"
#include "consumer.hpp"
#include "connection.hpp"
#include "route.hpp"

namespace hz_mq {

// -----------------------------------------------------------------------------
BrokerServer::BrokerServer(int port, const std::string& base_dir)
{
    // 1. 创建核心组件 ----------------------------------------------------------
    __loop  = std::make_unique<muduo::net::EventLoop>();
    __server= std::make_unique<muduo::net::TcpServer>(__loop.get(), muduo::net::InetAddress("0.0.0.0", port),
                                                      "hz_mq_server", muduo::net::TcpServer::kReusePort);

    __dispatcher = std::make_unique<ProtobufDispatcher>(
        std::bind(&BrokerServer::onUnknownMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    __codec = std::make_shared<ProtobufCodec>(
        std::bind(&ProtobufDispatcher::onProtobufMessage, __dispatcher.get(),
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // 2. 虚拟主机 & 管理器 -----------------------------------------------------
    std::string db_path = base_dir + DBFILE_PATH;
    __virtual_host       = std::make_shared<virtual_host>(HOST_NAME, base_dir, db_path);
    __consumer_manager   = std::make_shared<consumer_manager>();
    __connection_manager = std::make_shared<connection_manager>();
    __thread_pool        = std::make_shared<thread_pool>();

    // 3. 为已存在队列初始化消费者列表 -----------------------------------------
    for (const auto& [qname, _] : __virtual_host->all_queues()) {
        __consumer_manager->init_queue_consumer(qname);
    }

    // 4. 注册回调 --------------------------------------------------------------
#define REG(msgType, handler) \
    __dispatcher->registerMessageCallback<msgType>( std::bind(handler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) )

    REG(openChannelRequest,      &BrokerServer::on_openChannel);
    REG(closeChannelRequest,     &BrokerServer::on_closeChannel);
    REG(declareExchangeRequest,  &BrokerServer::on_declareExchange);
    REG(deleteExchangeRequest,   &BrokerServer::on_deleteExchange);
    REG(declareQueueRequest,     &BrokerServer::on_declareQueue);
    REG(deleteQueueRequest,      &BrokerServer::on_deleteQueue);
    REG(bindRequest,             &BrokerServer::on_bind);
    REG(unbindRequest,           &BrokerServer::on_unbind);
    REG(basicPublishRequest,     &BrokerServer::on_basicPublish);
    REG(basicAckRequest,         &BrokerServer::on_basicAck);
    REG(basicConsumeRequest,     &BrokerServer::on_basicConsume);
    REG(basicCancelRequest,      &BrokerServer::on_basicCancel);
    REG(basicQueryRequest,       &BrokerServer::on_basicQuery);
    REG(heartbeatRequest,        &BrokerServer::on_heartbeat);
    REG(queueStatusRequest,     &BrokerServer::on_queueStatusRequest);
#undef REG

    // 5. 网络层回调 ------------------------------------------------------------
    __server->setMessageCallback( std::bind(&ProtobufCodec::onMessage, __codec, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) );
    __server->setConnectionCallback( std::bind(&BrokerServer::onConnection, this, std::placeholders::_1) );

        __loop->runEvery(5.0, [this]() {
        __connection_manager->check_timeout(std::chrono::seconds(30));
    });
}

// -----------------------------------------------------------------------------
void BrokerServer::start()
{
    printServerInfo();
    __server->start();
    __loop->loop();
}

// -----------------------------------------------------------------------------
// 打印启动信息
// -----------------------------------------------------------------------------
void BrokerServer::printServerInfo()
{
    std::string addr = __server->ipPort();
    std::time_t now  = std::time(nullptr);
    std::string time_str = std::ctime(&now);
    struct passwd* pw = getpwuid(getuid());
    std::string user = pw ? pw->pw_name : "Unknown";

    pid_t pid = getpid();

    LOG(INFO) << "\n------------------- BrokerServer Start -------------------\n"
              << "Listen: " << addr << "\n"
              << "Time  : " << time_str
              << "User  : " << user << "\n"
              << "PID   : " << pid  << "\n"
              << "---------------------------------------------------------\n";
}

void BrokerServer::printConnectionInfo(const muduo::net::TcpConnectionPtr& conn)
{
    LOG_INFO << "\nNew Connection:"
            << "\nName  : "  << conn->name()
            << "\nLocal : "  << conn->localAddress().toIpPort()
            << "\nPeer  : "  << conn->peerAddress().toIpPort()
            << '\n';   // ← 换行用 '\n'
}

// -----------------------------------------------------------------------------
// 网络连接回调
// -----------------------------------------------------------------------------
void BrokerServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
    if (conn->connected()) {
        LOG(INFO) << "connected";
        printConnectionInfo(conn);
        __connection_manager->new_connection(__virtual_host, __consumer_manager, __codec, conn, __thread_pool);
    } else {
        LOG(INFO) << "disconnected";
        __connection_manager->delete_connection(conn);
    }
}

// -----------------------------------------------------------------------------
// 未知消息
// -----------------------------------------------------------------------------
void BrokerServer::onUnknownMessage(const muduo::net::TcpConnectionPtr& conn, const MessagePtr& msg, muduo::Timestamp)
{
    LOG(WARNING) << "Unknown Message: " << msg->GetTypeName();
    conn->shutdown();
}

// -----------------------------------------------------------------------------
// === 以下为各类请求处理，套路相同：检查连接 -> 选 channel -> 调用 ==========
// -----------------------------------------------------------------------------
#define GET_CONN_CTX()                                                                           \
    auto conn_ctx = __connection_manager->select_connection(conn);                               \
    if (!conn_ctx) {                                                                             \
        LOG_WARN << "unknown connection";                                                  \
        conn->shutdown();                                                                        \
        return;                                                                                  \
    }

    __connection_manager->refresh_connection(conn);

#define GET_CHANNEL(cid)                                                                         \
    auto ch = conn_ctx->select_channel(cid);                                                     \
    if (!ch) {                                                                                   \
        LOG(WARNING) << "unknown channel in this connection";                                   \
        return;                                                                                  \
    }

#define LOG_REQ(type)                                                                            \
    LOG_INFO << "<from " << conn->peerAddress().toIpPort() << "> Request: " #type;

void BrokerServer::on_openChannel(const muduo::net::TcpConnectionPtr& conn, const openChannelRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    LOG_REQ(openChannelRequest);
    conn_ctx->open_channel(msg);
}

void BrokerServer::on_closeChannel(const muduo::net::TcpConnectionPtr& conn, const closeChannelRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    LOG_REQ(closeChannelRequest);
    conn_ctx->close_channel(msg);
}

void BrokerServer::on_declareExchange(const muduo::net::TcpConnectionPtr& conn, const declareExchangeRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    GET_CHANNEL(msg->cid());
    LOG_REQ(declareExchangeRequest);
    ch->declare_exchange(msg);
}

void BrokerServer::on_deleteExchange(const muduo::net::TcpConnectionPtr& conn, const deleteExchangeRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    GET_CHANNEL(msg->cid());
    LOG_REQ(deleteExchangeRequest);
    ch->delete_exchange(msg);
}

void BrokerServer::on_declareQueue(const muduo::net::TcpConnectionPtr& conn, const declareQueueRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    GET_CHANNEL(msg->cid());
    LOG_REQ(declareQueueRequest);
    ch->declare_queue(msg);
}

void BrokerServer::on_deleteQueue(const muduo::net::TcpConnectionPtr& conn, const deleteQueueRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    GET_CHANNEL(msg->cid());
    LOG_REQ(deleteQueueRequest);
    ch->delete_queue(msg);
}

void BrokerServer::on_bind(const muduo::net::TcpConnectionPtr& conn, const bindRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    GET_CHANNEL(msg->cid());
    LOG_REQ(bindRequest);
    ch->bind(msg);
}

void BrokerServer::on_unbind(const muduo::net::TcpConnectionPtr& conn, const unbindRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    GET_CHANNEL(msg->cid());
    LOG_REQ(unbindRequest);
    ch->unbind(msg);
}

void BrokerServer::on_basicPublish(const muduo::net::TcpConnectionPtr& conn, const basicPublishRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    GET_CHANNEL(msg->cid());
    LOG_REQ(basicPublishRequest);
    ch->basic_publish(msg);
}

void BrokerServer::on_basicAck(const muduo::net::TcpConnectionPtr& conn, const basicAckRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    GET_CHANNEL(msg->cid());
    LOG_REQ(basicAckRequest);
    ch->basic_ack(msg);
}

void BrokerServer::on_basicConsume(const muduo::net::TcpConnectionPtr& conn, const basicConsumeRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    GET_CHANNEL(msg->cid());
    LOG_REQ(basicConsumeRequest);
    ch->basic_consume(msg);
}

void BrokerServer::on_basicCancel(const muduo::net::TcpConnectionPtr& conn, const basicCancelRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    GET_CHANNEL(msg->cid());
    LOG_REQ(basicCancelRequest);
    ch->basic_cancel(msg);
}

void BrokerServer::on_basicQuery(const muduo::net::TcpConnectionPtr& conn, const basicQueryRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    GET_CHANNEL(msg->cid());
    LOG_REQ(basicQueryRequest);
    ch->basic_query(msg);
}


void BrokerServer::on_heartbeat(const muduo::net::TcpConnectionPtr& conn, const heartbeatRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    __connection_manager->refresh_connection(conn);
    heartbeatResponse resp;
    resp.set_rid(msg->rid());
    __codec->send(conn, resp);
}

void BrokerServer::on_queueStatusRequest(const muduo::net::TcpConnectionPtr& conn, const queueStatusRequestPtr& msg, muduo::Timestamp ts)
{
    (void)ts;
    GET_CONN_CTX();
    GET_CHANNEL(msg->cid());
    LOG_REQ(queueStatusRequest);
    // 查询队列状态
    auto& queue_mgr = __virtual_host->__queue_mgr; // 若为private需加接口
    auto status = queue_mgr.get_queue_status(msg->queue_name());
    queueStatusResponse resp;
    resp.set_rid(msg->rid());
    resp.set_cid(msg->cid());
    resp.set_exists(status.exists);
    resp.set_durable(status.durable);
    resp.set_exclusive(status.exclusive);
    resp.set_auto_delete(status.auto_delete);
    for (const auto& [k, v] : status.args) {
        (*resp.mutable_args())[k] = v;
    }
    __codec->send(conn, resp);
}

#undef GET_CONN_CTX
#undef GET_CHANNEL
#undef LOG_REQ

} 
