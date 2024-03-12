#ifndef __SYLAR_TCP_SERVER_H__
#define __SYLAR_TCP_SERVER_H__

#include <memory>
#include <functional>
#include "iomanager.h"
#include "socket.h"
#include "address.h"
#include "noncopyable.h"

namespace sylar {

class TcpServer : public std::enable_shared_from_this<TcpServer>, Noncopyable {
public:
    typedef std::shared_ptr<TcpServer> ptr;
    // 构造函数
    TcpServer(sylar::IOManager* worker = sylar::IOManager::GetThis(), sylar::IOManager* accept_worker = sylar::IOManager::GetThis());
    virtual ~TcpServer();
    // 绑定地址以及监听
    virtual bool bind(sylar::Address::ptr addr);
    virtual bool bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails);
    // 服务器开始监听待连接的客户端
    virtual bool start();
    // 停止服务器
    virtual void stop();

    uint64_t getReadTimeout() const { return m_recvTimeout; }
    std::string getName() const { return m_name; }
    void setReadTimeout(uint64_t v) { m_recvTimeout = v; } 
    void setName(const std::string& v) { m_name = v; }

    bool isStop() const { return m_isStop; }
protected:
    virtual void handleClient(Socket::ptr client);
    virtual void startAccept(Socket::ptr sock);

private:
    // 多监听，多网卡
    std::vector<Socket::ptr> m_socks;
    // 新连接的Socket工作的调度器, IOManager就是线程池
    IOManager* m_worker;
    // 服务器Socket接收连接的调度器 
    IOManager* m_acceptWorker;
    // 接收超时时间
    uint64_t m_recvTimeout;
    // 服务器名称
    std::string m_name;
    // 服务是否停止
    bool m_isStop; 
};

}

#endif