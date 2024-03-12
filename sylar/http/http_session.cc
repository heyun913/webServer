#include "http_session.h"
#include "http_parser.h"

namespace sylar {
namespace http {

// 第一个参数接收一个Socket，第二个参数表示在析构时是否需要程序自定中断连接。
HttpSession::HttpSession(Socket::ptr sock, bool owner)
    :SocketStream(sock, owner) {
}

// 接收请求报文
HttpRequest::ptr HttpSession::recvRequest() {
    // 定义一个解析Http请求的对象
    HttpRequestParser::ptr parser(new HttpRequestParser);
    // 确定缓冲区大小
    uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
    // 创建一个缓冲区
    std::shared_ptr<char> buffer(
            new char[buff_size], [](char* ptr){
                delete[] ptr;
            });
    // 获取刚刚创建的缓冲区的原始指针
    char* data = buffer.get();
    // 偏移量
    int offset = 0;
    do {
        // 从连接中读取数据，从当前偏移位置开始，最多读取 buff_size - offset 字节， len为已经读取的字节
        int len = read(data + offset, buff_size - offset);
        // 读取异常
        if(len <= 0) {
            close();
            return nullptr;
        }
        // 由于数据是从offset位置开始写入的，我们需要将offset添加到len上，以得到从缓冲区开始到最新读取数据末尾的总长度。
        len += offset;
        // 解析刚才读取的数据，返回已经解析过的数据
        size_t nparse = parser->execute(data, len);
        // 判断解析是否异常
        if(parser->hasError()) {
            close();
            return nullptr;
        }
        // 剩余待解析的位置
        offset = len - nparse;
        // 缓冲区满了还没解析完
        if(offset == (int)buff_size) {
            close();
            return nullptr;
        }
        // 检查解析是否完成
        if(parser->isFinished()) {
            break;
        }
    } while(true);
    // 获取HTTP请求的内容长度，body
    int64_t length = parser->getContentLength();
    // 如果有则去处理
    if(length > 0) {
        std::string body;
        body.resize(length);
        // 将已读取的数据从data复制到body的开头
        int len = 0;
        // 如果body长度比缓冲区剩余的还大，将缓冲区全部加进来
        if(length >= offset) {
            memcpy(&body[0], data, offset);
            len = offset;
        } else {
            // 否则将取length
            memcpy(&body[0], data, length);
            len = length;
        }
        // 从原始的内容长度中减去已经复制的字节数，得到还需要读取的字节数。
        length -= offset;
        // 缓冲区里的数据也不够，继续读取直到满足length
        if(length > 0) {
            // 读取剩余的内容
            if(readFixSize(&body[len], length) <= 0) {
                close();
                return nullptr;
            }
        }
        // 设置body
        parser->getData()->setBody(body);
    }

    // parser->getData()->init();
    //返回解析完的HttpRequest
    return parser->getData();
}

// 发送响应报文
int HttpSession::sendResponse(HttpResponse::ptr rsp) {
    std::stringstream ss;
    ss << *rsp;
    std::string data = ss.str();
    return writeFixSize(data.c_str(), data.size());
}

}
}
