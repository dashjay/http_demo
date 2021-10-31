#include "cxxhttp.h"

#include <sstream>

Request::Request(
        std::string &&m,
        std::string &&u,
        std::string &&b) : method{m},
                           uri{u},
                           body{b},
                           proto{"HTTP/1.1"} {

}

Request::Request(
        std::string &m,
        std::string &u,
        std::string &b) : method{m},
                          uri{u},
                          body{b},
                          proto{"HTTP/1.1"} {

}

std::string Request::to_string() {
    std::ostringstream out;
    out << this->method << ' ' << this->uri << ' ' << this->proto << "\r\n";
    for (auto &kv:this->headers.hdr()) {
        out << kv.first << ": " << kv.second << "\r\n";
    }
    if (!this->body.empty()) {
        out << "\n" << this->body;
    }
    return out.str();
}

std::string Response::to_string() {
    std::ostringstream out;
    out << this->proto << ' ' << this->status << "\r\n";
    for (auto &kv:this->headers.hdr()) {
        out << kv.first << ": " << kv.second << "\r\n";
    }
    if (!this->body.empty()) {
        out << "\n" << this->body;
    }
    return out.str();
}
