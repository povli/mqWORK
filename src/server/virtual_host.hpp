#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <atomic>

#include "exchange.hpp"
#include "queue.hpp"
#include "binding.hpp"
#include "../common/message.hpp"
#include "../common/protocol.pb.h"  // ExchangeType
#include "../common/msg.pb.h"       // BasicProperties, Message

namespace hz_mq {

class queue_message;                       // 前向声明：单队列持久化 / 运行时消息存储
using queue_message_ptr = std::shared_ptr<queue_message>;

// ==============================================================
// virtual_host : Broker 核心状态（exchanges / queues / bindings）
// ==============================================================
class virtual_host {
public:
    using ptr = std::shared_ptr<virtual_host>;

    virtual_host(const std::string& name,
                 const std::string& base_dir,
                 const std::string& meta_db_path);

    // ------------------- Exchange -------------------
    bool declare_exchange(const std::string& exchange_name, ExchangeType type,
                          bool durable, bool auto_delete,
                          const std::unordered_map<std::string, std::string>& args);

    void delete_exchange(const std::string& exchange_name);
    exchange::ptr select_exchange(const std::string& exchange_name);

    // ------------------- Queue ----------------------
    bool declare_queue(const std::string& queue_name, bool durable, bool exclusive,
                       bool auto_delete,
                       const std::unordered_map<std::string, std::string>& args);

    void delete_queue(const std::string& queue_name);
    bool exists_queue(const std::string& queue_name);
    queue_map all_queues();

    // ------------------- Binding --------------------
    bool bind(const std::string& exchange_name, const std::string& queue_name,
              const std::string& binding_key);

    void unbind(const std::string& exchange_name, const std::string& queue_name);

    msg_queue_binding_map exchange_bindings(const std::string& exchange_name);

    bool basic_publish_queue(const std::string& queue_name,
        BasicProperties* bp,
        const std::string& body);   
    // ------------------- Message --------------------
    bool basic_publish(const std::string& queue_name,
        BasicProperties*   bp,
        const std::string& body);

    bool publish_ex(const std::string& exchange_name,
        const std::string& routing_key,
         BasicProperties*   bp,
        const std::string& body);
    message_ptr basic_consume(const std::string& queue_name);
    void basic_ack(const std::string& queue_name, const std::string& msg_id);

    std::string basic_query();  // 简化的 pull 查询

private:
    std::string                                   __name;
    std::string                                   __base_dir;

    exchange_manager                              __exchange_mgr;
    msg_queue_manager                             __queue_mgr;

    std::unordered_map<std::string, msg_queue_binding_map> __exchange_bindings; // exchange -> (queue -> binding)
    std::unordered_map<std::string, queue_message_ptr>     __queue_messages;    // queue -> message storage

    static std::string generate_id();  // 若调用方需要自行生成 msg_id
};

} 