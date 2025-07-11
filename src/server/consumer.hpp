// ======================= consumer.hpp =======================
#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../common/msg.pb.h"   // BasicProperties

namespace hz_mq {

// 回调：consumer_tag, message_properties, message_body
using consumer_callback =
    std::function<void(const std::string&, const BasicProperties*, const std::string&)>;

// --------- consumer ----------
struct consumer {
    using ptr = std::shared_ptr<consumer>;

    std::string tag;      // 标识
    std::string qname;    // 订阅队列
    bool auto_ack{false};
    consumer_callback callback;

    consumer() = default;
    consumer(const std::string& ctag, const std::string& queue_name,
             bool ack_flag, const consumer_callback& cb);
};

// --------- queue_consumer ----------
class queue_consumer {
public:
    using ptr = std::shared_ptr<queue_consumer>;

    explicit queue_consumer(const std::string& qname);

    consumer::ptr create(const std::string& ctag, const std::string& queue_name,
                         bool ack_flag, const consumer_callback& cb);
    void remove(const std::string& ctag);
    consumer::ptr rr_choose();      // 轮询选择
    bool empty();
    bool exists(const std::string& ctag);
    void clear();

private:
    std::string __qname;
    std::mutex __mtx;
    size_t __rr_index{0};
    std::vector<consumer::ptr> __consumers;
};

// --------- consumer_manager ----------
class consumer_manager {
public:
    using ptr = std::shared_ptr<consumer_manager>;

    consumer_manager() = default;

    void init_queue_consumer(const std::string& qname);
    void destroy_queue_consumer(const std::string& qname);

    consumer::ptr create(const std::string& ctag, const std::string& queue_name,
                         bool ack_flag, const consumer_callback& cb);
    void remove(const std::string& ctag, const std::string& queue_name);
    consumer::ptr choose(const std::string& queue_name);

private:
    std::mutex __mtx;
    std::unordered_map<std::string, queue_consumer::ptr> __queue_consumers;
};

} 
