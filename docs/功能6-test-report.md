# 死信队列功能 – 单元测试 测试报告

### 1. 目标

| 目标       | 说明                                                                                                    |
| -------- | ----------------------------------------------------------------------------------------------------- |
| **功能验证** | 验证死信队列功能在消息拒绝（NACK）场景下的正确性，包括死信队列配置、消息投递、死信消息路由等核心功能 |
| **覆盖率**  | 仅统计与死信队列直接相关的核心文件<br> *行覆盖率 ≥ 80 %*                                                    |
| **回归价值** | 作为后续死信队列功能扩展的安全网，任何破坏死信队列核心流程的变更都会被立即捕获                                                            |

---

### 2. 测试环境

| 项         | 值                                                    |
| --------- | ---------------------------------------------------- |
| **编译器**   | g++ 20 / `-std=c++20 -fprofile-arcs -ftest-coverage` |
| **测试框架**  | GoogleTest 1.14                                      |
| **覆盖率工具** | gcov + lcov 1.16                                     |
| **依赖服务**  | 无 – 全内存实现，IDE / CI 均可离线运行                            |
| **测试入口**  | `./test_dlq_test` 可执行文件（由编译命令生成）        |

---

### Google Test 接入方式

| 环节      | 关键点                                                                                                                                                                                                     | 片段/命令                         |
| ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------- |
| **依赖**  | Ubuntu 22.04 apt-get `libgtest-dev` + `cmake` 编译；或直接 `sudo apt install libgtest-dev`                                                                                                                    | –                             |
| **编译**  | 手动编译命令：<br>`g++ -std=c++20 -O0 -g -fprofile-arcs -ftest-coverage -Iinclude -Isrc/common -Isrc/server -Isrc/client -Isrc/tools/muduo/_install/include -Isrc/tools/muduo/examples -Isrc/tools -Isrc/tools/muduo/muduo -pthread test/test_dlq.cpp src/server/virtual_host.o src/server/consumer.o src/server/connection.o src/server/channel.o src/server/broker_server.o src/common/exchange.o src/common/queue.o src/common/thread_pool.o src/common/codec.o src/common/msg.pb.o src/common/protocol.pb.o -Lsrc/tools/muduo/_install/lib -lmuduo_net -lmuduo_base -lmuduo_protobuf_codec -lprotobuf -lpthread -lz -lgtest -lgtest_main -o test_dlq_test`                                                                | –                             |
| **源文件** | 全部测试集中到 `test/test_dlq.cpp`（死信队列功能测试）                                                                                                                           | –                             |
| **构建**  | 编译生成对象文件的 `.gcno/.gcda`                                                                                                                                                       | –                             |
| **运行**  | `./test_dlq_test --gtest_color=yes`                                                                                                                                                                           | 所有 19 用例须 PASS                |
| **统计**  | <br>`bash<br>lcov --capture --directory . --output-file coverage.info<br>lcov --extract coverage.info '*/virtual_host.cpp' '*/queue_message.hpp' --output-file coverage_core.info<br>`<br> | 提取核心文件覆盖率     |
| **可视化** | `genhtml coverage_core.info -o coverage_core_report` → 打开 `index.html`                                                                                                                                            | –                             |

---

### 3. 用例概览

#### 3.1 逻辑分层

| 层次             | 关键函数/类                                                    | 对应测试集合                  |
| -------------- | --------------------------------------------------------- | ----------------------- |
| 死信队列核心功能       | `virtual_host::basic_nack / declare_queue_with_dlq` | **BasicDLQFunctionality**               |
| 消息拒绝处理           | `basic_nack` 的 requeue 参数处理                                     | **NACKWithRequeue** |
| 无死信配置场景           | 普通队列的 NACK 处理                  | **QueueWithoutDLQConfig**                  |
| 交换机操作           | `declare_exchange / bind / unbind`              | **ExchangeOperations**             |
| 错误处理          | 不存在的队列/交换机操作             | **ErrorHandling**  |
| 消息属性           | `BasicProperties` 的 `delivery_mode` 等属性                                  | **MessageProperties**     |
| 交换机路由           | 不同类型交换机的路由逻辑                                  | **ExchangeRouting**     |
| 队列操作           | 不同类型队列的声明和操作                                  | **QueueOperations**     |
| 高级死信队列场景          | 多条消息的死信队列处理                                  | **DeadLetterQueueAdvanced**     |
| 附加功能           | `exists_queue / all_queues / delete_exchange` 等                                  | **AdditionalFunctions**     |
| 队列消息函数           | `queue_message` 的各种操作                                  | **QueueMessageFunctions**     |
| 边界情况           | 空ID、不存在ID等边界场景                                  | **EdgeCases**     |
| 交换机类型操作           | 所有交换机类型的测试                                  | **ExchangeTypeOperations**     |
| 队列属性           | 不同类型队列属性的测试                                  | **QueueProperties**     |
| 绑定操作           | 队列绑定的各种操作                                  | **BindingOperations**     |
| 消息投递模式           | 不同投递模式的测试                                  | **MessageDeliveryModes**     |
| 多虚拟主机           | 多个虚拟主机的隔离测试                                  | **MultipleVirtualHosts**     |
| 交换机和队列删除           | 删除操作的测试                                  | **ExchangeAndQueueDeletion**     |
| 复杂路由场景           | 复杂路由规则的测试                                  | **ComplexRoutingScenarios**     |

#### 3.2 统计范围

> 只回收下面 2 个核心路径的 `.gcda` 文件，专注于死信队列相关代码

```
src/server/virtual_host.cpp
src/server/queue_message.hpp
```

---

### 4. 详细用例

| 用例编号 | 用例名称                      | 说明                                                         |
|----------|-------------------------------|--------------------------------------------------------------|
| TC1      | BasicDLQFunctionality         | 消息被拒绝后应成功投递到死信队列，验证死信队列全链路功能正常 |
| TC2      | NACKWithRequeue               | 消息被拒绝但重新入队，验证重新入队机制正常                   |
| TC3      | QueueWithoutDLQConfig         | 无死信队列配置时，消息被拒绝后应被直接删除                   |
| TC4      | ExchangeOperations            | 验证交换机的声明、绑定、解绑等操作正常                       |
| TC5      | ErrorHandling                 | 验证对不存在的队列或交换机操作时的错误处理机制               |
| TC6      | MessageProperties             | 验证消息属性（如投递模式）的设置与处理正确                   |
| TC7      | ExchangeRouting               | 验证不同类型交换机的路由逻辑正确                             |
| TC8      | QueueOperations               | 验证不同类型队列的声明与基本操作正常                         |
| TC9      | DeadLetterQueueAdvanced       | 验证多条消息在死信队列中的处理逻辑正常                       |
| TC10     | AdditionalFunctions           | 验证各类辅助功能函数的正确性                                 |
| TC11     | QueueMessageFunctions         | 验证 `queue_message` 的各种操作正常                          |
| TC12     | EdgeCases                     | 验证空ID、不存在ID等边界情况下的系统健壮性                   |
| TC13     | ExchangeTypeOperations        | 验证所有交换机类型的声明与操作正常                           |
| TC14     | QueueProperties               | 验证不同类型队列的属性设置与处理正确                         |
| TC15     | BindingOperations             | 验证队列与交换机之间的绑定、解绑操作正常                     |
| TC16     | MessageDeliveryModes          | 验证不同消息投递模式（持久化/非持久化）的处理正确            |
| TC17     | MultipleVirtualHosts          | 验证多个虚拟主机之间的隔离机制正常                           |
| TC18     | ExchangeAndQueueDeletion      | 验证交换机和队列的删除操作正常                               |
| TC19     | ComplexRoutingScenarios       | 验证复杂路由规则下的消息路由行为正确                         |

---

### 5. 执行步骤

```bash
# 1. 生成 protobuf 文件
cd src/common
protoc --cpp_out=. msg.proto
protoc --cpp_out=. protocol.proto
cd ../..

# 2. 编译测试
g++ -std=c++20 -O0 -g -fprofile-arcs -ftest-coverage -Iinclude -Isrc/common -Isrc/server -Isrc/client -Isrc/tools/muduo/_install/include -Isrc/tools/muduo/examples -Isrc/tools -Isrc/tools/muduo/muduo -pthread test/test_dlq.cpp src/server/virtual_host.o src/server/consumer.o src/server/connection.o src/server/channel.o src/server/broker_server.o src/common/exchange.o src/common/queue.o src/common/thread_pool.o src/common/codec.o src/common/msg.pb.o src/common/protocol.pb.o -Lsrc/tools/muduo/_install/lib -lmuduo_net -lmuduo_base -lmuduo_protobuf_codec -lprotobuf -lpthread -lz -lgtest -lgtest_main -o test_dlq_test

# 3. 运行测试
./test_dlq_test

# 4. 生成覆盖率
lcov --capture --directory . --output-file coverage.info
lcov --extract coverage.info '*/virtual_host.cpp' '*/queue_message.hpp' --output-file coverage_core.info
genhtml coverage_core.info -o coverage_core_report
```

---

### 6. 测试结果

![测试结果](/docs/pic/功能6测试1.png)
![测试结果](/docs/pic/功能6测试2.png)
### 7. 核心文件覆盖率详情

![测试结果](/docs/pic/功能6覆盖率.png)

---

### 8. 结论

* 所有死信队列关键路径已被自动化验证，包括基本功能、边界情况、错误处理等场景。
* 行覆盖率超过目标 80%，满足高质量代码要求。
* 函数覆盖率达到100%，表明所有相关函数都经过了测试。
* 测试用例全部通过，死信队列功能稳定可靠。
* 文档、脚本与测试代码均已随项目提交，可直接在 CI 流程中复用。