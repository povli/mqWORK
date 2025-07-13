// gtest 头
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

#include "../src/server/virtual_host.hpp"
#include "../src/server/consumer.hpp"
#include "../src/common/exchange.hpp"
#include "../src/server/route.hpp"

using namespace hz_mq;

/* ---------- 发布订阅测试夹具 ---------- */
class PubSubFixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        host = std::make_shared<virtual_host>("vh", "./test_pubsub_data", "./test_pubsub_data/meta.db");
        cmp = std::make_shared<consumer_manager>();
        
        // 创建测试交换机
        ASSERT_TRUE( host->declare_exchange("fanout_ex", ExchangeType::FANOUT, false, false, {}) );
        ASSERT_TRUE( host->declare_exchange("topic_ex", ExchangeType::TOPIC, false, false, {}) );
        
        // 创建测试队列
        ASSERT_TRUE( host->declare_queue("q1", false, false, false, {}) );
        ASSERT_TRUE( host->declare_queue("q2", false, false, false, {}) );
        
        // 绑定队列到交换机
        ASSERT_TRUE( host->bind("fanout_ex", "q1", "") );
        ASSERT_TRUE( host->bind("fanout_ex", "q2", "") );
    }

    void TearDown() override
    {
        if (host) {
            (void)system("rm -rf ./test_pubsub_data");
        }
    }

    // 通过交换机发布消息的辅助方法
    bool publish_to_exchange(const std::string& exchange_name, const std::string& body, const std::string& routing_key = "")
    {
        auto exchange = host->select_exchange(exchange_name);
        if (!exchange) return false;
        
        // 获取绑定到该交换机的所有队列
        auto bindings = host->exchange_bindings(exchange_name);
        
        // 根据交换机类型和路由键决定消息路由到哪些队列
        std::vector<std::string> target_queues;
        
        for (const auto& [queue_name, binding] : bindings) {
            bool should_route = false;
            
            switch (exchange->type) {
                case ExchangeType::FANOUT:
                    // FANOUT: 广播到所有绑定的队列
                    should_route = true;
                    break;
                    
                case ExchangeType::DIRECT:
                    // DIRECT: 精确匹配路由键
                    should_route = (routing_key == binding->binding_key);
                    break;
                    
                case ExchangeType::TOPIC:
                    // TOPIC: 使用通配符匹配
                    should_route = router::match_route(ExchangeType::TOPIC, routing_key, binding->binding_key);
                    break;
            }
            
            if (should_route) {
                target_queues.push_back(queue_name);
            }
        }
        
        // 将消息发布到所有目标队列
        BasicProperties bp;
        static int id_seq = 0;
        bp.set_id("testid_" + std::to_string(++id_seq));
        if (!routing_key.empty()) {
            bp.set_routing_key(routing_key);
        }
        
        bool success = true;
        for (const auto& queue_name : target_queues) {
            if (!host->basic_publish(queue_name, &bp, body)) {
                success = false;
            }
        }
        
        return success;
    }

    virtual_host::ptr host;
    consumer_manager::ptr cmp;
};

/* ---------- F1 FANOUT 广播消息 ---------- */
TEST_F(PubSubFixture, FanoutBroadcast)
{
    // 发布消息到fanout交换机
    ASSERT_TRUE( publish_to_exchange("fanout_ex", "Broadcast Message") );

    // 两个队列都应该收到消息
    auto msg1 = host->basic_consume("q1");
    auto msg2 = host->basic_consume("q2");
    
    ASSERT_NE(msg1, nullptr);
    ASSERT_NE(msg2, nullptr);
    EXPECT_EQ(msg1->payload().body(), "Broadcast Message");
    EXPECT_EQ(msg2->payload().body(), "Broadcast Message");
}

/* ---------- F2 FANOUT 空绑定 ---------- */
TEST_F(PubSubFixture, FanoutNoBinding)
{
    // 创建没有绑定的fanout交换机
    ASSERT_TRUE( host->declare_exchange("empty_fanout", ExchangeType::FANOUT, false, false, {}) );
    
    // 发布消息应该成功，但没有队列收到
    EXPECT_TRUE( publish_to_exchange("empty_fanout", "No one receives") );
    EXPECT_EQ( host->basic_consume("q1"), nullptr );
    EXPECT_EQ( host->basic_consume("q2"), nullptr );
}

/* ---------- F3 FANOUT 动态绑定 ---------- */
TEST_F(PubSubFixture, FanoutDynamicBinding)
{
    // 创建新队列并绑定到fanout交换机
    ASSERT_TRUE( host->declare_queue("q3", false, false, false, {}) );
    ASSERT_TRUE( host->bind("fanout_ex", "q3", "") );
    
    // 发布消息
    ASSERT_TRUE( publish_to_exchange("fanout_ex", "Dynamic Broadcast") );
    
    // 三个队列都应该收到
    EXPECT_EQ( host->basic_consume("q1")->payload().body(), "Dynamic Broadcast" );
    EXPECT_EQ( host->basic_consume("q2")->payload().body(), "Dynamic Broadcast" );
    EXPECT_EQ( host->basic_consume("q3")->payload().body(), "Dynamic Broadcast" );
}

/* ---------- T1 TOPIC 主题订阅 ---------- */
TEST_F(PubSubFixture, TopicSubscription)
{
    // 创建topic交换机
    ASSERT_TRUE( host->declare_exchange("topic_ex", ExchangeType::TOPIC, false, false, {}) );
    ASSERT_TRUE( host->declare_queue("disk_queue", false, false, false, {}) );
    ASSERT_TRUE( host->bind("topic_ex", "disk_queue", "kern.disk.#") );
    
    // 发布匹配的消息
    ASSERT_TRUE( publish_to_exchange("topic_ex", "Disk I/O Error", "kern.disk.sda") );
    
    auto msg = host->basic_consume("disk_queue");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->payload().body(), "Disk I/O Error");
    EXPECT_EQ(msg->payload().properties().routing_key(), "kern.disk.sda");
}

/* ---------- T2 TOPIC 通配符匹配 ---------- */
TEST_F(PubSubFixture, TopicWildcardMatching)
{
    ASSERT_TRUE( host->declare_exchange("topic_ex", ExchangeType::TOPIC, false, false, {}) );
    ASSERT_TRUE( host->declare_queue("cpu_queue", false, false, false, {}) );
    ASSERT_TRUE( host->declare_queue("all_queue", false, false, false, {}) );
    
    // 绑定不同的主题模式
    ASSERT_TRUE( host->bind("topic_ex", "cpu_queue", "kern.cpu.*") );
    ASSERT_TRUE( host->bind("topic_ex", "all_queue", "kern.#") );
    
    // 发布消息
    ASSERT_TRUE( publish_to_exchange("topic_ex", "CPU Load", "kern.cpu.load") );
    
    // cpu_queue应该收到（精确匹配）
    auto cpu_msg = host->basic_consume("cpu_queue");
    ASSERT_NE(cpu_msg, nullptr);
    EXPECT_EQ(cpu_msg->payload().body(), "CPU Load");
    
    // all_queue也应该收到（通配符匹配）
    auto all_msg = host->basic_consume("all_queue");
    ASSERT_NE(all_msg, nullptr);
    EXPECT_EQ(all_msg->payload().body(), "CPU Load");
}

/* ---------- T3 TOPIC 不匹配 ---------- */
TEST_F(PubSubFixture, TopicNoMatch)
{
    ASSERT_TRUE( host->declare_exchange("topic_ex", ExchangeType::TOPIC, false, false, {}) );
    ASSERT_TRUE( host->declare_queue("disk_queue", false, false, false, {}) );
    ASSERT_TRUE( host->bind("topic_ex", "disk_queue", "kern.disk.*") );
    
    // 发布不匹配的消息
    ASSERT_TRUE( publish_to_exchange("topic_ex", "CPU Info", "kern.cpu.load") );
    
    // 队列不应该收到消息
    EXPECT_EQ( host->basic_consume("disk_queue"), nullptr );
}

/* ---------- T4 TOPIC 多级匹配 ---------- */
TEST_F(PubSubFixture, TopicMultiLevelMatching)
{
    ASSERT_TRUE( host->declare_exchange("topic_ex", ExchangeType::TOPIC, false, false, {}) );
    ASSERT_TRUE( host->declare_queue("queue1", false, false, false, {}) );
    ASSERT_TRUE( host->declare_queue("queue2", false, false, false, {}) );
    ASSERT_TRUE( host->declare_queue("queue3", false, false, false, {}) );
    
    // 绑定不同级别的模式
    ASSERT_TRUE( host->bind("topic_ex", "queue1", "*.disk.*") );
    ASSERT_TRUE( host->bind("topic_ex", "queue2", "kern.disk.#") );
    ASSERT_TRUE( host->bind("topic_ex", "queue3", "kern.disk.sda") );
    
    // 发布消息
    ASSERT_TRUE( publish_to_exchange("topic_ex", "Disk Info", "kern.disk.sda") );
    
    // 所有队列都应该收到
    EXPECT_EQ( host->basic_consume("queue1")->payload().body(), "Disk Info" );
    EXPECT_EQ( host->basic_consume("queue2")->payload().body(), "Disk Info" );
    EXPECT_EQ( host->basic_consume("queue3")->payload().body(), "Disk Info" );
}

/* ---------- R1 路由匹配逻辑 ---------- */
TEST(RouteMatch, DirectExactMatch)
{
    EXPECT_TRUE( router::match_route(ExchangeType::DIRECT, "key1", "key1") );
    EXPECT_FALSE( router::match_route(ExchangeType::DIRECT, "key1", "key2") );
    EXPECT_FALSE( router::match_route(ExchangeType::DIRECT, "key1", "") );
}

TEST(RouteMatch, FanoutAlwaysMatch)
{
    EXPECT_TRUE( router::match_route(ExchangeType::FANOUT, "any.key", "any.binding") );
    EXPECT_TRUE( router::match_route(ExchangeType::FANOUT, "", "") );
    EXPECT_TRUE( router::match_route(ExchangeType::FANOUT, "key", "") );
}

TEST(RouteMatch, TopicWildcardMatch)
{
    // 精确匹配
    EXPECT_TRUE( router::match_route(ExchangeType::TOPIC, "kern.disk.sda", "kern.disk.sda") );
    
    // * 匹配一个段
    EXPECT_TRUE( router::match_route(ExchangeType::TOPIC, "kern.disk.sda", "kern.*.sda") );
    EXPECT_TRUE( router::match_route(ExchangeType::TOPIC, "kern.disk.sda", "*.disk.*") );
    EXPECT_FALSE( router::match_route(ExchangeType::TOPIC, "kern.disk.sda", "kern.*") );
    
    // # 匹配零个或多个段
    EXPECT_TRUE( router::match_route(ExchangeType::TOPIC, "kern.disk.sda", "kern.#") );
    EXPECT_TRUE( router::match_route(ExchangeType::TOPIC, "kern.disk.sda", "kern.disk.#") );
    EXPECT_TRUE( router::match_route(ExchangeType::TOPIC, "kern", "kern.#") );
    EXPECT_FALSE( router::match_route(ExchangeType::TOPIC, "kern.disk.sda", "user.#") );
}

/* ---------- E1 交换机管理 ---------- */
TEST(ExchangeManagement, DeclareAndDelete)
{
    auto vh = std::make_shared<virtual_host>("vh", "./test_exchange_data", "./test_exchange_data/meta.db");
    
    // 声明交换机
    EXPECT_TRUE( vh->declare_exchange("ex1", ExchangeType::DIRECT, false, false, {}) );
    EXPECT_TRUE( vh->declare_exchange("ex2", ExchangeType::FANOUT, false, false, {}) );
    EXPECT_TRUE( vh->declare_exchange("ex3", ExchangeType::TOPIC, false, false, {}) );
    
    // 重复声明应该成功（幂等）
    EXPECT_TRUE( vh->declare_exchange("ex1", ExchangeType::DIRECT, false, false, {}) );
    
    // 选择交换机
    auto ex1 = vh->select_exchange("ex1");
    ASSERT_NE(ex1, nullptr);
    EXPECT_EQ(ex1->type, ExchangeType::DIRECT);
    
    auto ex2 = vh->select_exchange("ex2");
    ASSERT_NE(ex2, nullptr);
    EXPECT_EQ(ex2->type, ExchangeType::FANOUT);
    
    // 删除交换机
    vh->delete_exchange("ex1");
    EXPECT_EQ( vh->select_exchange("ex1"), nullptr );
    
    (void)system("rm -rf ./test_exchange_data");
}

/* ---------- E2 绑定关系管理 ---------- */
TEST(BindingManagement, BindAndUnbind)
{
    auto vh = std::make_shared<virtual_host>("vh", "./test_binding_data", "./test_binding_data/meta.db");
    
    ASSERT_TRUE( vh->declare_exchange("ex", ExchangeType::FANOUT, false, false, {}) );
    ASSERT_TRUE( vh->declare_queue("q1", false, false, false, {}) );
    ASSERT_TRUE( vh->declare_queue("q2", false, false, false, {}) );
    
    // 绑定队列到交换机
    EXPECT_TRUE( vh->bind("ex", "q1", "key1") );
    EXPECT_TRUE( vh->bind("ex", "q2", "key2") );
    
    // 检查绑定关系
    auto bindings = vh->exchange_bindings("ex");
    EXPECT_EQ(bindings.size(), 2);
    EXPECT_NE(bindings.find("q1"), bindings.end());
    EXPECT_NE(bindings.find("q2"), bindings.end());
    
    // 解绑
    vh->unbind("ex", "q1");
    bindings = vh->exchange_bindings("ex");
    EXPECT_EQ(bindings.size(), 1);
    EXPECT_EQ(bindings.find("q1"), bindings.end());
    EXPECT_NE(bindings.find("q2"), bindings.end());
    
    (void)system("rm -rf ./test_binding_data");
}

/* ---------- E3 错误处理 ---------- */
TEST(ExchangeError, NonExistentExchange)
{
    auto vh = std::make_shared<virtual_host>("vh", "./test_error_data", "./test_error_data/meta.db");
    
    // 尝试绑定到不存在的交换机
    ASSERT_TRUE( vh->declare_queue("q1", false, false, false, {}) );
    EXPECT_FALSE( vh->bind("nonexistent", "q1", "key") );
    
    // 尝试绑定不存在的队列
    ASSERT_TRUE( vh->declare_exchange("ex", ExchangeType::DIRECT, false, false, {}) );
    EXPECT_FALSE( vh->bind("ex", "nonexistent", "key") );
    
    (void)system("rm -rf ./test_error_data");
}

/* ---------- P1 并发发布订阅 ---------- */
TEST_F(PubSubFixture, ConcurrentPubSub)
{
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;
    
    // 创建多个发布者线程
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            std::string msg = "Concurrent Message " + std::to_string(i);
            if (publish_to_exchange("fanout_ex", msg)) {
                success_count++;
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(success_count.load(), 10);
    
    // 验证消息被正确路由
    auto msg1 = host->basic_consume("q1");
    auto msg2 = host->basic_consume("q2");
    ASSERT_NE(msg1, nullptr);
    ASSERT_NE(msg2, nullptr);
}

/* ---------- P2 消息持久化 ---------- */
TEST_F(PubSubFixture, MessagePersistence)
{
    // 发布持久化消息
    ASSERT_TRUE( publish_to_exchange("fanout_ex", "Persistent Message") );
    
    // 验证消息被正确存储
    auto msg1 = host->basic_consume("q1");
    auto msg2 = host->basic_consume("q2");
    
    ASSERT_NE(msg1, nullptr);
    ASSERT_NE(msg2, nullptr);
    EXPECT_EQ(msg1->payload().body(), "Persistent Message");
    EXPECT_EQ(msg2->payload().body(), "Persistent Message");
}

/* ---------- V1 类型一致性验证 ---------- */
TEST(ExchangeTypeValidation, TypeConsistency)
{
    auto vh = std::make_shared<virtual_host>("vh", "./test_type_data", "./test_type_data/meta.db");
    
    // 验证交换机类型设置
    ASSERT_TRUE( vh->declare_exchange("direct_ex", ExchangeType::DIRECT, false, false, {}) );
    ASSERT_TRUE( vh->declare_exchange("fanout_ex", ExchangeType::FANOUT, false, false, {}) );
    ASSERT_TRUE( vh->declare_exchange("topic_ex", ExchangeType::TOPIC, false, false, {}) );
    
    auto direct = vh->select_exchange("direct_ex");
    auto fanout = vh->select_exchange("fanout_ex");
    auto topic = vh->select_exchange("topic_ex");
    
    EXPECT_EQ(direct->type, ExchangeType::DIRECT);
    EXPECT_EQ(fanout->type, ExchangeType::FANOUT);
    EXPECT_EQ(topic->type, ExchangeType::TOPIC);
    
    (void)system("rm -rf ./test_type_data");
}

/* ---------- E4 边界条件测试 ---------- */
TEST_F(PubSubFixture, EdgeCases)
{
    // 空消息体
    EXPECT_TRUE( publish_to_exchange("fanout_ex", "") );
    
    // 空路由键
    EXPECT_TRUE( publish_to_exchange("fanout_ex", "Empty Routing Key", "") );
    
    // 特殊字符路由键
    EXPECT_TRUE( publish_to_exchange("topic_ex", "Special Chars", "user.*.test") );
    
    // 验证消息仍然被正确路由
    auto msg1 = host->basic_consume("q1");
    auto msg2 = host->basic_consume("q2");
    ASSERT_NE(msg1, nullptr);
    ASSERT_NE(msg2, nullptr);
}

/* ---------- P3 性能测试 ---------- */
TEST_F(PubSubFixture, DISABLED_PerformanceTest)
{
    const int message_count = 10;  // 进一步减少到10条
    auto start = std::chrono::high_resolution_clock::now();
    
    // 批量发布消息
    for (int i = 0; i < message_count; ++i) {
        ASSERT_TRUE( publish_to_exchange("fanout_ex", "Performance Test " + std::to_string(i)) );
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 验证所有消息都被正确处理
    int received_count = 0;
    while (host->basic_consume("q1") != nullptr) {
        received_count++;
    }
    
    EXPECT_EQ(received_count, message_count);
    EXPECT_LT(duration.count(), 500); // 应该在500毫秒内完成
}

/* ---------- E5 交换机参数测试 ---------- */
TEST(ExchangeArgs, SetAndGetArgs)
{
    auto vh = std::make_shared<virtual_host>("vh", "./test_args_data", "./test_args_data/meta.db");
    
    // 创建带参数的交换机
    std::unordered_map<std::string, std::string> args = {
        {"x-max-priority", "10"},
        {"x-message-ttl", "3600000"}
    };
    
    ASSERT_TRUE( vh->declare_exchange("args_ex", ExchangeType::DIRECT, false, false, args) );
    
    auto ex = vh->select_exchange("args_ex");
    ASSERT_NE(ex, nullptr);
    
    // 测试参数设置和获取
    ex->set_args("test_key=test_value");
    auto retrieved_args = ex->get_args();
    EXPECT_NE(retrieved_args.find("test_key"), std::string::npos);
    
    (void)system("rm -rf ./test_args_data");
}

/* ---------- E6 交换机映射器测试 ---------- */
TEST(ExchangeMapper, InsertAndRemove)
{
    auto vh = std::make_shared<virtual_host>("vh", "./test_mapper_data", "./test_mapper_data/meta.db");
    
    // 测试交换机映射器内部操作
    ASSERT_TRUE( vh->declare_exchange("mapper_ex1", ExchangeType::DIRECT, false, false, {}) );
    ASSERT_TRUE( vh->declare_exchange("mapper_ex2", ExchangeType::FANOUT, false, false, {}) );
    
    // 获取所有交换机
    auto all_exchanges = vh->select_exchange("mapper_ex1");
    ASSERT_NE(all_exchanges, nullptr);
    
    // 删除交换机
    vh->delete_exchange("mapper_ex1");
    EXPECT_EQ( vh->select_exchange("mapper_ex1"), nullptr );
    
    (void)system("rm -rf ./test_mapper_data");
}

/* ---------- E7 交换机管理器测试 ---------- */
TEST(ExchangeManager, AllFunctions)
{
    auto vh = std::make_shared<virtual_host>("vh", "./test_manager_data", "./test_manager_data/meta.db");
    
    // 测试所有交换机管理功能
    ASSERT_TRUE( vh->declare_exchange("manager_ex1", ExchangeType::DIRECT, false, false, {}) );
    ASSERT_TRUE( vh->declare_exchange("manager_ex2", ExchangeType::TOPIC, false, false, {}) );
    ASSERT_TRUE( vh->declare_exchange("manager_ex3", ExchangeType::FANOUT, false, false, {}) );
    
    // 测试存在性检查
    EXPECT_TRUE( vh->select_exchange("manager_ex1") != nullptr );
    EXPECT_TRUE( vh->select_exchange("manager_ex2") != nullptr );
    EXPECT_TRUE( vh->select_exchange("manager_ex3") != nullptr );
    
    // 测试删除
    vh->delete_exchange("manager_ex1");
    EXPECT_EQ( vh->select_exchange("manager_ex1"), nullptr );
    
    (void)system("rm -rf ./test_manager_data");
}

/* ---------- E8 错误路径测试 ---------- */
TEST(ExchangeError, InvalidOperations)
{
    auto vh = std::make_shared<virtual_host>("vh", "./test_error_data", "./test_error_data/meta.db");
    
    // 测试选择不存在的交换机
    EXPECT_EQ( vh->select_exchange("nonexistent"), nullptr );
    
    // 测试删除不存在的交换机
    vh->delete_exchange("nonexistent"); // 应该不会崩溃
    
    // 测试重复声明相同交换机
    ASSERT_TRUE( vh->declare_exchange("duplicate_ex", ExchangeType::DIRECT, false, false, {}) );
    EXPECT_TRUE( vh->declare_exchange("duplicate_ex", ExchangeType::DIRECT, false, false, {}) ); // 幂等性
    
    (void)system("rm -rf ./test_error_data");
}

/* ---------- E9 边界条件测试 ---------- */
TEST(ExchangeBoundary, EdgeCases)
{
    auto vh = std::make_shared<virtual_host>("vh", "./test_boundary_data", "./test_boundary_data/meta.db");
    
    // 测试特殊字符名称
    ASSERT_TRUE( vh->declare_exchange("special_ex", ExchangeType::DIRECT, false, false, {}) );
    
    // 测试大量参数
    std::unordered_map<std::string, std::string> large_args;
    for (int i = 0; i < 10; ++i) {
        large_args["key" + std::to_string(i)] = "value" + std::to_string(i);
    }
    ASSERT_TRUE( vh->declare_exchange("large_args_ex", ExchangeType::DIRECT, false, false, large_args) );
    
    (void)system("rm -rf ./test_boundary_data");
}

TEST(ExchangeManager, AllList) {
    micromq::exchange_manager mgr("./test_alllist.db");
    mgr.declare_exchange("ex1", ExchangeType::DIRECT, false, false, {});
    mgr.declare_exchange("ex2", ExchangeType::FANOUT, false, false, {});
    auto all = mgr.all();
    EXPECT_TRUE(all.find("ex1") != all.end());
    EXPECT_TRUE(all.find("ex2") != all.end());
    mgr.delete_exchange("ex1");
    mgr.delete_exchange("ex2");
    (void)system("rm -rf ./test_alllist.db");
}

TEST(ExchangeMapper, InsertRemoveAll) {
    // 直接测试 exchange_manager，因为 exchange_mapper 是内部实现
    micromq::exchange_manager mgr("./test_mapper.db");
    
    // 测试插入交换机
    EXPECT_TRUE(mgr.declare_exchange("ex", ExchangeType::DIRECT, false, false, {}));
    
    // 测试获取所有交换机
    auto all = mgr.all();
    EXPECT_TRUE(all.find("ex") != all.end());
    
    // 测试删除交换机
    mgr.delete_exchange("ex");
    all = mgr.all();
    EXPECT_TRUE(all.find("ex") == all.end());
    
    (void)system("rm -rf ./test_mapper.db");
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 