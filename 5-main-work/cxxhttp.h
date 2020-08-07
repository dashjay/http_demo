#ifndef HTTP_TEST_CXXHTTP_H
#define HTTP_TEST_CXXHTTP_H

#include "headers.h"
#include "bufio.cpp"

class Response;

class Request {
public:
    std::string method;
    std::string uri;
    std::string proto;

    Headers headers;

    std::string body;
    Response *resp;


    Request() = default;

    Request(std::string &&m, std::string &&u, std::string &&b);

    Request(std::string &m, std::string &u, std::string &b);

    std::string to_string();

    size_t ContentLength() const {
        if (headers.has_header("Content-Length")) {
            return static_cast<size_t>(strtol(
                    headers.get_header_value("Content-Length").c_str(),
                    nullptr, 10));
        } else {
            return 0;
        }
    }
};

class Response {
public:
    std::string proto;
    std::string status;

    Headers headers;
    std::string body;

    int32_t status_code;
    Request *req;

    std::string to_string();

    size_t ContentLength() const {
        if (headers.has_header("Content-Length")) {
            return static_cast<size_t>(strtol(
                    headers.get_header_value("Content-Length").c_str(),
                    nullptr, 10));
        } else {
            return 0;
        }
    }
};

namespace parser {

    bool parse_response(bufio::BufReader<MaxBufSize> &buf, Response &resp);

    bool parse_response_body(bufio::BufReader<MaxBufSize> &buf, Response &resp);

    bool parse_response_line(const char *beg, const char *end, Response &resp);

    bool parse_request(bufio::BufReader<MaxBufSize> &buf, Request &req);

    bool parse_request_line(const char *beg, const char *end, Request &req);

    bool parse_hdr(bufio::BufReader<MaxBufSize> &buf, Headers &hdr);

    bool parse_single_header(const char *beg, const char *end, Headers &hdr);
}
#endif //HTTP_TEST_CXXHTTP_H
