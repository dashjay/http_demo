#include<iostream>

#include "cxxhttp.h"

int main() {
    Request req("GET", "/", "body");
    std::cout << req.to_string();

    std::cout << "\n=========split========\n";
    Response resp;
    resp.proto = "HTTP/1.1";
    resp.status = "200 OK";
    resp.body = "body";
    std::cout << resp.to_string();
}