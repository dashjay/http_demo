#include "string"
#include "sockpp/tcp_connector.h"
#include "spdlog/spdlog.h"
#include "bufio.hpp"
#include "cxxhttp.h"
#include "server.h"
#include <cpptoml.h>


ServerCfg new_config_from_file(const std::string &filename) {
    auto config = cpptoml::parse_file(filename);

    auto listen_port = config->get_qualified_as<in_addr_t>("ENV.listen_port");
    assert(*listen_port > 0 && *listen_port < 65535);

    auto target_port = config->get_qualified_as<in_addr_t>("ENV.target_port");
    assert(*target_port > 0 && *target_port < 65535);

    return ServerCfg{static_cast<in_port_t>(*listen_port),
                     static_cast<in_port_t>(*target_port)};
}

void handle_conn(sockpp::tcp_socket &&sock, Core *c) {
    try {
        bufio::BufReader<MaxBufSize> Cr(&sock);
        for (;;) {
            // allocate req
            Request req;
            // allocate resp
            Response resp;
            resp.proto = "HTTP/1.1";
            resp.status = "200 OK";

            // exchange the pointer
            req.resp = &resp;
            resp.req = &req;

            // read_request
            if (!parser::parse_request(Cr, req)) {
                spdlog::error("parse_request error");
                break;
            }

            c->HandleRequest(req);

            Cr.write(req.resp->to_string());
        }
    } catch (errors::Error &error) {
        if (error.detail == ErrNegativeRead){
            spdlog::info("handle_conn over, tcp conn closed");
            return;
        }
        spdlog::error("handle_conn error, detail: {}", error.to_string());
    }
}

Core::Core(ServerCfg &cfg) {
    this->m_port = cfg.m_listen;
    this->m_target = cfg.m_target;
}

int Core::Run() {
    sockpp::socket_initializer sockInit;
    if (!acc) {
        spdlog::error("Error creating the acceptor: {}", acc.last_error_str());
        return 1;
    }
    spdlog::info("Awaiting connections on port: {}", m_port);
    while (true) {
        sockpp::inet_address peer;
        sockpp::tcp_socket sock = acc.accept(&peer);
        if (!sock) {
            spdlog::warn("Error accepting incoming connection: {}", acc.last_error_str());
            continue;
        }

        spdlog::info("Received a connection request from {}", peer.to_string());
        try {
            std::thread thr(handle_conn, std::move(sock), this);
            thr.detach();
        } catch (std::exception &exp) {
            spdlog::info("create thread {}", exp.what());
        }
    }
}

bool Core::Listen() {
    return acc.open(sockpp::inet_address("0.0.0.0", m_port));
}

bool Core::IsRunning() {
    return acc.is_open();
}

void Core::AddHandler(const Handler &handler) {
    handlers[handler_count] = handler;
    handler_count++;
    assert(handler_count < 8);
}

void Core::HandleRequest(Request &req) {
    try {
        for (auto i{0}; i < this->handler_count; i++) {
            this->handlers[i](req);
        }
    } catch (std::bad_function_call &fcn) {
        spdlog::error("bad_function_all {}", fcn.what());
    }
}

