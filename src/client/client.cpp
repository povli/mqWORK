#include <iostream>
#include <string>
#include <thread>
#include <sstream> 
#include "muduo/net/EventLoopThread.h"   
#include <memory>           // shared_ptr
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/protoc/codec.h"
#include "muduo/protoc/dispatcher.h"
#include "../common/protocol.pb.h"
#include "../common/msg.pb.h"

using namespace hz_mq;
using muduo::net::TcpConnectionPtr;
using ProtobufCodecPtr = std::shared_ptr<ProtobufCodec>;

muduo::net::EventLoopThread g_loopThread;        // ★ 由它生成 EventLoop
muduo::net::EventLoop*      g_loop = nullptr;    // 运行时取得指针
TcpConnectionPtr g_conn;          // ★ 线程安全保存连接
ProtobufCodecPtr g_codec;  
ProtobufDispatcher g_dispatcher(std::bind([](const TcpConnectionPtr&, const MessagePtr&, muduo::Timestamp){} , 
                                         std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

void onConnection(const TcpConnectionPtr& conn)
{
    if (conn->connected()) g_conn = conn;
    else                   g_conn.reset();
}
void onMessage(const TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp ts) {
    if (g_codec) g_codec->onMessage(conn, buf, ts);
}

// Handlers for responses from server
void onCommonResponse(const TcpConnectionPtr&, const std::shared_ptr<basicCommonResponse>& message, muduo::Timestamp) {
    std::cout << "[Response] (cid=" << message->cid() << ") OK=" << (message->ok() ? "true" : "false") << std::endl;
}
void onConsumeResponse(const TcpConnectionPtr&, const std::shared_ptr<basicConsumeResponse>& message, muduo::Timestamp) {
    std::cout << "[Message Received] consumer_tag=" << message->consumer_tag()
              << " body=\"" << message->body() << "\"" << std::endl;
    // 自动发送ack
    if (g_conn && g_codec && message->has_properties()) {
        basicAckRequest ack;
        ack.set_rid(message->consumer_tag()+"-ack-"+message->properties().id());
        ack.set_cid(message->cid());
        // 这里queue_name需要从业务侧传递，假设consumer_tag即queue_name（如有不同请调整）
        ack.set_queue_name(message->consumer_tag());
        ack.set_message_id(message->properties().id());
        g_codec->send(g_conn, ack);
    }
}
void onQueryResponse(const TcpConnectionPtr&, const std::shared_ptr<basicQueryResponse>& message, muduo::Timestamp) {
    std::string body = message->body();
    if (!body.empty()) {
        std::cout << "[Pulled Message] " << body << std::endl;
    } else {
        std::cout << "[Pulled Message] (none available)" << std::endl;
    }
}

void onQueueStatusResponse(const TcpConnectionPtr&, const std::shared_ptr<queueStatusResponse>& message, muduo::Timestamp) {
    std::cout << "[Queue Status] exists=" << (message->exists() ? "true" : "false");
    if (message->exists()) {
        std::cout << ", durable=" << (message->durable() ? "true" : "false")
                  << ", exclusive=" << (message->exclusive() ? "true" : "false")
                  << ", auto_delete=" << (message->auto_delete() ? "true" : "false")
                  << ", args={";
        for (const auto& pair : message->args()) {
            std::cout << pair.first << "=" << pair.second << ", ";
        }
        std::cout << "}";
    }
    std::cout << std::endl;
}

void onHeartbeatResponse(const TcpConnectionPtr&, const std::shared_ptr<heartbeatResponse>&, muduo::Timestamp) {
    // ignore
}

int main(int argc, char* argv[]) {
    /* ① 启动专用 EventLoop 线程 */
    g_loop = g_loopThread.startLoop();      // ★ 保证 loop() 在同线程

    /* ② 解析地址 */
    std::string host = (argc >= 2) ? argv[1] : "127.0.0.1";
    int         port = (argc >= 3) ? std::atoi(argv[2]) : 5555;
    muduo::net::InetAddress serverAddr(host, port);

    /* ③ 创建 TcpClient 用 g_loop */
    muduo::net::TcpClient client(g_loop, serverAddr, "MQClient");
    client.setConnectionCallback(onConnection);


    g_dispatcher.registerMessageCallback<basicCommonResponse>(onCommonResponse);
    g_dispatcher.registerMessageCallback<basicConsumeResponse>(onConsumeResponse);
    g_dispatcher.registerMessageCallback<basicQueryResponse>(onQueryResponse);
    g_dispatcher.registerMessageCallback<queueStatusResponse>(onQueueStatusResponse);
    g_dispatcher.registerMessageCallback<heartbeatResponse>(onHeartbeatResponse);

    g_codec = std::make_shared<ProtobufCodec>(
        std::bind(&ProtobufDispatcher::onProtobufMessage, &g_dispatcher,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    client.setMessageCallback(onMessage);
    client.connect();

    std::thread hb([&](){
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            if (!g_conn) continue;
            heartbeatRequest req;
            req.set_rid("hb");
            g_codec->send(g_conn, req);
        }
    });
    hb.detach();


    std::cout << "Connected to message queue server at " << host << ":" << port << std::endl;
    std::cout << "Commands:\n"
              << "open <cid>\n"
              << "close <cid>\n"
              << "exchange_declare <name> <direct|fanout|topic>\n"
              << "queue_declare <name>\n"
              << "queue_status <cid> <queue>\n"
              << "bind <exch> <queue> <binding_key>\n"
              << "publish <exch> <routing_key> <message>\n"
              << "pull <cid>\n"
              << "consume <cid> <queue> <consumer_tag>\n"
              << "cancel <cid> <consumer_tag> <queue>\n"
              << "exit\n";

    while (!g_conn) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::string command;

    // Command loop
    while (true) {
        std::cout << "MQClient> ";
        if (!std::getline(std::cin, command)) break;
        if (command.empty()) continue;
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;
        if (cmd == "open") {
            std::string cid;
            iss >> cid;
            openChannelRequest req;
            req.set_rid("cli-open-" + cid);
            req.set_cid(cid);
            g_codec->send(g_conn, req);
        } else if (cmd == "close") {
            std::string cid;
            iss >> cid;
            closeChannelRequest req;
            req.set_rid("cli-close-" + cid);
            req.set_cid(cid);
            g_codec->send(g_conn, req);
        } else if (cmd == "exchange_declare") {
            std::string ename, etype;
            iss >> ename >> etype;
            ExchangeType exType = ExchangeType::DIRECT;
            if (etype == "fanout") exType = ExchangeType::FANOUT;
            else if (etype == "topic") exType = ExchangeType::TOPIC;
            declareExchangeRequest req;
            req.set_rid("cli-exdec-" + ename);
            req.set_cid("0");
            req.set_exchange_name(ename);
            req.set_exchange_type(exType);
            req.set_durable(false);
            req.set_auto_delete(false);
            g_codec->send(g_conn, req);
        } else if (cmd == "queue_declare") {
            std::string qname;
            iss >> qname;
            declareQueueRequest req;
            req.set_rid("cli-qdec-" + qname);
            req.set_cid("0");
            req.set_queue_name(qname);
            req.set_exclusive(false);
            req.set_durable(false);
            req.set_auto_delete(false);
            g_codec->send(g_conn, req);
        } else if (cmd == "bind") {
            std::string exch, qname, key;
            iss >> exch >> qname >> key;
            bindRequest req;
            req.set_rid("cli-bind-" + exch + "-" + qname);
            req.set_cid("0");
            req.set_exchange_name(exch);
            req.set_queue_name(qname);
            req.set_binding_key(key);
            g_codec->send(g_conn, req);
        } else if (cmd == "publish") {
            std::string exch, rkey, msg;
            iss >> exch >> rkey;
            std::getline(iss, msg);
            if (!msg.empty() && msg[0] == ' ') msg.erase(0, 1);
            basicPublishRequest req;
            req.set_rid("cli-pub-" + exch);
            req.set_cid("0");
            req.set_exchange_name(exch);
            req.set_body(msg);
            // set properties with routing key if provided
            if (!rkey.empty()) {
                BasicProperties* props = req.mutable_properties();
                props->set_routing_key(rkey);
                props->set_delivery_mode(DeliveryMode::UNDURABLE);
            }
            g_codec->send(g_conn, req);
        } else if (cmd == "pull") {
            std::string cid;
            iss >> cid;
            basicQueryRequest req;
            req.set_rid("cli-query-" + cid);
            req.set_cid(cid);
            g_codec->send(g_conn, req);
        } else if (cmd == "consume") {
            std::string cid, qname, tag;
            iss >> cid >> qname >> tag;
            basicConsumeRequest req;
            req.set_rid("cli-consume-" + cid);
            req.set_cid(cid);
            req.set_queue_name(qname);
            req.set_consumer_tag(tag);
            req.set_auto_ack(true);
            g_codec->send(g_conn, req);
        } else if (cmd == "cancel") {
            std::string cid, tag, qname;
            iss >> cid >> tag >> qname;
            basicCancelRequest req;
            req.set_rid("cli-cancel-" + cid);
            req.set_cid(cid);
            req.set_consumer_tag(tag);
            req.set_queue_name(qname);
            g_codec->send(g_conn, req);
        } else if (cmd == "queue_status") {
            std::string cid, qname;
            iss >> cid >> qname;
            queueStatusRequest req;
            req.set_rid("cli-qstatus-" + qname);
            req.set_cid(cid);
            req.set_queue_name(qname);
            g_codec->send(g_conn, req); 
        } else if (cmd == "exit") {
            break;
        } else {
            std::cout << "Unknown command." << std::endl;
        }
    }

    // Cleanup
    client.disconnect();
    g_loop->runInLoop([&](){ g_loop->quit(); });   // 让工作线程安全退出
    return 0;
}
