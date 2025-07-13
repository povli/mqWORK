// ======================= queue.hpp =======================
#pragma once

#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>

namespace hz_mq {

    
// ---------- 死信队列配置 ----------
struct dead_letter_config {
    std::string exchange_name;  // 死信交换机名称
    std::string routing_key;    // 死信路由键
    uint32_t max_retries;       // 最大重试次数
    
    dead_letter_config() : max_retries(3) {}
    dead_letter_config(const std::string& ex, const std::string& key, uint32_t retries = 3)
        : exchange_name(ex), routing_key(key), max_retries(retries) {}
};
// ---------- 队列元数据 ----------
struct msg_queue {
    using ptr = std::shared_ptr<msg_queue>;

    std::string name;
    bool durable{false};
    bool exclusive{false};
    bool auto_delete{false};
    std::unordered_map<std::string, std::string> args;
    dead_letter_config dlq_config;  // 死信队列配置

    msg_queue() = default;
    msg_queue(const std::string& qname, bool qdurable, bool qexclusive,
              bool qauto_delete, const std::unordered_map<std::string, std::string>& qargs);

    // 死信队列相关方法
    bool has_dead_letter_config() const { return !dlq_config.exchange_name.empty(); }
    void set_dead_letter_config(const dead_letter_config& config) { dlq_config = config; }
    const dead_letter_config& get_dead_letter_config() const { return dlq_config; }
    // "k=v&k2=v2" <--> std::unordered_map
    void set_args(const std::string& str_args);
    [[nodiscard]] std::string get_args() const;
};

// 队列名 → 元数据
using queue_map = std::unordered_map<std::string, msg_queue::ptr>;

// ---------- 持久化映射层（示例省略真实持久化） ----------
class msg_queue_mapper {
public:
    explicit msg_queue_mapper(const std::string& dbfile);
    bool insert(msg_queue::ptr& q);
    void remove(const std::string& name);
    queue_map all();
};

// ---------- 内存队列管理器 ----------
class msg_queue_manager {
public:
    using ptr = std::shared_ptr<msg_queue_manager>;

    explicit msg_queue_manager(const std::string& dbfile);

    bool declare_queue(const std::string& qname, bool qdurable, bool qexclusive,
                       bool qauto_delete,
                       const std::unordered_map<std::string, std::string>& qargs);
    bool declare_queue_with_dlq(const std::string& qname, bool qdurable, bool qexclusive,
                        bool qauto_delete,
                        const std::unordered_map<std::string, std::string>& qargs,
                        const dead_letter_config& dlq_config);
    void delete_queue(const std::string& name);
    msg_queue::ptr select_queue(const std::string& name);
    queue_map all();
    bool exists(const std::string& name);

    // 新增：获取队列状态结构体
    struct queue_status {
        bool exists;
        bool durable;
        bool exclusive;
        bool auto_delete;
        std::unordered_map<std::string, std::string> args;
    };

    // 新增：获取队列状态
    queue_status get_queue_status(const std::string& name);

private:
    std::mutex __mtx;
    msg_queue_mapper __mapper;
    queue_map __msg_queues;
};

} 
