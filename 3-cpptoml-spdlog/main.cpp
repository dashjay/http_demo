#include<iostream>
#include "spdlog/spdlog.h"
#include "cxxhttp.h"
#include "cpptoml.h"

int main() {
    auto config = cpptoml::parse_file("./config.toml");
    auto port = config->get_qualified_as<in_addr_t>("ENV.port");
    std::cout << "server will start at port " << *port << '\n';

    Request req("GET", "/", "body");

    spdlog::info("request: {}", req.to_string());

    Response resp;
    resp.proto = "HTTP/1.1";
    resp.status = "200 OK";
    resp.body = "body";
    
    spdlog::info("response: {}", resp.to_string());
}