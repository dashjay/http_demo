#include "spdlog/spdlog.h"
#include "server.cpp"

int main(int argc, char **argv) {
    if (argc < 2) {
        spdlog::error("run as {} cfg_path", argv[0]);
        exit(-1);
    }
    auto cfg = new_config_from_file(std::string(argv[1]));

    Core core(cfg);

    core.AddHandler([](Request &req) -> bool {
        req.resp->headers.add_header("Hello", "Server");
        req.resp->headers.add_header("Content-Length", "0");
        return true;
    });

    core.AddHandler([](Request &req) -> bool {
        std::string body = "Hello Server";
        req.resp->body = body;
        req.resp->headers.set_header("Content-Length", std::to_string(body.length()));
        return false;
    });

    if (!core.Listen()) {
        spdlog::error("listen error");
        exit(-1);
    }
    core.Run();
}