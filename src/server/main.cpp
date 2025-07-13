#include "broker_server.hpp"
#include "management_http.hpp"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"
#include "muduo/protoc/dispatcher.h"

int main(int argc, char* argv[]) {
    int port = 5555;
    std::string base_dir = "./data";
    if (argc >= 2) {
        port = std::atoi(argv[1]);
    }
    if (argc >= 3) {
        base_dir = argv[2];
    }
    hz_mq::BrokerServer server(port, base_dir);
    hz_mq::management_http_server http_srv(server.get_virtual_host(), 8080);
    http_srv.start();
    server.start();
    return 0;
}
