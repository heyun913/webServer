#include "address.h"
#include "log.h"
#include <sstream>
#include <netdb.h>
#include <ifaddrs.h>
// #include <stddef.h>

#include "endian.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

template<class T>
static T createMask(uint32_t bits) {
    return ~(1 << (sizeof(T) * 8 - bits)) - 1;
}

template<class T>
static uint32_t CountBytes(T value) {
    uint32_t result = 0;
    for(; value; ++ result) {
        value &= value - 1;
    }
    return result;
}

/*
    result   : Address结果存储容器
    host     ：域名地址
    family   : 地址簇 默认AF_UNSPEC（它通常用于指定地址族（Address Family）未指定的情况。在IPv4和IPv6地址族中，它表示任意地址族，即无论是IPv4还是IPv6地址都可以使用。）
    type     : socket的类型
    protocol : 协议
*/
bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host,
            int family, int type, int protocol) {
    // 定义3个addrinfo结构体
    // hints : 期望获取的地址信息的特征
    // results : 结果容器
    // next : 用来遍历results
    addrinfo hints, *results, *next;
    // 初始化hints
    hints.ai_flags = 0;
    hints.ai_family = family;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;
    hints.ai_addrlen = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    // 主机名
    std::string node;
    // 端口号
    const char* service = NULL;

    //  host = www.baidu.com:http / [2001:0db8:85a3:0000:0000:8a2e:0370:7334]:80
    //  检查 ipv6地址:
    if(!host.empty() && host[0] == '[') { 
        // 当 host = [2001:0db8:85a3:0000:0000:8a2e:0370:7334]:80就就会进入

        // 去寻找第一个']'的位置，找到了返回该指针
        // memchr在一段内存区域中搜索指定字符的第一次出现的位置
        const char* endipv6 = (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
        if(endipv6) {
            // 查看endipv6下个位置是否为 ： 确定是否有端口号
            if(*(endipv6 + 1) == ':') {
                // 跳过 ] 和 : 指向端口号的起始位置
                service = endipv6 + 2;
            }
            // 去掉[] node
            node = host.substr(1, endipv6 - host.c_str() - 1);
        }
    }

    // 检查 ipv4地址 www.baidu.com:http
    if(node.empty()) {
        // 找到第一个 ：
        service = (const char*)memchr(host.c_str(), ':', host.size());
        // 如果前面找到了一个:，判断后续是否还有：
        if(service) {
            // 如果没有：，表示当前：位置后面就是端口号
            if(!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
                node = host.substr(0, service - host.c_str());
                ++ service;
            }
        }
    }
    // 如果没设置端口号，就将host赋给他
    if(node.empty()) {
        node = host;
    }
    // 根据host获取IP地址，结果保存到results中
    int error = getaddrinfo(node.c_str(), service, &hints, &results);
    if(error) {
        SYLAR_LOG_ERROR(g_logger) << "Address::Lookup getaddress(" << host << ","
            << family << ", " << type << ") err = " << error << " errstr = "
            << strerror(errno);
        return false;
    }
    next = results;
    // 遍历Address容器
    while(next) {
        // 每次迭代中，调用 Create 函数创建一个 Address 对象，并将其添加到 result 向量中。
        result.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
        next = next->ai_next;
    }
    // 释放 getaddrinfo 函数中分配的内存，避免内存泄漏。
    freeaddrinfo(results);
    return true;
}

Address::ptr Address::LookupAny(const std::string& host,int family, int type, int protocol) {
    std::vector<Address::ptr> result;
    // 调用Lookup方法
    if(Lookup(result, host, family, type, protocol)) {
        // 返回第一个Address结果
        return result[0];
    }
    return nullptr;
}

IPAddress::ptr Address::LookupAnyIPAdress(const std::string& host,int family, int type, int protocol) {
    std::vector<Address::ptr> result;
    if(Lookup(result, host, family, type, protocol)) {
        for(auto& i : result) {
            // 和LookupAny差不多，多了一个转换步骤
            IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(i);
            if(v) {
                return v;
            }
        }
    }
    return nullptr;
}

/*
    std::multimap<std::string, std::pair<Address::ptr, uint32_t>> : <网卡名，（地址信息，端口号）>
*/
bool Address::GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result, int family) {
    // 定义两个ifaddrs类型的对象，next用于遍历，results用于存储结果
    struct ifaddrs *next, *results;
    // getifaddrs创建一个链表，链表上的每个节点都是一个struct ifaddrs结构，getifaddrs()返回链表第一个元素的指针
    if(getifaddrs(&results) != 0) {
        SYLAR_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses getifaddrs "
            << " err = " << errno << " errstr = " << strerror(errno);
        return false;
    }
    try {
        // results指向头节点，用next遍历
        for(next = results; next; next = next->ifa_next) {
            Address::ptr addr;
            uint32_t prefix_length = ~0u;
            // 检查当前接口的地址簇是否符合要求
            if(family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
                continue;
            }
            // 分别处理IPv4/6地址
            switch(next->ifa_addr->sa_family) {
                // IPv4
                case AF_INET:
                    {
                        // 创建ipv4地址
                        addr = Create(next->ifa_addr, sizeof(sockaddr_in));
                        // 获取掩码地址
                        uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                        // 获取前缀长度，网络地址的长度
                        prefix_length = CountBytes(netmask);
                    }
                    break;
                // IPv6
                case AF_INET6:
                    {
                        // 创建ipv6地址
                        addr = Create(next->ifa_addr, sizeof(sockaddr_in6));
                        // 计算掩码
                        in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                        // 在IPv6地址的计算中，每个字节（共16个字节）都有可能包含子网掩码的部分信息，所以要累计计算
                        prefix_length = 0;
                        for(int i = 0; i < 16; ++ i) {
                            prefix_length += CountBytes(netmask.s6_addr[i]);
                        }
                    }
                default:
                    break;
            }
            // 如果成功创建了地址，保存<网卡名，地址和前缀长度>到结果容器中
            if(addr) {
                result.insert(std::make_pair(next->ifa_name, std::make_pair(addr, prefix_length)));
            }
        }
    } catch(...) {
        SYLAR_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses exception";
        freeifaddrs(results);
        return false;
    }
    freeifaddrs(results);
    return true;
}
    
bool Address::GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t>>& result, const std::string& iface, int family) {
    // 如果传入的接口名称为空或者为通配符*，表示获取所有接口的地址信息
    if(iface.empty() || iface == "*") {
        // 然后根据参数 family 的值，选择性地获取IPv4或IPv6地址
        if(family == AF_INET || family == AF_UNSPEC) {
            result.push_back(std::make_pair(Address::ptr(new IPv4Address()), 0u));
        }
        if(family == AF_INET6 || family == AF_UNSPEC) {
            result.push_back(std::make_pair(Address::ptr(new IPv6Address()), 0u));
        }
        return true;
    }
    // 定义一个结果容器
    std::multimap<std::string, std::pair<Address::ptr, uint32_t>> results;
    // 获取失败
    if(!GetInterfaceAddresses(results, family)) {
        return false;
    }
    // 返回pair，first为第一个等于的迭代器，second为第一个大于的迭代器
    // 找到指定接口
    auto its = results.equal_range(iface);
    for(; its.first != its.second; ++ its.first) {
        // its.first->second等于std::pair<Address::ptr, uint32_t>
        result.push_back(its.first->second);
    }
    return true;
}

int Address::getFamily() const {
    return getAddr()->sa_family;
}

std::string Address::toString() const{
    std::stringstream ss;
    insert(ss);
    return ss.str();
}

Address::ptr Address::Create(const sockaddr* addr, socklen_t addrlen) {
    // 判断地址是否为空
    if(addr == nullptr) {
        return nullptr;
    }
    // 定义一个Address对象
    Address::ptr result;
    // 通过地址簇信息去初始化Address对象
    switch(addr->sa_family) {
        // IPv4
        case AF_INET:
            // 初始化一个IPv4地址
            // 把addr强制转换成sockaddr_in
            result.reset(new IPv4Address(*(const sockaddr_in*)addr));
            break;
        // IPv6
        case AF_INET6:
            // 初始化一个Ipv6地址
            result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
            break;
        // 未知类型
        default:
            result.reset(new UnknownAddress(*addr));
            break;
    }
    // 返回创建的地址
    return result;
}

bool Address::operator<(const Address& rhs) const {
    socklen_t minlen = std::min(getAddrLen(), rhs.getAddrLen());
    int result = memcmp(getAddr(), rhs.getAddr(), minlen);
    if(result < 0) {
        return true;
    } else if(result > 0) {
        return false;
    } else if(getAddrLen() < rhs.getAddrLen()) {
        return true;
    }
    return false;
}

bool Address::operator==(const Address& rhs) const {
    return getAddrLen() == rhs.getAddrLen()
        && memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

bool Address::operator!=(const Address& rhs) const {
    return !(*this == rhs);
}
/// IPAddress
IPAddress::ptr IPAddress::Create(const char* address, uint16_t port) {
    addrinfo hints, *results;
    memset(&hints, 0, sizeof(addrinfo));

    // hints.ai_flags = AI_NUMERICHOST;
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = AF_UNSPEC;

    int error = getaddrinfo(address, NULL, &hints, &results);
    if(error) {
        SYLAR_LOG_ERROR(g_logger) << "IPAddress::Create(" << address << ", "
            << port << ") error = " << errno << " errno = " << errno
            << " errstr = " << strerror(errno);
        return nullptr;
    }
    try {
        IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen));
        if(result) {
            result->setPort(port);
        }
        freeaddrinfo(results);
        return result;
    } catch (...) {
        freeaddrinfo(results);
        return nullptr;
    }
}

/// IPv4
IPv4Address::ptr IPv4Address::Create(const char* address, uint16_t port) {
    IPv4Address::ptr rt(new IPv4Address);
    rt->m_addr.sin_port = byteswapOnLittileEndian(port);
    // 将一个IP地址的字符串表示转换为网络字节序的二进制形式
    int result = inet_pton(AF_INET, address, &rt->m_addr.sin_addr);
    if(result <= 0) {
        SYLAR_LOG_ERROR(g_logger) << "IPv4Address::Create(" << address << ", "
            << port << ") rt = " << result << " errno = " << errno
            << " errstr = " << strerror(errno);
        return nullptr;
    }
    return rt;
}

IPv4Address::IPv4Address(const sockaddr_in& address) {
    m_addr = address;
}


IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = byteswapOnLittileEndian(port);
    m_addr.sin_addr.s_addr = byteswapOnLittileEndian(address);
}

const sockaddr* IPv4Address::getAddr() const {
    return (sockaddr*)&m_addr;
}

sockaddr* IPv4Address::getAddr() {
    return (sockaddr*)&m_addr;
}

socklen_t IPv4Address::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream& IPv4Address::insert(std::ostream& os) const {
    // 保证取到值的主机字节序
    uint32_t addr = byteswapOnLittileEndian(m_addr.sin_addr.s_addr);
    // 获取IPv4地址的第一个字节（最高位）
    os << ((addr >> 24) & 0xff) << "."
       << ((addr >> 16) & 0xff) << "."
       << ((addr >> 8) & 0xff) << "."
       << (addr & 0xff);
    // 取得端口号
    os << ":" << byteswapOnLittileEndian(m_addr.sin_port);
    return os;
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
    if(prefix_len > 32) {
        return nullptr;
    }
    sockaddr_in baddr(m_addr);
    // 主机号全为1
    baddr.sin_addr.s_addr |= byteswapOnLittileEndian(createMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {
    if(prefix_len > 32) {
        return nullptr;
    }
    sockaddr_in baddr(m_addr);
    // 主机号全为0
    baddr.sin_addr.s_addr &= byteswapOnLittileEndian(createMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
    sockaddr_in subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin_family = AF_INET;
    // 根据前缀长度获得子网掩码
    subnet.sin_addr.s_addr = ~byteswapOnLittileEndian(createMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(subnet));
}

uint32_t IPv4Address::getPort() const {
    return byteswapOnLittileEndian(m_addr.sin_port);
}

void IPv4Address::setPort(uint16_t v) {
    m_addr.sin_port = byteswapOnLittileEndian(v);
}

/// IPv6
IPv6Address::ptr IPv6Address::Create(const char* address, uint16_t port) {
    IPv6Address::ptr rt(new IPv6Address);
    rt->m_addr.sin6_port = byteswapOnLittileEndian(port);
    int result = inet_pton(AF_INET6, address, &rt->m_addr.sin6_addr);
    if(result <= 0) {
        SYLAR_LOG_ERROR(g_logger) << "IPv6Address::Create(" << address << ", "
            << port << ") rt = " << result << " errno = " << errno
            << " errstr = " << strerror(errno);
        return nullptr;
    }
    return rt;
}


IPv6Address::IPv6Address(const sockaddr_in6& address) {
    m_addr = address;
}

IPv6Address::IPv6Address() {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    m_addr.sin6_port = byteswapOnLittileEndian(port);
    memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
}

const sockaddr* IPv6Address::getAddr() const {
    return (sockaddr*)&m_addr;
}

sockaddr* IPv6Address::getAddr() {
    return (sockaddr*)&m_addr;
}

socklen_t IPv6Address::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream& IPv6Address::insert(std::ostream& os) const {
    os << "[";
    // m_addr.sin6_addr.s6_addr 是一个 unsigned char[16] 数组，存储了 IPv6 地址的二进制表示。
    uint16_t* addr = (uint16_t*)m_addr.sin6_addr.s6_addr;
    // 标记是否已经输出过连续的零段
    bool used_zeros = false;
    // 遍历 IPv6 地址的每个 16 比特段（共 8 个段）
    for(size_t i = 0; i < 8; ++ i) {
        // 如果当前段的值为 0，且之前未输出过连续的零段，则跳过当前段的输出。
        if(addr[i] == 0 && !used_zeros) {
            continue;
        }
        // 如果当前段不是第一个段且前一个段的值为 0，且之前未输出过连续的零段，则输出一个冒号 :，表示连续的零段已经开始。
        if(i && addr[i - 1] == 0 && !used_zeros) {
            os << ":";
            used_zeros = true;
        }
        // 如果当前段不是第一个段，则输出一个冒号 :，用于分隔每个段。
        if(i) {
            os << ':';
        }
        os << std::hex << (int)byteswapOnLittileEndian(addr[i]) << std::dec;
    }
    // 如果未输出过连续的零段且最后一个段的值为 0，则输出双冒号 ::，表示连续的零段结束。
    if(!used_zeros && addr[7] ==0 ) {
        os << "::";
    }
    os << "]:" << byteswapOnLittileEndian(m_addr.sin6_port);
    return os;
}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {
    sockaddr_in6 baddr(m_addr);
    baddr.sin6_addr.s6_addr[prefix_len / 8] |= createMask<uint8_t>(prefix_len % 8);
    for(int i = prefix_len / 8 + 1; i < 16; ++ i) {
        baddr.sin6_addr.s6_addr[i] = 0xff;
    }
    return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {
    sockaddr_in6 naddr(m_addr);
    /*	找到前缀长度结尾在第几个字节，在该字节在哪个位置。
     *	将该字节前剩余位置全部置为0	*/
    naddr.sin6_addr.__in6_u.__u6_addr8[prefix_len / 8] &=
        createMask<uint8_t>(prefix_len % 8);
	
    // 将后面其余字节置为0
    for (int i = prefix_len / 8 + 1; i < 16; ++i) {
        naddr.sin6_addr.__in6_u.__u6_addr8[i] = 0x00;
    }

    return IPv6Address::ptr(new IPv6Address(naddr));
}

IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {
    sockaddr_in6 subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin6_family = AF_INET6;
    subnet.sin6_addr.s6_addr[prefix_len / 8] = ~createMask<uint8_t>(prefix_len % 8);
    for(uint32_t i = 0; i < prefix_len / 8; ++ i) {
        subnet.sin6_addr.s6_addr[i] = 0xFF;
    }
    return IPv6Address::ptr(new IPv6Address(subnet));
}

uint32_t IPv6Address::getPort() const {
    return byteswapOnLittileEndian(m_addr.sin6_port);
}

void IPv6Address::setPort(uint16_t v) {
    m_addr.sin6_port = byteswapOnLittileEndian(v);
}
// Unix域套接字路径名的最大长度。-1是减去'\0'
static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;

/// Unix
UnixAddress::UnixAddress() {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    // sun_path的偏移量+最大路径
    m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string& path) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    // 加上'\0'的长度
    m_length = path.size() + 1;

    if(!path.empty() && path[0] == '\0') {
        -- m_length;
    }
    if(m_length > sizeof(m_addr.sun_path)) {
        throw std::logic_error("path too long");
    }
    // 将path放入结构体
    memcpy(m_addr.sun_path, path.c_str(), m_length);
    // 偏移量+路径长度
    m_length += offsetof(sockaddr_un, sun_path);
}

const sockaddr* UnixAddress::getAddr() const {
    return (sockaddr*)&m_addr;
}

sockaddr* UnixAddress::getAddr() {
    return (sockaddr*)&m_addr;
}

socklen_t UnixAddress::getAddrLen() const {
    return m_length;
}

std::ostream& UnixAddress::insert(std::ostream& os) const {
    if(m_length > offsetof(sockaddr_un, sun_path)
            && m_addr.sun_path[0] == '\0') {
        
        return os << "\\0" << std::string(m_addr.sun_path +  1,
                m_length - offsetof(sockaddr_un, sun_path) - 1);
    }
    return os << m_addr.sun_path;
}

void UnixAddress::setAddrLen(uint32_t v) {
    m_length = v;
}

/// UnknownAddress
UnknownAddress::UnknownAddress(const sockaddr& addr) {
    m_addr = addr;
}

UnknownAddress::UnknownAddress(int family) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sa_family = family;
}

const sockaddr* UnknownAddress::getAddr() const {
   return &m_addr;
}

sockaddr* UnknownAddress::getAddr() {
    return (sockaddr*)&m_addr;
}

socklen_t UnknownAddress::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream& UnknownAddress::insert(std::ostream& os) const {
    os << "[UnknownAddress family] = " << m_addr.sa_family << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Address& addr) {
    return addr.insert(os);
}

}