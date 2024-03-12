#include "http_server.h"
#include "sylar/log.h"
// #include "sylar/http/servlets/config_servlet.h"
// #include "sylar/http/servlets/status_servlet.h"

namespace sylar {
namespace http {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");


HttpServer::HttpServer(bool keepalive
               ,sylar::IOManager* worker
               ,sylar::IOManager* io_worker
               ,sylar::IOManager* accept_worker) 
        :TcpServer(worker, accept_worker)
        ,m_isKeepalive(keepalive) {
         m_dispatch.reset(new ServletDispatch);
}


//     m_type = "http";
//     m_dispatch->addServlet("/_/status", Servlet::ptr(new StatusServlet));
//     m_dispatch->addServlet("/_/config", Servlet::ptr(new ConfigServlet));
// }

// void HttpServer::setName(const std::string& v) {
//     TcpServer::setName(v);
//     m_dispatch->setDefault(std::make_shared<NotFoundServlet>(v));
// }

void HttpServer::handleClient(Socket::ptr client) {
    SYLAR_LOG_DEBUG(g_logger) << "handleClient " << *client;
    // 创建会话
    HttpSession::ptr session(new HttpSession(client));
    do {
        // 获取解析完的HttpRequest
        auto req = session->recvRequest();
        // 如果接收失败
        if(!req) {
            SYLAR_LOG_DEBUG(g_logger) << "recv http request fail, errno="
                << errno << " errstr=" << strerror(errno)
                << " cliet:" << *client << " keep_alive=" << m_isKeepalive;
            break;
        }
        // 创建响应对象
        HttpResponse::ptr rsp(new HttpResponse(req->getVersion()
                            ,req->isClose() || !m_isKeepalive));
        // 使用servlet处理HTTP请求，并生成响应结果
        m_dispatch->handle(req, rsp, session);
        // 发送响应
        session->sendResponse(rsp);

        if(!m_isKeepalive || req->isClose()) {
            break;
        }
    } while(true);
    // 关闭会话
    session->close();
}

}
}
