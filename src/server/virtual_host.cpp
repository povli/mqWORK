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

bool virtual_host::declare_queue_with_dlq(const std::string& queue_name, bool durable, bool exclusive,
                                          bool auto_delete,
                                          const std::unordered_map<std::string, std::string>& args,
                                          const dead_letter_config& dlq_config)
{
    if (!__queue_mgr.declare_queue_with_dlq(queue_name, durable, exclusive, auto_delete, args, dlq_config))
        return false;

    if (!__queue_messages.count(queue_name)) {
        auto qm = std::make_shared<queue_message>(__base_dir, queue_name);
        if (durable) qm->recovery();
        __queue_messages[queue_name] = std::move(qm);
    }
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

bool virtual_host::bind(const std::string& exchange_name, const std::string& queue_name,
                        const std::string& binding_key,
                        const std::unordered_map<std::string, std::string>& binding_args)
{
    if (!__exchange_mgr.exists(exchange_name) || !__queue_mgr.exists(queue_name))
        return false;

    auto& binding_map = __exchange_bindings[exchange_name];
    binding_map[queue_name] = std::make_shared<binding>(exchange_name, queue_name, binding_key, binding_args);
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
if (bp && bp->id().empty()) {
        bp->set_id(generate_id());
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

bool virtual_host::publish_to_exchange(const std::string& exchange_name, BasicProperties* bp,
                                       const std::string& body)
{
    // 检查交换机是否存在
    auto exchange_ptr = select_exchange(exchange_name);
    if (!exchange_ptr) {
        LOG(ERROR) << "publish failed: exchange [" << exchange_name << "] not exist";
        return false;
    }

    // 获取交换机的绑定
    auto bindings = exchange_bindings(exchange_name);
    if (bindings.empty()) {
        LOG(WARNING) << "publish failed: exchange [" << exchange_name << "] has no bindings";
        return false;
    }

    // 获取路由键
    std::string routing_key;
    if (bp && !bp->routing_key().empty()) {
        routing_key = bp->routing_key();
    }

    // 获取消息头（用于Headers Exchange）
    std::unordered_map<std::string, std::string> message_headers;
    if (bp) {
        for (const auto& [key, value] : bp->headers()) {
            message_headers[key] = value;
        }
    }

    // 遍历所有绑定的队列，根据交换机类型进行匹配
    bool published = false;
    for (const auto& [qname, bind_ptr] : bindings) {
        bool should_publish = false;
        
        switch (exchange_ptr->type) {
        case ExchangeType::DIRECT:
        case ExchangeType::FANOUT:
        case ExchangeType::TOPIC:
            should_publish = router::match_route(exchange_ptr->type, routing_key, bind_ptr->binding_key);
            break;
            
        case ExchangeType::HEADERS:
            should_publish = router::match_headers(message_headers, bind_ptr->binding_args);
            break;
            
        default:
            should_publish = false;
            break;
        }
        
        if (should_publish) {
            // 投递到匹配的队列
            if (basic_publish(qname, bp, body)) {
                published = true;
            }
        }
    }

    return published;
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

message_ptr virtual_host::basic_consume_and_remove(const std::string& queue_name)
{
    auto it = __queue_messages.find(queue_name);
    if (it == __queue_messages.end()) {
        LOG(ERROR) << "consume failed: queue [" << queue_name << "] not exist";
        return {};
    }
    
    // 获取队首消息
    auto msg = it->second->front();
    if (!msg) {
        return {};
    }
    
    // 移除队首消息
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

oid virtual_host::basic_nack(const std::string& queue_name, const std::string& msg_id, 
                              bool requeue, const std::string& reason)
{
    auto it = __queue_messages.find(queue_name);
    if (it == __queue_messages.end()) return;
    
    // 获取队列配置
    auto queue_ptr = __queue_mgr.select_queue(queue_name);
    if (!queue_ptr) return;
    
    if (requeue) {
        // 重新入队，不做任何处理
        return;
    }
    
    // 检查是否有死信队列配置
    if (!queue_ptr->has_dead_letter_config()) {
        // 没有死信队列配置，直接删除消息
        it->second->remove(msg_id);
        return;
    }
    
    // 获取原始消息
    auto msg = it->second->front();
    if (!msg) return;
    
    // 查找消息ID匹配的消息
    auto all_msgs = it->second->get_all_messages();
    message_ptr target_msg = nullptr;
    for (const auto& m : all_msgs) {
        if (m->payload().properties().id() == msg_id) {
            target_msg = m;
            break;
        }
    }
    
    if (!target_msg) {
        return;
    }
    
    // 创建死信消息
    auto dead_letter_msg = std::make_shared<Message>();
    *dead_letter_msg->mutable_payload() = target_msg->payload();
    
    // 添加死信队列相关属性
    auto& dlq_config = queue_ptr->get_dead_letter_config();
    
    // 将死信消息投递到死信交换机
    BasicProperties dlq_props;
    dlq_props.set_id(generate_id());
    dlq_props.set_delivery_mode(DeliveryMode::DURABLE);
    dlq_props.set_routing_key(dlq_config.routing_key);
    
    // 投递到死信交换机
    if (!dlq_config.exchange_name.empty()) {
        bool dlq_published = publish_to_exchange(dlq_config.exchange_name, &dlq_props, target_msg->payload().body());
    }
    
    // 从原队列中删除消息
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

queue_message_ptr virtual_host::select_queue_message(const std::string& queue_name)
{
    auto it = __queue_messages.find(queue_name);
    if (it == __queue_messages.end()) return nullptr;
    return it->second;
}

queue_message::stats virtual_host::queue_runtime_stats(const std::string& queue_name)
{
    auto it = __queue_messages.find(queue_name);
    if (it == __queue_messages.end()) return {};
    return it->second->get_stats();
}

void virtual_host::compact_queue(const std::string& queue_name)
{
    auto it = __queue_messages.find(queue_name);
    if (it != __queue_messages.end()) {
        it->second->compact();
    }
}

} 
