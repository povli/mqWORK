#include <gtest/gtest.h>
#include "../src/server/virtual_host.hpp"
#include "../src/common/msg.pb.h"

using namespace hz_mq;

class AckTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        vh = std::make_shared<virtual_host>("TestHost", "./test_ack_data", "./test_ack_data/meta.db");
        vh->declare_queue("q1", false, false, false, {});
        vh->declare_queue("q2", false, false, false, {});
    }
    void TearDown() override {
        system("rm -rf ./test_ack_data");
    }
    virtual_host::ptr vh;
};

TEST_F(AckTestFixture, AckAfterConsume) {
    BasicProperties props;
    props.set_id("msg1");
    vh->basic_publish("q1", &props, "hello");
    auto msg = vh->basic_consume("q1");
    ASSERT_NE(msg, nullptr);
    vh->basic_ack("q1", "msg1");
    auto msg2 = vh->basic_consume("q1");
    EXPECT_EQ(msg2, nullptr); // 已被ack移除
}

TEST_F(AckTestFixture, AckWithoutConsume) {
    // 直接ack未消费的消息
    BasicProperties props;
    props.set_id("msg2");
    vh->basic_publish("q1", &props, "world");
    vh->basic_ack("q1", "msg2");
    auto msg = vh->basic_consume("q1");
    EXPECT_EQ(msg, nullptr); // 直接ack也能移除
}

TEST_F(AckTestFixture, AckWrongMsgId) {
    BasicProperties props;
    props.set_id("msg3");
    vh->basic_publish("q1", &props, "test");
    vh->basic_ack("q1", "not_exist_id");
    auto msg = vh->basic_consume("q1");
    EXPECT_NE(msg, nullptr); // ack错误id不影响队列
}

TEST_F(AckTestFixture, AckTwice) {
    BasicProperties props;
    props.set_id("msg4");
    vh->basic_publish("q1", &props, "repeat");
    vh->basic_ack("q1", "msg4");
    // 再次ack同一id
    vh->basic_ack("q1", "msg4");
    auto msg = vh->basic_consume("q1");
    EXPECT_EQ(msg, nullptr); // 幂等性
}

TEST_F(AckTestFixture, AutoAck) {
    BasicProperties props;
    props.set_id("msg5");
    vh->basic_publish("q1", &props, "auto");
    // 模拟auto_ack: 消费后立即ack
    auto msg = vh->basic_consume("q1");
    ASSERT_NE(msg, nullptr);
    vh->basic_ack("q1", "msg5"); // auto_ack等价于消费后立即ack
    auto msg2 = vh->basic_consume("q1");
    EXPECT_EQ(msg2, nullptr);
}

TEST_F(AckTestFixture, AckEmptyQueue) {
    // 空队列ack
    vh->basic_ack("q1", "no_msg");
    SUCCEED(); // 不抛异常即可
}

TEST_F(AckTestFixture, MultiQueueAck) {
    BasicProperties props1, props2;
    props1.set_id("m1");
    props2.set_id("m2");
    vh->basic_publish("q1", &props1, "a");
    vh->basic_publish("q2", &props2, "b");
    vh->basic_ack("q1", "m1");
    vh->basic_ack("q2", "m2");
    auto msg1 = vh->basic_consume("q1");
    auto msg2 = vh->basic_consume("q2");
    EXPECT_EQ(msg1, nullptr);
    EXPECT_EQ(msg2, nullptr);
} 