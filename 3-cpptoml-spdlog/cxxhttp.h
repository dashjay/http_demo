#ifndef HTTP_TEST_CXXHTTP_H
#define HTTP_TEST_CXXHTTP_H

#include "headers.h"

class Request {
public:
    std::string method;
    std::string uri;
    std::string proto;

    Headers headers;

    std::string body;

    Request() = default;

    Request(std::string &&m, std::string &&u, std::string &&b);

    Request(std::string &m, std::string &u, std::string &b);

    std::string to_string();
};

class Response {
public:
    std::string proto;
    std::string status;

    Headers headers;
    std::string body;

    int32_t status_code;

    std::string to_string();
};

#endif //HTTP_TEST_CXXHTTP_H
