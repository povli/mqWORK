/********************************************************************
 *  test_conn_mgr.cpp ―– TCP 连接管理 & 心跳检测 单元测试
 *  g++ … -lgtest -lpthread
 *******************************************************************/
#include <gtest/gtest.h>

// ① 先屏蔽真头:
#define MUDUO_NET_TCPCONNECTION_H
namespace muduo::net {
class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
    void shutdown() { closed = true; }
    bool closed{false};
};
} // namespace muduo::net

// ② 再正常包含业务头
#include "../server/connection.hpp"
#include "../server/connection_manager.hpp"
#include "../common/thread_pool.hpp"

using namespace hz_mq;

/* ------- ❶ Stub 版 TcpConnection （仅提供 shutdown()） -------- */
namespace muduo { namespace net {
class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
    void shutdown() { closed = true; }
    bool closed{false};
};
}} // namespace muduo::net

/* ------- ❷ 公共构造工具 -------------------------------------- */
struct ConnEnv
{
    virtual_host::ptr      host  { std::make_shared<virtual_host>("vh",".","./tmp.db") };
    consumer_manager::ptr  cmp   { std::make_shared<consumer_manager>()               };
    thread_pool::ptr       pool  { std::make_shared<thread_pool>(0)                   };
    connection_manager::ptr mgr  { std::make_shared<connection_manager>()             };

    muduo::net::TcpConnectionPtr newStub()
    {   return std::make_shared<muduo::net::TcpConnection>(); }
};

/* ======================================================================
 *  T1 新建 / 查询
 * ====================================================================*/
TEST(ConnectionMgr, NewAndSelect_OK)
{
    ConnEnv e;
    auto c = e.newStub();

    e.mgr->new_connection(e.host,e.cmp,nullptr,c,e.pool);
    EXPECT_NE( e.mgr->select_connection(c), nullptr );
}

/* ======================================================================
 *  T2 重复 new_connection 不应该插入重复项
 * ====================================================================*/
TEST(ConnectionMgr, DuplicateIgnored)
{
    ConnEnv e;
    auto c = e.newStub();

    e.mgr->new_connection(e.host,e.cmp,nullptr,c,e.pool);
    auto first = e.mgr->select_connection(c);
    /* 再来一次：应仍返回同一对象而不是新建 */
    e.mgr->new_connection(e.host,e.cmp,nullptr,c,e.pool);
    auto second = e.mgr->select_connection(c);

    EXPECT_EQ(first, second);
}

/* ======================================================================
 *  T3 delete_connection 移除成功
 * ====================================================================*/
TEST(ConnectionMgr, DeleteConnection)
{
    ConnEnv e;
    auto c = e.newStub();

    e.mgr->new_connection(e.host,e.cmp,nullptr,c,e.pool);
    e.mgr->delete_connection(c);
    EXPECT_EQ( e.mgr->select_connection(c), nullptr );
}

/* ======================================================================
 *  T4 expired / refresh 逻辑
 * ====================================================================*/
TEST(Connection, RefreshAndExpired)
{
    ConnEnv e;
    auto c = e.newStub();
    e.mgr->new_connection(e.host,e.cmp,nullptr,c,e.pool);

    auto ctx = e.mgr->select_connection(c);
    ASSERT_NE(ctx, nullptr);

    /* 立刻检查，应未过期 */
    EXPECT_FALSE( ctx->expired(std::chrono::seconds(0)) );

    /* 睡 2ms 后，以 0s 阈值判过期 ⇒ true */
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE( ctx->expired(std::chrono::seconds(0)) );

    /* 调用 refresh，再次检查应恢复为未过期 */
    ctx->refresh();
    EXPECT_FALSE( ctx->expired(std::chrono::seconds(0)) );
}

/* ======================================================================
 *  T5 check_timeout() 触发 shutdown()
 * ====================================================================*/
TEST(ConnectionMgr, TimeoutClose)
{
    ConnEnv e;
    auto c = e.newStub();
    e.mgr->new_connection(e.host,e.cmp,nullptr,c,e.pool);

    /* 立刻以 0s 超时检测 ⇒ 直接判过期并调用 shutdown() */
    e.mgr->check_timeout(std::chrono::seconds(0));
    EXPECT_TRUE( c->closed );
}

/* ======================================================================
 *  T6 心跳刷新（refresh_connection）防止被踢
 * ====================================================================*/
TEST(ConnectionMgr, HeartbeatRefresh)
{
    ConnEnv e;
    auto c = e.newStub();
    e.mgr->new_connection(e.host,e.cmp,nullptr,c,e.pool);

    /* 先睡 2ms，此时若直接检测超时会被关闭 */
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    /* 模拟 heartbeat： */
    e.mgr->refresh_connection(c);

    /* 以 0s 阈值检测，不应关闭 */
    e.mgr->check_timeout(std::chrono::seconds(0));
    EXPECT_FALSE( c->closed );
}
