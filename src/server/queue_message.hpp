
#pragma once
#include <deque>
#include <memory>
#include <string>
#include <algorithm>            // 新增
#include "../common/msg.pb.h"      // BasicProperties
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

    queue_message(const std::string&, const std::string&) {}   // base_dir / queue_name

    bool insert(BasicProperties* bp,
                const std::string& body,
                bool /*durable_unused*/)
    {
        auto msg = std::make_shared<Message>();

        if (bp)
            *msg->mutable_payload()->mutable_properties() = *bp;   // 复制属性

        msg->mutable_payload()->set_body(body);
        msgs_.push_back(std::move(msg));
        return true;
    }

    message_ptr front() const
    {   return msgs_.empty() ? nullptr : msgs_.front(); }

    void remove(const std::string& id)      // id 为空 ⇒ 删除队首
    {
        if (id.empty()) { if (!msgs_.empty()) msgs_.pop_front(); return; }

        msgs_.erase(std::remove_if(msgs_.begin(), msgs_.end(),
                    [&](const message_ptr& m){
                        return m && m->payload().properties().id() == id;
                    }),
                    msgs_.end());
    }

    std::size_t getable_count() const { return msgs_.size(); }

    void recovery() {}   // TODO: 以后实现磁盘恢复

private:
    std::deque<message_ptr> msgs_;
};

}   
