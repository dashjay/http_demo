#include "cxxhttp.h"
#include "sockpp/tcp_acceptor.h"
#include "sockpp/tcp_connector.h"
#include "bufio.hpp"
#include "spdlog/spdlog.h"

#define listen_port 8081
#define target_port 8080

int main() {
    // 开始监听
    sockpp::tcp_acceptor acc(listen_port);
    spdlog::info("start listen at port {}", listen_port);

    sockpp::inet_address peer;
    // 接受连接
    for (;;) {
        sockpp::tcp_socket sock = acc.accept(&peer);
        if (!sock) {
            spdlog::error("accept error");
            continue;
        }
        spdlog::info("accept a conn from {}", peer.address());

        bufio::BufReader<MaxBufSize> Src(&sock);
        Request req{};
        if (!parser::parse_request(Src, req)) {
            spdlog::info("read request error");
            continue;
        }
        spdlog::info("read request success {}", req.to_string());

        sockpp::tcp_connector conn;
        if (!conn.connect(sockpp::inet_address("0.0.0.0", target_port))) {
            return -1;
        }
        auto c_sock = static_cast<sockpp::tcp_socket>(std::move(conn));
        if (!c_sock) {
            spdlog::info("connect target server error");
            continue;
        }
        bufio::BufReader<MaxBufSize> Dst(&c_sock);
        spdlog::info("write request to target");
        Dst.write(req.to_string());
        Response resp{};
        parser::parse_response(Dst, resp);
        spdlog::info("write back to client");
        Src.write(resp.to_string());
    }
}