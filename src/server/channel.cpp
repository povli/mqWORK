#ifdef route
#   pragma message("macro route defined !!!")
#endif

#include "route.hpp"
// ======================= channel.cpp =======================
#include "channel.hpp"
#include "muduo/protoc/codec.h"             
#include "../common/logger.hpp"    // 日志
#include "../common/message.hpp"   // message_ptr

#include <functional>
#include <utility>

namespace hz_mq {

// -----------------------------------------------------------------------------
// 构造 / 析构
// -----------------------------------------------------------------------------
channel::channel(const std::string& cid,
                 const virtual_host::ptr& host,
                 const consumer_manager::ptr& cmp,
                 const ProtobufCodecPtr& codec,
                 const muduo::net::TcpConnectionPtr conn,
                 const thread_pool::ptr& pool)
    : __cid(cid), __conn(conn), __codec(codec), __cmp(cmp), __host(host), __pool(pool)
{
    // 初始没有 consumer
}

channel::~channel()
{
    if (__consumer) {
        __cmp->remove(__consumer->tag, __consumer->qname);
    }
}

// -----------------------------------------------------------------------------
// helpers
// -----------------------------------------------------------------------------
void channel::basic_response(bool ok, const std::string& rid, const std::string& cid)
{
    basicCommonResponse resp;
    resp.set_rid(rid);
    resp.set_cid(cid);
    resp.set_ok(ok);
    __codec->send(__conn, resp);
}

void channel::consume(const std::string& qname)
{
    // 1. 取出消息
    message_ptr mp = __host->basic_consume(qname);
    if (!mp) {
        LOG(ERROR) << "consume task: no message in queue [" << qname << "]"  ;
        return;
    }
    // 2. 选消费者
    consumer::ptr cp = __cmp->choose(qname);
    if (!cp) {
        LOG(ERROR) << "consume task: no consumer for queue [" << qname << "]" ;
        return;
    }
    // 3. 调用回调投递消息
    cp->callback(cp->tag, mp->mutable_payload()->mutable_properties(), mp->payload().body());
    // 4. 自动 ack
    if (cp->auto_ack) {
        __host->basic_ack(qname, mp->payload().properties().id());
    }
}

void channel::consume_cb(const std::string& tag,
                         const BasicProperties* bp,
                         const std::string& body)
{
    basicConsumeResponse resp;
    resp.set_cid(__cid);
    resp.set_consumer_tag(tag);
    resp.set_body(body);

    if (bp) {
        resp.mutable_properties()->set_id(bp->id());
        resp.mutable_properties()->set_delivery_mode(bp->delivery_mode());
        resp.mutable_properties()->set_routing_key(bp->routing_key());
    }
    __codec->send(__conn, resp);
}

// -----------------------------------------------------------------------------
// Exchange ops
// -----------------------------------------------------------------------------
void channel::declare_exchange(const declareExchangeRequestPtr& req)
{
    auto args_map = std::unordered_map<std::string, std::string>(req->args().begin(), req->args().end());
    bool ok = __host->declare_exchange(req->exchange_name(), req->exchange_type(),
                                       req->durable(), req->auto_delete(), args_map);
    basic_response(ok, req->rid(), req->cid());
}

void channel::delete_exchange(const deleteExchangeRequestPtr& req)
{
    __host->delete_exchange(req->exchange_name());
    basic_response(true, req->rid(), req->cid());
}

// -----------------------------------------------------------------------------
// Queue ops
// -----------------------------------------------------------------------------
void channel::declare_queue(const declareQueueRequestPtr& req)
{
    auto args_map = std::unordered_map<std::string, std::string>(req->args().begin(), req->args().end());
    bool ok = __host->declare_queue(req->queue_name(), req->durable(), req->exclusive(),
                                    req->auto_delete(), args_map);
    if (!ok) {
        basic_response(false, req->rid(), req->cid());
        return;
    }
    __cmp->init_queue_consumer(req->queue_name());
    basic_response(true, req->rid(), req->cid());
}

void channel::delete_queue(const deleteQueueRequestPtr& req)
{
    __cmp->destroy_queue_consumer(req->queue_name());
    __host->delete_queue(req->queue_name());
    basic_response(true, req->rid(), req->cid());
}

// -----------------------------------------------------------------------------
// Binding ops
// -----------------------------------------------------------------------------
void channel::bind(const bindRequestPtr& req)
{
    bool ok = __host->bind(req->exchange_name(), req->queue_name(), req->binding_key());
    basic_response(ok, req->rid(), req->cid());
}

void channel::unbind(const unbindRequestPtr& req)
{
    __host->unbind(req->exchange_name(), req->queue_name());
    basic_response(true, req->rid(), req->cid());
}

// -----------------------------------------------------------------------------
// Message ops
// -----------------------------------------------------------------------------
void channel::basic_publish(const basicPublishRequestPtr& req)
{
<<<<<<< HEAD
<<<<<<< HEAD
=======
    // 1. exchange 必须存在
>>>>>>> ae5edbe (完成功能1：点对点消息发送源码，测试代码，测试进行，功能描述文档，测试用例文档，测试报告文档)
=======
>>>>>>> 3e459da (删除muduo库，添加开发环境搭建文档，添加client.cpp，添加makefile，修改功能1设计文档和测试报告)
    auto ep = __host->select_exchange(req->exchange_name());
    if (!ep) {
        basic_response(false, req->rid(), req->cid());
        return;
    }

    // 2. 路由匹配
    msg_queue_binding_map bindings = __host->exchange_bindings(req->exchange_name());

    BasicProperties* properties = nullptr;
    std::string routing_key;
    if (req->has_properties()) {
        properties = req->mutable_properties();
        routing_key = properties->routing_key();
    }

    for (const auto& [qname, bind_ptr] : bindings) {
        if (router::match_route(ep->type, routing_key, bind_ptr->binding_key)) {
            // 3. 入队
            __host->basic_publish(qname, properties, req->body());
            // 4. 异步派发
            auto task = std::bind(&channel::consume, this, qname);
            __pool->push(task);
        }
    }
    basic_response(true, req->rid(), req->cid());
}

void channel::basic_ack(const basicAckRequestPtr& req)
{
    __host->basic_ack(req->queue_name(), req->message_id());
    basic_response(true, req->rid(), req->cid());
}

void channel::basic_consume(const basicConsumeRequestPtr& req)
{
    if (!__host->exists_queue(req->queue_name())) {
        basic_response(false, req->rid(), req->cid());
        return;
    }

    auto cb = std::bind(&channel::consume_cb, this, std::placeholders::_1,
                        std::placeholders::_2, std::placeholders::_3);
    __consumer = __cmp->create(req->consumer_tag(), req->queue_name(),
                               req->auto_ack(), cb);
    basic_response(true, req->rid(), req->cid());
}

void channel::basic_cancel(const basicCancelRequestPtr& req)
{
    __cmp->remove(req->consumer_tag(), req->queue_name());
    basic_response(true, req->rid(), req->cid());
}

void channel::basic_query(const basicQueryRequestPtr& req)
{
    std::string result_body = __host->basic_query();
    basicQueryResponse resp;
    resp.set_rid(req->rid());
    resp.set_cid(__cid);
    resp.set_body(result_body);
    __codec->send(__conn, resp);
}

// -----------------------------------------------------------------------------
// channel_manager
// -----------------------------------------------------------------------------
bool channel_manager::open_channel(const std::string& cid,
                                   const virtual_host::ptr& host,
                                   const consumer_manager::ptr& cmp,
                                   const ProtobufCodecPtr& codec,
                                   const muduo::net::TcpConnectionPtr conn,
                                   const thread_pool::ptr& pool)
{
    std::unique_lock<std::mutex> lock(__mtx);
    if (__channels.count(cid) != 0) return false;

    __channels[cid] = std::make_shared<channel>(cid, host, cmp, codec, conn, pool);
    return true;
}

void channel_manager::close_channel(const std::string& cid)
{
    std::unique_lock<std::mutex> lock(__mtx);
    __channels.erase(cid);
}

channel::ptr channel_manager::select_channel(const std::string& cid)
{
    std::unique_lock<std::mutex> lock(__mtx);
    auto it = __channels.find(cid);
    return (it == __channels.end()) ? nullptr : it->second;
}

} 
