# 从零开发该框架的的笔记内容
http://t.csdnimg.cn/An33n

# C++高性能服务器框架

主要模块介绍

## 线程模块

由于框架的定位是高并发服务器框架，所以基于pthread库实现了线程模块，C++11里面的Thread也是基于pthread实现，但是它不够完善，比如没有读写锁，而对于高并发服务器来说，很多数据大多数时候都是写少读多的场景，没有读写锁性能上的损失就非常大。

## 协程模块

使用非对称协程模型，也就是子协程只能和线程主协程切换，而不能和另一个子协程切换，并且在程序结束时，一定要再切回主协程，以保证程序能正常结束，

## 协程调度模块

实现了一个N-M的协程调度器，N个线程运行M个协程，协程可以在线程之间进行切换，也可以绑定到指定线程运行。

实现协程调度之后，可以解决前一章协程模块中子协程不能运行另一个子协程的缺陷，子协程可以通过向调度器添加调度任务的方式来运行另一个子协程。

## IO调度模块

继承自协程调度器，封装了epoll，支持为socket fd注册读写事件回调函数。

IO协程调度还解决了调度器在idle状态下忙等待导致CPU占用率高的问题。IO协程调度器使用一对管道fd来tickle调度协程，当调度器空闲时，idle协程通过epoll_wait阻塞在管道的读描述符上，等管道的可读事件。添加新任务时，tickle方法写管道，idle协程检测到管道可读后退出，调度器执行调度。

## 定时器模块

基于epoll超时实现定时器功能，精度毫秒级，支持在指定超时时间结束之后执行回调函数。

## HOOK模块

hook系统底层和socket相关的API，socket IO相关的API，以及sleep系列的API。hook的开启控制是线程粒度的，可以自由选择。通过hook模块，可以使一些不具异步功能的API，展现出异步的性能，如MySQL。

## Address模块

提供网络地址相关的类，支持与网络地址相关的操作

## Socket模块

套接字类，表示一个套接字对象。

## ByteArray模块

字节数组容器，提供基础类型的序列化与反序列化功能。

ByteArray的底层存储是固定大小的块，以链表形式组织。每次写入数据时，将数据写入到链表最后一个块中，如果最后一个块不足以容纳数据，则分配一个新的块并添加到链表结尾，再写入数据。ByteArray会记录当前的操作位置，每次写入数据时，该操作位置按写入大小往后偏移，如果要读取数据，则必须调用setPosition重新设置当前的操作位置。

## Stream模块

流结构，提供字节流读写接口。

所有的流结构都继承自抽象类Stream，Stream类规定了一个流必须具备read/write接口和readFixSize/writeFixSize接口，继承自Stream的类必须实现这些接口。

## TcpServer模块

TCP服务器封装。

TcpServer类支持同时绑定多个地址进行监听，只需要在绑定时传入地址数组即可。TcpServer还可以分别指定接收客户端和处理客户端的协程调度器。

## HTTP模块

提供HTTP服务，主要包含以下几个模块：

HTTP常量定义，包括HTTP方法HttpMethod与HTTP状态HttpStatus。
HTTP请求与响应结构，对应HttpRequest和HttpResponse。
HTTP解析器，包含HTTP请求解析器与HTTP响应解析器，对应HttpRequestParser和HttpResponseParser。
HTTP会话结构，对应HttpSession。
HTTP服务器。
HTTP Servlet。
HTTP客户端HttpConnection，用于发起GET/POST等请求，支持连接池。
HTTP模块依赖nodejs/http-parser提供的HTTP解析器，并且直接复用了nodejs/http-parser中定义的HTTP方法与状态枚举。

