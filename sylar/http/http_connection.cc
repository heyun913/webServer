#include "http_connection.h"
#include "http_parser.h"
#include "sylar/log.h"
// #include "sylar/streams/zlib_stream.h"

namespace sylar {
namespace http {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

std::string HttpResult::toString() const {
    std::stringstream ss;
    ss << "[HttpResult result=" << result
       << " error=" << error
       << " response=" << (response ? response->toString() : "nullptr")
       << "]";
    return ss.str();
}

HttpConnection::HttpConnection(Socket::ptr sock, bool owner)
    :SocketStream(sock, owner) {
}

HttpConnection::~HttpConnection() {
    SYLAR_LOG_DEBUG(g_logger) << "HttpConnection::~HttpConnection";
}

// 接收响应报文
HttpResponse::ptr HttpConnection::recvResponse() {
    // 创建响应报文解析器
    HttpResponseParser::ptr parser(new HttpResponseParser);
    // 获取http请求缓冲区的大小
    uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
    // 使用智能指针托管一个读取buffer
    std::shared_ptr<char> buffer(
            new char[buff_size + 1], [](char* ptr){
                delete[] ptr;
            });
    char* data = buffer.get();
    // 读取偏移量
    int offset = 0;
    // 循环读取buffer中的数据
    do {
        // 返回读取的长度
        int len = read(data + offset, buff_size - offset);
        // 读取发生错误，返回空指针
        if(len <= 0) {
            close();
            return nullptr;
        }
        // 当前已经读取的数据长度
        len += offset;
        // 这里是为了防止使用的状态机解析http时，一个断言报错
        data[len] = '\0';
        // 解析缓冲区data中的数据
        // execute会将data向前移动nparse个字节，nparse为已经成功解析的字节数
        size_t nparse = parser->execute(data, len, false);
        // 解析失败返回空指针
        if(parser->hasError()) {
            close();
            return nullptr;
        }
        // data剩余的长度
        offset = len - nparse;
        // 缓冲区满了还没解析完
        if(offset == (int)buff_size) {
            close();
            return nullptr;
        }
        // 解析结束
        if(parser->isFinished()) {
            break;
        }
    } while(true);
    // 获取刚才的解析结果
    auto& client_parser = parser->getParser();
    // 消息体
    std::string body;
    // 是否分块传输
    if(client_parser.chunked) {
        // 缓冲区剩余数据
        int len = offset;
        do {
            // 是否为新的读取操作
            bool begin = true;
            do {
                // // 如果不是新的读取操作或者缓冲区数据已经读取完毕
                if(!begin || len == 0) {
                    // 从数据流中读取数据到缓冲区
                    int rt = read(data + len, buff_size - len);
                    // 如果读取失败或者连接关闭，则关闭连接并返回空指针
                    if(rt <= 0) {
                        close();
                        return nullptr;
                    }
                    // 更新缓冲区数据长度
                    len += rt;
                }
                data[len] = '\0';
                // 解析操作，和上面类似
                size_t nparse = parser->execute(data, len, true);
                if(parser->hasError()) {
                    close();
                    return nullptr;
                }
                len -= nparse;
                if(len == (int)buff_size) {
                    close();
                    return nullptr;
                }
                begin = false;
            } while(!parser->isFinished());
            //len -= 2;
            
            SYLAR_LOG_DEBUG(g_logger) << "content_len=" << client_parser.content_len;
            // 如果当前块的长度小于等于缓冲区数据长度
            if(client_parser.content_len + 2 <= len) {
                // 将当前块的数据追加到响应体中
                body.append(data, client_parser.content_len);
                // 移动数据，删除已经处理过的块
                memmove(data, data + client_parser.content_len + 2
                        , len - client_parser.content_len - 2);
                // 更新缓冲区数据长度
                len -= client_parser.content_len + 2;
            } else {
                // 如果当前块的长度大于缓冲区数据长度
                // 将缓冲区数据追加到响应体中
                body.append(data, len);
                // 计算剩余需要读取的数据长度
                int left = client_parser.content_len - len + 2;
                // 继续从数据流中读取数据直到读取到足够的数据为止
                while(left > 0) {
                    int rt = read(data, left > (int)buff_size ? (int)buff_size : left);
                    if(rt <= 0) {
                        close();
                        return nullptr;
                    }
                    // 将读取的数据追加到响应体中
                    body.append(data, rt);
                    // 更新剩余需要读取的数据长度
                    left -= rt;
                }
                // 删除末尾的结束符
                body.resize(body.size() - 2);
                // 更新缓冲区数据长度为0
                len = 0;
            }
        } while(!client_parser.chunks_done);
    } else {
        // 如果未使用分块传输编码
        // 获取消息体的长度
        int64_t length = parser->getContentLength();
        // 如果消息体长度大于0
        if(length > 0) {
            body.resize(length);
            // 初始化响应体数据的起始位置
            int len = 0;
            // 如果消息体长度大于等于缓冲区数据长度
            if(length >= offset) {
                memcpy(&body[0], data, offset);
                len = offset;
            } else {
                memcpy(&body[0], data, length);
                len = length;
            }
            // 计算剩余需要读取的数据长度
            length -= offset;
            if(length > 0) {
                // 从数据流中读取数据到响应体的剩余位置
                if(readFixSize(&body[len], length) <= 0) {
                    close();
                    return nullptr;
                }
            }
            parser->getData()->setBody(body);
        }
    }
    // if(!body.empty()) {
    //     auto content_encoding = parser->getData()->getHeader("content-encoding");
    //     SYLAR_LOG_DEBUG(g_logger) << "content_encoding: " << content_encoding
    //         << " size=" << body.size();
    //     if(strcasecmp(content_encoding.c_str(), "gzip") == 0) {
    //         // auto zs = ZlibStream::CreateGzip(false);
    //         // zs->write(body.c_str(), body.size());
    //         // zs->flush();
    //         // zs->getResult().swap(body);
    //     } else if(strcasecmp(content_encoding.c_str(), "deflate") == 0) {
    //         // auto zs = ZlibStream::CreateDeflate(false);
    //         // zs->write(body.c_str(), body.size());
    //         // zs->flush();
    //         // zs->getResult().swap(body);
    //     }
    //     parser->getData()->setBody(body);
    // }
    return parser->getData();
}

// 发送请求报文
int HttpConnection::sendRequest(HttpRequest::ptr rsp) {
    std::stringstream ss;
    ss << *rsp;
    std::string data = ss.str();
    //std::cout << ss.str() << std::endl;
    return writeFixSize(data.c_str(), data.size());
}

HttpResult::ptr HttpConnection::DoGet(const std::string& url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers
                            , const std::string& body) {
    Uri::ptr uri = Uri::Create(url);
    if(!uri) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL
                , nullptr, "invalid url: " + url);
    }
    return DoGet(uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoGet(Uri::ptr uri
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers
                            , const std::string& body) {
    return DoRequest(HttpMethod::GET, uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoPost(const std::string& url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers
                            , const std::string& body) {
    Uri::ptr uri = Uri::Create(url);
    if(!uri) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL
                , nullptr, "invalid url: " + url);
    }
    return DoPost(uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoPost(Uri::ptr uri
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers
                            , const std::string& body) {
    return DoRequest(HttpMethod::POST, uri, timeout_ms, headers, body);
}

// 执行HTTP请求，接受 HTTP 方法、URL、超时时间、请求头部和请求体作为参数，并返回一个 HttpResult::ptr 类型的智能指针。
HttpResult::ptr HttpConnection::DoRequest(HttpMethod method
                            , const std::string& url
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers
                            , const std::string& body) {
    // 根据传入的url创建一个uri对象
    Uri::ptr uri = Uri::Create(url);
    // 如果创建失败返回错误消息
    if(!uri) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL
                , nullptr, "invalid url: " + url);
    }
    return DoRequest(method, uri, timeout_ms, headers, body);
}


HttpResult::ptr HttpConnection::DoRequest(HttpMethod method
                            , Uri::ptr uri
                            , uint64_t timeout_ms
                            , const std::map<std::string, std::string>& headers
                            , const std::string& body) {
    // 创建一个HttpRequest对象
    HttpRequest::ptr req = std::make_shared<HttpRequest>();
    // 使用uri初始化创建一个HttpRequest对象
    req->setPath(uri->getPath());
    req->setQuery(uri->getQuery());
    req->setFragment(uri->getFragment());
    req->setMethod(method);
    bool has_host = false;
    // 处理请求头
    for(auto& i : headers) {
        // 如果请求头部中包含了 Connection 字段并且值为 keep-alive，则设置请求不关闭连接，并继续下一次循环。
        if(strcasecmp(i.first.c_str(), "connection") == 0) {
            if(strcasecmp(i.second.c_str(), "keep-alive") == 0) {
                req->setClose(false);
            }
            continue;
        }
        // 如果请求头部中包含了 Host 字段并且值非空，则将 has_host 设置为 true。
        if(!has_host && strcasecmp(i.first.c_str(), "host") == 0) {
            has_host = !i.second.empty();
        }
        // 设置请求头部中除 Connection 和 Host 之外的其他字段。
        req->setHeader(i.first, i.second);
    }
    // 如果请求头部中没有 Host 字段，则根据解析得到的 URI 设置 Host 字段。
    if(!has_host) {
        req->setHeader("Host", uri->getHost());
    }
    req->setBody(body);
    return DoRequest(req, uri, timeout_ms);
}

// 最终的实现
HttpResult::ptr HttpConnection::DoRequest(HttpRequest::ptr req
                            , Uri::ptr uri
                            , uint64_t timeout_ms) {
    // bool is_ssl = uri->getScheme() == "https";
    // 根据解析得到的 URI 创建地址对象
    Address::ptr addr = uri->createAddress();
    if(!addr) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_HOST
                , nullptr, "invalid host: " + uri->getHost());
    }
    // Socket::ptr sock = is_ssl ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
    // 创建TCP套接字
    Socket::ptr sock = Socket::CreateTCP(addr);
    if(!sock) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::CREATE_SOCKET_ERROR
                , nullptr, "create socket fail: " + addr->toString()
                        + " errno=" + std::to_string(errno)
                        + " errstr=" + std::string(strerror(errno)));
    }
    // 连接到目标主机
    if(!sock->connect(addr)) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::CONNECT_FAIL
                , nullptr, "connect fail: " + addr->toString());
    }
    // 设置套接字的接收超时时间。
    sock->setRecvTimeout(timeout_ms);
    // 创建一个 HttpConnection 对象，传入创建的套接字。
    HttpConnection::ptr conn = std::make_shared<HttpConnection>(sock);
    // 发送请求
    int rt = conn->sendRequest(req);
    if(rt == 0) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_CLOSE_BY_PEER
                , nullptr, "send request closed by peer: " + addr->toString());
    }
    if(rt < 0) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_SOCKET_ERROR
                    , nullptr, "send request socket error errno=" + std::to_string(errno)
                    + " errstr=" + std::string(strerror(errno)));
    }
    // 接收服务器的响应
    auto rsp = conn->recvResponse();

    if(!rsp) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::TIMEOUT
                    , nullptr, "recv response timeout: " + addr->toString()
                    + " timeout_ms:" + std::to_string(timeout_ms));
    }
    return std::make_shared<HttpResult>((int)HttpResult::Error::OK, rsp, "ok");

}

HttpConnectionPool::ptr HttpConnectionPool::Create(const std::string& uri
                                                   ,const std::string& vhost
                                                   ,uint32_t max_size
                                                   ,uint32_t max_alive_time
                                                   ,uint32_t max_request) {
    Uri::ptr turi = Uri::Create(uri);
    if(!turi) {
        SYLAR_LOG_ERROR(g_logger) << "invalid uri=" << uri;
    }
    return std::make_shared<HttpConnectionPool>(turi->getHost()
            , vhost, turi->getPort(), turi->getScheme() == "https"
            , max_size, max_alive_time, max_request);
}

HttpConnectionPool::HttpConnectionPool(const std::string& host
                                        ,const std::string& vhost
                                        ,uint32_t port
                                        ,bool is_https
                                        ,uint32_t max_size
                                        ,uint32_t max_alive_time
                                        ,uint32_t max_request)
    :m_host(host)
    ,m_vhost(vhost)
    ,m_port(port ? port : (is_https ? 443 : 80))
    ,m_maxSize(max_size)
    ,m_maxAliveTime(max_alive_time)
    ,m_maxRequest(max_request)
    ,m_isHttps(is_https) {
}

// 获得连接
HttpConnection::ptr HttpConnectionPool::getConnection() {
    // 记录当前的时间
    uint64_t now_ms = sylar::GetCurrentMS();
    // 存储非法连接
    std::vector<HttpConnection*> invalid_conns;
    // 定义一个空的连接指针
    HttpConnection* ptr = nullptr;
    // 加锁
    MutexType::Lock lock(m_mutex);
    // 如果HttpConnection指针链表不为空
    while(!m_conns.empty()) {
        // 取出第一个connection
        auto conn = *m_conns.begin();
        m_conns.pop_front();
        // 不在连接状态，放入非法vec中
        if(!conn->isConnected()) {
            invalid_conns.push_back(conn);
            continue;
        }
        // 已经超过了最大连接时间，放入非法vec中
        if((conn->m_createTime + m_maxAliveTime) > now_ms) {
            invalid_conns.push_back(conn);
            continue;
        }
        // 获得当前connection
        ptr = conn;
        break;
    }
    lock.unlock();
    // 删除非法连接
    for(auto i : invalid_conns) {
        delete i;
    }
    // 更新总连接数
    m_total -= invalid_conns.size();

    // 如果没有连接
    if(!ptr) {
        // 根据host创建地址
        IPAddress::ptr addr = Address::LookupAnyIPAdress(m_host); 
        if(!addr) {
            SYLAR_LOG_ERROR(g_logger) << "get addr fail: " << m_host;
            return nullptr;
        }
        // 设置端口号
        addr->setPort(m_port);
        // Socket::ptr sock = m_isHttps ? SSLSocket::CreateTCP(addr) : Socket::CreateTCP(addr);
        // 创建TCPSocket
        Socket::ptr sock = Socket::CreateTCP(addr);
        if(!sock) {
            SYLAR_LOG_ERROR(g_logger) << "create sock fail: " << *addr;
            return nullptr;
        }
        // 连接
        if(!sock->connect(addr)) {
            SYLAR_LOG_ERROR(g_logger) << "sock connect fail: " << *addr;
            return nullptr;
        }
        // 成功创建一个连接
        ptr = new HttpConnection(sock);
        ++m_total;
    }
    return HttpConnection::ptr(ptr, std::bind(&HttpConnectionPool::ReleasePtr
                               , std::placeholders::_1, this));
}

// 释放connection
void HttpConnectionPool::ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool) {
    // 请求次数+1
    ++ptr->m_request;
    // 已经关闭了链接，超时，超过最大请求数量
    if(!ptr->isConnected()
            || ((ptr->m_createTime + pool->m_maxAliveTime) >= sylar::GetCurrentMS())
            || (ptr->m_request >= pool->m_maxRequest)) {
        delete ptr;
        --pool->m_total;
        return;
    }
    // 重新放入连接池中
    MutexType::Lock lock(pool->m_mutex);
    pool->m_conns.push_back(ptr);
}

HttpResult::ptr HttpConnectionPool::doGet(const std::string& url
                          , uint64_t timeout_ms
                          , const std::map<std::string, std::string>& headers
                          , const std::string& body) {
    return doRequest(HttpMethod::GET, url, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doGet(Uri::ptr uri
                                   , uint64_t timeout_ms
                                   , const std::map<std::string, std::string>& headers
                                   , const std::string& body) {
    std::stringstream ss;
    ss << uri->getPath()
       << (uri->getQuery().empty() ? "" : "?")
       << uri->getQuery()
       << (uri->getFragment().empty() ? "" : "#")
       << uri->getFragment();
    return doGet(ss.str(), timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doPost(const std::string& url
                                   , uint64_t timeout_ms
                                   , const std::map<std::string, std::string>& headers
                                   , const std::string& body) {
    return doRequest(HttpMethod::POST, url, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doPost(Uri::ptr uri
                                   , uint64_t timeout_ms
                                   , const std::map<std::string, std::string>& headers
                                   , const std::string& body) {
    std::stringstream ss;
    ss << uri->getPath()
       << (uri->getQuery().empty() ? "" : "?")
       << uri->getQuery()
       << (uri->getFragment().empty() ? "" : "#")
       << uri->getFragment();
    return doPost(ss.str(), timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method
                                    , const std::string& url
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string>& headers
                                    , const std::string& body) {
    HttpRequest::ptr req = std::make_shared<HttpRequest>();
    req->setPath(url);
    req->setMethod(method);
    req->setClose(false);
    bool has_host = false;
    for(auto& i : headers) {
        if(strcasecmp(i.first.c_str(), "connection") == 0) {
            if(strcasecmp(i.second.c_str(), "keep-alive") == 0) {
                req->setClose(false);
            }
            continue;
        }

        if(!has_host && strcasecmp(i.first.c_str(), "host") == 0) {
            has_host = !i.second.empty();
        }

        req->setHeader(i.first, i.second);
    }
    if(!has_host) {
        if(m_vhost.empty()) {
            req->setHeader("Host", m_host);
        } else {
            req->setHeader("Host", m_vhost);
        }
    }
    req->setBody(body);
    return doRequest(req, timeout_ms);
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method
                                    , Uri::ptr uri
                                    , uint64_t timeout_ms
                                    , const std::map<std::string, std::string>& headers
                                    , const std::string& body) {
    std::stringstream ss;
    ss << uri->getPath()
       << (uri->getQuery().empty() ? "" : "?")
       << uri->getQuery()
       << (uri->getFragment().empty() ? "" : "#")
       << uri->getFragment();
    return doRequest(method, ss.str(), timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpRequest::ptr req
                                        , uint64_t timeout_ms) {
    auto conn = getConnection();
    if(!conn) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::POOL_GET_CONNECTION
                , nullptr, "pool host:" + m_host + " port:" + std::to_string(m_port));
    }
    auto sock = conn->getSocket();
    if(!sock) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::POOL_INVALID_CONNECTION
                , nullptr, "pool host:" + m_host + " port:" + std::to_string(m_port));
    }
    sock->setRecvTimeout(timeout_ms);
    int rt = conn->sendRequest(req);
    if(rt == 0) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_CLOSE_BY_PEER
                , nullptr, "send request closed by peer: " + sock->getRemoteAddress()->toString());
    }
    if(rt < 0) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_SOCKET_ERROR
                    , nullptr, "send request socket error errno=" + std::to_string(errno)
                    + " errstr=" + std::string(strerror(errno)));
    }
    auto rsp = conn->recvResponse();
    if(!rsp) {
        return std::make_shared<HttpResult>((int)HttpResult::Error::TIMEOUT
                    , nullptr, "recv response timeout: " + sock->getRemoteAddress()->toString()
                    + " timeout_ms:" + std::to_string(timeout_ms));
    }
    return std::make_shared<HttpResult>((int)HttpResult::Error::OK, rsp, "ok");
}

}
}
