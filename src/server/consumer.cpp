// ======================= consumer.cpp =======================
#include "consumer.hpp"
#include "../common/logger.hpp"

namespace hz_mq {

// --------- consumer ----------
consumer::consumer(const std::string& ctag, const std::string& queue_name,
                   bool ack_flag, const consumer_callback& cb)
    : tag(ctag),
      qname(queue_name),
      auto_ack(ack_flag),
      callback(cb) {}

// --------- queue_consumer ----------
queue_consumer::queue_consumer(const std::string& qname)
    : __qname(qname), __rr_index(0) {}

consumer::ptr queue_consumer::create(const std::string& ctag,
                                     const std::string& queue_name,
                                     bool ack_flag,
                                     const consumer_callback& cb)
{
    std::unique_lock<std::mutex> lock(__mtx);
    for (const auto& c : __consumers) {
        if (c->tag == ctag) {
            LOG(WARNING) << "consumer duplicate tag, create failed";
            return {};
        }
    }
    auto new_consumer = std::make_shared<consumer>(ctag, queue_name, ack_flag, cb);
    __consumers.push_back(new_consumer);
    return new_consumer;
}

void queue_consumer::remove(const std::string& ctag)
{
    std::unique_lock<std::mutex> lock(__mtx);
    for (auto it = __consumers.begin(); it != __consumers.end(); ++it) {
        if ((*it)->tag == ctag) {
            __consumers.erase(it);
            return;
        }
    }
    LOG(WARNING) << "consumer tag [" << ctag << "] not found, remove failed";
}

consumer::ptr queue_consumer::rr_choose()
{
    std::unique_lock<std::mutex> lock(__mtx);
    if (__consumers.empty()) return {};
    consumer::ptr chosen = __consumers[__rr_index % __consumers.size()];
    __rr_index = (__rr_index + 1) % __consumers.size();
    return chosen;
}

bool queue_consumer::empty()
{
    std::unique_lock<std::mutex> lock(__mtx);
    return __consumers.empty();
}

bool queue_consumer::exists(const std::string& ctag)
{
    std::unique_lock<std::mutex> lock(__mtx);
    for (const auto& c : __consumers) {
        if (c->tag == ctag) return true;
    }
    return false;
}

void queue_consumer::clear()
{
    std::unique_lock<std::mutex> lock(__mtx);
    __consumers.clear();
    __rr_index = 0;
}

// --------- consumer_manager ----------
void consumer_manager::init_queue_consumer(const std::string& qname)
{
    std::unique_lock<std::mutex> lock(__mtx);
    if (__queue_consumers.find(qname) == __queue_consumers.end()) {
        __queue_consumers[qname] = std::make_shared<queue_consumer>(qname);
    }
}

void consumer_manager::destroy_queue_consumer(const std::string& qname)
{
    std::unique_lock<std::mutex> lock(__mtx);
    __queue_consumers.erase(qname);
}

consumer::ptr consumer_manager::create(const std::string& ctag,
                                       const std::string& queue_name,
                                       bool ack_flag,
                                       const consumer_callback& cb)
{
    queue_consumer::ptr qc;
    {
        std::unique_lock<std::mutex> lock(__mtx);
        auto it = __queue_consumers.find(queue_name);
        if (it == __queue_consumers.end()) {
            LOG(ERROR) << "queue_consumer for [" << queue_name << "] not found";
            return {};
        }
        qc = it->second;
    }
    return qc->create(ctag, queue_name, ack_flag, cb);
}

void consumer_manager::remove(const std::string& ctag, const std::string& queue_name)
{
    queue_consumer::ptr qc;
    {
        std::unique_lock<std::mutex> lock(__mtx);
        auto it = __queue_consumers.find(queue_name);
        if (it == __queue_consumers.end()) {
            LOG(ERROR) << "queue_consumer for [" << queue_name << "] not found";
            return;
        }
        qc = it->second;
    }
    qc->remove(ctag);
}

consumer::ptr consumer_manager::choose(const std::string& queue_name)
{
    std::unique_lock<std::mutex> lock(__mtx);
    auto it = __queue_consumers.find(queue_name);
    if (it == __queue_consumers.end()) {
        LOG(ERROR) << "queue_consumer for [" << queue_name << "] not found";
        return {};
    }
    return it->second->rr_choose();
}

} 
