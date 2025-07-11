// gtest 头
#include <gtest/gtest.h>
#include "../server/virtual_host.hpp"
#include "../server/consumer.hpp"
#include "../server/channel.hpp"
#include "../server/route.hpp"          // 直接覆盖 match_route
#include "../server/queue_message.hpp"  // 测 queue_message::remove()
#include "../common/thread_pool.hpp"    // 测线程池



using namespace hz_mq;

class PtpFixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        host = std::make_shared<virtual_host>("TestHost", "./data", "./data/meta.db");
        cmp  = std::make_shared<consumer_manager>();

        // 基础资源：exchange ex1 (direct) + queue q1 绑定 routing_key=q1
        ASSERT_TRUE( host->declare_exchange("ex1", ExchangeType::DIRECT, false,false,{}) );
        ASSERT_TRUE( host->declare_queue("q1", false,false,false,{}) );
        ASSERT_TRUE( host->bind("ex1","q1","q1") );

        cmp->init_queue_consumer("q1");
    }

    // 帮助直接 publish，不通过 channel
    bool publish(const std::string& body, const std::string& rkey="q1")
    {
        BasicProperties bp;
        bp.set_routing_key(rkey);
        return host->basic_publish("q1", &bp, body);
    }

    virtual_host::ptr host;
    consumer_manager::ptr cmp;
};

/* ---------- S1 基础成功路径 ---------- */
TEST_F(PtpFixture, PublishThenConsume_OK)
{
    ASSERT_TRUE( publish("Hello") );

    auto msg = host->basic_consume("q1");
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->payload().body(), "Hello");
    EXPECT_EQ(msg->payload().properties().routing_key(), "q1");
}

/* ---------- S2 队列不存在 ---------- */
TEST_F(PtpFixture, PublishToMissingQueue_Fail)
{
    BasicProperties bp; bp.set_routing_key("noq");
    EXPECT_FALSE( host->basic_publish("noq",&bp,"Hi") );
}

/* ---------- S3 FIFO 顺序 ---------- */
TEST_F(PtpFixture, FifoOrder)
{
    publish("m1"); publish("m2"); publish("m3");
    EXPECT_EQ(host->basic_consume("q1")->payload().body(), "m1");
    EXPECT_EQ(host->basic_consume("q1")->payload().body(), "m2");
    EXPECT_EQ(host->basic_consume("q1")->payload().body(), "m3");
}

/* ---------- S4 ACK 删除 ---------- */
TEST_F(PtpFixture, AckRemove)
{
    publish("m1");
    auto m = host->basic_consume("q1");
    ASSERT_NE(m, nullptr);
    std::string id = m->payload().properties().id();   // ! insert() 里暂未赋值，可自行生成
    host->basic_ack("q1", id);
    EXPECT_EQ(host->basic_consume("q1"), nullptr);
}

/* ---------- S5 错路由 ---------- */
TEST_F(PtpFixture, RoutingKeyMismatch)
{
    publish("bad", "otherKey");     // not match 'q1'
    EXPECT_EQ(host->basic_consume("q1"), nullptr);
}

/* ---------- S6 并发 ---------- */
TEST_F(PtpFixture, ConcurrentPublish)
{
    constexpr int N = 1000;
    std::vector<std::thread> ths;
    for(int i=0;i<N;++i)
        ths.emplace_back([&,i]{ publish("msg"+std::to_string(i)); });
    for(auto& t:ths) t.join();
    EXPECT_EQ( host->all_queues().at("q1")->durable, false );   // 随手测个属性
    EXPECT_EQ( host->basic_consume("q1")->payload().body().substr(0,3), "msg");
    EXPECT_EQ( host->basic_query().empty(), false );            // 至少还有一条
}

/* ---------- S7 自动 ack ---------- */
TEST_F(PtpFixture, AutoAck)
{
    // 建 consumer，auto_ack=true
    auto cb = [](const std::string&, const BasicProperties*, const std::string&) {};
    cmp->create("tag1","q1",true,cb);
    publish("one");
    // 模拟 channel::consume()  (不走线程池简化)
    message_ptr mp = host->basic_consume("q1");
    ASSERT_NE(mp,nullptr);
    host->basic_ack("q1", mp->payload().properties().id());
    EXPECT_EQ( host->basic_consume("q1"), nullptr );
}


/* ---------- E1 route.hpp ★ topic / fanout 逻辑 ---------- */
TEST(RouteMatch, TopicAndFanout)
{
    EXPECT_TRUE ( router::match_route(ExchangeType::FANOUT, "a.b", "whatever") );
    EXPECT_TRUE ( router::match_route(ExchangeType::TOPIC , "kern.cpu.load", "kern.*.*") );
    EXPECT_TRUE ( router::match_route(ExchangeType::TOPIC , "a.b.c.d"      , "a.#")     );
    EXPECT_FALSE( router::match_route(ExchangeType::TOPIC , "kern.cpu"     , "*.disk")  );
}

/* ---------- E2 queue_message ★ remove by id / front Null ---------- */
TEST(QueueMessage, InsertFrontRemove)
{
    queue_message qm(".", "q");
    BasicProperties bp;  bp.set_routing_key("rk");
    ASSERT_TRUE(qm.insert(&bp, "hello", false));
    ASSERT_NE(qm.front(), nullptr);

    std::string fake_id = "not_exist";
    qm.remove(fake_id);                   // should keep msg
    EXPECT_EQ(qm.getable_count(), 1u);

    qm.remove("");                        // remove head
    EXPECT_EQ(qm.front(), nullptr);
}

/* ---------- E3 virtual_host::publish_ex ★ fan-out ---------- */
TEST(VHostPublishEx, FanoutBroadcast)
{
    auto vh = std::make_shared<virtual_host>("vh",".","./tmp.db");
    vh->declare_exchange("fan", ExchangeType::FANOUT,false,false,{});
    vh->declare_queue("q1",false,false,false,{});
    vh->declare_queue("q2",false,false,false,{});
    vh->bind("fan","q1","");
    vh->bind("fan","q2","");

    EXPECT_TRUE( vh->publish_ex("fan","ignored", nullptr, "BCAST") );
    EXPECT_EQ( vh->basic_consume("q1")->payload().body(), "BCAST");
    EXPECT_EQ( vh->basic_consume("q2")->payload().body(), "BCAST");
}

/* ---------- E4 virtual_host::publish_ex ★ topic ---------- */
TEST(VHostPublishEx, TopicRouting)
{
    auto vh = std::make_shared<virtual_host>("vh",".","./tmp.db");
    vh->declare_exchange("topic", ExchangeType::TOPIC,false,false,{});
    vh->declare_queue("diskq",false,false,false,{});
    vh->bind("topic","diskq","kern.disk.#");

    BasicProperties bp;  bp.set_routing_key("kern.disk.sda");
    bool ok = vh->publish_ex("topic","kern.disk.sda",&bp,"disk-io");
    EXPECT_TRUE(ok);
    EXPECT_EQ( vh->basic_consume("diskq")->payload().body(), "disk-io" );
}

/* ---------- E5 declare_queue 的幂等、防重 ---------- */
TEST(VHostQueue, DeclareIdempotent)
{
    auto vh = std::make_shared<virtual_host>("vh",".","./tmp.db");
    EXPECT_TRUE(vh->declare_queue("dup",false,false,false,{}));
    EXPECT_TRUE(vh->declare_queue("dup",false,false,false,{}));   // 第二次也成功
    EXPECT_EQ( vh->all_queues().size(), 1u ); // 包括默认绑定 ""->dup
}

/* ---------- E6 thread_pool ★ push / 并发执行 ---------- */
TEST(ThreadPool, PushAndRun)
{
    thread_pool pool(4);
    std::atomic<int> sum{0};
    for(int i=0;i<100;i++)
        pool.push([&]{ sum.fetch_add(1,std::memory_order_relaxed); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(sum.load(), 100);
}



/* ---------- C2 delete_queue() & exists_queue() ---- */
TEST(VHostQueueOps, DeleteAndExists)
{
    auto vh = std::make_shared<virtual_host>("vh",".","./tmp.db");
    vh->declare_queue("bye",false,false,false,{});
    EXPECT_TRUE (vh->exists_queue("bye"));
    vh->delete_queue("bye");
    EXPECT_FALSE(vh->exists_queue("bye"));
}

/* ---------- C3 router 最边缘形态 ------------------- */
TEST(RouteMatchEdge, CornerCases)
{
    using router::match_route;
    EXPECT_TRUE (match_route(ExchangeType::TOPIC , "a..b",  "a.*.b"));   // 连续点
    EXPECT_FALSE(match_route(ExchangeType::DIRECT, "a.b",   "a.b.c"));   // 长度不同
    EXPECT_TRUE (match_route(ExchangeType::TOPIC , "a.b",   "#"));       // 单 # 全匹配
}

/* ---------- C4 thread_pool 析构路径 ---------------- */
TEST(ThreadPool, GracefulDestruct)
{
    std::atomic<int> counter{0};
    {
        thread_pool pool(2);
        for(int i=0;i<20;i++)
            pool.push([&]{ counter.fetch_add(1,std::memory_order_relaxed); });
    }   // 作用域结束触发析构，必须把 20 个任务都跑完
    EXPECT_EQ(counter.load(), 20);
}