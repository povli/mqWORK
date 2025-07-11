#include <gtest/gtest.h>
#include "../src/server/queue_message.hpp"
#include "../src/common/msg.pb.h"

using namespace hz_mq;

TEST(Persistence, StoreAndRecover) {
    {
        queue_message qm("./persist_data", "pq");
        BasicProperties bp;
        bp.set_delivery_mode(DeliveryMode::DURABLE);
        qm.insert(&bp, "hello", true);
        qm.insert(&bp, "world", true);
    }

    queue_message qm2("./persist_data", "pq");
    qm2.recovery();
    ASSERT_EQ(qm2.getable_count(), 2u);
    EXPECT_EQ(qm2.front()->payload().body(), "hello");
    qm2.remove("");
    EXPECT_EQ(qm2.front()->payload().body(), "world");

    std::filesystem::remove_all("./persist_data");
}
