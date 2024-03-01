#ifndef __SYLAR_ADDRESS_H__
#define __SYLAR_ADDRESS_H__

#include <memory>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <iostream>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <map>

namespace sylar {

class IPAddress;
class Address {
public:
    typedef std::shared_ptr<Address> ptr;
    // 通过host返回所有的Address（IPv4/v6）
    static bool Lookup(std::vector<Address::ptr>& result, const std::string& host,
            int family = AF_INET, int type = 0, int protocol = 0);

    // 通过host返回任意Address
    static Address::ptr LookupAny(const std::string& host,
            int family = AF_INET, int type = 0, int protocol = 0);
    
    // 通过host返回任意IPAddress
    static std::shared_ptr<IPAddress>LookupAnyIPAdress(const std::string& host,
            int family = AF_INET, int type = 0, int protocol = 0);
            
    // 通过sockaddr创建地址
    static Address::ptr Create(const sockaddr* addr, socklen_t addrlen);

    // 返回本机所有网卡的<网卡名，地址，子网掩码位数>
    static bool GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result, int family = AF_UNSPEC);

    // 获取指定网卡的<网卡名，地址，子网掩码位数>
    static bool GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t>>& result, const std::string& iface, int family = AF_UNSPEC);       

    virtual ~Address() {}

    int getFamily() const;
    // 获得sockaddr指针
    virtual const sockaddr* getAddr() const = 0;

    virtual sockaddr* getAddr()  = 0;

    // 获得sockaddr长度
    virtual socklen_t getAddrLen() const = 0;
    // 可读性输出地址
    virtual std::ostream& insert(std::ostream& os) const = 0;
    std::string toString();

    bool operator<(const Address& rhs) const;
    bool operator==(const Address& rhs) const;
    bool operator!=(const Address& rhs) const;
};

class IPAddress : public Address {
public:
    typedef std::shared_ptr<IPAddress> ptr;
    // 通过IP、域名、服务器名,端口号创建IPAddress
    static IPAddress::ptr Create(const char* address, uint16_t port = 0);
    // 获得广播地址
    virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;
    // 获得网络地址
    virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;
    // 获得子网掩码地址
    virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;
    // 获得端口号
    virtual uint32_t getPort() const = 0;
    // 设置端口号
    virtual void setPort(uint16_t v) = 0;
};

class IPv4Address : public IPAddress {
public:
    typedef std::shared_ptr<IPv4Address> ptr;

    // 通过IP，端口号创建IPv4Address
    static IPv4Address::ptr Create(const char* address, uint16_t port = 0);

    IPv4Address(const sockaddr_in& address);
    // INADDR_ANY：接收消息的地址
    IPv4Address(uint32_t address = INADDR_ANY, uint16_t prot = 0);
    // 返回sockaddr指针
    const sockaddr* getAddr() const override;

    sockaddr* getAddr() override;

    // 返回sockaddr的长度
    socklen_t getAddrLen() const override;
    // 可读性输出地址
    std::ostream& insert(std::ostream& os) const override;
    // 返回广播地址
    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    // 返回网络地址
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    // 返回子网掩码地址
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void setPort(uint16_t v) override;
private:
    // Socket结构体
    sockaddr_in m_addr;
};

class IPv6Address : public IPAddress {
public:
    typedef std::shared_ptr<IPv6Address> ptr;
    // 通过IPv6地址字符串构造IPv6Address
    static IPv6Address::ptr Create(const char* address, uint16_t port = 0);
    IPv6Address();
    IPv6Address(const sockaddr_in6& address);
    // INADDR_ANY：接收消息的地址
    IPv6Address(const uint8_t address[16], uint16_t port = 0);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;
    // 返回广播地址
    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void setPort(uint16_t v) override;
private:
    sockaddr_in6 m_addr;
};


class UnixAddress : public Address {
public:
    typedef std::shared_ptr<UnixAddress> ptr;
    UnixAddress();
    UnixAddress(const std::string& path);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    void setAddrLen(uint32_t v);
    std::ostream& insert(std::ostream& os) const override;
private:
    // 结构体
    struct sockaddr_un m_addr;
    // 长度
    socklen_t m_length;
};

class UnknownAddress : public Address {
public:
    typedef std::shared_ptr<UnknownAddress> ptr;
    UnknownAddress(int family);
    UnknownAddress(const sockaddr& aadr);
    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;
private:
    sockaddr m_addr;
};

}



#endif