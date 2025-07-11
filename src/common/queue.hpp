// ======================= queue.hpp =======================
#pragma once

#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>

namespace hz_mq {

// ---------- 队列元数据 ----------
struct msg_queue {
    using ptr = std::shared_ptr<msg_queue>;

    std::string name;
    bool durable{false};
    bool exclusive{false};
    bool auto_delete{false};
    std::unordered_map<std::string, std::string> args;

    msg_queue() = default;
    msg_queue(const std::string& qname, bool qdurable, bool qexclusive,
              bool qauto_delete, const std::unordered_map<std::string, std::string>& qargs);

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
