---
title: '手动用cpp实现http(一)'
date: '2020-07-31'
description: ''
author: 'dashjay'
---

在之前的[介绍课程(零)](/post/2020/07/30/手动用cpp实现http零/)中我们说了要用六节课来实现一个HTTP，

今天的任务是 "[易]简单的Cmake的教程，选用一个Socket库并实现一个echo"。

所有的代码都在 <https://github.com/dashjay/http_demo/tree/1-cmake-socket-echo> 中

Let's do it

## 0x1 编写一个HelloWorld并尝试使用Cmake编译它

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

但是我们要尝试使用一次Cmake[^1]

``` cmake
# 指定cmake的版本，随意了，我们不会用太高级的功能
cmake_minimum_required(VERSION 3.16)

# set可以设定一个变量
set(project_name http_demo)

# 项目名称 ${xxxx} 可以导入之前的变量
project(${project_name})

set(CMAKE_CXX_COMPILER "c++") # 编译器
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 17) # cpp指定标准

# 设定编译的FLAGS
set(CMAKE_CXX_FLAGS -g -W)
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS: " "${CMAKE_CXX_FLAGS}")

# 添加一个可执行文件
add_executable(main main.cpp)
```

通常情况下我们不会再项目目录里执行 `cmake .` ，这会导致很多混乱的问题，我们通常创建一个build文件夹做这个操作

``` bash
mkdir build; cd build
cmake .. && make
./main
Hello World!
```

build目录下会产生很多文件，通常我们不用理会他们，只需要管我们的输出文件即可。

### 为什么弄这么麻烦的东西

当项目复杂到一定程度时，有大量自动化的操作可以节省我们的时间，例如我们可以针对Hello World输出生成一个测试，每次编写完程序，我们只需要make一下（如果cmake文件没有变化的话），然后运行make test来进行自动化测试（测试样例需要我们自己编写）。当然这个Cmake在本次项目中的作用也不是很能体现出优势，但是笔者认为仍然有必要学习这种自动化工具。

## 0x2 选用一个cpp的socket库

经过一些挑选，选中了一个叫[Sockpp](https://github.com/fpagliughi/sockpp)的库。这个库用法很“现代”CPP，看起来一点都不古老，十分舒适。

安装也很简单

``` bash
git clone https://github.com/fpagliughi/sockpp
cd sockpp; mkdir build; cd build
cmake .. && make && sudo make install
```

这样，sockpp就安装到你系统中了，确切的说是分别安装了头文件和依赖库到你系统的include和lib中。

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

不多废话，直接看代码（省略了一个函数的实现）

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

> 如果到这里你不知道ptr是个指针，*ptr会取指针的值，那么你应该先去了解一下有关指针的内容。（占位符，我之后会自己写一篇）。

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

## 0x3 使用Cmake进行自动编译Server

我们在Cmake中添加一个可执行文件，向之前的FLAG添加一个`-lsockpp`

```cmake
set(CMAKE_CXX_FLAGS -g -W -lsockpp)
...
add_executable(server server.cpp)
```

然后我们立刻执行

```cmake
cd build
cmake .. # 每次CMakeLists.txt变化后需要重新执行
make

# 你有可能编译成功，有可能失败（在笔者这里失败了）
make
Scanning dependencies of target server
[ 25%] Building CXX object CMakeFiles/server.dir/server.cpp.o
clang: warning: -lsockpp: 'linker' input unused [-Wunused-command-line-argument]
/Users/dashjay/CLionProjects/http_demo/server.cpp:1:10: fatal error:
      'sockpp/tcp_acceptor.h' file not found
#include "sockpp/tcp_acceptor.h"

```

推测是cmake并没有使用系统的的include目录和lib链接目录，导致的，搜索文档后手动添加这两个目录，在笔者电脑上这两个目录分别是`/usr/local/include`和`/usr/local/lib`，修改后的CMake文件像这样。(已经去掉注释了)

```cmake
cmake_minimum_required(VERSION 3.16)

set(project_name http_demo)

project(${project_name})

set(CMAKE_CXX_COMPILER "c++")
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS -g -W -lsockpp)
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS: " "${CMAKE_CXX_FLAGS}")

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_executable(main main.cpp)
add_executable(server server.cpp)
```

再次执行`cmake .. && make` 后，成功编译出`server`，执行，并且使用 `telnet` 小工具帮助我们调试。

## 0x4 总结

今天我们做了以下事情：

- 用CPP写了一个Hello World
- 学习使用Cmake运行普通的程序，添加链接库等
- 简单的Sockpp的使用

[1] Cmake可以帮助跨平台软件的安装和编译，可以通过命令生成一系列Makefile，然后进一步执行编译，测试等指令（对我们来说就这个作用，实际上功能比这个要强大的多。）
