---
title: '手动用cpp实现http(三)'
date: '2020-08-03'
description: ''
author: 'dashjay'
---

创建于 2020-07-31，最终修订于 2020-08-06

在之前的[课程(二)](https://github.com/dashjay/http_demo/tree/master/2-http-request-response)中我们讲了有关 HTTP 头部，请求体和返回结构在 cpp中的定义，今天我们来说一些很不相关，但是也很重要的内容：

今天的任务是 "[易]引入cpptoml从文件读取配置，引入spdlog尝试打log，帮助调试"。

所有的代码都在 <https://github.com/dashjay/http_demo/tree/3-cpptoml-spdlog> 中

Let's do it

## 0x1 为什么要载入配置文件

众所周知，编译类的程序，编译一次，可运行n次，在通过一些测试后，即可常年工作。但是有些情况下，我们会尝试通过一些方法修改程序的行为，例如：

- 程序的配置项除了监听端口之外，其实还有很多其他的例如，tls证书位置。
- 平时日常使用时只会打印出一小部分log，防止大量log堆积导致一些问题，但是当程序出现异常的时候，希望能多打印一些程序，需要调整log程序的打印级别。

当遇到这些内容修改时，我们是不希望再编译一遍的。

而如果这些命令通过标准输入，或者运行时作为参数传入都不是很方便，因此我们会使用配置文件。

我推荐使用 [TOML](https://github.com/toml-lang/toml) 格式来作为配置，不用再因为担心缩进，支持的格式也很丰富，数组，浮点数，字符串，等等。

经过一些选择之后我找到一个库，叫 [cpptoml](https://github.com/skystrife/cpptoml)

## 0x2 cpptoml

`A header-only library for parsing TOML configuration files.`

### 安装


```bash
# git clone https://github.com/toml-lang/toml.git
# cd toml; mkdir build; cd build
# cmake .. && make && make install
# 上方的是传统安装方式，BAZEL安装的话，在 WORKSPACE 中编写以下内容
http_archive(
    name = "cpptoml",
    urls = ["https://github.com/skystrife/cpptoml/archive/refs/tags/v0.1.1.tar.gz"],
    sha256 = "23af72468cfd4040984d46a0dd2a609538579c78ddc429d6b8fd7a10a6e24403",
    strip_prefix = "cpptoml-0.1.1",
    build_file_content = """
filegroup(
    name = "include",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"], 
)
"""
)
```

> 看到这里你即便不知道 bazel 有什么用，但是也知道 bazel 是真的很方便
> 到目前为止我们电脑里已经安装了sockpp, cpptoml. 他们一个是有链接库的，一个是header only的。

### 小尝试

本次的所有代码都基于[上次课程](https://github.com/dashjay/http_demo/tree/master/2-http-request-response)的开始。

```cpp
#include<iostream>
#include "cxxhttp.h"
#include "cpptoml.h"

int main() {
    auto config = cpptoml::parse_file("./config.toml");
    auto port = config->get_qualified_as<in_addr_t>("ENV.port");
    std::cout << "server will start at port " << *port << '\n';

    Request req("GET", "/", "body");
    ...
}
```

当你写成这样尝试 bazel run 的 的时候，你会报错，因为bazel 不能帮你获取一个 config.yaml 文件到当前目录下面，因此你需要使用 `bazel build` 命令

```
$ bazel build //3-cpptoml-spdlog:main
INFO: Invocation ID: 71d2b650-9efb-4fed-9af9-bfe06afa5af0
DEBUG: Rule 'spdlog' indicated that a canonical reproducible form can be obtained by modifying arguments sha256 = "6fff9215f5cb81760be4cc16d033526d1080427d236e86d70bb02994f85e3d38"
DEBUG: Repository spdlog instantiated at:
  /Users/kevin/Desktop/code/http_demo/WORKSPACE:18:13: in <toplevel>
Repository rule http_archive defined at:
  /private/var/tmp/_bazel_kevin/ce6fd4f831835df0db0420815294e5b1/external/bazel_tools/tools/build_defs/repo/http.bzl:336:31: in <toplevel>
.....
INFO: Found 1 target...
Target //3-cpptoml-spdlog:main up-to-date:
  bazel-bin/3-cpptoml-spdlog/main
```

然后进入 3-cpptoml-spdlog 保证运行目录下方有一个 `config.toml` 文件

```toml
[ENV]
port = 8083
```

```text 
cd 3-cpptoml-spdlog
../bazel-bin/3-cpptoml-spdlog/main
server will start at port 8083
[2021-11-14 09:01:20.235] [info] request: GET / HTTP/1.1
server: http-demo-1

body
[2021-11-14 09:01:20.236] [info] response: HTTP/1.1 200 OK
server: http-demo-1

body
....
```

**为什么使用*port来取值：** 通过源代码我们发现 `auto port = ....` 的得到的不是目标类型的对象，而是一个叫做 option 的对象，下面有一些代码解释。

```cpp
template <class T>
class option
{
  public:
    ......  
    explicit operator bool() const
    {
        return !empty_;
    }

    const T& operator*() const
    {
        return value_;
    }

    const T* operator->() const
    {
        return &value_;
    }
    ...
```

我们可以看到，返回的对象被重载了 `bool()`, `*`, `->` 三种操作符：分别可以得到， **对象是否为空**，**对象的引用**，**指向对象的指针**等三个值。

## 0x3 为什么不使用std::cout或std::cerr 来输出日志

输出到标准输出和标准错误的日志，会因为多线程的原因，产生一些堆叠，在这里我就不进行演示了，你可以想象，100个线程毫无避讳的使用 `std::cout << ...` 时，会产生大量重叠，丢失，的情况。

虽然使用log程序会产生一些锁的开销，但是目前看来是值得的：

- 帮助我们debug，有不同的颜色，层级来标注
- 不会因为多线程的使用而错乱[^1]

## 0x4 spdlog

> Very fast, header-only/compiled, C++ logging library.

在搜索查询了不少 log 库，诸如什么 log4cpp 之类的，发现他们都比较臃肿，有的还需要单独的配置文件，没有这个库容易使用。

[spdlog](https://github.com/gabime/spdlog)

安装什么的应该不用我说吧，它有可以自己研究研究，和之前的操作一样即可。这个也是一个纯header的库，无需链接。

在 WORKSPACE 下添加如下代码

```
http_archive(
    name = "spdlog",
    urls = ["https://github.com/gabime/spdlog/archive/refs/tags/v1.9.2.tar.gz"],
    strip_prefix = "spdlog-1.9.2",
    build_file_content = """
filegroup(
    name = "include",
    srcs = glob(["include/**"]),
    visibility = ["//visibility:public"], 
)
"""
)
```

写了一些代码修改之后的主文件如下

```cpp
#include<iostream>
#include "spdlog/spdlog.h"
#include "cxxhttp.h"
#include "cpptoml.h"

int main() {
    auto config = cpptoml::parse_file("./config.toml");
    auto port = config->get_qualified_as<in_addr_t>("ENV.port");
    std::cout << "server will start at port " << *port << '\n';

    Request req("GET", "/", "body");

    spdlog::info("request: {}", req.to_string());

    Response resp;
    resp.proto = "HTTP/1.1";
    resp.status = "200 OK";
    resp.body = "body";

    spdlog::info("response: {}", resp.to_string());
}
```

`request: {}` 中的 `{}` 是一个占位符，他没有选择用常见的 `%s %d`， 来完成格式化字符串，而是使用了 `{}`，该库介绍说性能很高，不应该指定类型性能会更高么，不是很明白，不过不影响我们使用它。

这样全局调用即可，如果我们没有什么更高级的需求。

今天的内容有点水了，这可是为了我们下面最难的一节做一些铺垫，我可不想看到觉得自己没错，以为重新编译一遍就能解决问题的你，疯狂 bazel

[^1]: 在这一点上我并没有写代码确认，也没有阅读源代码，在这里这么描述是不负责的，但是多线程使用 `std::cout << 或 std::cerr <<` 是肯定会产生一些错误的。
