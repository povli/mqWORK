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

TEST(Persistence, NonDurableNotStored) {
    {
        queue_message qm("./persist_data2", "ndq");
        BasicProperties bp;
        bp.set_delivery_mode(DeliveryMode::UNDURABLE);
        qm.insert(&bp, "temp", false);
    }
    queue_message qm2("./persist_data2", "ndq");
    qm2.recovery();
    EXPECT_EQ(qm2.getable_count(), 0u);
    std::filesystem::remove_all("./persist_data2");
}

TEST(Persistence, RemoveDurableById) {
    {
        queue_message qm("./persist_data3", "rq");
        BasicProperties bp;
        bp.set_delivery_mode(DeliveryMode::DURABLE);
        bp.set_id("a");
        qm.insert(&bp, "msg1", true);
        bp.set_id("b");
        qm.insert(&bp, "msg2", true);
        qm.remove("a");
    }
    queue_message qm2("./persist_data3", "rq");
    qm2.recovery();
    ASSERT_EQ(qm2.getable_count(), 1u);
    EXPECT_EQ(qm2.front()->payload().properties().id(), "b");
    std::filesystem::remove_all("./persist_data3");
}

TEST(Persistence, RecoveryWithoutFile) {
    std::filesystem::remove_all("./persist_data4");
    queue_message qm("./persist_data4", "q");
    qm.recovery();
    EXPECT_EQ(qm.getable_count(), 0u);
    std::filesystem::remove_all("./persist_data4");
}

TEST(Persistence, RecoveryOrder) {
    {
        queue_message qm("./persist_data5", "q");
        BasicProperties bp;
        bp.set_delivery_mode(DeliveryMode::DURABLE);
        bp.set_id("1");
        qm.insert(&bp, "m1", true);
        bp.set_id("2");
        qm.insert(&bp, "m2", true);
        bp.set_id("3");
        qm.insert(&bp, "m3", true);
    }
    queue_message qm2("./persist_data5", "q");
    qm2.recovery();
    ASSERT_EQ(qm2.getable_count(), 3u);
    EXPECT_EQ(qm2.front()->payload().body(), "m1");
    qm2.remove("");
    EXPECT_EQ(qm2.front()->payload().body(), "m2");
    qm2.remove("");
    EXPECT_EQ(qm2.front()->payload().body(), "m3");
    std::filesystem::remove_all("./persist_data5");
}

TEST(Persistence, FrontOnEmpty) {
    queue_message qm("./persist_data6", "q");
    EXPECT_EQ(qm.front(), nullptr);
    std::filesystem::remove_all("./persist_data6");
}

TEST(Persistence, RemoveHeadAfterRecovery) {
    {
        queue_message qm("./persist_data7", "q");
        BasicProperties bp;
        bp.set_delivery_mode(DeliveryMode::DURABLE);
        bp.set_id("x");
        qm.insert(&bp, "first", true);
        bp.set_id("y");
        qm.insert(&bp, "second", true);
    }
    queue_message qm2("./persist_data7", "q");
    qm2.recovery();
    qm2.remove("");
    ASSERT_EQ(qm2.getable_count(), 1u);
    EXPECT_EQ(qm2.front()->payload().properties().id(), "y");
    std::filesystem::remove_all("./persist_data7");
}

TEST(Persistence, DiskFullSimulation)          /* —— P8 —— */
{
    const std::string dir = "./persist_full";
    std::filesystem::create_directories(dir);

#if defined(__linux__)
    /* 1) 制造一个只有 1 KiB 的 loop-back 文件系统（不需 sudo）： */
    std::string img = dir + "/lim.img";
    ::system(("dd if=/dev/zero of=" + img + " bs=1024 count=1").c_str());
    ::system(("mkfs.ext4 -q " + img).c_str());
    std::string mnt = dir + "/mnt";
    std::filesystem::create_directories(mnt);
    ::system(("sudo mount -o loop,ro " + img + " " + mnt).c_str());   // 只读挂载

    bool insert_ok = true;
    try {
        queue_message qm(mnt, "q");            // 落在只读文件系统
        BasicProperties bp; bp.set_delivery_mode(DeliveryMode::DURABLE);
        insert_ok = qm.insert(&bp, std::string(2048, 'x'), true); // >1 KiB
    } catch (...) { insert_ok = false; }

    EXPECT_FALSE(insert_ok);                   // ➜ 应该写盘失败

    ::system(("sudo umount " + mnt).c_str());
#else
    /* 非 Linux：以“目录只读”方式模拟 */
    std::filesystem::permissions(dir,
            std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_exec ,
            std::filesystem::perm_options::replace);

    bool insert_ok = true;
    try {
        queue_message qm(dir, "q");
        BasicProperties bp; bp.set_delivery_mode(DeliveryMode::DURABLE);
        insert_ok = qm.insert(&bp, "dummy", true);   // 预计抛异常 / 返回 false
    } catch (...) { insert_ok = false; }

    EXPECT_FALSE(insert_ok);
#endif
    std::filesystem::remove_all(dir);
}