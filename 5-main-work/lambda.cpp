#include <iostream>
#include <functional>
#include "headers.h"

void split(const std::string &input, const std::function<void(std::string, std::string)> &fcn);

int main() {
    // 无参数 无返回值 的lambda表达式
    []() {
        std::cout << "hello lambda" << '\n';
    }();

    int num = 5;
    // 有参数 无返回值 的lambda表达式
    [](int a) {
        std::cout << a << '\n';
    }(num);

    // 有参数 有返回值 的lambda表达式
    auto res = [](int a) -> int {
        return ++a;
    }(num);
    std::cout << "res: " << res << '\n';

    // 拷贝捕获 列表
    auto res2 = [res]() -> std::string {
        return std::to_string(res + 1);
    }();

    std::cout << "res2: " << res2 << '\n';

    // 引用捕获 列表
    auto res3 = [&res]() -> std::string {
        res += 1;
        return std::to_string(res);
    }();
    std::cout << "res3: " << res3 << '\n';

    std::string line("Content-Length: 10086");

    Headers hdr;
    split(line, [&hdr](const std::string &key, const std::string &value) {
        hdr.set_header(key.c_str(), value);
    });

    for (auto &kv:hdr.m_hdr) {
        std::cout << kv.first << ": " << kv.second << '\n';
    }
}

void split(const std::string &input, const std::function<void(std::string, std::string)> &fcn) {
    auto colon{input.find(':')};
    if (colon == std::string::npos) {
        return;
    }
    fcn(input.substr(0, colon), input.substr(colon + 2));
}