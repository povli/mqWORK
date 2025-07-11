// ======================= route.hpp =======================
#pragma once
#ifdef route
#  undef route          // 彻底摆脱宏污染
#endif

#ifdef match_route
#  undef match_route
#endif

#include <string>
#include "../common/protocol.pb.h"   // ExchangeType

namespace hz_mq::router {

// 判断 routing_key 是否匹配 binding_key（根据交换机类型）
inline bool match_route(ExchangeType type,
                  const std::string& routing_key,
                  const std::string& binding_key)
{
    switch (type) {
    case ExchangeType::DIRECT:
        // 精确匹配
        return routing_key == binding_key;

    case ExchangeType::FANOUT:
        // 全量投递
        return true;

    case ExchangeType::TOPIC: {
        // rudimentary AMQP-like topic 匹配：*、# 通配
        auto matchSegments = [](const std::string& key,
                                const std::string& pattern) {
            size_t ki = 0, pi = 0;
            while (ki < key.size() && pi < pattern.size()) {
                size_t kdot = key.find('.', ki);
                size_t pdot = pattern.find('.', pi);

                std::string ks = kdot == std::string::npos
                                   ? key.substr(ki)
                                   : key.substr(ki, kdot - ki);
                std::string ps = pdot == std::string::npos
                                   ? pattern.substr(pi)
                                   : pattern.substr(pi, pdot - pi);

                if (ps != "#") {
                    if (ps != "*" && ps != ks) return false;
                }

                if (pdot == std::string::npos) {
                    // pattern 已到末尾
                    if (ps == "#") return true;   // '#' 匹配剩余全部
                }

                ki = kdot == std::string::npos ? key.size() : kdot + 1;
                pi = pdot == std::string::npos ? pattern.size() : pdot + 1;
            }
            return ki >= key.size() && pi >= pattern.size();
        };
        return matchSegments(routing_key, binding_key);
    }

    default:
        return false;
    }
}

} 