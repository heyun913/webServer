#include "sylar/tcp_server.h"
#include "sylar/log.h"
#include "sylar/iomanager.h"
#include "sylar/bytearray.h"
#include "sylar/address.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

// EchoServer继承于TcpServer
class EchoServer : public sylar::TcpServer {
public:
    EchoServer(int type);
    // 重写
    void handleClient(sylar::Socket::ptr client);

private:
    // 控制服务器行为：是否以文本或二进制形式输出接收到的数据
    int m_type = 0;
};

EchoServer::EchoServer(int type)
    :m_type(type) {
}

// 处理客户端连接
void EchoServer::handleClient(sylar::Socket::ptr client) {
    // 日志打印正在处理的来自某个客户端的连接
    SYLAR_LOG_INFO(g_logger) << "handleClient " << *client;   
    // 存储从客户端接收到的数据
    sylar::ByteArray::ptr ba(new sylar::ByteArray);
    // 循环接收数据
    while(true) {
        // 清空缓冲区
        ba->clear();
        /* struct iovec
            {
                void *iov_base;	 Pointer to data.  
                size_t iov_len;	 Length of data.  
            };
        */
        std::vector<iovec> iovs;
        // 用于接收客户端发送的数,这里指定了每个iovec的大小为1024字节。
        ba->getWriteBuffers(iovs, 1024);
        // 接收数据， rt保存实际接收到的字节数
        int rt = client->recv(&iovs[0], iovs.size());
        if(rt == 0) {
            SYLAR_LOG_INFO(g_logger) << "client close: " << *client;
            break;
        } else if(rt < 0) {
            SYLAR_LOG_INFO(g_logger) << "client error rt=" << rt
                << " errno=" << errno << " errstr=" << strerror(errno);
            break;
        }
        // 移动位置指针在接收到的数据后面，不能删除，防止新接收的数据覆盖之前的数据
        ba->setPosition(ba->getPosition() + rt);
        // 设置为0方便从头读取或处理数据
        ba->setPosition(0);
        // SYLAR_LOG_INFO(g_logger) << "recv rt=" << rt << " data=" << std::string((char*)iovs[0].iov_base, rt);
        if(m_type == 1) {//text 
            std::cout << ba->toString() << std::endl;
            // SYLAR_LOG_INFO(g_logger) << ba->toString();
        } else {
            std::cout << ba->toHexString() << std::endl;
            // SYLAR_LOG_INFO(g_logger) << ba->toHexString();
        }
        std::cout.flush();
    }
}

int type = 1;

void run() {
    SYLAR_LOG_INFO(g_logger) << "server type=" << type;
    EchoServer::ptr es(new EchoServer(type));
    // 解析传入的字符串地址 "0.0.0.0"是一个特殊的IP地址，表示服务器应该监听所有可用的网络接口。
    auto addr = sylar::Address::LookupAny("0.0.0.0:8020");
    while(!es->bind(addr)) {
        sleep(2);
    }
    es->start();
}

int main(int argc, char** argv) {
    if(argc < 2) {
        SYLAR_LOG_INFO(g_logger) << "used as[" << argv[0] << " -t] or [" << argv[0] << " -b]";
        return 0;
    }

    if(!strcmp(argv[1], "-b")) {
        type = 2;
    }

    sylar::IOManager iom(2);
    iom.schedule(run);
    return 0;
}
