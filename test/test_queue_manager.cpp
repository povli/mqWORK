// gtest 头
#include <gtest/gtest.h>
#include "../src/common/queue.hpp"

using namespace micromq;

class QueueManagerFixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 创建队列管理器
        queue_mgr = std::make_shared<msg_queue_manager>("./test_queue.db");
    }

    void TearDown() override
    {
        // 清理测试数据
        system("rm -f ./test_queue.db");
    }

    msg_queue_manager::ptr queue_mgr;
};

/* ---------- 基础队列操作测试 ---------- */
TEST_F(QueueManagerFixture, DeclareQueue_Success)
{
    std::unordered_map<std::string, std::string> args = {{"x-max-priority", "10"}};
    bool result = queue_mgr->declare_queue("test_queue", true, false, false, args);
    EXPECT_TRUE(result);
    EXPECT_TRUE(queue_mgr->exists("test_queue"));
    
    auto queue = queue_mgr->select_queue("test_queue");
    ASSERT_NE(queue, nullptr);
    EXPECT_EQ(queue->name, "test_queue");
    EXPECT_TRUE(queue->durable);
    EXPECT_FALSE(queue->exclusive);
    EXPECT_FALSE(queue->auto_delete);
    EXPECT_EQ(queue->args["x-max-priority"], "10");
}

TEST_F(QueueManagerFixture, DeclareQueue_Duplicate)
{
    // 第一次声明
    EXPECT_TRUE(queue_mgr->declare_queue("dup_queue", false, false, false, {}));
    EXPECT_TRUE(queue_mgr->exists("dup_queue"));
    
    // 重复声明应该成功（幂等性）
    EXPECT_TRUE(queue_mgr->declare_queue("dup_queue", false, false, false, {}));
    EXPECT_TRUE(queue_mgr->exists("dup_queue"));
    
    // 验证队列数量
    auto all_queues = queue_mgr->all();
    EXPECT_EQ(all_queues.size(), 1);
}

TEST_F(QueueManagerFixture, DeleteQueue_Success)
{
    // 先创建队列
    EXPECT_TRUE(queue_mgr->declare_queue("delete_test", false, false, false, {}));
    EXPECT_TRUE(queue_mgr->exists("delete_test"));
    
    // 删除队列
    queue_mgr->delete_queue("delete_test");
    EXPECT_FALSE(queue_mgr->exists("delete_test"));
    
    auto queue = queue_mgr->select_queue("delete_test");
    EXPECT_EQ(queue, nullptr);
}

TEST_F(QueueManagerFixture, DeleteQueue_NonExistent)
{
    // 删除不存在的队列应该不会崩溃
    EXPECT_NO_THROW(queue_mgr->delete_queue("non_existent"));
    EXPECT_FALSE(queue_mgr->exists("non_existent"));
}

/* ---------- 队列状态查询测试 ---------- */
TEST_F(QueueManagerFixture, GetQueueStatus_Exists)
{
    std::unordered_map<std::string, std::string> args = {{"x-message-ttl", "3600000"}};
    EXPECT_TRUE(queue_mgr->declare_queue("status_test", true, true, true, args));
    
    auto status = queue_mgr->get_queue_status("status_test");
    EXPECT_TRUE(status.exists);
    EXPECT_TRUE(status.durable);
    EXPECT_TRUE(status.exclusive);
    EXPECT_TRUE(status.auto_delete);
    EXPECT_EQ(status.args["x-message-ttl"], "3600000");
}

TEST_F(QueueManagerFixture, GetQueueStatus_NonExistent)
{
    auto status = queue_mgr->get_queue_status("non_existent");
    EXPECT_FALSE(status.exists);
    EXPECT_FALSE(status.durable);
    EXPECT_FALSE(status.exclusive);
    EXPECT_FALSE(status.auto_delete);
    EXPECT_TRUE(status.args.empty());
}

TEST_F(QueueManagerFixture, GetQueueStatus_EmptyArgs)
{
    EXPECT_TRUE(queue_mgr->declare_queue("empty_args", false, false, false, {}));
    
    auto status = queue_mgr->get_queue_status("empty_args");
    EXPECT_TRUE(status.exists);
    EXPECT_FALSE(status.durable);
    EXPECT_FALSE(status.exclusive);
    EXPECT_FALSE(status.auto_delete);
    EXPECT_TRUE(status.args.empty());
}

/* ---------- 队列属性测试 ---------- */
TEST_F(QueueManagerFixture, QueueProperties_Durable)
{
    EXPECT_TRUE(queue_mgr->declare_queue("durable_queue", true, false, false, {}));
    auto queue = queue_mgr->select_queue("durable_queue");
    ASSERT_NE(queue, nullptr);
    EXPECT_TRUE(queue->durable);
    EXPECT_FALSE(queue->exclusive);
    EXPECT_FALSE(queue->auto_delete);
}

TEST_F(QueueManagerFixture, QueueProperties_Exclusive)
{
    EXPECT_TRUE(queue_mgr->declare_queue("exclusive_queue", false, true, false, {}));
    auto queue = queue_mgr->select_queue("exclusive_queue");
    ASSERT_NE(queue, nullptr);
    EXPECT_FALSE(queue->durable);
    EXPECT_TRUE(queue->exclusive);
    EXPECT_FALSE(queue->auto_delete);
}

TEST_F(QueueManagerFixture, QueueProperties_AutoDelete)
{
    EXPECT_TRUE(queue_mgr->declare_queue("auto_delete_queue", false, false, true, {}));
    auto queue = queue_mgr->select_queue("auto_delete_queue");
    ASSERT_NE(queue, nullptr);
    EXPECT_FALSE(queue->durable);
    EXPECT_FALSE(queue->exclusive);
    EXPECT_TRUE(queue->auto_delete);
}

/* ---------- 队列参数测试 ---------- */
TEST_F(QueueManagerFixture, QueueArgs_Complex)
{
    std::unordered_map<std::string, std::string> args = {
        {"x-max-priority", "10"},
        {"x-message-ttl", "3600000"},
        {"x-max-length", "1000"},
        {"x-overflow", "drop-head"}
    };
    
    EXPECT_TRUE(queue_mgr->declare_queue("complex_args", false, false, false, args));
    auto queue = queue_mgr->select_queue("complex_args");
    ASSERT_NE(queue, nullptr);
    
    EXPECT_EQ(queue->args["x-max-priority"], "10");
    EXPECT_EQ(queue->args["x-message-ttl"], "3600000");
    EXPECT_EQ(queue->args["x-max-length"], "1000");
    EXPECT_EQ(queue->args["x-overflow"], "drop-head");
    EXPECT_EQ(queue->args.size(), 4);
}

/* ---------- 边界条件测试 ---------- */
TEST_F(QueueManagerFixture, EmptyQueueName)
{
    // 空字符串作为队列名在当前实现中是允许的
    EXPECT_TRUE(queue_mgr->declare_queue("", false, false, false, {}));
    EXPECT_TRUE(queue_mgr->exists(""));
    
    auto status = queue_mgr->get_queue_status("");
    EXPECT_TRUE(status.exists);
    EXPECT_FALSE(status.durable);
    EXPECT_FALSE(status.exclusive);
    EXPECT_FALSE(status.auto_delete);
    EXPECT_TRUE(status.args.empty());
}

TEST_F(QueueManagerFixture, SpecialCharactersInName)
{
    std::string special_name = "test-queue_with.special@chars#123";
    EXPECT_TRUE(queue_mgr->declare_queue(special_name, false, false, false, {}));
    EXPECT_TRUE(queue_mgr->exists(special_name));
    
    auto status = queue_mgr->get_queue_status(special_name);
    EXPECT_TRUE(status.exists);
    EXPECT_EQ(status.args.size(), 0);
}

/* ---------- msg_queue 结构体测试 ---------- */
TEST_F(QueueManagerFixture, MsgQueueConstructor)
{
    std::unordered_map<std::string, std::string> args = {{"test", "value"}};
    msg_queue queue("test_queue", true, false, true, args);
    
    EXPECT_EQ(queue.name, "test_queue");
    EXPECT_TRUE(queue.durable);
    EXPECT_FALSE(queue.exclusive);
    EXPECT_TRUE(queue.auto_delete);
    EXPECT_EQ(queue.args["test"], "value");
}

TEST_F(QueueManagerFixture, MsgQueueArgsParsing)
{
    msg_queue queue;
    queue.set_args("key1=value1&key2=value2&key3=value3");
    
    EXPECT_EQ(queue.args["key1"], "value1");
    EXPECT_EQ(queue.args["key2"], "value2");
    EXPECT_EQ(queue.args["key3"], "value3");
    EXPECT_EQ(queue.args.size(), 3);
    
    std::string args_str = queue.get_args();
    EXPECT_TRUE(args_str.find("key1=value1") != std::string::npos);
    EXPECT_TRUE(args_str.find("key2=value2") != std::string::npos);
    EXPECT_TRUE(args_str.find("key3=value3") != std::string::npos);
}

TEST_F(QueueManagerFixture, MsgQueueEmptyArgs)
{
    msg_queue queue;
    queue.set_args("");
    EXPECT_TRUE(queue.args.empty());
    
    std::string args_str = queue.get_args();
    EXPECT_TRUE(args_str.empty());
}