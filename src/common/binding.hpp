// ======================= binding.hpp =======================
#pragma once

#include <string>
#include <memory>
#include <unordered_map>

namespace micromq {

// 交换机与队列的绑定关系
struct binding {
    using ptr = std::shared_ptr<binding>;

    std::string exchange_name;
    std::string queue_name;
    std::string binding_key;
    std::unordered_map<std::string, std::string> binding_args;  // 绑定参数，用于Headers Exchange过滤

    binding(const std::string& ex,
            const std::string& q,
            const std::string& key)
        : exchange_name(ex), queue_name(q), binding_key(key) {}
    
    binding(const std::string& ex,
            const std::string& q,
            const std::string& key,
            const std::unordered_map<std::string, std::string>& args)
        : exchange_name(ex), queue_name(q), binding_key(key), binding_args(args) {}
};

// 对某个交换机来说：队列名 → 绑定信息
using msg_queue_binding_map = std::unordered_map<std::string, binding::ptr>;

} 


