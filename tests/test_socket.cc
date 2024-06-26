#include "../sylar/socket.h"
#include "../sylar/sylar.h"
#include "../sylar/iomanager.h"
#include "../sylar/address.h"

static sylar::Logger::ptr g_looger = SYLAR_LOG_ROOT();

void test_socket() {
    sylar::IPAddress::ptr addr = sylar::Address::LookupAnyIPAdress("www.baidu.com");
    if(addr) {
        SYLAR_LOG_INFO(g_looger) << "get address: " << addr->toString();
    } else {
        SYLAR_LOG_ERROR(g_looger) << "get address fail";
        return;
    }

    sylar::Socket::ptr sock = sylar::Socket::CreateTCP(addr);
    addr->setPort(80);
    if(!sock->connect(addr)) {
        SYLAR_LOG_ERROR(g_looger) << "connect " << addr->toString() << " fail";
        return;
    } else {
        SYLAR_LOG_INFO(g_looger) << "connect " << addr->toString() << " connected";
    }
    const char buff[] = "GET / HTTP/1.0\r\n\r\n";
    int rt = sock->send(buff, sizeof(buff));
    if(rt <= 0) {
        SYLAR_LOG_ERROR(g_looger) << "send fail rt = " << rt;
        return;
    }

    std::string buffs;
    buffs.resize(4096);
    rt = sock->recv(&buffs[0], buffs.size());

    if(rt <= 0) {
        SYLAR_LOG_ERROR(g_looger) << "recv fail rt = " << rt;
        return;
    }

    buffs.resize(rt);
    SYLAR_LOG_INFO(g_looger) << buffs;
}

int main(int argc, char** argv) {
    sylar::IOManager iom;
    iom.schedule(&test_socket);
    return 0;
}