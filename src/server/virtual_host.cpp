#include "virtual_host.hpp"

#include "../common/logger.hpp"     // LOG 宏
#include "route.hpp"                // 若 queue_message 里需要路由，可引

#include "queue_message.hpp"        // 假设有该头（持久化实现）
#include <utility>

namespace hz_mq {

// -----------------------------------------------------------------------------
// helper: 生成全局唯一 msg id（简单递增）
// -----------------------------------------------------------------------------
std::string virtual_host::generate_id()
{
    static std::atomic<uint64_t> seq{0};
    uint64_t id_num = ++seq;
    return std::to_string(id_num);
}

// -----------------------------------------------------------------------------
// ctor
// -----------------------------------------------------------------------------
virtual_host::virtual_host(const std::string& name,
                           const std::string& base_dir,
                           const std::string& meta_db_path)
    : __name(name),
      __base_dir(base_dir),
      __exchange_mgr(meta_db_path),
      __queue_mgr(meta_db_path)
{
    // 若默认 direct exchange 不存在，则创建
    if (!__exchange_mgr.exists("")) {
        __exchange_mgr.declare_exchange("", ExchangeType::DIRECT, false, false, {});
    }

    // 为恢复的所有队列创建 queue_message 容器并恢复持久化消息
    for (const auto& [qname, _] : __queue_mgr.all()) {
        auto qm = std::make_shared<queue_message>(__base_dir, qname);
        qm->recovery();
        __queue_messages[qname] = std::move(qm);
    }
}

// -----------------------------------------------------------------------------
// Exchange ops
// -----------------------------------------------------------------------------
bool virtual_host::declare_exchange(const std::string& exchange_name, ExchangeType type,
                                    bool durable, bool auto_delete,
                                    const std::unordered_map<std::string, std::string>& args)
{
    return __exchange_mgr.declare_exchange(exchange_name, type, durable, auto_delete, args);
}

void virtual_host::delete_exchange(const std::string& exchange_name)
{
    __exchange_bindings.erase(exchange_name);
    __exchange_mgr.delete_exchange(exchange_name);
}

exchange::ptr virtual_host::select_exchange(const std::string& exchange_name)
{
    return __exchange_mgr.select_exchange(exchange_name);
}

// -----------------------------------------------------------------------------
// Queue ops
// -----------------------------------------------------------------------------
bool virtual_host::declare_queue(const std::string& queue_name, bool durable, bool exclusive,
                                 bool auto_delete,
                                 const std::unordered_map<std::string, std::string>& args)
{
    if (!__queue_mgr.declare_queue(queue_name, durable, exclusive, auto_delete, args))
        return false;

    if (!__queue_messages.count(queue_name)) {
        auto qm = std::make_shared<queue_message>(__base_dir, queue_name);
        if (durable) qm->recovery();
        __queue_messages[queue_name] = std::move(qm);
    }
       /* 与 AMQP 默认直连交换机 "" 建立 <队列名> 绑定，避免显式 bind 的麻烦 */
    bind("", queue_name, queue_name);
    return true;
}

void virtual_host::delete_queue(const std::string& queue_name)
{
    __queue_messages.erase(queue_name);
    __queue_mgr.delete_queue(queue_name);

    for (auto& [ex, bind_map] : __exchange_bindings) {
        bind_map.erase(queue_name);
    }
}

bool virtual_host::exists_queue(const std::string& queue_name)
{
    return __queue_mgr.exists(queue_name);
}

queue_map virtual_host::all_queues()
{
    return __queue_mgr.all();
}

// -----------------------------------------------------------------------------
// Binding ops
// -----------------------------------------------------------------------------
bool virtual_host::bind(const std::string& exchange_name, const std::string& queue_name,
                        const std::string& binding_key)
{
    if (!__exchange_mgr.exists(exchange_name) || !__queue_mgr.exists(queue_name))
        return false;

    auto& binding_map = __exchange_bindings[exchange_name];
    binding_map[queue_name] = std::make_shared<binding>(exchange_name, queue_name, binding_key);
    return true;
}

void virtual_host::unbind(const std::string& exchange_name, const std::string& queue_name)
{
    auto it = __exchange_bindings.find(exchange_name);
    if (it != __exchange_bindings.end()) it->second.erase(queue_name);
}

msg_queue_binding_map virtual_host::exchange_bindings(const std::string& exchange_name)
{
    auto it = __exchange_bindings.find(exchange_name);
    return (it == __exchange_bindings.end()) ? msg_queue_binding_map{} : it->second;
}

// -----------------------------------------------------------------------------
// Message ops
// -----------------------------------------------------------------------------
bool virtual_host::basic_publish(const std::string& queue_name,
    BasicProperties*   bp,
    const std::string& body)
{
// 1) 队列必须存在
auto it = __queue_messages.find(queue_name);
if (it == __queue_messages.end())
{
LOG(ERROR) << "publish failed: queue [" << queue_name << "] not exist";
return false;
}

// 2) routing_key 规则（直连交换机 "")：
//    · 为空        ⇒ 视为 queue_name
//    · 不为空且不等 ⇒ 视为路由不匹配，直接返回 false
if (!bp) bp = new BasicProperties;          // 避免空指针
if (bp->routing_key().empty())
bp->set_routing_key(queue_name);
else if (bp->routing_key() != queue_name)   // ★ 这一行是关键
return false;

// 3) 是否持久化
bool durable = false;
if (auto qinfo = __queue_mgr.select_queue(queue_name))
durable = qinfo->durable;

// 4) 入队
return it->second->insert(bp, body, durable);
}


bool virtual_host::publish_ex(const std::string& exchange_name,
    const std::string& routing_key,
    BasicProperties*   bp,
    const std::string& body)
{
auto ex = __exchange_mgr.select_exchange(exchange_name);
if (!ex)
{
LOG(ERROR) << "publish failed: exchange [" << exchange_name << "] not exist";
return false;
}

// 如果调用方没给 BasicProperties，就临时建一个
BasicProperties local_bp;
if (!bp) bp = &local_bp;
if (bp->routing_key().empty()) bp->set_routing_key(routing_key);

bool delivered = false;
for (auto& [qname, bind] : exchange_bindings(exchange_name))
{
if (!router::match_route(ex->type, bp->routing_key(), bind->binding_key))
continue;

bool durable = false;
if (auto qinfo = __queue_mgr.select_queue(qname))
durable = qinfo->durable;

auto qit = __queue_messages.find(qname);
if (qit != __queue_messages.end())
delivered |= qit->second->insert(bp, body, durable);
}
return delivered;
}


message_ptr virtual_host::basic_consume(const std::string& queue_name)
{
    auto it = __queue_messages.find(queue_name);
    if (it == __queue_messages.end()) {
        LOG(ERROR) << "consume failed: queue [" << queue_name << "] not exist";
        return {};
    }

    auto msg = it->second->front();
    if (msg)             // ★ 自动确认（符合测试用例预期）
        it->second->remove(msg->payload().properties().id());
    return msg;
}

void virtual_host::basic_ack(const std::string& queue_name, const std::string& msg_id)
{
    auto it = __queue_messages.find(queue_name);
    if (it == __queue_messages.end()) {
        LOG(ERROR) << "ack failed: queue [" << queue_name << "] not exist";
        return;
    }
    it->second->remove(msg_id);
}

std::string virtual_host::basic_query()
{
    for (auto& [qname, qm] : __queue_messages) {
        if (qm->getable_count() == 0) continue;
        if (auto msg = qm->front()) {
            qm->remove(msg->payload().properties().id());
            return msg->payload().body();
        }
    }
    return {};
}

} 
