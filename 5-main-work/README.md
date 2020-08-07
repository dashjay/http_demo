---
title: '手动用cpp实现http(五)'
date: '2020-08-06'
description: ''
author: 'dashjay'
---

创建于 2020-08-06，完成于 2020-08-07

在之前的[课程(四)](https://github.com/dashjay/http_demo/tree/master/4-bufreader)中我们讲了有关如何套用 socket 实现 bufReader，并在此基础上读取 http 请求和返回值。

今天的任务是 "[易]实现主程序逻辑，监听端口，接收请求"。

所有的代码都在 <https://github.com/dashjay/http_demo/tree/5-main-work> 中

Let's do it

## 0x1 多线程编程

作者对多线程编程了解不是很多，基本上就是在此场景下够用而已。

`multi_thread`

```cpp
#include <iostream>
#include <thread>

void thread_sleep(int second) {
    std::this_thread::sleep_for(std::chrono::seconds(second));
    std::cout << "thread exit" << '\n';
}

int main() {
    std::thread thr(thread_sleep, 5);
    thr.join();

    std::thread thr2(thread_sleep, 999);
    thr2.detach();
    std::cout << "program exit directly" << '\n';
}
```

注意几个点：

1. thread创建的时候，参数可以直接从第二个参数开始传入
2. 创建好的线程可以由两种行为
    - join 然后阻塞等待线程结束
    - detach 然后立刻回到当前执行语句下方开始执行。
    - 什么也不做，当主线程结束时，子线程未执行结束，离开作用域时，会报错 `libc++abi.dylib: terminating` 并且返回 exit_code: 6

更多的内容我也不知道了，这些知识都是从python库那边迁移过来的，更多请查询百科。

## 0x2 匿名函数

我说这是和函数你信不？

```cpp
[]() {}();
```

这确实是个函数（无返回值的lambda）可以分为三个部分。

- `[]`：我经常叫它捕获列表
- `()`: 是参数列表
- `{}`: 函数语句

> 最后一个括号是执行的意思

```cpp
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

// 捕获列表
auto res2 = [res]() -> std::string {
    return std::to_string(res + 1);
}();
std::cout << "res2: " << res2 << '\n';
```

这种lambda表达式通常是创建然后被执行，基本不存储下来，可以称为一次性函数了。

### 捕获列表

前面的捕获列表，可以捕获当前作用域内的变量，通过拷贝或者引用两种方式。
如果使用的是拷贝捕获，你不能修改那个捕获变量，你可以认为他是一个 r-value，始终是一个临时值。

如果你用的是引用捕获，你可以修改它。

```cpp
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
```

### lambda的使用场景

我一直很想不通，cpp 为什么没有split这个函数，经过很长时间的实践之后发现一小部分原因是不知道返回什么内容比较好。可以有以下方案：

- 返回一个 `std::vector<std::string>`
- 返回一个 .....

如果我们有这样一个场景，有一个字符串 `Content-Length: 10086`，然后我们可以尝试手写一个 `split` 函数，返回一对 `std::pair<std::string, std::string>`

可是我们为什么一个劲的想让他返回呢？

为什么我们不能传一个lambda进去呢？

HOW TO DO ?

```cpp
void split_header(const std::string &input, ....)
```

额，如果我们要把lambda弄成一个变量名称应该怎么办？

这时候我们就要用 `functional` 头了，为了 `lambda` 更好的储存和运输，我们可以这样使用

```cpp
#include <functional>
void split(const std::string &input, 
    const std::function<void(std::string, std::string)> &fcn) {
    auto colon{input.find(':')};
    if (colon == std::string::npos) {
        return;
    }
    fcn(input.substr(0, colon), input.substr(colon + 2));
}

int main(){
    std::string line("Content-Length: 10086");

    Headers hdr;

    split(line, [&hdr](const std::string &key, const std::string &value) {
        hdr.set_header(key.c_str(), value);
    });

    for (auto &kv:hdr.m_hdr) {
        std::cout << kv.first << ": " << kv.second << '\n';
    }
}
```

其中 `std::function<void(std::string, std::string)>` 代表无返回值，传入两个 `std::string`

有了这个利器，我已经知道咱们的handler怎么写了，你们呢？

## 0x3 HTTP handler

不知道从什么时候开始，http server 流行使用 `handler` 这种编写模式。

也就是根据路由，为HTTP请求添加handler。

例如 `add_handler(std::string, std::function<bool(Request &)>)` 这样来给对应路由添加一个处理函数。

我们本次也应该不会根据路由和方法来添加了，那样做对我们来说太复杂了，要处理很多，定义很多内容才能封装的比较优雅。

我们直接不根据任何属性，设置handler，直接设置成根handlers，我下面这么操作你们会理解的

我给Core增加了一些接口你可以尝试实现它们

```cpp
using Handler = std::function<bool(Request &)>;

class Core {
public:
    ...
    void AddHandler(const Handler &);

    void HandleRequest(Request &req);

private:
    ...
    Handler handlers[8];
    int handler_count = 0;
};
```

有了它们我们的handle_conn函数可以改写了。

```cpp
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
        spdlog::error("handle_conn error, detail: {}", error.to_string());
    }
}
```

我用最简单的方式对Server做了一个实现，写到这里我已经激动起来了，我不知道屏幕对面你是否能感受到我的激动。

我从刚刚认识CPP开始，仔细学习了基础操作，学到OOP，封装，接口，抽象的时候我以为我肯定能利用这些特性写出非常好的程序。

```cpp
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
```

结果是，大部分我学的非常仔细的特性，并没有使用就已经开发出令我满意的程序了，我们可以来给程序测试一个性能。

单次执行结果。

```cpp
$ curl localhost:8080  -i
HTTP/1.1 200 OK
Content-Length: 12
Hello: Server
server: http-demo-1

Hello Server
```

100线程并发请求执行效果。

```cpp
$ wrk -c 100 -t 100 -d 10s  http://0.0.0.0:8080
Running 10s test @ http://0.0.0.0:8080
  100 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.07ms  675.90us  24.33ms   96.78%
    Req/Sec     0.96k   332.09     9.29k    92.12%
  889998 requests in 10.10s, 73.84MB read
  Socket errors: connect 0, read 262, write 0, timeout 0
Requests/sec:  88125.52
Transfer/sec:      7.31MB
```

十秒打了80万个请求过去，错误有200多个，怀疑是 bufReader 的问题。

你完成了之前的代码了么？

## 0x4 拓展

到这里已经到达我的能力水平的极限了，我也不知道能给你什么拓展意见了。

这个教程之后的更新基本上会保持，但是不会那么频繁了，无非就是更新一些 HTTP 特殊情况之下的处理。例如 Upgrade status 101 升级到 socket.io 等等类似的内容。

我建议你好好阅读以下RFC，并且定义一份头文件，有关各种各样的 HTTP status_code 和文字描述，常见头部，特殊头部等等。

再见小伙伴们。
