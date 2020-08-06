---
title: '手动用cpp实现http(二)'
date: '2020-08-02'
description: ''
author: 'dashjay'
---



在之前的[介绍课程(一)](https://github.com/dashjay/http_demo/tree/1-cmake-socket-echo)中我们说有关Cmake和知识，并且选用了一个Socket库

今天的任务是 "[易]定义HTTP请求和返回体的结构，构建并输出HTTP请求和返回体到标准输出。"。

所有的代码都在 <https://github.com/dashjay/http_demo/tree/2-http-request-response> 中

Let's do it

## 0x1 初识HTTP请求结构

这就是一个简单的HTTP请求，我习惯把他分为三个部分：**请求行（第一行）**，**请求头（n行）**，**请求体（之前有一个空行）**

```cpp
-----
GET / HTTP/1.1\r\n
Key1: Value1\r\n
Key2: Value2\r\n
\r\n
body(if these is a body)
-----
```

到这里，网上充斥着大量的教程，讲解着有关请求结构的，我不打算做过多赘述，它非常简单，大多都还遵循一套[rfc2616](https://www.rfc-editor.org/rfc/rfc2616.html#section-4)中所描述。

> 如果你想了解更多，建议直接看 RFC 并且使用 `curl xxx -v` 来了解更多，而不要相信那些营销号的文章《99%的人不知道GET和POST的区别》或者《99%的人都错用了POST请求》，很多错误的言论诸如”POST 请求会发两个包“，这种错误描述，明明学了计算机网络的我却深信不疑。（PS：这种文章在知乎也有，拉低平均水平）。

我们可以使用这样的命令，`curl baidu.com -v`（对不起了，百度，又让你的服务器负担加重了），会得到如下结果。`curl` 亲切的使用了 `>` 开头表示客户端发给服务端的数据，用 `<` 开头表示服务端发给客户端的。

```bash
curl baidu.com -v
*   Trying 220.181.38.148...
* TCP_NODELAY set
* Connected to baidu.com (220.181.38.148) port 80 (#0)
> GET / HTTP/1.1
> Host: baidu.com
> User-Agent: curl/7.64.1
> Accept: */*
>
< HTTP/1.1 200 OK
< Date: Sun, 02 Aug 2020 11:53:43 GMT
< Server: Apache
< Last-Modified: Tue, 12 Jan 2010 13:48:00 GMT
< ETag: "51-47cf7e6ee8400"
< Accept-Ranges: bytes
< Content-Length: 81
< Cache-Control: max-age=86400
< Expires: Mon, 03 Aug 2020 11:53:43 GMT
< Connection: Keep-Alive
< Content-Type: text/html
<
<html>
<meta http-equiv="refresh" content="0;url=http://www.baidu.com/">
</html>
```

有些 curl 的命令参数你可以试一试

> 1. `curl -X POST` 表示发送 post 请求，
> 2. `curl -d '{"a":"b"}'` 表示发送带有请求体的请求，
> 3. 加 `-H 'Key: Value'` 表示添加头部。

你可以自己动手试试看，请求的结构是什么样子的。

在上方的日志中，GET 开头的就是发出去的数据，HTTP/1.1 开头的就是接收的数据

## 0x2 初识HTTP返回结构

```cpp
HTTP/1.1 200 OK\r\n
Server: Apache\r\n
\r\n
body
```

对照这之前使用 curl 发送请求的到的内容，我们可以大概知道返回结构就是上方描述的这样。

如果你想只看返回结构，可以通过 `curl baidu.com -i` 得到整个返回体的结构

```cpp
curl baidu.com -i
HTTP/1.1 200 OK
Date: Sun, 02 Aug 2020 12:01:40 GMT
Server: Apache
Last-Modified: Tue, 12 Jan 2010 13:48:00 GMT
ETag: "51-47cf7e6ee8400"
Accept-Ranges: bytes
Content-Length: 81
Cache-Control: max-age=86400
Expires: Mon, 03 Aug 2020 12:01:40 GMT
Connection: Keep-Alive
Content-Type: text/html

<html>
<meta http-equiv="refresh" content="0;url=http://www.baidu.com/">
</html>
```

以上就是请求和返回的结构，再次说明，这些结构都非常简单，无需看各种复杂的解说，你只需要自己试一试，看一看，请注意我写`\r\n`的位置，这些在之后写代码的时候有帮助。

## 0x3 在CPP中定义请求和返回体的结构

请求体的数据结构，我们可以简单考虑成这样，其中有一个Header我会在下面接着说。

```cpp
class Request{
    public:
    std::string method, path, proto; // GET / HTTP/1.1
    Headers headers; // Key: Value1\r\nKey2: Value2\r\n...
                    // \r\n
    std::string body; // bodyxxxxxx.....
}
```

### 头部是什么结构

HTTP请求和返回的头部中，有几个特殊情况我们需要注意，先说一个我们亟待解决的：

头部并不是简单唯一KV对，一个Key可以对应多个Value。这正好对应了我们 CPP STL 中的 `multimap`。因此我们定义这个 Headers 为一个 multimap 的别名，使用 using 关键词。

```cpp
#include<map>

using Headers = std::multimap<std::string, std::string>;
```

我们可以专门定义一个Headers类，然后把它的方法定义在里面，也可以使用 `using` 语句，给指定的multimap一个别名，并且把针对头部的操作，放到请求类中。

像上面描述的这样做时，考虑到请求和返回中都存在同样的Headers，并且方法也一样，在请求和返回中写两遍确实有些难受，因此我选择了定义一个 Headers 类。

我们可以简单看一下这个类的定义，底部会有一些解释。

```cpp
#ifndef HTTP_TEST_HEADERS_H
#define HTTP_TEST_HEADERS_H

#include <map>
#include <string>

const std::string version = "http-demo-1";

class Headers {
    using Hdr = std::multimap<std::string, std::string>;
private:
    Hdr m_hdr;
public:
    Headers();

    bool has_header(const char *key) const;

    size_t get_header_value_count(const char *key) const;

    std::string get_header_value(const char *key, size_t id = 0, char *def = nullptr) const;

    void set_header(const char *key, const char *val);

    void set_header(const char *key, const std::string &val);

    void add_header(const char *key, const char *val);

    void add_header(const char *key, const std::string &val);

    void del_header(const char *key);

    const Hdr &hdr();
};


#endif //HTTP_TEST_HEADERS_H
```

为什么重载两份：

```cpp
void set_header(const char *key, const char *val);
void set_header(const char *key, const std::string &val);
```

- 当我们直接手写字符串的时候传入的是 `const char *key`，例如 `headers.add_header("Content-Length","0")`
- `const std::string val`，如果我们想直接添加，就必须这样写`headers.add_header(val.c_str(),"0")`，重载帮助我们直传 `std::string`

具体的实现，无非就是使用了以下几个方法，你可以自己实现试试，在文末的附录中，我会把代码一一列出

- `iterator find( const Key& key );`
- `std::pair<iterator,iterator> equal_range( const Key& key );`
- `iterator emplace( Args&&... args );`
- `std::distance`
- .....

### 定义请求结构

有了上方的headers做基础，定义一套请求和返回值，尤其的简单

```cpp

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
};

class Response {
public:
    std::string proto;
    std::string status;

    Headers headers;
    std::string body;

    int32_t status_code;
};

#endif //HTTP_TEST_CXXHTTP_H
```

以上就是请求和返回值的数据结构，我在请求类中添加了两个 public 方法，返回类中添加了一个public方法。

```cpp
class Request{
    .....

    Request(std::string &m, std::string &u, std::string &b);
    std::string to_string();
}

class Response{
    .....

    std::string to_string();
}
```

有了这两个方法，我们今天的课程差不多可以收尾了，当然还有返回体的结构，我们也要定义同样的 `to_string()` 方法。

## 0x4 验证请求和返回结构体的输出

```cpp
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
```

通常情况下我们都只在请求体中定义构造函数，返回结构一般是不会由用户构造的，在这里我也不提供构造函数，你有兴趣可以自己实现一份。

以上代码编译运行，得到输出。

```cpp
./main
GET / HTTP/1.1
server: http-demo-1

body
=========split========
HTTP/1.1 200 OK
server: http-demo-1

body
```

如果你看起来觉得不太像，你可以自己手动添加一些头部，并且学习这些头部的知识点。他们的文档在这里[rfc2616-5.3](https://www.rfc-editor.org/rfc/rfc2616.html#section-5.3)

```bash
   request-header = Accept                   ; Section 14.1
                      | Accept-Charset           ; Section 14.2
                      | Accept-Encoding          ; Section 14.3
                      | Accept-Language          ; Section 14.4
                      | Authorization            ; Section 14.8
                      | Expect                   ; Section 14.20
                      | From                     ; Section 14.22
                      | Host                     ; Section 14.23
                      | If-Match                 ; Section 14.24
                      | If-Modified-Since        ; Section 14.25
                      | If-None-Match            ; Section 14.26
                      | If-Range                 ; Section 14.27
                      | If-Unmodified-Since      ; Section 14.28
                      | Max-Forwards             ; Section 14.31
                      | Proxy-Authorization      ; Section 14.34
                      | Range                    ; Section 14.35
                      | Referer                  ; Section 14.36
                      | TE                       ; Section 14.39
                      | User-Agent               ; Section 14.43
```

一切都是那么简单，HTTP协议本身不是一个特别特别复杂的协议，很多附加功能都基于头部展开。

例如 Cookie，你也可以尝试自己实现一个 Cookie类：Cookie在头部中的储存形式是和其他头部不同的，他们这样存储：`Cookies: k1=v1; k2=v2`

## 0x5 总结

今天我们进行了如下工作：

- 尝试简单的使用 multimap 来实现一个 HTTP 头
- 尝试使用构造的 HTTP 头类来构造请求类和返回类，并且给他们提供字符串输出的方式

除此之外，你还可以做一些适当 **延伸** ，这里可以给你一个简单的方向。

1. multimap的模板构造器可以传入第三个参数，作为一个比较器，用来对 key value 进行排序，你可以探究一下第三个参数，提示：`std::lexicographical_compare`

2. Cookies 的头部构造和其他的 Headers 稍许不同，因为 Cookies 可以为不止 1 对键值对，但是必须存储在一个 Header Value 中: 可以实现一个 Cookies 类或结构，并在 Request 中提供添加，删除的方法，并且在最后 `to_string` 时追加到header中

3. 命名空间，整个 http 放在空命名空间“::”容易发生冲突，自己定义一个命名空间（可以是http_demo）存放所有代码，并且在 main 中使用。

## 0x6 附录

### 代码片段1：Headers类实现代码

```cpp

#include "headers.h"

Headers::Headers() {
    m_hdr.emplace("server", version);
}

bool Headers::has_header(const char *key) const {
    return m_hdr.find(key) == m_hdr.end();
}


size_t Headers::get_header_value_count(const char *key) const {
    auto r{m_hdr.equal_range(key)};
    return static_cast<size_t>(std::distance(r.first, r.second));
}

std::string Headers::get_header_value(const char *key, size_t id, char *def) const {
    auto rng = m_hdr.equal_range(key);
    auto it = rng.first;
    std::advance(it, static_cast<ssize_t>(id));
    if (it != rng.second) {
        return it->second;
    }
    return def;
}

void Headers::set_header(const char *key, const char *val) {
    if (this->has_header(key)) {
        m_hdr.erase(key);
    }
    m_hdr.emplace(key, val);
}

void Headers::set_header(const char *key, const std::string &val) {
    if (this->has_header(key)) {
        m_hdr.erase(key);
    }
    m_hdr.emplace(key, val);
}

void Headers::add_header(const char *key, const char *val) {
    m_hdr.emplace(key, val);
}

void Headers::add_header(const char *key, const std::string &val) {
    m_hdr.emplace(key, val);
}

void Headers::del_header(const char *key) {
    m_hdr.erase(key);
}

const Headers::Hdr &Headers::hdr() {
    return this->m_hdr;
}

```

### 代码片段2：两个类的成员函数

```cpp

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

### 代码片段2解释

构造函数中，使用了这样的构造函数，其中的 `&&` 表示传入的是 **右值(r-value)**，会触发 **移动语义(move semantics)**

```cpp
Request::Request(
        std::string &&m,
        std::string &&u,
        std::string &&b) :
        ....
```

如果你还不知道这些知识，你可以忽略他们，或者用你自己的方法。

```cpp
 std::string m, u, b;
    m = "GET";
    u = "/";
    b = "body";
    Request req(m, u, b);
```

这样的代码也能起到同样的效果，只不过，`method，url，body` 三个参数作为引用传入，但是在 **赋值** 给类内变量时，触发的是 **拷贝语义(copy semantics)**，三个 `std::string` 会被拷贝一份，消耗更多的性能，你可以暂时忽略这个知识点。（我在这里立一个flag，未来再根据这个写一篇文章）。
