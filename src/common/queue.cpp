// ======================= queue.cpp =======================
#include "queue.hpp"

namespace hz_mq {

// ---------- msg_queue ----------
msg_queue::msg_queue(const std::string& qname, bool qdurable, bool qexclusive,
                     bool qauto_delete,
                     const std::unordered_map<std::string, std::string>& qargs)
    : name(qname),
      durable(qdurable),
      exclusive(qexclusive),
      auto_delete(qauto_delete),
      args(qargs) {}

void msg_queue::set_args(const std::string& str_args)
{
    size_t start = 0;
    while (start < str_args.size()) {
        size_t pos = str_args.find('&', start);
        std::string pair = str_args.substr(start, pos - start);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = pair.substr(0, eq);
            std::string val = pair.substr(eq + 1);
            args[key] = val;
        }
        if (pos == std::string::npos) break;
        start = pos + 1;
    }
}

std::string msg_queue::get_args() const
{
    std::string result;
    for (auto it = args.begin(); it != args.end(); ++it) {
        result += it->first + "=" + it->second;
        if (std::next(it) != args.end()) result += "&";
    }
    return result;
}

// ---------- msg_queue_mapper ----------
msg_queue_mapper::msg_queue_mapper(const std::string& dbfile)
{
    (void)dbfile; // 真实环境可在此打开 DB、建表等
}

bool msg_queue_mapper::insert(msg_queue::ptr& q)
{
    (void)q;
    return true;
}

void msg_queue_mapper::remove(const std::string& name)
{
    (void)name;
}

queue_map msg_queue_mapper::all()
{
    return {}; // 示例：启动时无持久化数据
}

// ---------- msg_queue_manager ----------
msg_queue_manager::msg_queue_manager(const std::string& dbfile)
    : __mapper(dbfile)
{
    __msg_queues = __mapper.all();
}

bool msg_queue_manager::declare_queue(const std::string& qname, bool qdurable,
                                      bool qexclusive, bool qauto_delete,
                                      const std::unordered_map<std::string, std::string>& qargs)
{
    std::unique_lock<std::mutex> lock(__mtx);

    if (__msg_queues.find(qname) != __msg_queues.end()) return true; // 已存在

    auto qptr = std::make_shared<msg_queue>(qname, qdurable, qexclusive,
                                            qauto_delete, qargs);

    if (qdurable && !__mapper.insert(qptr)) return false;

    __msg_queues[qname] = std::move(qptr);
    return true;
}

void msg_queue_manager::delete_queue(const std::string& name)
{
    std::unique_lock<std::mutex> lock(__mtx);

    auto it = __msg_queues.find(name);
    if (it == __msg_queues.end()) return;

    if (it->second->durable) __mapper.remove(name);
    __msg_queues.erase(it);
}

msg_queue::ptr msg_queue_manager::select_queue(const std::string& name)
{
    std::unique_lock<std::mutex> lock(__mtx);

    auto it = __msg_queues.find(name);
    return (it == __msg_queues.end()) ? nullptr : it->second;
}

queue_map msg_queue_manager::all()
{
    std::unique_lock<std::mutex> lock(__mtx);
    return __msg_queues;
}

bool msg_queue_manager::exists(const std::string& name)
{
    std::unique_lock<std::mutex> lock(__mtx);
    return __msg_queues.find(name) != __msg_queues.end();
}

// 新增：获取队列状态
msg_queue_manager::queue_status msg_queue_manager::get_queue_status(const std::string& name)
{
    std::unique_lock<std::mutex> lock(__mtx);
    queue_status status{};
    auto it = __msg_queues.find(name);
    if (it == __msg_queues.end()) {
        status.exists = false;
        return status;
    }
    status.exists = true;
    status.durable = it->second->durable;
    status.exclusive = it->second->exclusive;
    status.auto_delete = it->second->auto_delete;
    status.args = it->second->args;
    return status;
}

} 
