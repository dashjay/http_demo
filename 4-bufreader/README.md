---
title: '手动用cpp实现http(四)'
date: '2020-08-04'
description: ''
author: 'dashjay'
---


在之前的[介绍课程(三)](https://github.com/dashjay/http_demo/tree/3-cpptoml-spdlog)中我们讲了一些和配置加载与log程序的工作。

今天的任务是 "[难]定义一个bufReader类，并且使用该bufReader从TCP流中解析HTTP请求和返回体"，这是唯一一个被我标识为难的东西，其实也不难，只是相对繁琐。

所有的代码都在 <https://github.com/dashjay/http_demo/tree/4-bufreader> 中

本节课的代码，全部在上一节课的基础上

Let's do it

## 0x1 TCP基于流传输

TCP传输从不以包为单位，也就是说，一个 GET 请求或一个 POST 请求并不是一个包。也不是你所想象的（我们之前描述的那个样子）。

```cpp
GET / HTTP/1.1
Key: Value

body
```

把他画成这样只是为了方便理解，其实他是这样的：`GET / HTTP/1.1\r\nKey: Value\r\n\r\nbody`。而且，请求和请求之间没有什么界限。因此他们会是这样的：`GET / HTTP/1.1\r\nKey: Value\r\n\r\nbodyGET /...`，你在读取的过程中可能会遇得到很多奇怪的事情：

- 请求长度比你想象的长
- 请求长度比你想象的短一些
- ....

在读取过程中总会遇到各种不同的问题，读起来会很痛苦，如果不写一个reader帮助我们实现一些类似 `readline()` 或 `read_n()`的函数，这样我们起码能在读取的过程中，节省一些精力。

我们倒过来思考吧，如果我现在有了一行数据，我应该怎么提取出数据呢？

我们一起看一下这段代码，我们尝试简单的从一行 char 字符串中提取出请求的 method。

```cpp
const char *line = "GET / HTTP/1.1\r\n"
auto idx{line};

while(*idx!=' ' || *idx != '\n' || *idx != '\r'){
    idx++;
}
std::string method;
if(idx == ' '){
    method.assign(line, idx);
}else{
    // 头部parse失败
}
```

大概的方式就是在字符串中寻找空格，然后再分割字符串并且赋值到每个请求单元中。

写这类代码需要会使用C-Style字符串，指针操作等，并且常用以下方法：

- std::find(...)
- std::string.assign(...)

## 0x2 BufReader

```cpp
buf -> |G|E|T| |/| |H|T|T|P|/|1|.|1|\r|\n|.....
       |↑|
```

我们大概会这样做这件事，我们会创建一个固定长度的buf，然后通过这个buf实现一些类似于readline，或者read_n这样的操作，buf需要有以下的功能和性质

1. 它有非常高的性能
2. 功能简洁丰富
3. 能够将socket完美封装，对外不展示任何socket的属性。

要做到以上三点，我们分别会通过以下三个方式来实现：

1. 不允许进行大量的堆分配和释放，一次性实例化在栈空间。
2. 我们都不是代码大神，本次我将从golang中借鉴（抄袭），来实现简洁的功能
3. 如果buf尺寸不够用了，会提供其他方法，并且直接赋值到目标中，减少拷贝次数。

### 纯栈区操作

可以认为 `char *key = new char[n];` 这样分配的内存在堆上，操作慢，并且需要手动释放，唯一的优点就是空间大。

为了速度我们必须要在栈区分配空间，我们有两个方式，第一是通过模板(cpp的模板又可以开讲一节课了……我不立flag了)来创建，另一个是直接写死在类中，两种情况下，都必须是常量，用起来都差不多，我选择模板创建。

下面就是我认为一个 BufReader 应该有的成员。

```cpp
template<size_t siz>
class BufReader {
private:
    // use template for allocate the m_buf in stack for speed
    sockpp::tcp_socket *m_sock;
    char *m_r;
    char *m_w;
    int error_num;
    char m_buf[siz];

}
```

写成这样我需要做一些解释：本来也可以用 `std::array<char, siz>` 来取代 `char m_buf[siz]`，但是在这个类中我们完全使用栈空间，要求极速，我相信纯 char 数组应该是要快一些（在我们安全的使用下），另外 std::array 的其他功能我也用不到呀，只会是累赘。

两个指针 m_r 和 m_w 分别对应着 ***读指针** 和 **写指针**。按照原理来说 m_buf[siz] 其实也是一个指针。那么一共是三个 char 指针，他们一开始是重合的：`m_r = m_w = m_buf;`。

有新的数据写入的时候，会从 m_w 的位置开始写，并且写多少 m_w 就往后移动多少。

你没猜错，读也是一样的，从 m_r 开始读，读多少 m_r 就往后移动多少。当要读的数量 n > (m_w - m_r) 时，就需要进行填充 `fill()` 了。

让我们先来尝试使用一个填充函数吧。

### 1. fill()

该函数的作用是在在尝试 read，没数据或者有数据但是数据不够的时候调用。（下面代码包含注释写清楚了为什么这样写）

```cpp
#define MaxConsecutiveEmptyReads 100
template<size_t siz>
    void BufReader<siz>::fill() {

        // 读取过的数据，m_r 前有一段空的位置
        // 如果 m_r 不在 buf 的头部 ...
        if (m_r > m_buf) {
            // ... 计算拥有多少数据
            int dist{static_cast<int>(m_w - m_r)};
            // 如果有数据...
            if (dist > 0) {
                // ... 将他们在 buf 中顶格存放
                std::memcpy(m_buf, m_r, dist);
                m_w -= dist;
                m_r = m_buf;
            } else {
                // ... 直接归位三个指针
                m_r = m_w = m_buf;
            }
        }
        errors::Error err("BufReader", "fill", ErrFillFullBuffer);
            // 如果 buf 已经满了，还在读就会返回（这里未来应该抛出异常）
        if (static_cast<size_t>(m_w - m_buf) >= siz) {
            throw std::move(err);
        }

        // 尝试 MaxConsecutiveEmptyReads 次读取
        for (auto i{MaxConsecutiveEmptyReads}; i > 0; i--) {
            // 读取到 m_w 位置上，直接写入最大的能读取的数值
            // 因为我们是 bufReader，因此不怕多读，反正迟早要读的。
            auto n = m_sock->read(m_w, siz - (m_w - m_buf));
            // n 小于 0 已经是 socket报错了
            // 未来为抛出异常，在示例代码中也是
            // 现在为了不影响大家学习，暂时用return代替
            if (n < 0) {
                err.detail = ErrNegativeCount;
                throw std::move(err);
            }
            // m_w 指针向后偏移
            m_w += n;

            // 出错了应该返回
            if (m_sock->last_error() != 0) {
                error_num = m_sock->last_error();
                err.detail = m_sock->last_error_str();
                throw std::move(err);
            }
            // 读到东西就返回
            if (n > 0) {
                return;
            }
        }

        // 100次 没读到东西也要返回了
        err.detail = ErrNoProgress;
        throw std::move(err);
    }

```

首先说明一下，如果你不了解异常，不用担心，这个概念很简单，你可以暂停阅读并且阅读一些资料。（再次立flag，等文章全部结束了，我全文搜索flag来一个一个补全）。

抛出异常是程序，自己发现运行有错误并且无法继续的时候，抛出一个对象（一般派生自 `std::exception`）。可以通过 catch 语句来捕捉，就像 python 的 except 语句那样。

一个fill函数就是这样的，每当我们需要读取数据的时候，发现数据不够我们就先调用一次 `fill()`，然后再尝试，直到读取到为止。

> size_t read(void *buf, size_t n) 这个在sockpp中定义的函数，明明已经传入了要读的尺寸，为什么还要返回一个读到的数据长度呢？因为它这个函数的源码中描述了，它并不是尽力读取，而是只尝试读取，我们可以理解为，读了多少算多少，下面是 sockpp 源码中的函数说明：

```cpp
 /**
  * Reads from the port
  * @param buf Buffer to get the incoming data.
  * @param n The number of bytes to try to read.
  * @return The number of bytes read on success, or @em -1 on error.
  */
 virtual ssize_t read(void *buf, size_t n);
```

### read_until(char delim)

我们在HTTP的操作中比较常见的是希望能读到 `\r\n` 或 `\n` 为结束。因此希望能定义一个叫做 read_until 函数，能够在某个字符出现之前，一直读取。

这个函数你可以先尝试自己写，我会为你提供头部定义

```cpp
template<size_t siz>
std::pair<char *, char *> BufReader<siz>::read_until(char delim);

// 使用的时候
res = reader.read_until('\n')
// res.first 和 res.second 就是上方 line 的开头和尾巴
// 如果你不了解pair，也没关系，我们把它当做返回两个值的小伎俩
// res.first 代表开始读的地方
// res.second 代表第一个碰到的 '\n' 的指针
```

### readline

这个函数依赖于上方的函数，实现起来也很简单，所以我把实现直接写出来了

```cpp
template<size_t siz>
std::pair<char *, char *> BufReader<siz>::readline(){
    return this->read_until('\n');
}
```

### read_n(size_t n)

尝试读取指定长度的内容，我计划为这个函数提供两套实现（重载）。

```cpp
template<size_t siz>
std::pair<char *, char *> BufReader<siz>::read_n(size_t n);

template<size_t siz>
bool BufReader<siz>::read_n(std::string &buf, size_t n);
```

当希望读取指定长度 n 时，我们不太确定是否能在bufReader上做这个操作，因为我们不知道现在缓存里有多少内容了，也不知道 n 是否会 大于 buf总数。

我们要尽力 `read`，因此我们这里希望能提供两个函数的实现，一个是常规读取，读取n个字符长度。

另一个的实现方案是定义一个 `std::string`，当 `n > bufsize` 的时候，直接 `assign` 到 `std::string` 里然后再把 std::string 返回来。

可是这样做会进行一次拷贝，因此我建议直接把要写入的 `std::string` 作为引用传入，让内部 `bufReader` 直接从 socket 直接写到 `std::string` 内。

我想这应该是一个好办法，处理流程建议为：

1. 如果 n < 缓存数量，直接assign进去，然后返回。
2. 如果 n > 缓存数量 < bufSize，先执行 fill 到足够再执行 1
3. 如果 n > bufSize，先把缓存部分直接assign进去，剩下的部分直接从socket读取，高效。

### 暂停 + 思考

写到这里我已经不知道教程怎么写了，因为也许你没有完成这几个函数，想完成了再继续。

目前按照模块化编程的道理，我们应该写一系列的 test，并且对 bufReader类进行大量自动化测试，没问题了再继续，但是我不决定这样做，我们的最主要的目的是为了理解 HTTP 和 手动尝试一次 HTTP 编程，如果你现在没有完成 bufReader 类，你不需要太担心。

这个 bufReader 我是参考的 golang 的 bufio，它的源代码在这里 <https://github.com/golang/go/blob/master/src/bufio/bufio.go>, 自动化测试流程也在文件夹内：<https://github.com/golang/go/tree/master/src/bufio>，我准备先屏蔽这小部分内容，对下面的内容进行仔细讲解。

在底部我们有整个bufReader实现的源代码，github仓库中也有我们课程配套的代码。

## 0x3 使用 bufReader 解析 HTTP 请求

有时候要有勇气，大胆往前走，如果你因为什么东西卡主了，多半是肺热，吃点葵花牌小儿……

开始吧，不皮了 ;)

### 解析请求行

记得我们刚才readline返回的是一个 pair 么？这个pair有头有尾，我们的解析请求行的函数就这么写吧。

```cpp
namespace parser{

bool parse_request_line(const char *beg,
        const char *end, Request &req){

    // 申请一个 char 指针标记 end 的位置
    auto p_end{end};
    // 尝试寻找第一个 ' '(space)
    auto space1{std::find(beg, end, ' ')};
    // 如果没找到...
    if (*space1 != ' ') {
        return false;
    }
    // method 就成功获取了
    req.method.assign(beg, space1);
    // 第二个空格的寻找
    auto space2{std::find(space1 + 1, end, ' ')};
    if (*space2 != ' ') {
        return false;
    }
    req.path.assign(space1 + 1, space2);
    // 尾部 CRLF 的去除
    while (*p_end == '\r' || *p_end == '\n') {
        p_end--;
    }
    req.proto.assign(space2 + 1, p_end + 1);
    return true;
}

}
```

上节课有小伙伴问，命名空间什么的自己从来没有尝试使用过，这次我们可以试试，把整个解析函数们都定义在 parser 命名空间中，如上方所定义的那样。

**拓展：** 这里使用字符串指针操作并搜索第一个空格的方法，也许没有使用 `std::cmatch` 方法的快，你可以了解并且尝试。

在上面的函数的帮助下，下方的test可以正常运行

```cpp
#include <iostream>
#include "cxxhttp.h"
int main() {
    Request req{};
    std::string line = "GET / HTTP/1.1\r\n";
    parser::parse_request_line(static_cast<char *>(line.data()),
        static_cast<char *>(line.data() + line.size() - 1), req);
    std::cout << req.to_string() << '\n';
}
```

运行输出

```cpp
GET / HTTP/1.1
server: http-demo-1
```

已经和我们的预期一样，接下来继续解析剩下的header和头部。

> **说明：** 这些解析函数不建议写在 Request 类中，虽然这个函数是操作 Request 类的，但是通常一个请求对象不会自己做这样的操作，应该是服务类来完成这个操作，因此我建议保持函数的独立，传入 Request 作为引用。

### 解析头部

试着自己实现这样的函数

```cpp
parse_single_header(const char *beg, const char *end, Headers &hdr);
```

同样类似的函数定义，你可以试一试，思路很简单，就是查找 `':'` 并且分割他们添加到header中，`hdr.emplace(k,v)` 可以帮我们插入一对键值对。

有了上方的定义，那么下方的代码也能正常执行了。

```cpp
#include <iostream>
#include "cxxhttp.h"


int main() {
    Request req{};
    std::string line = "GET / HTTP/1.1\r\n";
    parser::parse_request_line(static_cast<char *>(line.data()),
        static_cast<char *>(line.data() + line.size() - 1), req);

    line = "Content-Length: 9\r\n";
    parser::parse_single_header(static_cast<char *>(line.data()),
        static_cast<char *>(line.data() + line.size() - 1), req.headers);
    std::cout << req.to_string() << '\n';
}
```

输出

```cpp
GET / HTTP/1.1
Content-Length: 9
server: http-demo-1
```

很棒，整个请求读取已经就快要完成了，就差一个读取全部header和读取body的了。

```cpp
bool parse_hdr(BufReader<MaxBufSize> &buf, Headers &hdr) {
    // .....得到 hdr_slice
    if (!parse_single_header(hdr_slice.first, hdr_slice.second, hdr)) {
        return False
    }
    // .....
}
```

我们可以用这样一个函数来每次读取到 `'\n'`，并且调用 `parse_single_header()` 完成头部的插入，当然我们也可选择这部分代码直接放置到 read_request 这个函数中。

### 读取body + 整体读取

因为读取请求body的方式暂时只考虑一个按照 `Content-Length` 来读取这种情况。

因此我们定义这样一个函数

```cpp
bool parse_request(BufReader<MaxBufSize> &buf, SapRequest &req) {
    auto req_slice{buf.readline()};

    if (!parse_request_line_re(req_slice.first, req_slice.second, req)) {
        return false;
    }
    if (!parse_hdr(buf, req.headers)) {
        return false;
    }
    if (req.ContentLength() > 0) {
        auto body_slice{buf.read_n(req.ContentLength())};
        if (body_slice.first == nullptr || body_slice.second == nullptr) {
            return false;
        }
        req.body.assign(body_slice.first, body_slice.second);
    }
    return true;
}
```

其中 `req.ContentLength()` 是我为了方便给 Request 类定义的成员函数，它的实现很简单

```cpp
size_t ContentLength() const {
    if (headers.has_header("Content-Length")) {
        return static_cast<size_t>(strtol(
                headers.get_header_value("Content-Length").c_str(),
                nullptr, 10));
    } else {
        return 0;
    }
}
```

在上方这些函数的帮助下，我们应该可以顺利的完成请求的读取了，让我们写一个例子试试。

```cpp
#include "cxxhttp.h"
#include "sockpp/tcp_acceptor.h"
#include "bufio.hpp"
#include "spdlog/spdlog.h"


int main() {
    Request req{};
    sockpp::tcp_acceptor acc(8080);
    spdlog::info("start listen at port {}", 8080);
    sockpp::inet_address peer;
    sockpp::tcp_socket sock = acc.accept(&peer);
    bufio::BufReader<MaxBufSize> reader(&sock);
    parser::parse_request(reader, req);
    std::cout << req.to_string() << '\n';
}
```

在我这里运行，并且使用curl请求发送到8080端口 `curl localhost:8080 -v`

会得到输出：

```text
[2020-08-05 10:34:20.210] [info] start listen at port 8080
GET / HTTP/1.1
Accept: */*
Host: localhost:8080
User-Agent: curl/7.64.1
server: http-demo-1
```

## 0x4 完成bufio

你那边因为bufio并没有完成，所以暂时无法和我一样运行并获得结果，因此让我们先浏览一下我写的bufio的源代码吧。

⚠️，这段代码本身就很难控制，我也是初学者，即便是抄，也很难从golang那边完全抄过来。并不保证这段代码能用于生产环境，请谨慎使用。

仅仅对于初学，这段代码完全够了。

📌既然都从golang那边抄过来了，为什么不直接用golang，非要用cpp在这里墨迹？

> 写这个项目的初衷并不是要写一个HTTP服务器，我在生产中遇到过一个问题，要在同一个端口同时处理 HTTP 和 TCP，知道golang的能可能会说，用hijike啊！hijike确实可以提取出tcp连接来，就没办法再进行HTTP操作，换句话说，太不灵活，结论：
>
> 1. golang的库不够灵活
>
> 在TCP层尝试用手动撸 HTTP 的时候，发现golang内部的很多内容都是堆操作，在堆区拷贝过来拷贝过去，写代码的效率很高，执行起来也不慢，可是内存消耗，就稳不住了，结论：
>
> 2. 大量 GC(垃圾回收) 引起内存抖动
>
> 这时候我已经坐不住了，开始尝试手动纯栈区，基于 TCP 实现 HTTP 操作，于是有了今天你们看到的这个坑。

### bufio 的源代码

我引入了一个 errors 命名空间，在里面定义了一个异常类，如果你不知道异常……（我在这里立一个flag，要开一篇文章讲讲异常）

我在底部放了一些Q&A，看到一半觉得很难受的可以到底部看看

```cpp
#ifndef HTTPDEMO_BUFIO_HPP
#define HTTPDEMO_BUFIO_HPP

#include "sockpp/tcp_acceptor.h"
#include "sockpp/tcp_connector.h"


#define  MaxBufSize 4096
#define MaxConsecutiveEmptyReads 100


#define  ErrNegativeCount "negative count"
#define  ErrNoProgress "multiple Read calls return no data or error"
#define  ErrFillFullBuffer "tried to fill full buffer"
#define  ErrNegativeRead "reader returned negative count from Read"


namespace errors {
// http error class
    class Error : public std::exception {
    public:
        // error detail should be
        // 1. where is the error (in which file)
        // 2. what's the problem
        // 3. detail of it, may be a str or other thing
        std::string where, what, detail;

        Error(std::string &&_where,
              std::string &&_what,
              std::string _detail) : where{std::move(_where)},
                                     what{std::move(_what)},
                                     detail{std::move((_detail))} {}

        friend std::ostream &operator<<(std::ostream &out, Error &error) {
            out << error.where << ": " << error.what << ", detail: " << error.detail;
            return out;
        }

        std::string to_string() {
            return this->where + ":" + this->what + ", detail:" + this->detail;
        }
    };
} // namespace errors


namespace bufio {
    template<size_t siz>
    class BufReader {

    private:
        // use template for allocate the m_buf in stack for speed
        sockpp::tcp_socket *m_sock;
        char *m_r;
        char *m_w;
        int error_num;
        char m_buf[siz];

        void fill();

    public:

        explicit BufReader(sockpp::tcp_socket *sock) : m_sock{sock} {
            reset();
        };

        explicit BufReader(sockpp::tcp_connector *sock) {
            m_sock = dynamic_cast<sockpp::tcp_socket *>(sock);
            reset();
        }

        void reset();

        std::string read_err();

        std::string peek(size_t n);

        size_t discard(size_t n);

        size_t buffered();

        char read_byte();

        std::pair<char *, char *> read_until(char delim);

        std::pair<char *, char *> readline();

        std::pair<char *, char *> read_n(size_t n);

        bool read_n(size_t n, std::string &buf);

        size_t write(const std::string &s) {
            return m_sock->write(s);
        }

        size_t write(char *buf, size_t n) {
            return m_sock->write(buf, n);
        }

        size_t write_n(const void *buf, size_t n) {
            return m_sock->write_n(buf, n);
        }
    };

    template<size_t siz>
    void BufReader<siz>::reset() {

        error_num = 0;
        m_r = m_w = m_buf;
    }

    template<size_t siz>
    void BufReader<siz>::fill() {
        // slide existing data to beginning

        if (m_r > m_buf) {
            int dist{static_cast<int>(m_w - m_r)};
            if (dist > 0) {
                std::memcpy(m_buf, m_r, dist);
                m_w -= dist;
                m_r = m_buf;
            } else {
                m_r = m_w = m_buf;
            }
        }

        errors::Error err("BufReader", "fill", ErrFillFullBuffer);


        if (static_cast<size_t>(m_w - m_buf) >= siz) {
            throw std::move(err);
        }

        for (auto i{MaxConsecutiveEmptyReads}; i > 0; i--) {
            auto n = m_sock->read(m_w, siz - (m_w - m_buf));
            if (n < 0) {
                err.detail = ErrNegativeCount;
                throw std::move(err);
            }
            m_w += n;
            if (m_sock->last_error() != 0) {
                error_num = m_sock->last_error();
                return;
            }
            if (n > 0) {
                return;
            }
        }
        err.detail = ErrNoProgress;


        throw std::move(err);
    }

    template<size_t siz>
    std::string BufReader<siz>::read_err() {
        if (error_num != 0) {
            return sockpp::tcp_socket::error_str(error_num);
        } else {
            return "";
        }
    }

    template<size_t siz>
    std::pair<char *, char *> BufReader<siz>::read_until(char delim) {
        errors::Error err("BufReader", "read_until", "");
        std::pair<char *, char *> res;
        for (;;) {
            // find from (m_r + s) -> m_w if there is a delim in them
            // s is a start indicator, we will not rescan on same mem
            // if we find delim in ....
            if (auto f{std::find(m_r, m_w, delim)}; *f == delim) {
                // ... make pair for return
                res = std::make_pair(m_r, f);

                // move m_r to next
                m_r = f + 1;
                return res;
            }
            // we can't find a delim
            if (error_num != 0) {
                err.detail = sockpp::tcp_socket::error_str(error_num);
                throw std::move(err);
            }

            if (buffered() >= siz) {
                err.detail = ErrFillFullBuffer;
                throw std::move(err);
            }

            fill(); // buffer is not full. fill it
        }
    }

    template<size_t siz>
    size_t BufReader<siz>::buffered() {
        return m_w - m_r;
    }

    template<size_t siz>
    char BufReader<siz>::read_byte() {
        errors::Error err("BufReader", "read_byte", "");
        while (m_r == m_w) {
            if (error_num != 0) {
                err.detail = sockpp::tcp_socket::error_str(error_num);
                throw std::move(err);
            }
            fill();
        }
        char buf = *m_r;
        ++m_r;
        return buf;
    }

    template<size_t siz>
    std::pair<char *, char *> BufReader<siz>::read_n(size_t n) {
        auto max_try_time{10};
        while (static_cast<size_t>(m_w - m_r) < n) {
            fill();
            if (--max_try_time < 0) {
                return std::make_pair(nullptr, nullptr);
            }
        }
        char *p{m_r};
        m_r += n;
        return std::make_pair(p, p + n);
    }

    template<size_t siz>
    bool BufReader<siz>::read_n(size_t n, std::string &buf) {
        auto remain{static_cast<int>(n)};
        for (; remain > 0;) {
            int buf_siz{static_cast<int>(buffered())};
            // buffer enough...
            if (buf_siz > remain) {
                buf += std::string(m_r, remain);
                m_r += remain;
                break;
            }
            // buffer not enough
            if (buf_siz < remain && buffered() > 0) {
                remain -= buffered();
                buf += std::string(m_r, buffered());
                m_r = m_w = m_buf;
            }
            fill();
        }
        return true;
    }

    template<size_t siz>
    std::string BufReader<siz>::peek(size_t n) {
        if (n < 0) {
            return std::string();
        }

        while (buffered() < n && buffered() < siz && error_num != 0) {
            fill();
        }
        std::string temp;

        if (auto avail = buffered();avail < n) {
            n = avail;
        }
        temp.clear();
        return temp.assign(m_r, m_r + n);
    }

    template<size_t siz>
    std::pair<char *, char *> BufReader<siz>::readline() {
        return read_until('\n');
    }

}

#endif // HTTPDEMO_BUFIO_HPP
```

### Q&As

1. 为什么你的代码框的边角那么奇怪，博客地址<https://dashjay.github.io/>。

    因为我设定了 border-radius: 5% 结果，代码越长，这个半径越大，看起来很难受。我知道可以分别设置宽高的radius，我这不是写教程太忙了没时间改么。

2. throw std::move(err)是什么意思，和直接 throw err 有什么区别。

    throw本身是建议直接返回一个临时变量（r-value），也就是说建议使用 `throw "error"` 这种操作，抛出一个临时变量，否则还要进行一次拷贝，像我们这样提前定义的 err，要把它转换成一个临时变量就要使用 std::move，这个叫移动语义，可以防止拷贝。虽然现在编译器比你聪明多了，就算你不那么做，编译器也应该会帮你优化……什么？你比编译器聪明，好吧 -_-!

3. 你可以继续提问题在这个项目提ISSUE <https://github.com/dashjay/http_demo>

### 附录 使用bufReader 解析 HTTP 请求代码示例

```cpp
namespace parser {

    bool parse_request(bufio::BufReader<MaxBufSize> &buf, Request &req) {
        auto req_slice{buf.readline()};
        if (!parse_request_line(req_slice.first, req_slice.second, req)) {
            return false;
        }
        if (!parse_hdr(buf, req.headers)) {
            return false;
        }
        if (req.ContentLength() > 0) {
            auto body_slice{buf.read_n(req.ContentLength())};
            if (body_slice.first == nullptr || body_slice.second == nullptr) {
                return false;
            }
            req.body.assign(body_slice.first, body_slice.second);
        }
        return true;
    }

    bool parse_request_line(const char *beg, const char *end, Request &req) {
        auto p_end{end};
        auto space1{std::find(beg, end, ' ')};
        if (*space1 != ' ') {
            return false;
        }
        req.method.assign(beg, space1);
        auto space2{std::find(space1 + 1, end, ' ')};
        if (*space2 != ' ') {
            return false;
        }
        req.uri.assign(space1 + 1, space2);
        while (*p_end == '\r' || *p_end == '\n') {
            p_end--;
        }
        req.proto.assign(space2 + 1, p_end + 1);
        return true;
    }

    bool parse_hdr(bufio::BufReader<MaxBufSize> &buf, Headers &hdr) {
        for (;;) {
            auto hdr_slice{buf.readline()};

            if (*hdr_slice.first == '\r' && *hdr_slice.second == '\n') {
                break; // \r\n means header read over
            }
            if (!parse_single_header(hdr_slice.first, hdr_slice.second, hdr)) {
                std::cerr
                        << "parse_single_header error, detail:" + std::string(hdr_slice.first, hdr_slice.second)
                        << '\n';
            }
        }
        return true;
    }

    bool parse_single_header(const char *beg, const char *end, Headers &hdr) {
        auto p = beg;
        while (p < end && *p != ':') {
            p++;
        }
        if (p < end) {
            auto key_end = p;
            p++; // skip ':'
            while (p < end && (*p == ' ' || *p == '\t')) {
                p++;
            }
            if (p < end) {
                auto val_begin = p;
                while (p < end && *p != '\r' && *p != '\n') {
                    p++;
                }

                hdr.m_hdr.emplace(std::string(beg, key_end), std::string(val_begin, end - 1));
                return true;
            }
        }
        return false;
    }
}
```

## 0x5 使用 bufReader 解析 HTTP 返回

不多说了，情况都差不多，贴上代码然后你可以自己看看：

```cpp
    bool parse_response(bufio::BufReader<MaxBufSize> &buf, Response &resp) {

        std::pair<char *, char *> resp_slice;
        resp_slice = buf.readline();
        if (!parse_response_line(resp_slice.first, resp_slice.second, resp)) {
            return false;
        }

        // parse headers
        if (!parse_hdr(buf, resp.headers)) {
            return false;
        }

        // if we get head...
        if (resp.req != nullptr && resp.req->method == "HEAD") {
            // ... we can ignore body (not exsits)
            return true;
        }

        return parse_response_body(buf, resp);
    }

    bool parse_response_body(bufio::BufReader<MaxBufSize> &buf, Response &resp) {
        if (resp.ContentLength() > 0) {
            auto body{buf.read_n(resp.ContentLength())};
            resp.body.assign(body.first, body.second);
        } else {
            // 读取其他种类的body
            // Transfer-Encoding == "chunked"
            // <https://www.rfc-editor.org/rfc/rfc2616.html#section-14.41>
        }
        return true;
    }

    bool parse_response_line(const char *beg, const char *end, Response &resp) {
        if (end - beg <= 2) {
            spdlog::error("no space, end-beg = {}", end - beg);
            return false;
        }

        auto p_end{end};

        auto space{std::find(beg, end, ' ')};

        if (*space != ' ') {
            spdlog::error("no space, find {}", *space);
            return false;
        }
        resp.proto.assign(beg, space);
        while (*p_end == '\r' || *p_end == '\n') {
            p_end--;
        }
        resp.status.assign(space + 1, p_end + 1);
        space = std::find(space + 1, p_end, ' ');
        resp.status_code = static_cast<int>(std::strtol(space + 1, nullptr, 10));
        return true;
    }
```

写到这里我们可以整理一下整个项目的目录。

## 0x6 同时测试请求和返回

请求的读取测试起来还挺方便的返回的测试就比较难办了，那我们应该怎么办呢？

我们这次尝试编写一个代理服务器：

你使用 curl 发送请求给服务，服务将你的请求转发到第三方服务器，第三方服务器返回，再转发回用户你，很酷吧。

让我们先来整理一下我们的目录：

### 整理项目目录

```cpp
├── CMakeLists.txt
├── README.md
├── bufio.hpp // bufio 头部和实现都在一个文件内，因为使用了模板类，不这样做就会报错，所以命名为hpp了
├── config.toml
├── cxxhttp.cpp // 请求和返回，与解析请求返回的函数声明
├── cxxhttp.h
├── headers.cpp // 头部的实现和声明
├── headers.h
└── main.cpp // 主程序入口
```

errors类也直接声明在 bufio.hpp 里了，为了方便，减少文件数。

### 启动一个第三方服务器

请放心，不会配置什么 apache 或者 nginx 的。

如果你的计算机中安装了 `python3`,请随便选一个目录运行以下命令

```python
python -m http.server 8080
Serving HTTP on 0.0.0.0 port 8080 (http://0.0.0.0:8080/) ...
```

尝试 使用 curl 或者浏览器直接访问localhost:8000，端口，你会发现你的目录被映射出去了。

请确保以上方法成功，或者你自己有其他更简单的方式能够启动一个 http 服务器。

注意：如果你使用了 npm 的 http-server，它的返回包使用的是 Transfer-Encoding 的方式，我们暂时没有支持这种读取body的方式，你可以从 <https://www.rfc-editor.org/rfc/rfc2616.html#section-14.41> 了解更多。

### 编写主程序

```cpp
#include "cxxhttp.h"
#include "sockpp/tcp_acceptor.h"
#include "sockpp/tcp_connector.h"
#include "bufio.hpp"
#include "spdlog/spdlog.h"

#define listen_port 8081
#define target_port 8080

int main() {
    // 开始监听
    sockpp::tcp_acceptor acc(listen_port);
    spdlog::info("start listen at port {}", listen_port);

    sockpp::inet_address peer;
    // 接受连接
    for (;;) {
        sockpp::tcp_socket sock = acc.accept(&peer);
        if (!sock) {
            spdlog::error("accept error");
            continue;
        }
        spdlog::info("accept a conn from {}", peer.address());

        bufio::BufReader<MaxBufSize> Src(&sock);
        Request req{};
        if (!parser::parse_request(Src, req)) {
            spdlog::info("read request error");
            continue;
        }
        spdlog::info("read request success {}", req.to_string());

        sockpp::tcp_connector conn;
        if (!conn.connect(sockpp::inet_address("0.0.0.0", target_port))) {
            return -1;
        }
        auto c_sock = static_cast<sockpp::tcp_socket>(std::move(conn));
        if (!c_sock) {
            spdlog::info("connect target server error");
            continue;
        }
        bufio::BufReader<MaxBufSize> Dst(&c_sock);
        spdlog::info("write request to target");
        Dst.write(req.to_string());
        Response resp{};
        parser::parse_response(Dst, resp);
        spdlog::info("write back to client");
        Src.write(resp.to_string());
    }
}
```

请注意顶部 listen_port 和 target_port.

写到这里我必须承认个错误，之前两个to_string 函数写错了，原处也已经修改，下面才是正确写法，我那天不知道脑子在想什么。

```cpp
std::string Request::to_string() {
    std::ostringstream out;
    out << this->method << ' ' << this->uri << ' ' << this->proto << "\r\n";
    for (auto &kv:this->headers.hdr()) {
        out << kv.first << ": " << kv.second << "\r\n";
    }
    out << "\r\n";
    if (!this->body.empty()) {
        out << this->body;
    }
    return out.str();
}

std::string Response::to_string() {
    std::ostringstream out;
    out << this->proto << ' ' << this->status << "\r\n";
    for (auto &kv:this->headers.hdr()) {
        out << kv.first << ": " << kv.second << "\r\n";
    }
    out << "\r\n";
    if (!this->body.empty()) {
        out << this->body;
    }
    return out.str();
}
```

### 运行结果

当我们把http的服务器运行在8080端口，并且可以直接通过curl或者浏览器方式时。

我们再把这个程序运行在8081端口，并且访问8081端口，程序执行流程如下。

你的curl同http-demo之间建立连接，并且发送一个请求，http-demo读取到请求，然后转发给 python 的服务器并且得到返回值。读取到返回包后，你将返回包，写回客户端并且断开连接，程序退出。

我这边curl收到的输出，和直接请求8080服务器的输出是一模一样的。

```cpp
> curl localhost:8081  -i
HTTP/1.0 200 OK
Content-Length: 809
Content-type: text/html; charset=utf-8
Date: Wed, 05 Aug 2020 15:09:25 GMT
Server: SimpleHTTP/0.6 Python/3.7.3
server: http-demo-1

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<title>Directory listing for /</title>
</head>
<body>
<h1>Directory listing for /</h1>
<hr>
<ul>
<li><a href=".DS_Store">.DS_Store</a></li>
.....
<li><a href="themes/">themes/</a></li>
</ul>
<hr>
</body>
</html>
```

## 0x7 总结

**我们本次工作完成了哪些内容？**

这是内容最多的一节课

1. 我们使用基础 C-Style char 数组实现了一个BufReader，帮助我们完成 `readline` 和 `read_n` 这类的操作。

2. 我们用写出来的 BufReader 搭配 sockpp 分别读取 HTTP 请求和返回体。

3. 尝试使用以上成果搭建了一个一次性的 HTTP 代理服务器。

延伸：

你还有哪些工作可以做：

1. 读取返回包的方式除了根据 `Content-Length` 读取之外，还有 `Transfer-Encoding: chunked` 的模式，你可以尝试查询资料并实现。

2. 程序中并未引入线程概念，因此程序在同一时刻只能接收一份http服务，这明显不符合逻辑，下节课我将于此点展开，在此之前，你可以先驱去了解一下 thread 头文件中包含的内容。

谢谢你的阅读，这点东西，一不小心就写了将近6个小时。
