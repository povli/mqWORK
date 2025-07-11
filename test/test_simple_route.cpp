/******************************************************************
 *  功能 8：简单路由 单元测试（GTest）
 ******************************************************************/
 #include <gtest/gtest.h>
 #include "../server/virtual_host.hpp"
 #include "../server/route.hpp"
 
 using namespace hz_mq;
 
 /* ---------- F1 direct：精确投递 ---------- */
 TEST(SimpleRoute, DirectMatch)
 {
     auto vh = std::make_shared<virtual_host>("vh",".","./tmp.db");
     vh->declare_exchange("ex", ExchangeType::DIRECT,false,false,{});
     vh->declare_queue("qa",false,false,false,{});
     vh->bind("ex","qa","keyA");
 
     BasicProperties bp; bp.set_routing_key("keyA");
     EXPECT_TRUE ( vh->publish_ex("ex","keyA",&bp,"hello") );
     EXPECT_EQ   ( vh->basic_consume("qa")->payload().body(), "hello" );
     EXPECT_EQ   ( vh->basic_consume("qa"), nullptr );   // 只有一条
 }
 
 /* ---------- F2 direct：不匹配 ---------- */
 TEST(SimpleRoute, DirectUnmatch)
 {
     auto vh = std::make_shared<virtual_host>("vh",".","./tmp.db");
     vh->declare_exchange("ex", ExchangeType::DIRECT,false,false,{});
     vh->declare_queue("qb",false,false,false,{});
     vh->bind("ex","qb","keyB");
 
     BasicProperties bp; bp.set_routing_key("XXX");
     EXPECT_FALSE( vh->publish_ex("ex","XXX",&bp,"msg") );      // 未路由到任何队列
     EXPECT_EQ   ( vh->basic_consume("qb"), nullptr );
 }
 
 /* ---------- F3 fanout：广播 ---------- */
 TEST(SimpleRoute, FanoutBroadcast)
 {
     auto vh = std::make_shared<virtual_host>("vh",".","./tmp.db");
     vh->declare_exchange("fan", ExchangeType::FANOUT,false,false,{});
     vh->declare_queue("q1",false,false,false,{});
     vh->declare_queue("q2",false,false,false,{});
     vh->bind("fan","q1","");
     vh->bind("fan","q2","");
 
     EXPECT_TRUE ( vh->publish_ex("fan","",nullptr,"BCAST") );
     EXPECT_EQ   ( vh->basic_consume("q1")->payload().body(), "BCAST" );
     EXPECT_EQ   ( vh->basic_consume("q2")->payload().body(), "BCAST" );
 }
 
 /* ---------- F4 topic：通配 ---------- */
 TEST(SimpleRoute, TopicWildcard)
 {
     auto vh = std::make_shared<virtual_host>("vh",".","./tmp.db");
     vh->declare_exchange("top", ExchangeType::TOPIC,false,false,{});
     vh->declare_queue("disk",false,false,false,{});
     vh->declare_queue("cpu" ,false,false,false,{});
     vh->bind("top","disk","kern.disk.#");
     vh->bind("top","cpu" ,"kern.*.cpu");
 
     BasicProperties bp; bp.set_routing_key("kern.disk.sda1");
     EXPECT_TRUE( vh->publish_ex("top","kern.disk.sda1",&bp,"d-io") );
     EXPECT_EQ  ( vh->basic_consume("disk")->payload().body(), "d-io" );
     EXPECT_EQ  ( vh->basic_consume("cpu"),  nullptr );               // 不该投递
 
     bp.set_routing_key("kern.main.cpu");
     EXPECT_TRUE( vh->publish_ex("top","kern.main.cpu",&bp,"c-util") );
     EXPECT_EQ  ( vh->basic_consume("cpu")->payload().body(), "c-util" );
 }
 
 /* ---------- F5 解绑后不再接收 ---------- */
 TEST(SimpleRoute, Unbind)
 {
     auto vh = std::make_shared<virtual_host>("vh",".","./tmp.db");
     vh->declare_exchange("ex", ExchangeType::DIRECT,false,false,{});
     vh->declare_queue("q",false,false,false,{});
     vh->bind("ex","q","key");
     vh->unbind("ex","q");
 
     BasicProperties bp; bp.set_routing_key("key");
     EXPECT_FALSE( vh->publish_ex("ex","key",&bp,"bye") );
     EXPECT_EQ   ( vh->basic_consume("q"), nullptr );
 }
 