/********************************************************************
*  test_recv.cpp —— 功能 2：消息接收（Pull / Push）测试全集
********************************************************************/
#include <gtest/gtest.h>
#include "../server/virtual_host.hpp"
#include "../server/consumer.hpp"

using namespace hz_mq;

/* ---------- 方便在本地直接触发一次 Push 投递 ---------- */
static void push_once(const virtual_host::ptr& host,
                      const consumer_manager::ptr& cmp,
                      const std::string& qname)
{
    message_ptr mp = host->basic_consume(qname);
    if (!mp) return;

    consumer::ptr cp = cmp->choose(qname);     // RR 选消费者
    if (!cp) return;

    cp->callback(cp->tag,
                 mp->mutable_payload()->mutable_properties(),
                 mp->payload().body());
    if (cp->auto_ack)
        host->basic_ack(qname, mp->payload().properties().id());
}

/* ======================== 公用夹具 ======================== */
class ReceiveFixture : public ::testing::Test {
protected:
    void SetUp() override {
        host = std::make_shared<virtual_host>("vh","./data","./tmp.db");
        cmp  = std::make_shared<consumer_manager>();

        ASSERT_TRUE(host->declare_queue("q1", false,false,false,{}));
        cmp->init_queue_consumer("q1");
    }
    /* 便捷发布 */
    void pub(const std::string& body) {
        BasicProperties bp; bp.set_routing_key("q1");
        ASSERT_TRUE( host->basic_publish("q1", &bp, body) );
    }

    virtual_host::ptr     host;
    consumer_manager::ptr cmp;
};

/* ------------------------------------------------------------------
 *  PULL 方式
 * ----------------------------------------------------------------*/
TEST_F(ReceiveFixture, PullSingleMessage)          /* 有消息 */
{
    pub("one");

    auto msg = host->basic_consume("q1");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->payload().body(), "one");

    host->basic_ack("q1", msg->payload().properties().id());
    EXPECT_EQ(host->basic_consume("q1"), nullptr);   // 被 ack 删除
}

TEST_F(ReceiveFixture, PullFromEmptyQueue)         /* 空队列 */
{
    EXPECT_EQ(host->basic_consume("q1"), nullptr);
}

TEST_F(ReceiveFixture, PullFifoOrder)              /* FIFO 顺序 */
{
    pub("a"); pub("b"); pub("c");
    auto m1 = host->basic_consume("q1");
    auto m2 = host->basic_consume("q1");
    auto m3 = host->basic_consume("q1");
    ASSERT_NE(m1,nullptr); ASSERT_NE(m2,nullptr); ASSERT_NE(m3,nullptr);
    EXPECT_EQ(m1->payload().body(),"a");
    EXPECT_EQ(m2->payload().body(),"b");
    EXPECT_EQ(m3->payload().body(),"c");
}

/* ------------------------------------------------------------------
 *  PUSH 方式
 * ----------------------------------------------------------------*/
TEST_F(ReceiveFixture, PushDeliver)                /* 普通 push-NACK */
{
    std::string recv;
    cmp->create("tag","q1",false,                       // auto_ack = false
        [&](const std::string&,const BasicProperties*,const std::string& b){ recv=b; });

    pub("p1");
    push_once(host, cmp, "q1");

    EXPECT_EQ(recv, "p1");                             // 收到
    EXPECT_EQ(host->basic_consume("q1"), nullptr);    // 未 ack 仍在队
}

TEST_F(ReceiveFixture, PushAutoAck)                 /* auto-ack */
{
    std::string recv;
    cmp->create("tag1","q1",true,
        [&](auto,auto,const std::string& body){ recv=body; });

    pub("hello");
    push_once(host, cmp, "q1");

    EXPECT_EQ(recv, "hello");
    EXPECT_EQ(host->basic_consume("q1"), nullptr);     // 已被 auto_ack
}

TEST_F(ReceiveFixture, PushRoundRobin)             /* 轮询均衡 */
{
    int c1=0,c2=0;
    cmp->create("t1","q1",true,[&](auto,auto,const std::string&){ ++c1; });
    cmp->create("t2","q1",true,[&](auto,auto,const std::string&){ ++c2; });

    for(int i=0;i<4;++i){ pub("m"); push_once(host,cmp,"q1"); }

    EXPECT_EQ(c1+c2, 4);
    EXPECT_TRUE(std::abs(c1-c2)<=1);                  // 近似均衡
}

TEST_F(ReceiveFixture, PushNoConsumerDrop)         /* 无消费者 -> 最终队空 */
{
    pub("lost");                     // 没有注册消费者
    push_once(host, cmp, "q1");      // choose 返回 null，drop
    EXPECT_EQ(host->basic_consume("q1"), nullptr);
}



TEST_F(ReceiveFixture, PullThenPushSequence)       /* 模式切换序列 */
{
    pub("first");
    auto m = host->basic_consume("q1");
    ASSERT_NE(m,nullptr);
    host->basic_ack("q1", m->payload().properties().id());   // pull-ack

    std::string got;
    cmp->create("t","q1",true,[&](auto,auto,const std::string& b){ got=b; });
    pub("second");
    push_once(host,cmp,"q1");

    EXPECT_EQ(got,"second");
}
