// ======================= connection.hpp =======================
#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <chrono>

#include "channel.hpp"  // channel / channel_manager

// 前向声明，避免重复包含重量级头文件 -------------------------------
namespace muduo {
namespace net {
class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
} // namespace net
} // namespace muduo

class ProtobufCodec;  // Muduo Protobuf 编解码器

namespace hz_mq {

// ================================================================
// connection : 管理单条 TCP 连接及其 channels
// ================================================================
class connection {
public:
    using ptr = std::shared_ptr<connection>;

    connection(const virtual_host::ptr& host,
               const consumer_manager::ptr& cmp,
               const std::shared_ptr<ProtobufCodec>& codec,
               const muduo::net::TcpConnectionPtr& conn,
               const thread_pool::ptr& pool);
    ~connection();

    void open_channel(const openChannelRequestPtr& req);
    void close_channel(const closeChannelRequestPtr& req);

    void refresh();
    bool expired(std::chrono::seconds timeout) const;
    muduo::net::TcpConnectionPtr tcp() const { return __conn; }

    channel::ptr select_channel(const std::string& cid);

private:
    void basic_response(bool ok, const std::string& rid, const std::string& cid);

    muduo::net::TcpConnectionPtr  __conn;
    std::shared_ptr<ProtobufCodec>__codec;
    consumer_manager::ptr         __cmp;
    virtual_host::ptr             __host;
    thread_pool::ptr              __pool;
    channel_manager::ptr          __channels;
    std::chrono::steady_clock::time_point __last_active;
}; 

// ================================================================
// connection_manager : 管理服务器上的所有 TCP 连接
// ================================================================
class connection_manager {
public:
    using ptr = std::shared_ptr<connection_manager>;

    connection_manager() = default;
    ~connection_manager() = default;

    void new_connection(const virtual_host::ptr& host,
                        const consumer_manager::ptr& cmp,
                        const std::shared_ptr<ProtobufCodec>& codec,
                        const muduo::net::TcpConnectionPtr& conn,
                        const thread_pool::ptr& pool);

    void delete_connection(const muduo::net::TcpConnectionPtr& conn);

    connection::ptr select_connection(const muduo::net::TcpConnectionPtr& conn);

    void refresh_connection(const muduo::net::TcpConnectionPtr& conn);
    void check_timeout(std::chrono::seconds timeout);

private:
    std::mutex                                                      __mtx;
    std::unordered_map<muduo::net::TcpConnectionPtr, connection::ptr> __conns;
};

} 

