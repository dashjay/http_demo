---
title: '手动用cpp实现http(零)'
date: '2020-07-30'
description: ''
author: 'dashjay'
---

2020年7月，笔者经过学习了cpp后打算入坑尝试手动实现 HTTP 服务器，过程中遇到很多问题，本文记录了笔者留下的一些记录，提供参考。

> 下面的叙述中含有一些脚注，为了不影响已经非常熟悉的同学的阅读，我把一些说明放到了脚注里，方便阅读。

Repo我开在这里，欢迎大家点个Star或者Fork操作。<https://github.com/dashjay/http_demo.git>

本教程[1]，使用6节课程/文章，尝试使用最简单的CPP知识实现一个高性能，简单的的HTTP Server，课程使用 bazel 进行构建，简化了构建流程。

1. [易]简单的 bazel 的教程，选用一个Socket库[2]并实现一个echo[3]。[link](https://github.com/dashjay/http_demo/tree/master/1-cmake-socket-echo)
2. [易]定义HTTP请求和返回体的结构，构建并输出HTTP请求和返回体到标准输出。[link](https://github.com/dashjay/http_demo/tree/master/2-http-request-response)
3. [易]引入cpptoml从文件读取配置，引入spdlog尝试打log，帮助调试。[link](https://github.com/dashjay/http_demo/tree/master/3-cpptoml-spdlog)
4. [难]定义一个bufReader类，并且使用该bufReader从TCP流中解析HTTP请求和返回体。[link](https://github.com/dashjay/http_demo/tree/master/4-bufreader)
5. [易]实现主程序逻辑，监听端口，接收请求。[link](https://github.com/dashjay/http_demo/tree/master/5-main-work)
6. [选]实现其他HTTP附加功能。

## 更新日志

- 2020-08-06：维护 4 个分支太麻烦了，所有教程全部放到 master 分支的不同文件夹下
- 2020-08-05：更新教程 4-bufreader
- 2021-11-01：最近学习了 bazel 的使用，于是将项目 bazel 化

## 我们要准备的东西(推荐）

- 一个顺手的IDE，推荐CLion，VScode，也可以用VIM，Emacs或你顺手的。
- 建议使用在Linux或MacOS下进行，因为我本地没Windows，没法为大家验证过程可行性。
- 你的耐心

解释

1. 算不上什么教程，顺便测试一下脚注。
2. HTTP/GRPC 等等这样的协议通常底层采用TCP来实现，也就是学校里学到的那种面向流的服务，cpp中的系统调用实现的socker接口用起来有些不顺手。因此我找到了一个现代的Socket库<https://github.com/fpagliughi/sockpp>来帮助我们在这节课中完成HTTP服务器。
3. 你发给它什么，他就回复什么，常见的网络库都会用这个代替HELLOWORLD
4. bazel 是一个构建工具，可以帮助项目进行构建，类似一个 Cmake ，文档在这里 [bazel c++](https://docs.bazel.build/versions/4.2.1/tutorial/cpp.html)，比如第一个程序可以通过命令 `bazel run //1-cmake-socket-echo:hello-world` 来构建并且运行。
