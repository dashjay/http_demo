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
    std::cout << "try accept a conn at port " << port << '\n';
    // 接收连接请求
    sockpp::tcp_socket sock = acc.accept();

    char buf[1024];
    
    // 这个进程是一个单线程，只能处理一个请求来源，并且通过 for 循环，从对应的 fd 中取出从网卡收到的信息
    // 在后面的课程中，我们会设计多线程io复用模型
    for(;;){
        if(auto siz = sock.read(buf, 1024); siz > 0){
            print_without_crlf(buf);
            sock.write(buf, siz);
        }else{
            break;
        }
    }
    std::cout << "closed" << '\n';
}

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