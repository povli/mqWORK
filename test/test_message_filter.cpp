#include <gtest/gtest.h>
#include <memory>
#include <unordered_map>
#include <string>

#include "../src/server/virtual_host.hpp"
#include "../src/common/protocol.pb.h"
#include "../src/common/msg.pb.h"
#include "../src/server/route.hpp"

using namespace micromq;

class MessageFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        vh = std::make_shared<virtual_host>("test_host", "/tmp", "/tmp/test.db");
    }

    void TearDown() override {
        // 清理测试数据
    }

    std::shared_ptr<virtual_host> vh;
};

// 测试Headers Exchange的基本功能
TEST_F(MessageFilterTest, HeadersExchangeBasic) {
    // 声明Headers Exchange
    ASSERT_TRUE(vh->declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));
    
    // 声明队列
    ASSERT_TRUE(vh->declare_queue("queue1", false, false, false, {}));
    ASSERT_TRUE(vh->declare_queue("queue2", false, false, false, {}));
    
    // 绑定队列到Headers Exchange，使用不同的匹配条件
    std::unordered_map<std::string, std::string> binding_args1;
    binding_args1["x-match"] = "all";
    binding_args1["priority"] = "high";
    binding_args1["type"] = "alert";
    
    std::unordered_map<std::string, std::string> binding_args2;
    binding_args2["x-match"] = "any";
    binding_args2["priority"] = "low";
    binding_args2["category"] = "info";
    
    ASSERT_TRUE(vh->bind("headers_ex", "queue1", "", binding_args1));
    ASSERT_TRUE(vh->bind("headers_ex", "queue2", "", binding_args2));
    
    // 发布消息1：匹配queue1（all模式，所有条件都满足）
    BasicProperties props1;
    props1.set_id("msg1");
    props1.set_routing_key("test");
    (*props1.mutable_headers())["priority"] = "high";
    (*props1.mutable_headers())["type"] = "alert";
    
    ASSERT_TRUE(vh->publish_to_exchange("headers_ex", &props1, "Message 1"));
    
    // 验证queue1收到消息
    auto msg1 = vh->basic_consume_and_remove("queue1");
    ASSERT_TRUE(msg1 != nullptr);
    EXPECT_EQ(msg1->payload().body(), "Message 1");
    
    // 验证queue2没有收到消息（不匹配any条件）
    auto msg2 = vh->basic_consume_and_remove("queue2");
    ASSERT_TRUE(msg2 == nullptr);
}

// 测试Headers Exchange的any模式
TEST_F(MessageFilterTest, HeadersExchangeAnyMode) {
    // 声明Headers Exchange
    ASSERT_TRUE(vh->declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));
    
    // 声明队列
    ASSERT_TRUE(vh->declare_queue("queue1", false, false, false, {}));
    
    // 绑定队列，使用any模式
    std::unordered_map<std::string, std::string> binding_args;
    binding_args["x-match"] = "any";
    binding_args["priority"] = "high";
    binding_args["category"] = "info";
    
    ASSERT_TRUE(vh->bind("headers_ex", "queue1", "", binding_args));
    
    // 发布消息：只匹配一个条件（priority=high）
    BasicProperties props;
    props.set_id("msg1");
    props.set_routing_key("test");
    (*props.mutable_headers())["priority"] = "high";
    (*props.mutable_headers())["type"] = "normal";
    
    ASSERT_TRUE(vh->publish_to_exchange("headers_ex", &props, "Message 1"));
    
    // 验证queue1收到消息（any模式下只要一个条件匹配即可）
    auto msg = vh->basic_consume_and_remove("queue1");
    ASSERT_TRUE(msg != nullptr);
    EXPECT_EQ(msg->payload().body(), "Message 1");
}

// 测试Headers Exchange的all模式
TEST_F(MessageFilterTest, HeadersExchangeAllMode) {
    // 声明Headers Exchange
    ASSERT_TRUE(vh->declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));
    
    // 声明队列
    ASSERT_TRUE(vh->declare_queue("queue1", false, false, false, {}));
    
    // 绑定队列，使用all模式
    std::unordered_map<std::string, std::string> binding_args;
    binding_args["x-match"] = "all";
    binding_args["priority"] = "high";
    binding_args["type"] = "alert";
    binding_args["source"] = "system";
    
    ASSERT_TRUE(vh->bind("headers_ex", "queue1", "", binding_args));
    
    // 发布消息1：部分匹配（不满足all条件）
    BasicProperties props1;
    props1.set_id("msg1");
    props1.set_routing_key("test");
    (*props1.mutable_headers())["priority"] = "high";
    (*props1.mutable_headers())["type"] = "alert";
    // 缺少source=system
    
    ASSERT_FALSE(vh->publish_to_exchange("headers_ex", &props1, "Message 1")); // 修正断言
    
    // 验证queue1没有收到消息（不满足all条件）
    auto msg1 = vh->basic_consume_and_remove("queue1");
    ASSERT_TRUE(msg1 == nullptr);
    
    // 发布消息2：完全匹配（满足all条件）
    BasicProperties props2;
    props2.set_id("msg2");
    props2.set_routing_key("test");
    (*props2.mutable_headers())["priority"] = "high";
    (*props2.mutable_headers())["type"] = "alert";
    (*props2.mutable_headers())["source"] = "system";
    
    ASSERT_TRUE(vh->publish_to_exchange("headers_ex", &props2, "Message 2"));
    
    // 验证queue1收到消息（满足all条件）
    auto msg2 = vh->basic_consume_and_remove("queue1");
    ASSERT_TRUE(msg2 != nullptr);
    EXPECT_EQ(msg2->payload().body(), "Message 2");
}

// 测试Headers Exchange的无绑定参数情况
TEST_F(MessageFilterTest, HeadersExchangeNoBindingArgs) {
    // 声明Headers Exchange
    ASSERT_TRUE(vh->declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));
    
    // 声明队列
    ASSERT_TRUE(vh->declare_queue("queue1", false, false, false, {}));
    
    // 绑定队列，不指定绑定参数（应该匹配所有消息）
    ASSERT_TRUE(vh->bind("headers_ex", "queue1", ""));
    
    // 发布消息
    BasicProperties props;
    props.set_id("msg1");
    props.set_routing_key("test");
    (*props.mutable_headers())["priority"] = "high";
    (*props.mutable_headers())["type"] = "alert";
    
    ASSERT_TRUE(vh->publish_to_exchange("headers_ex", &props, "Message 1"));
    
    // 验证queue1收到消息（无绑定参数时匹配所有消息）
    auto msg = vh->basic_consume_and_remove("queue1");
    ASSERT_TRUE(msg != nullptr);
    EXPECT_EQ(msg->payload().body(), "Message 1");
}

// 测试Headers Exchange的复杂匹配场景
TEST_F(MessageFilterTest, HeadersExchangeComplexMatching) {
    // 声明Headers Exchange
    ASSERT_TRUE(vh->declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));
    
    // 声明多个队列
    ASSERT_TRUE(vh->declare_queue("high_priority", false, false, false, {}));
    ASSERT_TRUE(vh->declare_queue("system_alerts", false, false, false, {}));
    ASSERT_TRUE(vh->declare_queue("user_notifications", false, false, false, {}));
    
    // 绑定高优先级队列
    std::unordered_map<std::string, std::string> high_priority_args;
    high_priority_args["x-match"] = "all";
    high_priority_args["priority"] = "high";
    ASSERT_TRUE(vh->bind("headers_ex", "high_priority", "", high_priority_args));
    
    // 绑定系统告警队列
    std::unordered_map<std::string, std::string> system_alerts_args;
    system_alerts_args["x-match"] = "any";
    system_alerts_args["type"] = "alert";
    system_alerts_args["source"] = "system";
    ASSERT_TRUE(vh->bind("headers_ex", "system_alerts", "", system_alerts_args));
    
    // 绑定用户通知队列
    std::unordered_map<std::string, std::string> user_notifications_args;
    user_notifications_args["x-match"] = "all";
    user_notifications_args["type"] = "notification";
    user_notifications_args["target"] = "user";
    ASSERT_TRUE(vh->bind("headers_ex", "user_notifications", "", user_notifications_args));
    
    // 发布高优先级消息
    BasicProperties high_priority_props;
    high_priority_props.set_id("msg1");
    (*high_priority_props.mutable_headers())["priority"] = "high";
    (*high_priority_props.mutable_headers())["type"] = "alert";
    
    ASSERT_TRUE(vh->publish_to_exchange("headers_ex", &high_priority_props, "High Priority Message"));
    
    // 验证高优先级队列收到消息
    auto high_msg = vh->basic_consume_and_remove("high_priority");
    ASSERT_TRUE(high_msg != nullptr);
    EXPECT_EQ(high_msg->payload().body(), "High Priority Message");
    
    // 验证系统告警队列也收到消息（any模式，type=alert匹配）
    auto system_msg = vh->basic_consume_and_remove("system_alerts");
    ASSERT_TRUE(system_msg != nullptr);
    EXPECT_EQ(system_msg->payload().body(), "High Priority Message");
    
    // 验证用户通知队列没有收到消息（不匹配条件）
    auto user_msg = vh->basic_consume_and_remove("user_notifications");
    ASSERT_TRUE(user_msg == nullptr);
}

// 测试消息属性的完整功能
TEST_F(MessageFilterTest, MessagePropertiesComplete) {
    // 声明Headers Exchange
    ASSERT_TRUE(vh->declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));
    
    // 声明队列
    ASSERT_TRUE(vh->declare_queue("queue1", false, false, false, {}));
    
    // 绑定队列
    std::unordered_map<std::string, std::string> binding_args;
    binding_args["x-match"] = "all";
    binding_args["priority"] = "high";
    binding_args["content_type"] = "application/json";
    
    ASSERT_TRUE(vh->bind("headers_ex", "queue1", "", binding_args));
    
    // 创建完整的消息属性
    BasicProperties props;
    props.set_id("msg1");
    props.set_delivery_mode(DeliveryMode::DURABLE);
    props.set_routing_key("test.route");
    props.set_priority(5);
    props.set_timestamp(1234567890);
    props.set_content_type("application/json");
    props.set_content_encoding("utf-8");
    props.set_correlation_id("corr-123");
    props.set_reply_to("reply.queue");
    props.set_expiration(3600000);  // 1小时
    props.set_message_id("msg-123");
    props.set_delivery_tag(1);
    props.set_redelivered(false);
    props.set_exchange("headers_ex");
    props.set_user_id("user1");
    props.set_app_id("app1");
    props.set_cluster_id("cluster1");
    
    // 设置headers
    (*props.mutable_headers())["priority"] = "high";
    (*props.mutable_headers())["content_type"] = "application/json";
    (*props.mutable_headers())["custom_header"] = "custom_value";
    
    ASSERT_TRUE(vh->publish_to_exchange("headers_ex", &props, "Complete Message"));
    
    // 验证消息被正确投递
    auto msg = vh->basic_consume_and_remove("queue1");
    ASSERT_TRUE(msg != nullptr);
    EXPECT_EQ(msg->payload().body(), "Complete Message");
    
    // 验证消息属性被正确保存
    const auto& msg_props = msg->payload().properties();
    EXPECT_EQ(msg_props.id(), "msg1");
    EXPECT_EQ(msg_props.delivery_mode(), DeliveryMode::DURABLE);
    EXPECT_EQ(msg_props.routing_key(), "test.route");
    EXPECT_EQ(msg_props.priority(), 5);
    EXPECT_EQ(msg_props.timestamp(), 1234567890);
    EXPECT_EQ(msg_props.content_type(), "application/json");
    EXPECT_EQ(msg_props.content_encoding(), "utf-8");
    EXPECT_EQ(msg_props.correlation_id(), "corr-123");
    EXPECT_EQ(msg_props.reply_to(), "reply.queue");
    EXPECT_EQ(msg_props.expiration(), 3600000);
    EXPECT_EQ(msg_props.message_id(), "msg-123");
    EXPECT_EQ(msg_props.delivery_tag(), 1);
    EXPECT_EQ(msg_props.redelivered(), false);
    EXPECT_EQ(msg_props.exchange(), "headers_ex");
    EXPECT_EQ(msg_props.user_id(), "user1");
    EXPECT_EQ(msg_props.app_id(), "app1");
    EXPECT_EQ(msg_props.cluster_id(), "cluster1");
    
    // 验证headers
    EXPECT_EQ(msg_props.headers().at("priority"), "high");
    EXPECT_EQ(msg_props.headers().at("content_type"), "application/json");
    EXPECT_EQ(msg_props.headers().at("custom_header"), "custom_value");
}

// 边界用例：headers为空
TEST_F(MessageFilterTest, HeadersExchangeEmptyHeaders) {
    ASSERT_TRUE(vh->declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));
    ASSERT_TRUE(vh->declare_queue("queue1", false, false, false, {}));
    std::unordered_map<std::string, std::string> binding_args;
    binding_args["x-match"] = "all";
    binding_args["priority"] = "high";
    ASSERT_TRUE(vh->bind("headers_ex", "queue1", "", binding_args));
    BasicProperties props;
    props.set_id("msg1");
    props.set_routing_key("test");
    // 不设置headers
    ASSERT_FALSE(vh->publish_to_exchange("headers_ex", &props, "Message 1"));
    auto msg = vh->basic_consume_and_remove("queue1");
    ASSERT_TRUE(msg == nullptr);
}
// 边界用例：x-match缺失，默认all
TEST_F(MessageFilterTest, HeadersExchangeNoXMatch) {
    ASSERT_TRUE(vh->declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));
    ASSERT_TRUE(vh->declare_queue("queue1", false, false, false, {}));
    std::unordered_map<std::string, std::string> binding_args;
    binding_args["priority"] = "high";
    ASSERT_TRUE(vh->bind("headers_ex", "queue1", "", binding_args));
    BasicProperties props;
    props.set_id("msg1");
    props.set_routing_key("test");
    (*props.mutable_headers())["priority"] = "high";
    ASSERT_TRUE(vh->publish_to_exchange("headers_ex", &props, "Message 1"));
    auto msg = vh->basic_consume_and_remove("queue1");
    ASSERT_TRUE(msg != nullptr);
}
// 边界用例：大小写敏感
TEST_F(MessageFilterTest, HeadersExchangeCaseSensitive) {
    ASSERT_TRUE(vh->declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));
    ASSERT_TRUE(vh->declare_queue("queue1", false, false, false, {}));
    std::unordered_map<std::string, std::string> binding_args;
    binding_args["x-match"] = "all";
    binding_args["Priority"] = "high";
    ASSERT_TRUE(vh->bind("headers_ex", "queue1", "", binding_args));
    BasicProperties props;
    props.set_id("msg1");
    props.set_routing_key("test");
    (*props.mutable_headers())["priority"] = "high";
    ASSERT_FALSE(vh->publish_to_exchange("headers_ex", &props, "Message 1")); // key大小写不一致
    auto msg = vh->basic_consume_and_remove("queue1");
    ASSERT_TRUE(msg == nullptr);
}
// 边界用例：多队列复杂场景
TEST_F(MessageFilterTest, HeadersExchangeMultiQueueComplex) {
    ASSERT_TRUE(vh->declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));
    ASSERT_TRUE(vh->declare_queue("q1", false, false, false, {}));
    ASSERT_TRUE(vh->declare_queue("q2", false, false, false, {}));
    ASSERT_TRUE(vh->declare_queue("q3", false, false, false, {}));
    std::unordered_map<std::string, std::string> args1;
    args1["x-match"] = "all";
    args1["a"] = "1";
    std::unordered_map<std::string, std::string> args2;
    args2["x-match"] = "any";
    args2["b"] = "2";
    args2["c"] = "3";
    std::unordered_map<std::string, std::string> args3;
    args3["x-match"] = "all";
    args3["d"] = "4";
    ASSERT_TRUE(vh->bind("headers_ex", "q1", "", args1));
    ASSERT_TRUE(vh->bind("headers_ex", "q2", "", args2));
    ASSERT_TRUE(vh->bind("headers_ex", "q3", "", args3));
    BasicProperties props;
    props.set_id("msg1");
    (*props.mutable_headers())["a"] = "1";
    (*props.mutable_headers())["b"] = "2";
    (*props.mutable_headers())["d"] = "4";
    ASSERT_TRUE(vh->publish_to_exchange("headers_ex", &props, "MultiQ"));
    auto m1 = vh->basic_consume_and_remove("q1");
    auto m2 = vh->basic_consume_and_remove("q2");
    auto m3 = vh->basic_consume_and_remove("q3");
    ASSERT_TRUE(m1 != nullptr);
    ASSERT_TRUE(m2 != nullptr);
    ASSERT_TRUE(m3 != nullptr);
}

// 新增：测试virtual_host的其他方法
TEST_F(MessageFilterTest, VirtualHostExchangeOperations) {
    std::string baseDir = "./test_vh_exchange_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 测试交换机声明
    ASSERT_TRUE(vh.declare_exchange("test_ex", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("fanout_ex", ExchangeType::FANOUT, true, false, {}));
    ASSERT_TRUE(vh.declare_exchange("topic_ex", ExchangeType::TOPIC, false, true, {}));
    ASSERT_TRUE(vh.declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));

    // 测试交换机选择
    auto ex1 = vh.select_exchange("test_ex");
    ASSERT_NE(ex1, nullptr);
    EXPECT_EQ(ex1->type, ExchangeType::DIRECT);

    auto ex2 = vh.select_exchange("fanout_ex");
    ASSERT_NE(ex2, nullptr);
    EXPECT_EQ(ex2->type, ExchangeType::FANOUT);

    // 测试删除交换机
    vh.delete_exchange("test_ex");
    auto ex3 = vh.select_exchange("test_ex");
    EXPECT_EQ(ex3, nullptr);

    // 清理
    system("rm -rf ./test_vh_exchange_data");
}

TEST_F(MessageFilterTest, VirtualHostQueueOperations) {
    std::string baseDir = "./test_vh_queue_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 测试队列声明
    ASSERT_TRUE(vh.declare_queue("queue1", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue2", true, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue3", false, true, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue4", false, false, true, {}));

    // 测试队列存在性检查
    EXPECT_TRUE(vh.exists_queue("queue1"));
    EXPECT_FALSE(vh.exists_queue("nonexistent"));

    // 测试获取所有队列
    auto all_queues = vh.all_queues();
    EXPECT_GE(all_queues.size(), 4);

    // 测试删除队列
    vh.delete_queue("queue1");
    EXPECT_FALSE(vh.exists_queue("queue1"));

    // 清理
    system("rm -rf ./test_vh_queue_data");
}

TEST_F(MessageFilterTest, VirtualHostBindingOperations) {
    std::string baseDir = "./test_vh_binding_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明交换机和队列
    ASSERT_TRUE(vh.declare_exchange("test_ex", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));

    // 测试绑定
    ASSERT_TRUE(vh.bind("test_ex", "test_queue", "test_key"));

    // 测试获取绑定
    auto bindings = vh.exchange_bindings("test_ex");
    EXPECT_EQ(bindings.size(), 1);
    EXPECT_TRUE(bindings.find("test_queue") != bindings.end());

    // 测试解绑
    vh.unbind("test_ex", "test_queue");
    auto bindings2 = vh.exchange_bindings("test_ex");
    EXPECT_EQ(bindings2.size(), 0);

    // 测试绑定不存在的交换机和队列
    EXPECT_FALSE(vh.bind("nonexistent_ex", "test_queue", "key"));
    EXPECT_FALSE(vh.bind("test_ex", "nonexistent_queue", "key"));

    // 清理
    system("rm -rf ./test_vh_binding_data");
}

TEST_F(MessageFilterTest, VirtualHostMessageOperations) {
    std::string baseDir = "./test_vh_message_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明队列
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));

    // 测试消息发布
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.basic_publish("test_queue", &props, "test message"));

    // 测试消息消费
    auto msg = vh.basic_consume("test_queue");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->payload().body(), "test message");

    // 测试消息确认
    vh.basic_ack("test_queue", msg->payload().properties().id());

    // 测试消费不存在的队列
    auto msg2 = vh.basic_consume("nonexistent_queue");
    EXPECT_EQ(msg2, nullptr);

    // 测试确认不存在的队列
    vh.basic_ack("nonexistent_queue", "msg_id");

    // 清理
    system("rm -rf ./test_vh_message_data");
}

TEST_F(MessageFilterTest, VirtualHostBasicQuery) {
    std::string baseDir = "./test_vh_query_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明队列并发布消息
    ASSERT_TRUE(vh.declare_queue("queue1", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue2", false, false, false, {}));

    BasicProperties props1, props2;
    props1.set_routing_key("key1");
    props2.set_routing_key("key2");

    ASSERT_TRUE(vh.basic_publish("queue1", &props1, "message1"));
    ASSERT_TRUE(vh.basic_publish("queue2", &props2, "message2"));

    // 测试basic_query
    std::string result = vh.basic_query();
    EXPECT_FALSE(result.empty());

    // 清理
    system("rm -rf ./test_vh_query_data");
}

TEST_F(MessageFilterTest, VirtualHostConsumeAndRemove) {
    std::string baseDir = "./test_vh_consume_remove_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明队列
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));

    // 发布消息
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.basic_publish("test_queue", &props, "test message"));

    // 测试consume_and_remove
    auto msg = vh.basic_consume_and_remove("test_queue");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->payload().body(), "test message");

    // 验证消息已被移除
    auto msg2 = vh.basic_consume("test_queue");
    EXPECT_EQ(msg2, nullptr);

    // 清理
    system("rm -rf ./test_vh_consume_remove_data");
}

TEST_F(MessageFilterTest, RouteMatchingDirect) {
    // 测试Direct Exchange路由匹配
    EXPECT_TRUE(router::match_route(ExchangeType::DIRECT, "key1", "key1"));
    EXPECT_FALSE(router::match_route(ExchangeType::DIRECT, "key1", "key2"));
    EXPECT_FALSE(router::match_route(ExchangeType::DIRECT, "", "key1"));
    EXPECT_TRUE(router::match_route(ExchangeType::DIRECT, "", ""));
}

TEST_F(MessageFilterTest, RouteMatchingFanout) {
    // 测试Fanout Exchange路由匹配
    EXPECT_TRUE(router::match_route(ExchangeType::FANOUT, "any_key", "any_binding"));
    EXPECT_TRUE(router::match_route(ExchangeType::FANOUT, "", ""));
    EXPECT_TRUE(router::match_route(ExchangeType::FANOUT, "key1", "key2"));
}

TEST_F(MessageFilterTest, RouteMatchingTopic) {
    // 测试Topic Exchange路由匹配
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user.login", "user.*"));
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user.logout", "user.*"));
    EXPECT_FALSE(router::match_route(ExchangeType::TOPIC, "admin.login", "user.*"));
    
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user.login", "*.login"));
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "admin.login", "*.login"));
    
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user.login", "#"));
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user.login", "user.#"));
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user.login", "#.login"));
    
    EXPECT_FALSE(router::match_route(ExchangeType::TOPIC, "user.login", "user.logout"));
    EXPECT_FALSE(router::match_route(ExchangeType::TOPIC, "user", "user.*"));
}

TEST_F(MessageFilterTest, RouteMatchingHeaders) {
    // 测试Headers Exchange路由匹配
    std::unordered_map<std::string, std::string> headers1{{"priority", "high"}, {"type", "alert"}};
    std::unordered_map<std::string, std::string> headers2{{"priority", "low"}, {"type", "normal"}};
    
    // 测试all模式
    std::unordered_map<std::string, std::string> binding_args1{{"priority", "high"}, {"type", "alert"}};
    EXPECT_TRUE(router::match_headers(headers1, binding_args1));
    EXPECT_FALSE(router::match_headers(headers2, binding_args1));
    
    // 测试any模式
    std::unordered_map<std::string, std::string> binding_args2{{"x-match", "any"}, {"priority", "high"}};
    EXPECT_TRUE(router::match_headers(headers1, binding_args2));
    EXPECT_FALSE(router::match_headers(headers2, binding_args2));
    
    // 测试空绑定参数
    std::unordered_map<std::string, std::string> empty_binding;
    EXPECT_TRUE(router::match_headers(headers1, empty_binding));
    EXPECT_TRUE(router::match_headers(headers2, empty_binding));
}

TEST_F(MessageFilterTest, VirtualHostErrorHandling) {
    std::string baseDir = "./test_vh_error_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 测试发布到不存在的队列
    BasicProperties props;
    props.set_routing_key("test_key");
    EXPECT_FALSE(vh.basic_publish("nonexistent_queue", &props, "test message"));

    // 测试消费不存在的队列
    auto msg = vh.basic_consume("nonexistent_queue");
    EXPECT_EQ(msg, nullptr);

    // 测试确认不存在的队列
    vh.basic_ack("nonexistent_queue", "msg_id");

    // 测试拒绝不存在的队列
    vh.basic_nack("nonexistent_queue", "msg_id", false, "error");

    // 测试发布到不存在的交换机
    EXPECT_FALSE(vh.publish_to_exchange("nonexistent_exchange", &props, "test message"));

    // 清理
    system("rm -rf ./test_vh_error_data");
}

TEST_F(MessageFilterTest, VirtualHostMessageIdGeneration) {
    std::string baseDir = "./test_vh_msgid_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明队列
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));

    // 测试自动生成消息ID
    BasicProperties props1;
    props1.set_routing_key("key1");
    ASSERT_TRUE(vh.basic_publish("test_queue", &props1, "message1"));
    EXPECT_FALSE(props1.id().empty());

    // 测试手动设置消息ID
    BasicProperties props2;
    props2.set_routing_key("key2");
    props2.set_id("custom_id_123");
    ASSERT_TRUE(vh.basic_publish("test_queue", &props2, "message2"));
    EXPECT_EQ(props2.id(), "custom_id_123");

    // 清理
    system("rm -rf ./test_vh_msgid_data");
}

TEST_F(MessageFilterTest, VirtualHostExchangePublishAllTypes) {
    std::string baseDir = "./test_vh_exchange_publish_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明不同类型的交换机
    ASSERT_TRUE(vh.declare_exchange("direct_ex", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("fanout_ex", ExchangeType::FANOUT, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("topic_ex", ExchangeType::TOPIC, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));

    // 声明队列
    ASSERT_TRUE(vh.declare_queue("queue1", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue2", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue3", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue4", false, false, false, {}));

    // 绑定队列到交换机
    ASSERT_TRUE(vh.bind("direct_ex", "queue1", "key1"));
    ASSERT_TRUE(vh.bind("fanout_ex", "queue2", ""));
    ASSERT_TRUE(vh.bind("topic_ex", "queue3", "user.*"));
    ASSERT_TRUE(vh.bind("headers_ex", "queue4", "", {{"priority", "high"}}));

    // 测试Direct Exchange发布
    BasicProperties props1;
    props1.set_routing_key("key1");
    EXPECT_TRUE(vh.publish_to_exchange("direct_ex", &props1, "direct message"));

    // 测试Fanout Exchange发布
    BasicProperties props2;
    props2.set_routing_key("any_key");
    EXPECT_TRUE(vh.publish_to_exchange("fanout_ex", &props2, "fanout message"));

    // 测试Topic Exchange发布
    BasicProperties props3;
    props3.set_routing_key("user.login");
    EXPECT_TRUE(vh.publish_to_exchange("topic_ex", &props3, "topic message"));

    // 测试Headers Exchange发布
    BasicProperties props4;
    (*props4.mutable_headers())["priority"] = "high";
    EXPECT_TRUE(vh.publish_to_exchange("headers_ex", &props4, "headers message"));

    // 清理
    system("rm -rf ./test_vh_exchange_publish_data");
}

TEST_F(MessageFilterTest, VirtualHostDeclareQueueWithDLQ) {
    std::string baseDir = "./test_vh_dlq_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 测试带死信队列配置的队列声明
    dead_letter_config dlq_config;
    dlq_config.exchange_name = "dlq_exchange";
    dlq_config.routing_key = "dlq_key";

    ASSERT_TRUE(vh.declare_queue_with_dlq("test_queue", false, false, false, {}, dlq_config));
    EXPECT_TRUE(vh.exists_queue("test_queue"));

    // 清理
    system("rm -rf ./test_vh_dlq_data");
}

TEST_F(MessageFilterTest, VirtualHostBasicNackWithDLQ) {
    std::string baseDir = "./test_vh_nack_dlq_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明死信交换机
    ASSERT_TRUE(vh.declare_exchange("dlq_exchange", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("dlq_queue", false, false, false, {}));
    ASSERT_TRUE(vh.bind("dlq_exchange", "dlq_queue", "dlq_key"));

    // 声明带死信队列配置的队列
    dead_letter_config dlq_config;
    dlq_config.exchange_name = "dlq_exchange";
    dlq_config.routing_key = "dlq_key";
    ASSERT_TRUE(vh.declare_queue_with_dlq("test_queue", false, false, false, {}, dlq_config));

    // 发布消息
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.basic_publish("test_queue", &props, "test message"));

    // 消费消息
    auto msg = vh.basic_consume("test_queue");
    ASSERT_NE(msg, nullptr);

    // 拒绝消息，不重新入队
    vh.basic_nack("test_queue", msg->payload().properties().id(), false, "Processing failed");

    // 验证消息被投递到死信队列
    auto dlq_msg = vh.basic_consume("dlq_queue");
    ASSERT_NE(dlq_msg, nullptr);
    EXPECT_EQ(dlq_msg->payload().body(), "test message");

    // 清理
    system("rm -rf ./test_vh_nack_dlq_data");
}

TEST_F(MessageFilterTest, VirtualHostBasicNackRequeue) {
    std::string baseDir = "./test_vh_nack_requeue_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明队列
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));

    // 发布消息
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.basic_publish("test_queue", &props, "test message"));

    // 消费消息
    auto msg = vh.basic_consume("test_queue");
    ASSERT_NE(msg, nullptr);

    // 拒绝消息，重新入队
    vh.basic_nack("test_queue", msg->payload().properties().id(), true, "Temporary failure");

    // 验证消息重新入队
    auto requeued_msg = vh.basic_consume("test_queue");
    ASSERT_NE(requeued_msg, nullptr);
    EXPECT_EQ(requeued_msg->payload().body(), "test message");

    // 清理
    system("rm -rf ./test_vh_nack_requeue_data");
}

TEST_F(MessageFilterTest, VirtualHostBasicNackNoDLQ) {
    std::string baseDir = "./test_vh_nack_no_dlq_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明普通队列（无死信队列配置）
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));

    // 发布消息
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.basic_publish("test_queue", &props, "test message"));

    // 消费消息
    auto msg = vh.basic_consume("test_queue");
    ASSERT_NE(msg, nullptr);

    // 拒绝消息，不重新入队
    vh.basic_nack("test_queue", msg->payload().properties().id(), false, "Processing failed");

    // 验证消息被删除
    auto remaining_msg = vh.basic_consume("test_queue");
    EXPECT_EQ(remaining_msg, nullptr);

    // 清理
    system("rm -rf ./test_vh_nack_no_dlq_data");
}

TEST_F(MessageFilterTest, RouteMatchingTopicComplex) {
    // 测试更复杂的Topic Exchange路由匹配
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user.login", "user.#"));
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user.logout", "user.#"));
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user.profile.view", "user.#"));
    
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "order.created", "order.*"));
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "order.cancelled", "order.*"));
    EXPECT_FALSE(router::match_route(ExchangeType::TOPIC, "order", "order.*"));
    
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user.login", "#.login"));
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "admin.login", "#.login"));
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "system.login", "#.login"));
    
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user.login", "*.login"));
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "admin.login", "*.login"));
    EXPECT_FALSE(router::match_route(ExchangeType::TOPIC, "user.logout", "*.login"));
    
    // 测试多级通配符
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user.profile.view", "user.*.view"));
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user.profile.edit", "user.*.edit"));
    EXPECT_FALSE(router::match_route(ExchangeType::TOPIC, "user.profile.view", "user.*.edit"));
    
    // 测试边界情况
    EXPECT_TRUE(router::match_route(ExchangeType::TOPIC, "user", "user"));
    EXPECT_FALSE(router::match_route(ExchangeType::TOPIC, "user", "user.*"));
    EXPECT_FALSE(router::match_route(ExchangeType::TOPIC, "user.login", "user"));
}

TEST_F(MessageFilterTest, RouteMatchingHeadersComplex) {
    // 测试更复杂的Headers Exchange路由匹配
    std::unordered_map<std::string, std::string> headers1{{"priority", "high"}, {"type", "alert"}, {"source", "system"}};
    std::unordered_map<std::string, std::string> headers2{{"priority", "low"}, {"type", "info"}};
    std::unordered_map<std::string, std::string> headers3{{"priority", "high"}, {"type", "info"}};
    
    // 测试all模式 - 所有参数都必须匹配
    std::unordered_map<std::string, std::string> binding_args1{{"priority", "high"}, {"type", "alert"}};
    EXPECT_TRUE(router::match_headers(headers1, binding_args1));
    EXPECT_FALSE(router::match_headers(headers2, binding_args1));
    EXPECT_FALSE(router::match_headers(headers3, binding_args1));
    
    // 测试any模式 - 至少一个参数匹配
    std::unordered_map<std::string, std::string> binding_args2{{"x-match", "any"}, {"priority", "high"}, {"type", "alert"}};
    EXPECT_TRUE(router::match_headers(headers1, binding_args2));
    EXPECT_FALSE(router::match_headers(headers2, binding_args2));
    EXPECT_TRUE(router::match_headers(headers3, binding_args2));
    
    // 测试部分匹配
    std::unordered_map<std::string, std::string> binding_args3{{"priority", "high"}};
    EXPECT_TRUE(router::match_headers(headers1, binding_args3));
    EXPECT_FALSE(router::match_headers(headers2, binding_args3));
    EXPECT_TRUE(router::match_headers(headers3, binding_args3));
    
    // 测试大小写敏感
    std::unordered_map<std::string, std::string> headers4{{"Priority", "High"}, {"Type", "Alert"}};
    std::unordered_map<std::string, std::string> binding_args4{{"Priority", "High"}, {"Type", "Alert"}};
    EXPECT_TRUE(router::match_headers(headers4, binding_args4));
    
    // 测试空值匹配
    std::unordered_map<std::string, std::string> headers5{{"priority", ""}, {"type", "alert"}};
    std::unordered_map<std::string, std::string> binding_args5{{"priority", ""}, {"type", "alert"}};
    EXPECT_TRUE(router::match_headers(headers5, binding_args5));
}

TEST_F(MessageFilterTest, VirtualHostPublishToExchangeNoBindings) {
    std::string baseDir = "./test_vh_exchange_no_bindings_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明交换机但不绑定队列
    ASSERT_TRUE(vh.declare_exchange("test_ex", ExchangeType::DIRECT, false, false, {}));

    // 测试发布到没有绑定的交换机
    BasicProperties props;
    props.set_routing_key("test_key");
    EXPECT_FALSE(vh.publish_to_exchange("test_ex", &props, "test message"));

    // 清理
    system("rm -rf ./test_vh_exchange_no_bindings_data");
}

TEST_F(MessageFilterTest, VirtualHostPublishToExchangeWithHeaders) {
    std::string baseDir = "./test_vh_exchange_headers_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明Headers Exchange
    ASSERT_TRUE(vh.declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue1", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue2", false, false, false, {}));

    // 绑定队列到Headers Exchange
    ASSERT_TRUE(vh.bind("headers_ex", "queue1", "", {{"priority", "high"}}));
    ASSERT_TRUE(vh.bind("headers_ex", "queue2", "", {{"priority", "low"}}));

    // 发布带headers的消息
    BasicProperties props1, props2;
    (*props1.mutable_headers())["priority"] = "high";
    (*props2.mutable_headers())["priority"] = "low";

    EXPECT_TRUE(vh.publish_to_exchange("headers_ex", &props1, "high priority message"));
    EXPECT_TRUE(vh.publish_to_exchange("headers_ex", &props2, "low priority message"));

    // 验证消息投递
    auto msg1 = vh.basic_consume("queue1");
    ASSERT_NE(msg1, nullptr);
    EXPECT_EQ(msg1->payload().body(), "high priority message");

    auto msg2 = vh.basic_consume("queue2");
    ASSERT_NE(msg2, nullptr);
    EXPECT_EQ(msg2->payload().body(), "low priority message");

    // 清理
    system("rm -rf ./test_vh_exchange_headers_data");
}

TEST_F(MessageFilterTest, VirtualHostMessageIdGenerationMultiple) {
    std::string baseDir = "./test_vh_msgid_multiple_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明队列
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));

    // 测试多次自动生成消息ID
    BasicProperties props1, props2, props3;
    props1.set_routing_key("key1");
    props2.set_routing_key("key2");
    props3.set_routing_key("key3");

    ASSERT_TRUE(vh.basic_publish("test_queue", &props1, "message1"));
    ASSERT_TRUE(vh.basic_publish("test_queue", &props2, "message2"));
    ASSERT_TRUE(vh.basic_publish("test_queue", &props3, "message3"));

    // 验证每个消息都有唯一的ID
    EXPECT_FALSE(props1.id().empty());
    EXPECT_FALSE(props2.id().empty());
    EXPECT_FALSE(props3.id().empty());
    EXPECT_NE(props1.id(), props2.id());
    EXPECT_NE(props2.id(), props3.id());
    EXPECT_NE(props1.id(), props3.id());

    // 清理
    system("rm -rf ./test_vh_msgid_multiple_data");
}

TEST_F(MessageFilterTest, VirtualHostExchangePublishAllTypesComplex) {
    std::string baseDir = "./test_vh_exchange_complex_data";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明不同类型的交换机
    ASSERT_TRUE(vh.declare_exchange("direct_ex", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("fanout_ex", ExchangeType::FANOUT, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("topic_ex", ExchangeType::TOPIC, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("headers_ex", ExchangeType::HEADERS, false, false, {}));

    // 声明多个队列
    ASSERT_TRUE(vh.declare_queue("queue1", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue2", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue3", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue4", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue5", false, false, false, {}));

    // 绑定队列到交换机
    ASSERT_TRUE(vh.bind("direct_ex", "queue1", "key1"));
    ASSERT_TRUE(vh.bind("direct_ex", "queue2", "key2"));
    ASSERT_TRUE(vh.bind("fanout_ex", "queue1", ""));
    ASSERT_TRUE(vh.bind("fanout_ex", "queue2", ""));
    ASSERT_TRUE(vh.bind("topic_ex", "queue3", "user.*"));
    ASSERT_TRUE(vh.bind("topic_ex", "queue4", "*.login"));
    ASSERT_TRUE(vh.bind("headers_ex", "queue5", "", {{"priority", "high"}}));

    // 测试Direct Exchange发布
    BasicProperties props1;
    props1.set_routing_key("key1");
    EXPECT_TRUE(vh.publish_to_exchange("direct_ex", &props1, "direct message 1"));

    BasicProperties props2;
    props2.set_routing_key("key2");
    EXPECT_TRUE(vh.publish_to_exchange("direct_ex", &props2, "direct message 2"));

    // 测试Fanout Exchange发布
    BasicProperties props3;
    props3.set_routing_key("any_key");
    EXPECT_TRUE(vh.publish_to_exchange("fanout_ex", &props3, "fanout message"));

    // 测试Topic Exchange发布
    BasicProperties props4;
    props4.set_routing_key("user.login");
    EXPECT_TRUE(vh.publish_to_exchange("topic_ex", &props4, "topic message 1"));

    BasicProperties props5;
    props5.set_routing_key("admin.login");
    EXPECT_TRUE(vh.publish_to_exchange("topic_ex", &props5, "topic message 2"));

    // 测试Headers Exchange发布
    BasicProperties props6;
    (*props6.mutable_headers())["priority"] = "high";
    EXPECT_TRUE(vh.publish_to_exchange("headers_ex", &props6, "headers message"));

    // 验证消息投递
    auto msg1 = vh.basic_consume("queue1");
    ASSERT_NE(msg1, nullptr);

    auto msg2 = vh.basic_consume("queue2");
    ASSERT_NE(msg2, nullptr);

    auto msg3 = vh.basic_consume("queue3");
    ASSERT_NE(msg3, nullptr);

    auto msg4 = vh.basic_consume("queue4");
    ASSERT_NE(msg4, nullptr);

    auto msg5 = vh.basic_consume("queue5");
    ASSERT_NE(msg5, nullptr);

    // 清理
    system("rm -rf ./test_vh_exchange_complex_data");
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 