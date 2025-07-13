#pragma once
#include "virtual_host.hpp"
#include <thread>

namespace hz_mq {
class management_http_server {
public:
    management_http_server(const virtual_host::ptr& host, int port);
    void start();
private:
    void run();
    void handle_client(int client_fd);
    virtual_host::ptr __host;
    int __port;
    int __listen_fd{-1};
    std::thread __th;
};
}
