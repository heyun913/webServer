#include "../sylar/address.h"
#include "../sylar/log.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test() {
    // 域名转IP地址
    std::vector<sylar::Address::ptr> addrs;
    bool v = sylar::Address::Lookup(addrs, "www.baidu.com");
    if(!v) {
        SYLAR_LOG_ERROR(g_logger) << "lookup fail";
        return;
    }

    for(size_t i = 0; i < addrs.size(); ++ i) {
        SYLAR_LOG_INFO(g_logger) << i << " - " << addrs[i]->toString();
    }
}

void test_iface() {
    // 测试网卡
    std::multimap<std::string, std::pair<sylar::Address::ptr, uint32_t>> results;

    bool v = sylar::Address::GetInterfaceAddresses(results);

    if(!v) {
        SYLAR_LOG_ERROR(g_logger) << "GetInterfaceAddresses fail";
        return;
    }

    for(auto& i : results) {
        SYLAR_LOG_INFO(g_logger) << i.first << " - " << i.second.first->toString() << " - " << i.second.second;
    }
}

void test_ipv4() {
    // auto addr = sylar::IPAddress::Create("www.baidu.com");
    auto addr = sylar::IPAddress::Create("183.2.172.1");
    if(addr) {
        SYLAR_LOG_INFO(g_logger) << addr->toString();
    }
}

void test2() {
    auto addr = sylar::IPv4Address::Create("112.80.248.75", 80);
    auto saddr = addr->subnetMask(24);
    auto baddr = addr->broadcastAddress(24);
    auto naddr = addr->networkAddress(24);
    if (addr) {
        SYLAR_LOG_INFO(g_logger) << addr->toString();
    }
    if (saddr) {
        SYLAR_LOG_INFO(g_logger) << saddr->toString();
    }
    if (baddr) {
        SYLAR_LOG_INFO(g_logger) << baddr->toString();
    }
    if (naddr) {
        SYLAR_LOG_INFO(g_logger) << naddr->toString();
    }

}

void test3() {
    auto addr = sylar::IPv6Address::Create("fe80::215:5dff:fe20:e26a", 8020);
    if (addr) {
        SYLAR_LOG_INFO(g_logger) << addr->toString();
    }
    auto saddr = addr->subnetMask(64);
    auto baddr = addr->broadcastAddress(64);
    auto naddr = addr->networkAddress(64);
    if (addr) {
        SYLAR_LOG_INFO(g_logger) << addr->toString();
    }
    if (saddr) {
        SYLAR_LOG_INFO(g_logger) << saddr->toString();
    }
    if (baddr) {
        SYLAR_LOG_INFO(g_logger) << baddr->toString();
    }
    if (naddr) {
        SYLAR_LOG_INFO(g_logger) << naddr->toString();
    }

}

int main(int argc, char** argv) {
    // test();
    // test_iface();
    // test_ipv4();
    // hy();
    // test2();
    test3();
    return 0;
}