// ======================= channel.hpp =======================
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "../common/msg.pb.h"     // 请求/响应 Protobuf
#include "../common/protocol.pb.h"

#include "consumer.hpp"
#include "virtual_host.hpp"
#include "../common/thread_pool.hpp"
#include "muduo/protoc/codec.h"

// --- 前向声明以减少编译依赖 --------------------------------------
namespace muduo {
namespace net {
class TcpConnection;                         // 前向声明 Muduo TCP 连接
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
} // namespace net
} // namespace muduo



namespace hz_mq {

// -----------------------------------------------------------------
// 便捷别名（各类请求的 shared_ptr）
// -----------------------------------------------------------------
using ProtobufCodec            = ::ProtobufCodec;
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
using basicCommonResponsePtr   = std::shared_ptr<basicCommonResponse>;

// =================================================================
// channel : 表示一条逻辑通道（AMQP 风格）
// =================================================================
class channel {
public:
    using ptr = std::shared_ptr<channel>;

    channel(const std::string& cid,
            const virtual_host::ptr& host,
            const consumer_manager::ptr& cmp,
            const ProtobufCodecPtr& codec,
            const muduo::net::TcpConnectionPtr conn,
            const thread_pool::ptr& pool);
    ~channel();

    // ------------------- Exchange -------------------
    void declare_exchange(const declareExchangeRequestPtr& req);
    void delete_exchange(const deleteExchangeRequestPtr& req);

    // ------------------- Queue ----------------------
    void declare_queue(const declareQueueRequestPtr& req);
    void delete_queue(const deleteQueueRequestPtr& req);

    // ------------------- Binding --------------------
    void bind(const bindRequestPtr& req);
    void unbind(const unbindRequestPtr& req);

    // ------------------- Message --------------------
    void basic_publish(const basicPublishRequestPtr& req);
    void basic_ack(const basicAckRequestPtr& req);
    void basic_consume(const basicConsumeRequestPtr& req);
    void basic_cancel(const basicCancelRequestPtr& req);
    void basic_query(const basicQueryRequestPtr& req);

private:
    // helpers ------------------------------------------------------
    void basic_response(bool ok, const std::string& rid, const std::string& cid);
    void consume(const std::string& qname); // 在线程池中执行
    void consume_cb(const std::string& tag, const BasicProperties* bp, const std::string& body);

    // data ---------------------------------------------------------
    std::string                    __cid;
    consumer::ptr                  __consumer;   // 若该通道作消费者
    muduo::net::TcpConnectionPtr   __conn;
    ProtobufCodecPtr               __codec;
    consumer_manager::ptr          __cmp;
    virtual_host::ptr              __host;
    thread_pool::ptr               __pool;
};

// =================================================================
// channel_manager : 负责同一 TCP 连接内的多条 channel
// =================================================================
class channel_manager {
public:
    using ptr = std::shared_ptr<channel_manager>;

    bool open_channel(const std::string& cid,
                      const virtual_host::ptr& host,
                      const consumer_manager::ptr& cmp,
                      const ProtobufCodecPtr& codec,
                      const muduo::net::TcpConnectionPtr conn,
                      const thread_pool::ptr& pool);

    void close_channel(const std::string& cid);
    channel::ptr select_channel(const std::string& cid);

private:
    std::unordered_map<std::string, channel::ptr> __channels;
    std::mutex                                    __mtx;
};

} 
