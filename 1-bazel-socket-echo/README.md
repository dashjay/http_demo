---
title: '手动用cpp实现http(一)'
date: '2020-07-31'
description: ''
author: 'dashjay'
---

创建于 2020-07-31，最终修订于 2020-08-06

在之前的[介绍课程(零)](https://github.com/dashjay/http_demo)中我们说了要用六节课来实现一个HTTP，

今天的任务是 "[易]简单的 Bazel 的教程，选用一个Socket库并实现一个 echo"。

所有的代码都在 <https://github.com/dashjay/http_demo/tree/master/1-cmake-socket-echo> 中

Let's do it

## 0x1 编写一个HelloWorld并尝试使用 Bazel 编译它

``` cpp
#include<iostream>

int main(){
    std::cout << "Hello World!" << '\n';
}
```

我们本可以使用如下命令进行编译并且运行

``` bash
g++ -o main main.cpp
./main
Hello World!

```

但是我们要尝试使用一次 Bazel[1]

``` python
cc_binary(
    name = "hello-world",
    srcs = ["main.cpp"]
)
```

``` bash
# 执行 bazel 的根目录，需要创建一个空文件叫做 WORKSPACE
$ bazel run //1-cmake-socket-echo:hello-world
INFO: Invocation ID: e86403fc-209c-4115-9d28-333fb8abfa22
INFO: Analyzed target //1-cmake-socket-echo:hello-world (14 packages loaded, 61 targets configured).
INFO: Found 1 target...
Target //1-cmake-socket-echo:hello-world up-to-date:
  bazel-bin/1-cmake-socket-echo/hello-world
INFO: Elapsed time: 19.948s, Critical Path: 2.25s
INFO: 8 processes: 6 internal, 2 darwin-sandbox.
INFO: Build completed successfully, 8 total actions
INFO: Build completed successfully, 8 total actions
Hello World!
```

### 为什么弄这么麻烦的东西

[bazel C++ 新手教程](https://docs.bazel.build/versions/4.2.1/tutorial/cpp.html)

当项目复杂到一定程度时，bazel 可以帮助我们实现自动化，减小后期维护过程中修改构建脚本的成本。

## 0x2 选用一个 cpp 的 socket 库

经过一些挑选，选中了一个叫 [Sockpp](https://github.com/fpagliughi/sockpp)的库。这个库用法很“现代”CPP，十分舒适，有很多上古socket库，用法或者代码比较古老了，我就只推荐这个了。

安装也很简单

``` bash
# git clone https://github.com/fpagliughi/sockpp
# cd sockpp; mkdir build; cd build
# cmake .. && make && sudo make install

# 但是因为我们在 bazel 中，于是不再这么安装了
# 我们在 WORKSPACE 文件中编写一些 starlack 就可以集成。
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
http_archive(
    name = "libsockpp",
    urls = ["https://github.com/fpagliughi/sockpp/archive/refs/tags/v0.7.tar.gz"],
    sha256 = "5cbf593f534fef5e12a4aff97498f0917bbfcd67d71c7b376a50c92b9478a86b",
    strip_prefix = "sockpp-0.7",
    build_file_content = """
cc_library(
    name = "sockpp", 
    srcs = glob(["src/*.cpp"]), 
    hdrs = glob(["include/sockpp/*.h"]),
    includes = ["include"],
    visibility = ["//visibility:public"], 
)
"""
)

```

这样，sockpp 就能再构建的过程中直接引用了。

### 1. 了解sockpp的使用

我们在本节课中暂时只简单的使用 tcp 接收一些数据，因此我们在本节课程里只引入 sockpp 的一个头部 `#include "sockpp/tcp_acceptor.h"`
这样几行代码，就可以指定端口绑定，监听，等待连接的到来。

``` cpp
sockpp::tcp_acceptor acc(port);
if (!acc){
    std::cerr << acc.last_error_str() << '\n';
    exit(1)
}
```

使用accept函数就可以和到来的连接创建一个socket链接，

``` cpp
sockpp::tcp_socket sock = acc.accept();
```

### 2. 写一个Echo

不多废话，直接看代码

``` cpp
#include "sockpp/tcp_acceptor.h"
#include <iostream>

void print_without_crlf(char *ptr);

int main(){
    // 指定监听端口
    int16_t port = 8080;
    // 使用端口启动监听
    sockpp::tcp_acceptor acc(port);
    if (!acc){
        std::cerr << acc.last_error_str() << '\n';
        exit(1);
    }
    std::cout << "try accept a conn at port " << port << '\n'
    // 接收连接请求
    sockpp::tcp_socket sock = acc.accept();

    // 创建缓存
    char buf[1024];

    // 循环接收打印并且echo
    for(;;){
        if(auto siz = sock.read(buf, 1024); siz > 0){
            print_without_crlf(buf);
            sock.write(buf, siz);
        }else{
            // siz < 0（断开连接）会退出
            break;
        }
    }
    std::cout << "closed" << '\n';
}

// 这个函数为什么这么写稍后解释
void print_without_crlf(char *ptr){
    std::cout << "recv: ";
    for(;;){
        if(*ptr == '\r' || *ptr == '\n'){
            break;
        }
        std::cout << *ptr;
        ptr+=1;
    }
    std::cout << '\n';
}
```

### 3. Echo代码编译运行

我们首先尝试直接使用 g++ 编译这串代码

```bash
g++ -o server server.cpp
# 以下是输出
~/CLionProjects/http_demo » g++ -o server server.cpp
In file included from server.cpp:1:
In file included from /usr/local/include/sockpp/tcp_acceptor.h:48:
In file included from /usr/local/include/sockpp/acceptor.h:48:
In file included from /usr/local/include/sockpp/inet_address.h:50:
/usr/local/include/sockpp/sock_address.h:94:3: warning: 'auto'
      type specifier is a C++11 extension [-Wc++11-extensions]
                auto p = sockaddr_ptr();
                ^
/usr/local/include/sockpp/sock_address.h:116:9: error: unknown
      type name 'constexpr'
        static constexpr size_t MAX_SZ = sizeof(sockaddr_storage);
```

为什么直接编译会这样报错

- (1) C++ 版本

我不知道 g++(clang++) 默认使用什么版本，也许是 c++11 或者 c++98 但是因为 sockpp 是一个比较新的库，我们这里推荐指定 c++11 以上 c++17 当然更好了，在 C++17 中还可以使用 `if(STATEMENT;CONDITIONS){}`这种用法，美滋滋。

> 上方的有关 cpp 版本的描述不是十分专业，请自行查询更详细的资料

```text
c++03           c++2a           gnu++03         gnu++2a         iso9899:1990  
c++11           c++98           gnu++11         gnu++98         iso9899:199409
c++14           c11             gnu++14         gnu11           iso9899:1999  
c++17           c89             gnu++17         gnu89           iso9899:2011  
c++1y           c90             gnu++1y         gnu90           iso9899:2017  
c++1z           c99             gnu++1z         gnu99
```

- (2) 编译链接库

```bash
g++ -std=c++17  -o server server.cpp
# 以下是输出
Undefined symbols for architecture x86_64:
  "sockpp::inet_address::create(unsigned int, unsigned short)", referenced from:
      sockpp::inet_address::inet_address(unsigned short) in server-40d3ec.o
  "sockpp::stream_socket::read_timeout(std::__1::chrono::duration<long long, std::__1::ratio<1l, 1000000l> > const&)", referenced from:
  ....
```

在指定了版本之后，为什么还是会报错，因为sockpp不仅仅是一个头部申明，他复杂还有函数定义被编译成了动态链接库。（这里放置一个占位符，等找到教程了之后放在这里），因此我们需要添加链接库：`-lsockpp`，不出意外的话，下面的命令可以帮助你成功编译我们的server。

```bash
g++ -std=c++17  -o server server.cpp -lsockpp
./server
try accept a conn at port 8080
recv: helloworld # 看下方的telnet可以找到输入
closed
```

- (3) `print_without_crlf` 是什么? [了解即可]

CRLF其实是两个字符，回车换行的意思，他们分别是

- `(CR, ASCII 13, \r)`
- `(LF, ASCII 10, \n)`

全称是 Carriage-Return Line-Feed ，可以代表回车的东西，[这里有一个Stack Overflow的提问](https://stackoverflow.com/questions/1552749/difference-between-cr-lf-lf-and-cr-line-break-types)可以帮助你思考这个东西。

> 如果到这里你不知道ptr是个指针，*ptr 会取指针的值，那么你应该先去了解一下有关指针的内容。（占位符，我之后会自己写一篇）。

- (4) 运行结果

这串代码，编译成功后按照预期运行。我们即可尝试连接他，并且发送消息。在linux和macos下（好像window也有，只是默认隐藏），有一个小工具叫做 `telnet`。这个工具在我们前期的开发中帮助很大，建议找出来并且使用，或者搜索下载一个类似的网络测试助手，可以发起TCP链接并发送数据的小公举。

```bash
telnet 0.0.0.0 8080
Trying 0.0.0.0...
Connected to 0.0.0.0.
Escape character is '^]'.
hello world # <- 发出去的数据
hello world # <- 收到的数据
```

## 0x3 使用 Bazel 进行自动编译Server

我们在 Bazel 中添加一个可执行文件，并且添加依赖 `"@libsockpp//:sockpp"`

```python
cc_binary(
    name = "server",
    srcs = ["server.cpp"],
    deps = ["@libsockpp//:sockpp"],
    copts = ["-std=c++17"],
)
```

然后我们立刻执行

```bash
$ bazel run //1-cmake-socket-echo:server
INFO: Invocation ID: 9075ee1e-cb2d-4f51-b5b9-ea2d20c4f1a5
INFO: Analyzed target //1-bazel-socket-echo:server (1 packages loaded, 34 targets configured).
INFO: Found 1 target...
Target //1-bazel-socket-echo:server up-to-date:
  bazel-bin/1-bazel-socket-echo/server
INFO: Elapsed time: 5.252s, Critical Path: 2.28s
INFO: 18 processes: 7 internal, 11 darwin-sandbox.
INFO: Build completed successfully, 18 total actions
INFO: Build completed successfully, 18 total actions
try accept a conn at port 8080

```

## 0x4 总结

今天我们做了以下事情：

- 用CPP写了一个Hello World
- 学习使用 Bazel 运行普通的程序，添加链接库等
- 简单的 Sockpp 的使用

还有什么问题可以直接到ISSUE中提问，我或者社区里的其他小伙伴会帮你解答的。

[1] Bazel 快速可靠的测试和构建任何大小的软件 [link](https://bazel.build/)
