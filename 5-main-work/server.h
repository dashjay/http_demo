#include "string"
#include "sockpp/tcp_connector.h"
#include "sockpp/tcp_acceptor.h"
#include "spdlog/spdlog.h"
#include "bufio.cpp"
#include "cxxhttp.h"
#include <cpptoml.h>
#include <functional>

struct ServerCfg {
    in_port_t m_listen;
    in_port_t m_target;
};

using Handler = std::function<bool(Request &)>;

ServerCfg new_config_from_file(const std::string &filename);

class Core {
public:
    explicit Core(ServerCfg &cfg);

    int Run();

    bool Listen();

    bool IsRunning();

    void AddHandler(const Handler &);

    void HandleRequest(Request &req);

private:
    in_port_t m_port;
    in_port_t m_target;
    sockpp::tcp_acceptor acc;
    Handler handlers[8];
    int handler_count = 0;
};

void handle_conn(sockpp::tcp_socket &&sock, Core *c);