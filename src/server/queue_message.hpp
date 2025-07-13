
#pragma once
#include <deque>
#include <memory>
#include <string>
#include <fstream>
#include <mutex>
#include <filesystem>
#include <algorithm>

#include "../common/msg.pb.h"      // BasicProperties / Message     // 新增
#include "../common/message.hpp"   // 若已有真正定义则直接用它

namespace hz_mq {

// ---------------------------------------------------------------------------
// 兼容用的小壳：在真正 message.hpp 不可用时启用
// （如果你后来补上了完整实现，把下面 #ifndef-#endif 整段删掉即可）
// ---------------------------------------------------------------------------
#ifndef HZ_MQ_MESSAGE_HPP                // 假设真文件里有这个宏

#endif
// ---------------------------------------------------------------------------

using message_ptr = std::shared_ptr<Message>;

class queue_message {
public:
    using ptr = std::shared_ptr<queue_message>;

    queue_message(const std::string& base_dir, const std::string& queue_name);
    ~queue_message();

    bool insert(BasicProperties* bp,
                const std::string& body,
                 bool durable);

    message_ptr front() const
    {   return msgs_.empty() ? nullptr : msgs_.front(); }

    void remove(const std::string& id); 

    std::size_t getable_count() const { return msgs_.size(); }
    std::deque<message_ptr> get_all_messages() const { return msgs_; }
    void recovery();   // 从磁盘恢复


private:
    bool write_persistent(message_ptr& msg);
    void invalidate_persistent(const message_ptr& msg);
    std::deque<message_ptr> msgs_;
    std::string            file_path_;
    mutable std::mutex     mtx_;
    std::fstream           file_;
};

} // namespace hz_mq

// ==================== Implementation ====================
inline hz_mq::queue_message::queue_message(const std::string& base_dir,
                                           const std::string& queue_name)
    : file_path_(base_dir + "/" + queue_name + ".mqd")
{
    namespace fs = std::filesystem;
    if (!fs::exists(base_dir))
        fs::create_directories(base_dir);

    file_.open(file_path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        file_.clear();
        file_.open(file_path_, std::ios::out | std::ios::binary);
        file_.close();
        file_.open(file_path_, std::ios::in | std::ios::out | std::ios::binary);
    }
}

inline hz_mq::queue_message::~queue_message()
{
    if (file_.is_open()) file_.close();
}

inline bool hz_mq::queue_message::write_persistent(message_ptr& msg)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (!file_.is_open()) return false;

    msg->mutable_payload()->set_valid("1");
    std::string data;
    msg->payload().SerializeToString(&data);
    uint32_t len = static_cast<uint32_t>(data.size());

    file_.seekp(0, std::ios::end);
    std::streampos pos = file_.tellp();
    file_.write(reinterpret_cast<const char*>(&len), sizeof(len));
    file_.write(data.data(), data.size());
    file_.flush();

    msg->set_offset(static_cast<uint64_t>(pos));
    msg->set_length(sizeof(len) + len);
    return file_.good();
}

inline bool hz_mq::queue_message::insert(BasicProperties* bp,
                                         const std::string& body,
                                         bool durable)
{
    auto msg = std::make_shared<Message>();
    if (bp)
        *msg->mutable_payload()->mutable_properties() = *bp;
    msg->mutable_payload()->set_body(body);

    if (durable)
        write_persistent(msg);

    msgs_.push_back(std::move(msg));
    return true;
}

inline void hz_mq::queue_message::invalidate_persistent(const message_ptr& msg)
{
    if (msg->length() == 0) return;

    std::lock_guard<std::mutex> lk(mtx_);
    if (!file_.is_open()) return;

    MessagePayload payload = msg->payload();
    payload.set_valid("0");
    std::string data;
    payload.SerializeToString(&data);
    uint32_t len = static_cast<uint32_t>(data.size());

    file_.seekp(msg->offset() + sizeof(uint32_t), std::ios::beg);
    file_.write(data.data(), len);
    file_.flush();
}

inline void hz_mq::queue_message::remove(const std::string& id)
{
    if (msgs_.empty()) return;

    if (id.empty()) {
        auto msg = msgs_.front();
        invalidate_persistent(msg);
        msgs_.pop_front();
        return;
    }

    for (auto it = msgs_.begin(); it != msgs_.end(); ++it) {
        if ((*it)->payload().properties().id() == id) {
            invalidate_persistent(*it);
            msgs_.erase(it);
            break;
        }
    }
}

inline void hz_mq::queue_message::recovery()
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (!file_.is_open()) return;

    file_.seekg(0, std::ios::beg);
    std::streampos pos = file_.tellg();
    while (true) {
        uint32_t len = 0;
        if (!file_.read(reinterpret_cast<char*>(&len), sizeof(len))) break;
        std::string data(len, '\0');
        if (!file_.read(&data[0], len)) break;

        MessagePayload payload;
        if (!payload.ParseFromString(data)) break;

        auto msg = std::make_shared<Message>();
        *msg->mutable_payload() = payload;
        msg->set_offset(static_cast<uint64_t>(pos));
        msg->set_length(sizeof(len) + len);

        if (payload.valid() == "1")
            msgs_.push_back(std::move(msg));

        pos = file_.tellg();
    }
}   
