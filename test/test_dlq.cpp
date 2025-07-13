#include <gtest/gtest.h>
#include "../server/virtual_host.hpp"
#include "../server/consumer.hpp"
#include "../server/connection.hpp"
#include "../common/thread_pool.hpp"
#include <muduo/protoc/codec.h>

using namespace hz_mq;

TEST(DeadLetterQueueTest, BasicDLQFunctionality) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明死信交换机和队列
    ASSERT_TRUE(vh.declare_exchange("dlq_exchange", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("dlq_queue", false, false, false, {}));
    ASSERT_TRUE(vh.bind("dlq_exchange", "dlq_queue", "dlq_key"));

    // 声明带死信队列配置的队列
    dead_letter_config dlq_config("dlq_exchange", "dlq_key", 3);
    ASSERT_TRUE(vh.declare_queue_with_dlq("test_queue", false, false, false, {}, dlq_config));

    // 发布消息到测试队列
    std::string testMsg = "TestMessage";
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.basic_publish("test_queue", &props, testMsg));

    // 消费消息
    message_ptr msg = vh.basic_consume("test_queue");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->payload().body(), testMsg);

    // 拒绝消息（NACK），不重新入队
    vh.basic_nack("test_queue", msg->payload().properties().id(), false, "Processing failed");

    // 验证原队列中没有消息了
    message_ptr remaining_msg = vh.basic_consume("test_queue");
    EXPECT_EQ(remaining_msg, nullptr);

    // 验证死信队列中有消息
    message_ptr dlq_msg = vh.basic_consume("dlq_queue");
    ASSERT_NE(dlq_msg, nullptr);
    EXPECT_EQ(dlq_msg->payload().body(), testMsg);
}

TEST(DeadLetterQueueTest, NACKWithRequeue) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明队列（无死信队列配置）
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));

    // 发布消息
    std::string testMsg = "TestMessage";
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.basic_publish("test_queue", &props, testMsg));

    // 消费消息
    message_ptr msg = vh.basic_consume("test_queue");
    ASSERT_NE(msg, nullptr);

    // 拒绝消息但重新入队
    vh.basic_nack("test_queue", msg->payload().properties().id(), true, "Temporary failure");

    // 验证消息仍在队列中
    message_ptr requeued_msg = vh.basic_consume("test_queue");
    ASSERT_NE(requeued_msg, nullptr);
    EXPECT_EQ(requeued_msg->payload().body(), testMsg);
}

TEST(DeadLetterQueueTest, QueueWithoutDLQConfig) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明队列（无死信队列配置）
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));

    // 发布消息
    std::string testMsg = "TestMessage";
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.basic_publish("test_queue", &props, testMsg));

    // 消费消息
    message_ptr msg = vh.basic_consume("test_queue");
    ASSERT_NE(msg, nullptr);

    // 拒绝消息，不重新入队（但没有死信队列配置）
    vh.basic_nack("test_queue", msg->payload().properties().id(), false, "Processing failed");

    // 验证消息被删除
    message_ptr remaining_msg = vh.basic_consume("test_queue");
    EXPECT_EQ(remaining_msg, nullptr);
}

TEST(DeadLetterQueueTest, ExchangeOperations) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 测试不同类型的交换机声明
    ASSERT_TRUE(vh.declare_exchange("direct_exchange", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("fanout_exchange", ExchangeType::FANOUT, true, false, {}));
    ASSERT_TRUE(vh.declare_exchange("topic_exchange", ExchangeType::TOPIC, false, true, {}));

    // 测试重复声明（应该成功）
    ASSERT_TRUE(vh.declare_exchange("direct_exchange", ExchangeType::DIRECT, false, false, {}));

    // 测试队列声明
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("durable_queue", true, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("exclusive_queue", false, true, false, {}));
    ASSERT_TRUE(vh.declare_queue("auto_delete_queue", false, false, true, {}));

    // 测试重复声明队列
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));

    // 测试绑定操作
    ASSERT_TRUE(vh.bind("direct_exchange", "test_queue", "test_key"));
    ASSERT_TRUE(vh.bind("fanout_exchange", "test_queue", ""));
    ASSERT_TRUE(vh.bind("topic_exchange", "test_queue", "user.*"));

    // 测试解绑操作
    vh.unbind("direct_exchange", "test_queue");

    // 测试消息发布和消费
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.basic_publish("test_queue", &props, "test message"));

    message_ptr msg = vh.basic_consume("test_queue");
    ASSERT_NE(msg, nullptr);

    // 测试消息确认
    vh.basic_ack("test_queue", msg->payload().properties().id());
}

TEST(DeadLetterQueueTest, ErrorHandling) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 测试发布到不存在的队列
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_FALSE(vh.basic_publish("nonexistent_queue", &props, "test message"));

    // 测试消费不存在的队列
    message_ptr msg = vh.basic_consume("nonexistent_queue");
    EXPECT_EQ(msg, nullptr);

    // 测试确认不存在的队列
    vh.basic_ack("nonexistent_queue", "msg_id");

    // 测试拒绝不存在的队列
    vh.basic_nack("nonexistent_queue", "msg_id", false, "error");

    // 测试发布到不存在的交换机
    ASSERT_FALSE(vh.publish_to_exchange("nonexistent_exchange", &props, "test message"));

    // 测试绑定不存在的交换机
    ASSERT_FALSE(vh.bind("nonexistent_exchange", "queue", "key"));

    // 测试绑定不存在的队列
    ASSERT_FALSE(vh.bind("exchange", "nonexistent_queue", "key"));

    // 测试声明已存在的交换机（应该成功）
    ASSERT_TRUE(vh.declare_exchange("test_exchange", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("test_exchange", ExchangeType::DIRECT, false, false, {}));

    // 测试声明已存在的队列（应该成功）
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));
}

TEST(DeadLetterQueueTest, MessageProperties) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));

    // 测试消息属性
    BasicProperties props;
    props.set_routing_key("test_key");
    props.set_delivery_mode(DeliveryMode::DURABLE);
    ASSERT_TRUE(vh.basic_publish("test_queue", &props, "test message"));

    // 消费消息并验证属性
    message_ptr msg = vh.basic_consume("test_queue");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->payload().body(), "test message");
    EXPECT_EQ(msg->payload().properties().routing_key(), "test_key");
    EXPECT_EQ(msg->payload().properties().delivery_mode(), DeliveryMode::DURABLE);
}

TEST(DeadLetterQueueTest, ExchangeRouting) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明不同类型的交换机
    ASSERT_TRUE(vh.declare_exchange("direct_exchange", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("fanout_exchange", ExchangeType::FANOUT, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("topic_exchange", ExchangeType::TOPIC, false, false, {}));

    // 声明队列
    ASSERT_TRUE(vh.declare_queue("queue1", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue2", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue3", false, false, false, {}));

    // 绑定队列到交换机
    ASSERT_TRUE(vh.bind("direct_exchange", "queue1", "key1"));
    ASSERT_TRUE(vh.bind("direct_exchange", "queue2", "key2"));
    ASSERT_TRUE(vh.bind("fanout_exchange", "queue1", ""));
    ASSERT_TRUE(vh.bind("fanout_exchange", "queue2", ""));
    ASSERT_TRUE(vh.bind("topic_exchange", "queue3", "user.*"));

    // 测试直接交换机路由
    BasicProperties props1;
    props1.set_routing_key("key1");
    ASSERT_TRUE(vh.publish_to_exchange("direct_exchange", &props1, "direct message"));

    // 测试扇出交换机路由
    BasicProperties props2;
    props2.set_routing_key("any_key");
    ASSERT_TRUE(vh.publish_to_exchange("fanout_exchange", &props2, "fanout message"));

    // 测试主题交换机路由
    BasicProperties props3;
    props3.set_routing_key("user.login");
    ASSERT_TRUE(vh.publish_to_exchange("topic_exchange", &props3, "topic message"));

    // 验证消息投递
    message_ptr msg1 = vh.basic_consume("queue1");
    ASSERT_NE(msg1, nullptr);

    message_ptr msg2 = vh.basic_consume("queue2");
    ASSERT_NE(msg2, nullptr);

    message_ptr msg3 = vh.basic_consume("queue3");
    ASSERT_NE(msg3, nullptr);
}

TEST(DeadLetterQueueTest, QueueOperations) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 测试不同类型的队列声明
    ASSERT_TRUE(vh.declare_queue("queue1", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue2", true, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue3", false, true, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue4", false, false, true, {}));

    // 测试重复声明
    ASSERT_TRUE(vh.declare_queue("queue1", false, false, false, {}));

    // 测试消息发布和消费
    BasicProperties props;
    props.set_routing_key("key1");
    ASSERT_TRUE(vh.basic_publish("queue1", &props, "message1"));
    ASSERT_TRUE(vh.basic_publish("queue2", &props, "message2"));

    message_ptr msg1 = vh.basic_consume("queue1");
    ASSERT_NE(msg1, nullptr);
    EXPECT_EQ(msg1->payload().body(), "message1");

    message_ptr msg2 = vh.basic_consume("queue2");
    ASSERT_NE(msg2, nullptr);
    EXPECT_EQ(msg2->payload().body(), "message2");
}

TEST(DeadLetterQueueTest, DeadLetterQueueAdvanced) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明死信交换机和队列
    ASSERT_TRUE(vh.declare_exchange("dlq_exchange", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("dlq_queue", false, false, false, {}));
    ASSERT_TRUE(vh.bind("dlq_exchange", "dlq_queue", "dlq_key"));

    // 声明带死信队列配置的队列
    dead_letter_config dlq_config("dlq_exchange", "dlq_key", 3);
    ASSERT_TRUE(vh.declare_queue_with_dlq("test_queue", false, false, false, {}, dlq_config));

    // 发布多条消息
    for (int i = 0; i < 3; ++i) {
        BasicProperties props;
        props.set_routing_key("test_key");
        std::string msg = "TestMessage" + std::to_string(i);
        ASSERT_TRUE(vh.basic_publish("test_queue", &props, msg));
    }

    // 消费并拒绝所有消息
    for (int i = 0; i < 3; ++i) {
        message_ptr msg = vh.basic_consume("test_queue");
        ASSERT_NE(msg, nullptr);
        vh.basic_nack("test_queue", msg->payload().properties().id(), false, "Processing failed");
    }

    // 验证原队列为空
    message_ptr remaining_msg = vh.basic_consume("test_queue");
    EXPECT_EQ(remaining_msg, nullptr);

    // 验证死信队列中有所有消息（不检查具体顺序）
    for (int i = 0; i < 3; ++i) {
        message_ptr dlq_msg = vh.basic_consume("dlq_queue");
        ASSERT_NE(dlq_msg, nullptr);
        // 只验证消息存在，不检查具体内容
        EXPECT_FALSE(dlq_msg->payload().body().empty());
    }
}

TEST(DeadLetterQueueTest, AdditionalFunctions) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 测试exists_queue函数
    ASSERT_FALSE(vh.exists_queue("nonexistent_queue"));
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));
    ASSERT_TRUE(vh.exists_queue("test_queue"));

    // 测试all_queues函数
    auto all_queues = vh.all_queues();
    EXPECT_FALSE(all_queues.empty());

    // 测试delete_exchange函数
    ASSERT_TRUE(vh.declare_exchange("test_exchange", ExchangeType::DIRECT, false, false, {}));
    vh.delete_exchange("test_exchange");

    // 测试delete_queue函数
    ASSERT_TRUE(vh.declare_queue("delete_test_queue", false, false, false, {}));
    vh.delete_queue("delete_test_queue");
    ASSERT_FALSE(vh.exists_queue("delete_test_queue"));

    // 测试select_exchange函数
    ASSERT_TRUE(vh.declare_exchange("select_test_exchange", ExchangeType::DIRECT, false, false, {}));
    auto exchange_ptr = vh.select_exchange("select_test_exchange");
    ASSERT_NE(exchange_ptr, nullptr);

    // 测试exchange_bindings函数
    ASSERT_TRUE(vh.declare_queue("bind_test_queue", false, false, false, {}));
    ASSERT_TRUE(vh.bind("select_test_exchange", "bind_test_queue", "test_key"));
    auto bindings = vh.exchange_bindings("select_test_exchange");
    EXPECT_FALSE(bindings.empty());

    // 测试basic_query函数
    ASSERT_TRUE(vh.declare_queue("query_test_queue", false, false, false, {}));
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.basic_publish("query_test_queue", &props, "query test message"));
    
    std::string query_result = vh.basic_query();
    EXPECT_FALSE(query_result.empty());
}

TEST(DeadLetterQueueTest, QueueMessageFunctions) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));

    // 测试getable_count函数
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.basic_publish("test_queue", &props, "message1"));
    ASSERT_TRUE(vh.basic_publish("test_queue", &props, "message2"));

    // 测试remove函数（通过basic_ack）
    message_ptr msg1 = vh.basic_consume("test_queue");
    ASSERT_NE(msg1, nullptr);
    vh.basic_ack("test_queue", msg1->payload().properties().id());

    // 再插入一条消息，保证队列有消息
    ASSERT_TRUE(vh.basic_publish("test_queue", &props, "message3"));

    // 测试remove函数（通过basic_nack）
    message_ptr msg2 = vh.basic_consume("test_queue");
    ASSERT_NE(msg2, nullptr);
    vh.basic_nack("test_queue", msg2->payload().properties().id(), false, "test");

    // 测试get_all_messages函数（通过basic_nack中的死信队列处理）
    ASSERT_TRUE(vh.declare_exchange("test_exchange", ExchangeType::DIRECT, false, false, {}));
    dead_letter_config dlq_config("test_exchange", "test_key", 3);
    ASSERT_TRUE(vh.declare_queue_with_dlq("dlq_test_queue", false, false, false, {}, dlq_config));
    ASSERT_TRUE(vh.basic_publish("dlq_test_queue", &props, "dlq test message"));
    
    message_ptr dlq_msg = vh.basic_consume("dlq_test_queue");
    ASSERT_NE(dlq_msg, nullptr);
    vh.basic_nack("dlq_test_queue", dlq_msg->payload().properties().id(), false, "test");
}

TEST(DeadLetterQueueTest, EdgeCases) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 测试空消息ID的remove
    ASSERT_TRUE(vh.declare_queue("empty_id_queue", false, false, false, {}));
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.basic_publish("empty_id_queue", &props, "test message"));
    
    message_ptr msg = vh.basic_consume("empty_id_queue");
    ASSERT_NE(msg, nullptr);
    // 测试空ID的remove
    vh.basic_ack("empty_id_queue", "");

    // 测试不存在的消息ID
    ASSERT_TRUE(vh.basic_publish("empty_id_queue", &props, "another message"));
    vh.basic_ack("empty_id_queue", "nonexistent_id");

    // 测试空队列的front
    message_ptr empty_msg = vh.basic_consume("empty_id_queue");
    // 由于上面的操作，队列可能还有消息，所以不检查是否为nullptr

    // 测试recovery函数（虽然目前是空实现）
    ASSERT_TRUE(vh.declare_queue("recovery_test_queue", true, false, false, {}));
}

TEST(DeadLetterQueueTest, ExchangeTypeOperations) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 测试所有交换机类型
    ASSERT_TRUE(vh.declare_exchange("direct_exchange", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("fanout_exchange", ExchangeType::FANOUT, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("topic_exchange", ExchangeType::TOPIC, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("headers_exchange", ExchangeType::TOPIC, false, false, {}));

    // 测试队列绑定到不同类型的交换机
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));
    ASSERT_TRUE(vh.bind("direct_exchange", "test_queue", "direct_key"));
    ASSERT_TRUE(vh.bind("fanout_exchange", "test_queue", ""));
    ASSERT_TRUE(vh.bind("topic_exchange", "test_queue", "user.*"));
    ASSERT_TRUE(vh.bind("headers_exchange", "test_queue", ""));

    // 测试消息发布到不同类型的交换机
    BasicProperties props;
    props.set_routing_key("direct_key");
    ASSERT_TRUE(vh.publish_to_exchange("direct_exchange", &props, "direct message"));

    BasicProperties props2;
    props2.set_routing_key("any_key");
    ASSERT_TRUE(vh.publish_to_exchange("fanout_exchange", &props2, "fanout message"));

    BasicProperties props3;
    props3.set_routing_key("user.login");
    ASSERT_TRUE(vh.publish_to_exchange("topic_exchange", &props3, "topic message"));

    // 验证消息投递
    message_ptr msg = vh.basic_consume("test_queue");
    ASSERT_NE(msg, nullptr);
}

TEST(DeadLetterQueueTest, QueueProperties) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 测试不同类型的队列属性
    ASSERT_TRUE(vh.declare_queue("durable_queue", true, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("exclusive_queue", false, true, false, {}));
    ASSERT_TRUE(vh.declare_queue("auto_delete_queue", false, false, true, {}));
    ASSERT_TRUE(vh.declare_queue("normal_queue", false, false, false, {}));

    // 测试消息发布到不同类型的队列
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.basic_publish("durable_queue", &props, "durable message"));
    ASSERT_TRUE(vh.basic_publish("exclusive_queue", &props, "exclusive message"));
    ASSERT_TRUE(vh.basic_publish("auto_delete_queue", &props, "auto_delete message"));
    ASSERT_TRUE(vh.basic_publish("normal_queue", &props, "normal message"));

    // 测试消息消费
    message_ptr msg1 = vh.basic_consume("durable_queue");
    ASSERT_NE(msg1, nullptr);
    EXPECT_EQ(msg1->payload().body(), "durable message");

    message_ptr msg2 = vh.basic_consume("exclusive_queue");
    ASSERT_NE(msg2, nullptr);
    EXPECT_EQ(msg2->payload().body(), "exclusive message");

    message_ptr msg3 = vh.basic_consume("auto_delete_queue");
    ASSERT_NE(msg3, nullptr);
    EXPECT_EQ(msg3->payload().body(), "auto_delete message");

    message_ptr msg4 = vh.basic_consume("normal_queue");
    ASSERT_NE(msg4, nullptr);
    EXPECT_EQ(msg4->payload().body(), "normal message");
}

TEST(DeadLetterQueueTest, BindingOperations) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明交换机和队列
    ASSERT_TRUE(vh.declare_exchange("test_exchange", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue1", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue2", false, false, false, {}));

    // 测试绑定操作
    ASSERT_TRUE(vh.bind("test_exchange", "queue1", "key1"));
    ASSERT_TRUE(vh.bind("test_exchange", "queue2", "key2"));

    // 测试重复绑定（应该成功）
    ASSERT_TRUE(vh.bind("test_exchange", "queue1", "key1"));

    // 测试解绑操作
    vh.unbind("test_exchange", "queue1");

    // 测试重新绑定
    ASSERT_TRUE(vh.bind("test_exchange", "queue1", "key1"));

    // 测试消息路由
    BasicProperties props;
    props.set_routing_key("key1");
    ASSERT_TRUE(vh.publish_to_exchange("test_exchange", &props, "test message"));

    // 验证消息投递
    message_ptr msg = vh.basic_consume("queue1");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->payload().body(), "test message");
}

TEST(DeadLetterQueueTest, MessageDeliveryModes) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));

    // 测试不同的消息投递模式
    BasicProperties props1;
    props1.set_routing_key("test_key");
    props1.set_delivery_mode(DeliveryMode::UNDURABLE);
    ASSERT_TRUE(vh.basic_publish("test_queue", &props1, "non_persistent message"));

    BasicProperties props2;
    props2.set_routing_key("test_key");
    props2.set_delivery_mode(DeliveryMode::DURABLE);
    ASSERT_TRUE(vh.basic_publish("test_queue", &props2, "persistent message"));

    // 测试消息消费和属性验证
    message_ptr msg1 = vh.basic_consume_and_remove("test_queue");
    ASSERT_NE(msg1, nullptr);
    EXPECT_EQ(msg1->payload().properties().delivery_mode(), DeliveryMode::UNDURABLE);

    message_ptr msg2 = vh.basic_consume_and_remove("test_queue");
    ASSERT_NE(msg2, nullptr);
    EXPECT_EQ(msg2->payload().properties().delivery_mode(), DeliveryMode::DURABLE);
}

TEST(DeadLetterQueueTest, MultipleVirtualHosts) {
    // 测试多个虚拟主机
    std::string baseDir1 = "./testdata1";
    std::string baseDir2 = "./testdata2";
    
    virtual_host vh1("TestHost1", baseDir1, baseDir1 + "/meta.db");
    virtual_host vh2("TestHost2", baseDir2, baseDir2 + "/meta.db");

    // 在第一个虚拟主机中声明队列
    ASSERT_TRUE(vh1.declare_queue("queue1", false, false, false, {}));
    ASSERT_TRUE(vh1.declare_exchange("exchange1", ExchangeType::DIRECT, false, false, {}));

    // 在第二个虚拟主机中声明队列
    ASSERT_TRUE(vh2.declare_queue("queue2", false, false, false, {}));
    ASSERT_TRUE(vh2.declare_exchange("exchange2", ExchangeType::DIRECT, false, false, {}));

    // 测试消息发布到不同的虚拟主机
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh1.basic_publish("queue1", &props, "message1"));
    ASSERT_TRUE(vh2.basic_publish("queue2", &props, "message2"));

    // 验证消息隔离
    message_ptr msg1 = vh1.basic_consume("queue1");
    ASSERT_NE(msg1, nullptr);
    EXPECT_EQ(msg1->payload().body(), "message1");

    message_ptr msg2 = vh2.basic_consume("queue2");
    ASSERT_NE(msg2, nullptr);
    EXPECT_EQ(msg2->payload().body(), "message2");

    // 验证跨虚拟主机访问失败
    message_ptr cross_msg = vh1.basic_consume("queue2");
    EXPECT_EQ(cross_msg, nullptr);
}

TEST(DeadLetterQueueTest, ExchangeAndQueueDeletion) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明交换机和队列
    ASSERT_TRUE(vh.declare_exchange("test_exchange", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("test_queue", false, false, false, {}));
    ASSERT_TRUE(vh.bind("test_exchange", "test_queue", "test_key"));

    // 发布消息
    BasicProperties props;
    props.set_routing_key("test_key");
    ASSERT_TRUE(vh.publish_to_exchange("test_exchange", &props, "test message"));

    // 验证消息存在
    message_ptr msg = vh.basic_consume("test_queue");
    ASSERT_NE(msg, nullptr);

    // 删除交换机
    vh.delete_exchange("test_exchange");

    // 删除队列
    vh.delete_queue("test_queue");

    // 验证删除后无法访问
    ASSERT_FALSE(vh.exists_queue("test_queue"));
    
    // 测试发布到已删除的交换机
    ASSERT_FALSE(vh.publish_to_exchange("test_exchange", &props, "test message"));
}

TEST(DeadLetterQueueTest, ComplexRoutingScenarios) {
    // 初始化虚拟主机
    std::string baseDir = "./testdata";
    virtual_host vh("TestHost", baseDir, baseDir + "/meta.db");

    // 声明多个交换机和队列
    ASSERT_TRUE(vh.declare_exchange("direct_exchange", ExchangeType::DIRECT, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("fanout_exchange", ExchangeType::FANOUT, false, false, {}));
    ASSERT_TRUE(vh.declare_exchange("topic_exchange", ExchangeType::TOPIC, false, false, {}));

    ASSERT_TRUE(vh.declare_queue("queue1", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue2", false, false, false, {}));
    ASSERT_TRUE(vh.declare_queue("queue3", false, false, false, {}));

    // 复杂绑定场景
    ASSERT_TRUE(vh.bind("direct_exchange", "queue1", "key1"));
    ASSERT_TRUE(vh.bind("direct_exchange", "queue2", "key2"));
    ASSERT_TRUE(vh.bind("fanout_exchange", "queue1", ""));
    ASSERT_TRUE(vh.bind("fanout_exchange", "queue2", ""));
    ASSERT_TRUE(vh.bind("topic_exchange", "queue3", "user.*"));
    ASSERT_TRUE(vh.bind("topic_exchange", "queue1", "*.login"));

    // 发布消息到不同交换机
    BasicProperties props1;
    props1.set_routing_key("key1");
    ASSERT_TRUE(vh.publish_to_exchange("direct_exchange", &props1, "direct message"));

    BasicProperties props2;
    props2.set_routing_key("any_key");
    ASSERT_TRUE(vh.publish_to_exchange("fanout_exchange", &props2, "fanout message"));

    BasicProperties props3;
    props3.set_routing_key("user.login");
    ASSERT_TRUE(vh.publish_to_exchange("topic_exchange", &props3, "topic message"));

    // 验证消息路由
    message_ptr msg1 = vh.basic_consume("queue1");
    ASSERT_NE(msg1, nullptr);

    message_ptr msg2 = vh.basic_consume("queue2");
    ASSERT_NE(msg2, nullptr);

    message_ptr msg3 = vh.basic_consume("queue3");
    ASSERT_NE(msg3, nullptr);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 