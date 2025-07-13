#include "management_http.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace hz_mq {

management_http_server::management_http_server(const virtual_host::ptr& host, int port)
    : __host(host), __port(port) {}

void management_http_server::start() {
    __th = std::thread(&management_http_server::run, this);
    __th.detach();
}

void management_http_server::run() {
    __listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    ::setsockopt(__listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(__port);
    ::bind(__listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::listen(__listen_fd, 16);
    while (true) {
        int cfd = ::accept(__listen_fd, nullptr, nullptr);
        if (cfd >= 0) {
            handle_client(cfd);
            ::close(cfd);
        }
    }
}

void management_http_server::handle_client(int client_fd) {
    char buf[4096];
    ssize_t n = ::read(client_fd, buf, sizeof(buf)-1);
    if (n <= 0) return;
    buf[n] = '\0';
    std::string req(buf, n);
    auto pos1 = req.find(' ');
    if (pos1 == std::string::npos) return;
    auto pos2 = req.find(' ', pos1 + 1);
    if (pos2 == std::string::npos) return;
    std::string method = req.substr(0, pos1);
    std::string path = req.substr(pos1 + 1, pos2 - pos1 - 1);

    if (method == "GET" && path.rfind("/queues/",0) == 0 && path.size() > 8 &&
        path.find("/stats", 8) != std::string::npos) {
        std::string qname = path.substr(8, path.size() - 8 - 6);
        auto qstat = __host->queue_runtime_stats(qname);
        bool exists = __host->exists_queue(qname);
        char body[256];
        std::snprintf(body, sizeof(body),
                      "{\"exists\":%s,\"depth\":%zu,\"file_size\":%zu,\"invalid_ratio\":%.3f}",
                      exists?"true":"false", qstat.depth, qstat.file_size, qstat.invalid_ratio);
        char header[256];
        std::snprintf(header, sizeof(header),
                      "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n",
                      std::strlen(body));
        ::write(client_fd, header, std::strlen(header));
        ::write(client_fd, body, std::strlen(body));
    } else if (method == "POST" && path.rfind("/queues/",0) == 0 && path.size() > 8 &&
               path.find("/compact", 8) != std::string::npos) {
        std::string qname = path.substr(8, path.size() - 8 - 8);
        __host->compact_queue(qname);
        const char* body = "{\"ok\":true}";
        char header[128];
        std::snprintf(header, sizeof(header),
                      "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n",
                      std::strlen(body));
        ::write(client_fd, header, std::strlen(header));
        ::write(client_fd, body, std::strlen(body));
    } else {
        const char* body = "Not Found";
        char header[128];
        std::snprintf(header, sizeof(header),
                      "HTTP/1.1 404 Not Found\r\nContent-Length: %zu\r\n\r\n",
                      std::strlen(body));
        ::write(client_fd, header, std::strlen(header));
        ::write(client_fd, body, std::strlen(body));
    }
}

} // namespace hz_mq
