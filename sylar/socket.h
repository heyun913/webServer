#ifndef __SYLAR_SOCKET_H__
#define __SYLAR_SOCKET_H__

#include <memory>
#include "address.h"
#include "noncopyable.h"

namespace sylar {

class Socket : public std::enable_shared_from_this<Socket>, Noncopyable {
public:
    typedef std::shared_ptr<Socket> ptr;
    typedef std::weak_ptr<Socket> weak_ptr;

    // socket类型
    enum Type {
        TCP = SOCK_STREAM,
        UDP = SOCK_DGRAM
    };
    // 协议簇
    enum Family {
        IPv4 = AF_INET,
        IPv6 = AF_INET6,
        UNIX = AF_UNIX
    };

    // 根据地址创建TCP套接字
    static Socket::ptr CreateTCP(sylar::Address::ptr address);
    // 根据地址创建UDP套接字
    static Socket::ptr CreateUDP(sylar::Address::ptr address);

    // 创建IPv4TCP套接字
    static Socket::ptr CreateTCPSocket();
    // 创建IPv4UDP套接字
    static Socket::ptr CreateUDPSocket();

    // 创建IPv6TCP套接字
    static Socket::ptr CreateTCPSocket6();
    // 创建IPv6UDP套接字
    static Socket::ptr CreateUDPSocket6();

    // 创建UnixTCP套接字
    static Socket::ptr CreateUnixTCPSocket();
    // 创建UnixUDP套接字
    static Socket::ptr CreateUnixUDPSocket();

    // 构造函数
    Socket(int family, int type, int protocol = 0);
    // 析构函数
    ~Socket();
    
    // 返回发送超时时间
    int64_t getSendTimeout();
    // 设置发送超时时间
    void setSendTimeout(int64_t v);

    // 获取接收超时时间
    int64_t getRecvTimeout();
    // 设置接收超时时间
    void setRecvTimeout(int64_t v);

    // 获取socketfd信息
    bool getOption(int level, int option, void* result, size_t* len);
    template<class T>
    bool getOption(int level, int option, T& result) {
        size_t length = sizeof(T);
        return getOption(level, option, &result, &length);
    }

    // 设置socketfd信息
    bool setOption(int level, int option, const void* result, size_t len);
    template<class T>
    bool setOption(int level, int option, const T& value) {
        return setOption(level, option, &value, sizeof(T));
    }

    

    // bool init(int sock);

    // 接收connect连接
    Socket::ptr accept();
    // 绑定地址
    bool bind(const Address::ptr addr);
    // 连接地址
    bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);
    // 监听地址
    bool listen(int backlog = SOMAXCONN);
    // 关闭socket
    bool close();

    // 发送数据：单数据块
    int send(const void* buffer, size_t length, int flag = 0);
    // 多数据块
    int send(const iovec* buffers, size_t length, int flag = 0);
    // 指定地址发送数据
    int sendTo(const void* buffer, size_t length, const Address::ptr to,int flag = 0);
    int sendTo(const iovec* buffers, size_t length, const Address::ptr to,int flag = 0);

    // 接收数据
    int recv(void* buffer, size_t length, int flag = 0);
    int recv(iovec* buffers, size_t length, int flag = 0);
    int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0);
    int recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags = 0);

    // 返回远端地址
    Address::ptr getRemoteAddress();
    // 返回本地地址
    Address::ptr getLocalAddress();

    // 获取协议簇
    int getFamily() const { return m_family; }
    // 获取类型
    int getType() const { return m_type; }
    // 获取协议
    int getProtocol() const { return m_protocol; }

    bool isConnected() const { return m_isConnected; }
    bool isValid() const;
    int getError();

    std::ostream& dump(std::ostream& os) const;
    int getSocket() const { return m_sock; }

    // 取消读事件
    bool cancelRead();
    // 取消写事件
    bool cancelWrite();
    // 取消Accept
    bool cancelAccept();
    // 取消全部事件
    bool cancelAll();

private:
    // 初始化socketfd
    void initSock();
    // 创建socketfd
    void newSock();
    // 初始化Socket对象
    bool init(int sock);

private:
    // socketfd
    int m_sock;
    // 协议簇
    int m_family;
    // 类型
    int m_type;
    // 协议
    int m_protocol;
    // 是否连接
    bool m_isConnected;
    // 本地地址
    Address::ptr m_localAddress;
    // 远端地址
    Address::ptr m_remoteAddress;
};

std::ostream& operator<<(std::ostream& os, const Socket& addr);

}

#endif