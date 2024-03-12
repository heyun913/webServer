#include "tcp_server.h"
#include "config.h"
#include "log.h"

namespace sylar {

static sylar::ConfigVar<uint64_t>::ptr g_tcp_server_read_timeout =
    sylar::Config::Lookup("tcp_server.read_timeout", (uint64_t)(60 * 1000 * 2),
        "tcp server read timeout");

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

TcpServer::TcpServer(sylar::IOManager* worker, sylar::IOManager* accept_worker)
    :m_worker(worker)
    ,m_acceptWorker(accept_worker)
    ,m_recvTimeout(g_tcp_server_read_timeout->getValue())
    ,m_name("sylar/1.0.0")
    ,m_isStop(true) {

}

TcpServer::~TcpServer() {
    for(auto& i : m_socks) {
        i->close();
    }
    m_socks.clear();
}

// 绑定单个地址
bool TcpServer::bind(sylar::Address::ptr addr) {
    // 绑定成功的地池容器
    std::vector<Address::ptr> addrs;
    // 绑定失败的地址容器
    std::vector<Address::ptr> fail;
    addrs.push_back(addr);
    return bind(addrs, fail);
}

// 绑定多个地址容器
bool TcpServer::bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails) {
    //  m_ssl = ssl;
    // 遍历传入的地址容器
    for(auto& addr : addrs) {
        // Socket::ptr sock = ssl ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
        // 调用之前封装好的hook函数，创建一个TCP连接
        Socket::ptr sock = Socket::CreateTCP(addr);
        // 绑定失败
        if(!sock->bind(addr)) {
            SYLAR_LOG_ERROR(g_logger) << "bind fail errno="
                << errno << " errstr=" << strerror(errno)
                << " addr=[" << addr->toString() << "]";
            // 记录当前失败的地址
            fails.push_back(addr);
            // 继续下一个地址
            continue;
        }
        // 监听失败
        if(!sock->listen()) {
            SYLAR_LOG_ERROR(g_logger) << "listen fail errno="
                << errno << " errstr=" << strerror(errno)
                << " addr=[" << addr->toString() << "]";
            fails.push_back(addr);
            continue;
        }
        // 绑定、监听都成功
        m_socks.push_back(sock);
    }
    // 如果绑定失败的地址容器不为空，bind调用函数返回false，清空所有Socket
    if(!fails.empty()) {
        m_socks.clear();
        return false;
    }
    // 终端打印出绑定好的地址
    for(auto& i : m_socks) {
        // SYLAR_LOG_INFO(g_logger) << "type=" << m_type
        //     << " name=" << m_name
        //     << " ssl=" << m_ssl
        SYLAR_LOG_INFO(g_logger)  << " server bind success: " << *i;
    }
    return true;
}

// 服务器不断监听传入的Socket对象，等待并接收客户端的连接
void TcpServer::startAccept(Socket::ptr sock) {
    // 如果服务器没有停止
    while(!m_isStop) {
        // 接受一个新的客户端连接
        Socket::ptr client = sock->accept();
        // 成功接收了一个连接
        if(client) {
            // 设置服务器等待客户端发送数据的最大时间
            client->setRecvTimeout(m_recvTimeout);
            // m_ioWorker->schedule(std::bind(&TcpServer::handleClient,
            //             shared_from_this(), client));
            // 将handleClient加入到工作线程队列m_worker中
            m_worker->schedule(std::bind(&TcpServer::handleClient,
                        shared_from_this(), client));
        } else {
            SYLAR_LOG_ERROR(g_logger) << "accept errno=" << errno
                << " errstr=" << strerror(errno);
        }
    }
}

bool TcpServer::start() {
    if(!m_isStop) {
        return true;
    }
    m_isStop = false;
    for(auto& sock : m_socks) {
        // 异步执行startAccept
        m_acceptWorker->schedule(std::bind(&TcpServer::startAccept,
                    shared_from_this(), sock));
    }
    return true;
}

void TcpServer::stop() {
    // 标记服务器为停止状态
    m_isStop = true;
    // 使用shared_from_this()获取当前TcpServer对象的共享指针，并将其存储在局部变量self中。
    // 这是为了确保在异步任务执行期间TcpServer对象不会被销毁。
    auto self = shared_from_this();
    // 这个lambda表达式捕获了this指针（即当前对象的指针）和self（即TcpServer的共享指针）。
    m_acceptWorker->schedule([this, self]() {
        for(auto& sock : m_socks) {
            sock->cancelAll();
            sock->close();
        }
        m_socks.clear();
    });
}

void TcpServer::handleClient(Socket::ptr client) {
    SYLAR_LOG_INFO(g_logger) << "handleClient: " << *client;
}


}