# 测试用例

## 功能1：点对点消息发送



| ID     | 名称                          | 关键断言                           | 预期结果               |
| ------ | --------------------------- | ------------------------------ | ------------------ |
| **S1** | PublishThenConsume\_OK      | 消费到同一条 `"Hello"` 消息            | ✔ 发布、路由、入队、出队全链路成功 |
| **S2** | PublishToMissingQueue\_Fail | 返回 `false`                     | ✔ 非法队列被拒绝          |
| **S3** | FifoOrder                   | 消息顺序 m1→m2→m3                  | ✔ FIFO 语义          |
| **S4** | AckRemove                   | ack 后再 consume 得到 `nullptr`    | ✔ 确认即出队            |
| **S5** | RoutingKeyMismatch          | consume 返回空                    | ✔ 路由键不匹配不投递        |
| **S6** | ConcurrentPublish           | 1000 并发发布全部入队                  | ✔ 线程安全、没有丢失        |
| **S7** | AutoAck                     | 自动确认后队列为空                      | ✔ auto-ack 生效      |
| **E1** | TopicAndFanout              | topic - \* / # & fanout 全匹配    | ✔ 路由算法正确           |
| **E2** | InsertFrontRemove           | `remove(id)` 与 `remove("")` 行为 | ✔ 指定/队首删除正常        |
| **E3** | FanoutBroadcast             | 两队列均收到 `"BCAST"`               | ✔ fan-out 广播       |
| **E4** | TopicRouting                | 仅匹配 `kern.disk.#` 的队列收到        | ✔ topic 按段匹配       |
| **E5** | DeclareIdempotent           | 重复声明不新增                        | ✔ 幂等性              |
| **E6** | PushAndRun                  | sum==100                       | ✔ 线程池任务完成          |
| **C2** | DeleteAndExists             | 删后不存在                          | ✔ 元数据一致            |
| **C3** | CornerCases                 | topic 连续点、direct 长度不同          | ✔ 极端路由场景           |
| **C4** | GracefulDestruct            | 20 任务全部执行                      | ✔ 线程池析构阻塞          |

---

## 功能8：简单路由

| ID     | 名称              | 交换机类型    | 关键断言                                  | 预期                       |
| ------ | --------------- | -------- | ------------------------------------- | ------------------------ |
| **F1** | DirectMatch     | `DIRECT` | *routing\_key == binding\_key* 时成功投递  | 1 条消息写入 `qa`             |
| **F2** | DirectUnmatch   | `DIRECT` | key ≠ binding 时 `publish_ex` 返回 false | `qb` 无消息                 |
| **F3** | FanoutBroadcast | `FANOUT` | 不看 key，广播到全部绑定队列                      | `q1`、`q2` 均收到 “BCAST”    |
| **F4** | TopicWildcard   | `TOPIC`  | `kern.disk.#`、`kern.*.cpu` 通配段式匹配     | 仅匹配队列收到                  |
| **F5** | Unbind          | `DIRECT` | 解绑后再次发送收不到                            | `publish_ex`→false，`q` 空 |



## 功能2：消息接收

| ID     | 名称                   | 模式           | 关键断言                       | 预期           |       |       |
| ------ | -------------------- | ------------ | -------------------------- | ------------ | ----- | ----- |
| **R1** | PullSingleMessage    | Pull         | 非空队列返回 `Message`           | 正确取到 “one”   |       |       |
| **R2** | PullFromEmptyQueue   | Pull         | 空队列返回 `nullptr`            | 无消息          |       |       |
| **R3** | PullFifoOrder        | Pull         | 多次 `basic_consume` 按 FIFO  | 依次 a → b → c |       |       |
| **R4** | PushDeliver          | Push‐NACK    | callback 收到；未 ACK          | 队列仍有 1 条     |       |       |
| **R5** | PushAutoAck          | Push‐autoACK | callback 收到；auto\_ack=true | 队列为空         |       |       |
| **R6** | PushRoundRobin       | Push‐RR      | 2 个消费者轮询                   | \`           | c1-c2 | ≤ 1\` |
| **R7** | PushNoConsumerDrop   | Push         | 无消费者时 drop                 | 队列最终为空       |       |       |
| **R8** | PullThenPushSequence | Pull→Push    | 先 Pull-ACK，再 Push          | 第二条成功投递      |       |       |

---

## 功能3：队列管理

| ID     | 名称                          | 关键断言                           | 预期结果               |
| ------ | --------------------------- | ------------------------------ | ------------------ |
| **Q1** | DeclareQueue_Success        | 声明队列成功，属性正确设置                | ✔ 队列创建、属性验证成功 |
| **Q2** | DeclareQueue_Duplicate      | 重复声明返回 true，队列数量不变            | ✔ 幂等性验证          |
| **Q3** | DeleteQueue_Success         | 删除后队列不存在                      | ✔ 队列删除成功        |
| **Q4** | DeleteQueue_NonExistent     | 删除不存在的队列不崩溃                  | ✔ 异常处理正常        |
| **Q5** | GetQueueStatus_Exists       | 查询存在的队列返回正确状态               | ✔ 状态查询成功        |
| **Q6** | GetQueueStatus_NonExistent  | 查询不存在的队列返回 exists=false        | ✔ 不存在队列处理      |
| **Q7** | GetQueueStatus_EmptyArgs    | 空参数队列状态查询正常                  | ✔ 空参数处理          |
| **Q8** | QueueProperties_Durable     | durable=true 的队列属性正确            | ✔ 持久化属性验证      |
| **Q9** | QueueProperties_Exclusive   | exclusive=true 的队列属性正确           | ✔ 独占属性验证        |
| **Q10** | QueueProperties_AutoDelete  | auto_delete=true 的队列属性正确         | ✔ 自动删除属性验证    |
| **Q11** | QueueArgs_Complex           | 复杂参数正确解析和存储                  | ✔ 参数处理正确        |
| **Q12** | EmptyQueueName              | 空字符串队列名允许创建                  | ✔ 边界条件处理        |
| **Q13** | SpecialCharactersInName     | 特殊字符队列名正常处理                  | ✔ 特殊字符支持        |
| **Q14** | MsgQueueConstructor         | msg_queue 构造函数正确初始化            | ✔ 构造函数验证        |
| **Q15** | MsgQueueArgsParsing         | 参数字符串解析和重构正确                | ✔ 参数解析验证        |
| **Q16** | MsgQueueEmptyArgs           | 空参数处理正常                        | ✔ 空参数边界条件      |

---

## 功能5：消息确认
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