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

## 功能10：发布订阅
| ID     | 名称                          | 关键断言                           | 预期结果               |
| ------ | --------------------------- | ------------------------------ | ------------------ |
| **P1** | FanoutBroadcast             | 两个队列都收到广播消息                  | ✔ FANOUT 交换机广播成功 |
| **P2** | FanoutNoBinding             | 无绑定队列时消息被丢弃                  | ✔ 无绑定不投递          |
| **P3** | FanoutDynamicBinding       | 动态绑定后新队列收到消息                | ✔ 动态绑定生效          |
| **P4** | TopicSubscription           | 主题匹配的队列收到消息                  | ✔ TOPIC 交换机路由正确 |
| **P5** | TopicWildcardMatching      | `kern.#` 匹配 `kern` 消息        | ✔ 通配符匹配正确        |
| **P6** | TopicNoMatch                | 不匹配的队列收不到消息                  | ✔ 路由键过滤生效        |
| **P7** | TopicMultiLevelMatching    | 多级主题匹配正确                      | ✔ 复杂路由场景          |
| **P8** | ConcurrentPubSub            | 并发发布订阅无冲突                     | ✔ 线程安全            |
| **P9** | MessagePersistence          | 消息持久化正确                       | ✔ 消息不丢失          |
| **P10** | EdgeCases                   | 边界条件处理正确                      | ✔ 异常场景健壮         |
| **R1** | DirectExactMatch            | 直接交换机精确匹配                     | ✔ DIRECT 路由正确     |
| **R2** | FanoutAlwaysMatch           | 扇出交换机总是匹配                     | ✔ FANOUT 总是广播     |
| **R3** | TopicWildcardMatch          | 主题通配符匹配                        | ✔ TOPIC 通配符正确    |
| **E1** | DeclareAndDelete            | 交换机声明和删除                      | ✔ 交换机生命周期管理     |
| **E2** | BindAndUnbind               | 队列绑定和解绑                        | ✔ 绑定关系管理         |
| **E3** | NonExistentExchange         | 不存在的交换机处理                     | ✔ 错误处理正确         |
| **E4** | InvalidOperations           | 无效操作处理                         | ✔ 异常场景处理         |
| **E5** | TypeConsistency             | 交换机类型一致性                       | ✔ 类型验证正确         |
| **E6** | SetAndGetArgs               | 交换机参数设置和获取                   | ✔ 参数管理功能         |
| **E7** | InsertAndRemove             | 交换机映射器操作                      | ✔ 内部映射管理         |
| **E8** | AllFunctions                | 交换机管理器全功能                     | ✔ 管理器功能完整       |
| **E9** | AllList                     | 获取所有交换机列表                     | ✔ 列表管理功能         |
| **E10** | InsertRemoveAll             | 插入删除和获取所有                     | ✔ 完整生命周期         |
| **E11** | EdgeCases                   | 边界条件测试                         | ✔ 边界场景处理         |

---

## 功能6：死信队列
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

## 功能11：消息过滤
| ID     | 名称                          | 关键断言                           | 预期结果               |
| ------ | --------------------------- | ------------------------------ | ------------------ |
| **H1** | HeadersExchangeBasic      | 消息头匹配时投递成功            | ✔ 基本Headers过滤功能正常 |
| **H2** | HeadersExchangeAnyMode | any模式下至少一个属性匹配即可 | ✔ any模式匹配逻辑正确 |
| **H3** | HeadersExchangeAllMode | all模式下所有属性都必须匹配 | ✔ all模式匹配逻辑正确 |
| **H4** | HeadersExchangeNoBindingArgs | 无绑定参数时匹配所有消息 | ✔ 空绑定参数处理正确 |
| **H5** | HeadersExchangeComplexMatching | 复杂多属性匹配场景 | ✔ 复杂匹配场景正常 |
| **H6** | HeadersExchangeEmptyHeaders | 空消息头处理 | ✔ 边界情况处理正确 |
| **H7** | HeadersExchangeNoXMatch | 无x-match参数处理 | ✔ 默认all模式生效 |
| **H8** | HeadersExchangeCaseSensitive | 大小写敏感匹配 | ✔ 大小写敏感处理正确 |
| **H9** | HeadersExchangeMultiQueueComplex | 多队列复杂匹配 | ✔ 多队列场景正常 |
| **H10** | HeadersExchangeComplex | 复杂Headers匹配测试 | ✔ 复杂场景覆盖完整 |
| **R1** | RouteMatchingDirect | Direct Exchange精确匹配 | ✔ Direct路由正确 |
| **R2** | RouteMatchingFanout | Fanout Exchange全量投递 | ✔ Fanout广播正确 |
| **R3** | RouteMatchingTopic | Topic Exchange通配符匹配 | ✔ Topic路由正确 |
| **R4** | RouteMatchingTopicComplex | 复杂Topic匹配场景 | ✔ 复杂Topic场景正确 |
| **V1** | VirtualHostExchangeOperations | 交换机声明、选择、删除 | ✔ 交换机操作正常 |
| **V2** | VirtualHostQueueOperations | 队列声明、存在性检查、删除 | ✔ 队列操作正常 |
| **V3** | VirtualHostBindingOperations | 绑定、解绑、获取绑定 | ✔ 绑定操作正常 |
| **V4** | VirtualHostMessageOperations | 消息发布、消费、确认 | ✔ 消息操作正常 |
| **V5** | VirtualHostBasicQuery | 基本查询功能 | ✔ 查询功能正常 |
| **V6** | VirtualHostConsumeAndRemove | 消费并移除消息 | ✔ 消费移除正常 |
| **V7** | VirtualHostErrorHandling | 错误处理场景 | ✔ 错误处理正确 |
| **V8** | VirtualHostMessageIdGeneration | 消息ID自动生成 | ✔ ID生成正确 |
| **V9** | VirtualHostExchangePublishAllTypes | 所有类型交换机发布 | ✔ 多类型发布正常 |
| **V10** | VirtualHostDeclareQueueWithDLQ | 带死信队列的队列声明 | ✔ 死信队列声明正常 |
| **V11** | VirtualHostBasicNackWithDLQ | 带死信队列的消息拒绝 | ✔ 死信队列处理正确 |
| **V12** | VirtualHostBasicNackRequeue | 消息拒绝重新入队 | ✔ 重新入队正常 |
| **V13** | VirtualHostBasicNackNoDLQ | 无死信队列的消息拒绝 | ✔ 直接删除正确 |
| **V14** | VirtualHostPublishToExchangeNoBindings | 发布到无绑定交换机 | ✔ 无绑定处理正确 |
| **V15** | VirtualHostPublishToExchangeWithHeaders | 带Headers的交换机发布 | ✔ Headers发布正常 |
| **D1** | VirtualHostMessageIdGenerationMultiple | 多次消息ID生成 | ✔ ID唯一性正确 |
| **D2** | VirtualHostExchangePublishAllTypesComplex | 复杂交换机发布场景 | ✔ 复杂发布正常 |
| **I1** | MessagePropertiesComplete | 完整消息属性测试 | ✔ 消息属性完整 |
| **I2** | RouteMatchingHeadersComplex | 复杂Headers路由匹配 | ✔ 复杂路由正确 |
---