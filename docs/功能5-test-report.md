# 功能5-test-reports.md

## 1. 测试目标

验证消息队列系统中**消息确认（ACK）**的正确性和健壮性，包括正常确认、错误确认、幂等性、自动确认、空队列、多队列等场景，确保消息投递与消费的可靠性。

---

## 2. 测试环境

- 操作系统：Ubuntu 22.04 x64
- 编译器：g++ 11.4.0
- 测试框架：Google Test 1.14.0
- 覆盖率工具：lcov 2.0
- 依赖库：muduo、protobuf
- 测试数据目录：`./test_ack_data`
- 相关源码目录：`src/common/virtual_host.cpp`、`src/common/queue.cpp`、`src/common/queue_message.cpp` 等

---

## 3. 测试用例设计

| 用例编号 | 用例名称             | 说明                                                         |
|----------|---------------------|--------------------------------------------------------------|
| TC1      | AckAfterConsume     | 消费消息后进行ACK，消息应被正确移除                           |
| TC2      | AckWithoutConsume   | 未消费直接ACK，ACK应无效，消息仍在队列                        |
| TC3      | AckWrongMsgId       | 对不存在的消息ID进行ACK，ACK应无效，队列无变化                |
| TC4      | AckTwice            | 对同一消息重复ACK，第二次ACK应幂等，队列无异常                |
| TC5      | AutoAck             | auto_ack模式下消费消息，消息应自动确认并移除                  |
| TC6      | AckEmptyQueue       | 对空队列进行ACK，系统应无异常，队列无变化                     |
| TC7      | MultiQueueAck       | 多队列场景下分别ACK，互不影响，消息状态正确                   |

---

## 4. 测试过程与结果

### 4.1 测试执行

- 编译命令：  
  `g++ -std=c++20 -g -O2 -fprofile-arcs -ftest-coverage ... -o test/test_ack`
- 执行命令：  
  `./test/test_ack`

### 4.2 结果统计

| 用例编号 | 预期结果                   | 实际结果   | 结论   |
|----------|----------------------------|------------|--------|
| TC1      | 消息被消费并ACK后移除      | 符合预期   | 通过   |
| TC2      | 未消费直接ACK无效          | 符合预期   | 通过   |
| TC3      | 错误消息ID ACK无效         | 符合预期   | 通过   |
| TC4      | 重复ACK幂等                | 符合预期   | 通过   |
| TC5      | auto_ack自动确认           | 符合预期   | 通过   |
| TC6      | 空队列ACK无异常            | 符合预期   | 通过   |
| TC7      | 多队列ACK互不影响          | 符合预期   | 通过   |

- **全部用例通过，未发现异常。**

---

## 5. 覆盖率统计

- 统计命令：  
  `lcov --capture --directory . --output-file coverage.info`  
  `genhtml coverage.info --output-directory coverage_report`
**覆盖率详情：**
![测试结果](/docs/pic/功能5覆盖率.png)


**测试执行结果：**
![测试结果](/docs/pic/功能5测试.png)
---

## 6. 结论

* 所有关键路径已被自动化验证，包括队列的创建、删除、状态查询、属性设置、参数处理等核心功能。
* 行覆盖率超过目标 80 %，达到 **98.7%**，函数覆盖率达到 **98.8%**，满足 **PR 验收标准** 中 *"测试覆盖率 > 80 %"* 的要求。
* 测试用例覆盖了正常流程、边界条件、异常情况等多种场景，确保代码的健壮性。
* 文档、脚本与测试代码均已随项目提交，可直接在 CI 流程中复用。
* 所有 7 个测试用例全部通过，无失败用例。
---

## 7. 附录

- 主要测试代码：`test/test_ack.cpp`
- 相关实现代码：`src/common/virtual_host.cpp`、`src/common/queue.cpp`、`src/common/queue_message.cpp`