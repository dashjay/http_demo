---
title: '手动用cpp实现http(零)'
date: '2020-07-30'
description: ''
author: 'dashjay'
---

2020年7月，笔者经过学习了cpp后打算入坑尝试手动实现 HTTP 服务器，过程中遇到很多问题，本文记录了笔者留下的一些记录，提供参考。

> 下面的叙述中含有大量脚注，为了不影响已经非常熟悉的同学的阅读，我把一些说明放到了脚注里，方便阅读。

Repo我开在这里，欢迎大家点个Star或者Fork操作。<https://github.com/dashjay/http_demo.git>

本教程[^1]，使用6节课程/文章，尝试使用最简单的CPP知识实现一个高性能，简单的的HTTP Server。

1. [易]简单的Cmake的教程，选用一个Socket库[^2]并实现一个echo[^3]。[link](https://github.com/dashjay/http_demo/tree/1-cmake-socket-echo)
2. [易]定义HTTP请求和返回体的结构，构建并输出HTTP请求和返回体到标准输出。
3. [易]引入cpptoml从文件读取配置，引入spdlog尝试打log，帮助调试。
4. [难]定义一个bufReader类，并且使用该bufReader从TCP流中解析HTTP请求和返回体。
5. [易]实现主程序逻辑，监听端口，接收请求。
6. [选]实现其他HTTP附加功能。

我们要准备的东西(推荐）

- 一个顺手的IDE，推荐CLion，VScode，也可以用VIM，Emacs或你顺手的。
- 建议使用在Linux或MacOS下进行，因为我本地没Windows，没法为大家验证过程可行性。
- 你的耐心

- [1] 算不上什么教程，顺便测试一下脚注。
- [2] HTTP/GRPC 等等这样的协议通常底层采用TCP来实现，也就是学校里学到的那种面向流的服务，cpp中的系统调用实现的socker接口用起来有些不顺手。因此我找到了一个现代的Socket库<https://github.com/fpagliughi/sockpp>来帮助我们在这节课中完成HTTP服务器。
- [3] 你发给它什么，他就回复什么，常见的网络库都会用这个代替HELLOWORLD
