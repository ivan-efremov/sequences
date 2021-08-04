/**
 * @file TcpServer.cpp
 */

#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <iostream>
#include <cstring>
#include <cerrno>
#include <stdexcept>

#include "TcpServer.h"


/**
 * class CtxConnection.
 */
CtxConnection::CtxConnection():
    m_readyWrite(false),
    m_exportSeq(false)
{
    m_strin.reserve(1024);
    m_strout.reserve(16 * 4096);
}


/**
 * class BaseTcpServer.
 */
std::mutex BaseTcpServer::s_lock;


BaseTcpServer::BaseTcpServer(
    const std::string& a_addr,
    const std::string& a_port)
:
    m_address(a_addr),
    m_port(a_port),
    m_context(),
    m_poolThreads(),
    m_events(nullptr),
    m_sfd(-1),
    m_efd(-1),
    m_running(true)
{
}

BaseTcpServer::~BaseTcpServer()
{
    if(m_sfd != -1) {
        close(m_sfd);
    }
    if(m_efd != -1) {
        close(m_efd);
    }
    if(m_events) {
        free(m_events);
    }
}

void BaseTcpServer::sockInit()
{
    struct addrinfo addr, *res = nullptr;

    bzero(&addr, sizeof(addr));
    addr.ai_family = AF_INET;
    addr.ai_socktype = SOCK_STREAM;
    addr.ai_flags = AI_PASSIVE;

    int err = getaddrinfo(m_address.c_str(), m_port.c_str(), &addr, &res);
    if(err != 0) {
        syserror("getaddrinfo");
    }
     
    m_sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(m_sfd == -1) {
        freeaddrinfo(res);
        syserror("socket");
    }

    int yes = 1;
    if(setsockopt(m_sfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) == -1) {
        close(m_sfd);
        m_sfd = -1;
        freeaddrinfo(res);
        syserror("setsockopt");
    }

    if(bind(m_sfd, res->ai_addr, res->ai_addrlen) == -1) {
        close(m_sfd);
        m_sfd = -1;
        freeaddrinfo(res);
        syserror("bind");
    }
    freeaddrinfo(res);
}

void BaseTcpServer::setNonBlocking(int a_fd)
{
    int flags = fcntl(a_fd, F_GETFL, 0);
    if(flags == -1) {
        flags = 0;
    }
    flags |= O_NONBLOCK;
    if(fcntl(a_fd, F_SETFL, flags) == -1) {
        syserror("fcntl");
    }
}

void BaseTcpServer::syserror(const std::string& a_errmsg)
{
    std::string errmsg(a_errmsg);
    errmsg += ": ";
    errmsg += std::to_string(errno);
    errmsg += ": ";
    errmsg += strerror(errno);
    throw std::runtime_error(errmsg);
}

void BaseTcpServer::listenInit()
{
    if(listen(m_sfd, 1024) == -1) {
        syserror("listen");
    }
}

void BaseTcpServer::epollInit()
{
    struct epoll_event event;
    
    m_efd = epoll_create1(0);
    if(m_efd == -1) {
        syserror("epoll_create1");
    }
    
    event.data.fd = m_sfd;
    event.events = EPOLLIN | EPOLLET;
    
    if(epoll_ctl(m_efd, EPOLL_CTL_ADD, m_sfd, &event) == -1) {
        syserror("epoll_ctl: EPOLL_CTL_ADD");
    }

    m_events = (epoll_event*) calloc(MAXEVENTS, sizeof(epoll_event));
    if(m_events == nullptr) {
        syserror("m_events: out of memory");
    }
}

void BaseTcpServer::doAccept()
{
    struct epoll_event event;
    struct sockaddr client;
    socklen_t clientlen = sizeof(client);
    int clientfd = accept(m_sfd, &client, &clientlen);
    if(clientfd == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return;
        } else {
            syserror("accept");
        }
    }
    setNonBlocking(clientfd);
    std::lock_guard<std::mutex> lock(s_lock);
    onAccept(clientfd);
    event.data.fd = clientfd;
    event.events = EPOLLIN | EPOLLET;
    if(epoll_ctl(m_efd, EPOLL_CTL_ADD, clientfd, &event) == -1) {
        syserror("epoll_ctl: EPOLL_CTL_ADD");
    }
}

void BaseTcpServer::doRead(int a_fd)
{
    char buf[BUFFERSIZE];
    ssize_t count = read(a_fd, buf, sizeof(buf));
    if(count == -1) {
        if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            std::lock_guard<std::mutex> lock(s_lock);
            doClose(a_fd);
            return;
        }
    } else if(count == 0) {
        std::lock_guard<std::mutex> lock(s_lock);
        doClose(a_fd);
        return;
    }
    buf[count] = 0;
    std::lock_guard<std::mutex> lock(s_lock);
    onRead(a_fd, buf, count);
    if(m_context.at(a_fd)->m_readyWrite) {
        struct epoll_event event;
        event.data.fd = a_fd;
        event.events = EPOLLIN | EPOLLOUT | EPOLLET;
        epoll_ctl(m_efd, EPOLL_CTL_MOD, a_fd, &event);
    }
}

void BaseTcpServer::doWrite(int a_fd)
{
    std::lock_guard<std::mutex> lock(s_lock);
    struct epoll_event event;
    auto &ctx = m_context.at(a_fd);
    auto &strout = ctx->m_strout;
    if(!strout.empty()) {
        ssize_t count = write(a_fd, strout.data(), strout.size());
        if(count == -1) {
            if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                doClose(a_fd);
                return;
            }
        } else if(count == 0) {
            doClose(a_fd);
            return;
        }
        onWrite(a_fd, count);
    }
    if(ctx->m_readyWrite) {
        event.data.fd = a_fd;
        event.events = EPOLLIN | EPOLLOUT | EPOLLET;
        epoll_ctl(m_efd, EPOLL_CTL_MOD, a_fd, &event);
    } else {
        event.data.fd = a_fd;
        event.events = EPOLLIN | EPOLLET;
        epoll_ctl(m_efd, EPOLL_CTL_MOD, a_fd, &event);
    }
}

void BaseTcpServer::doClose(int a_fd)
{
    onClose(a_fd);
    //epoll_ctl(m_efd, EPOLL_CTL_DEL, a_fd, nullptr);
    shutdown(a_fd, SHUT_RDWR);
    close(a_fd);
}

void BaseTcpServer::run()
{
    sockInit();
    setNonBlocking(m_sfd);
    listenInit();
    epollInit();
    startThread();
    joinThread();
}

void BaseTcpServer::stop()
{
    m_running = false;
}

void BaseTcpServer::startThread()
{
    unsigned int nThreads = std::thread::hardware_concurrency();
    
    if(nThreads == 0U) {
        nThreads = 1U;
    }
    
    for(unsigned int i = 0; i < nThreads; ++i) {
        m_poolThreads.emplace_back(
            std::thread([this]() { runThread(); })
        );
    }
}

void BaseTcpServer::runThread()
{
    while(m_running) try {
        int n = epoll_wait(m_efd, m_events, MAXEVENTS, 500);
        if(n == -1 && errno != EINTR) {
            syserror("epoll_wait");
        }
        for(int i = 0; i < n; ++i) {
            const uint32_t events = m_events[i].events;
            const int eventfd = m_events[i].data.fd;
            if(events & (EPOLLERR | EPOLLHUP)) {
                doClose(eventfd);
            } else if(events & EPOLLIN) {
                if(eventfd == m_sfd) {
                    doAccept();
                } else {
                    doRead(eventfd);
                }
            } else if(events & EPOLLOUT) {
                doWrite(eventfd);
            } else {
            #ifdef DEBUG
                std::cout << "Undefined events: " << events << std::endl;
            #endif
            }
        } ///< for
    } catch(const std::exception& err) { ///< while
        std::cerr << "Epoll loop error: " << err.what() << std::endl;
    }
}

void BaseTcpServer::joinThread()
{
    for(auto &thread : m_poolThreads) {
        if(thread.joinable()) {
            thread.join();
        }
    }
    m_poolThreads.clear();
}

void BaseTcpServer::onAccept(int a_fd)
{
#ifdef DEBUG
    std::cout << "onAccept: " << a_fd << std::endl;
#endif
    m_context[a_fd] = std::make_shared<CtxConnection>();
}

void BaseTcpServer::onRead(int a_fd, const char *a_buf, size_t a_size)
{
#ifdef DEBUG
    std::cout << "onRead: " << a_fd << ": " << a_size << std::endl;
#endif
    auto &ctx = m_context.at(a_fd);
    auto &strin = ctx->m_strin;
    auto &strout = ctx->m_strout;
    strin.append(a_buf, a_size);
    while(1) {
        const size_t n = strin.find('\n');
        if(n == std::string::npos) {
            break;
        }
        strout += strin.substr(0, n + 1);
        strin.erase(0, n + 1);
        ctx->m_readyWrite = true;
    }
}

void BaseTcpServer::onWrite(int a_fd, size_t a_size)
{
    auto &ctx = m_context.at(a_fd);
    auto &strout = ctx->m_strout;
#ifdef DEBUG
    std::cout << "onWrite: " << a_fd << " : " << a_size << " : " << strout.size() << std::endl;
#endif
    strout.erase(0, a_size);
    if(strout.empty()) {
        ctx->m_readyWrite = false;
    }
}

void BaseTcpServer::onClose(int a_fd)
{
#ifdef DEBUG
    std::cout << "onClose: " << a_fd << std::endl;
#endif
    m_context.erase(a_fd);
}


/**
 * class TcpServer.
 */
TcpServer::TcpServer(
    const std::string& a_addr,
    const std::string& a_port)
:
    BaseTcpServer(a_addr, a_port)
{
}

void TcpServer::onRead(int a_fd, const char *a_buf, size_t a_size)
{
#ifdef DEBUG
    std::cout << "onRead: " << a_fd << " : " << a_size << std::endl;
#endif
    auto &ctx = m_context.at(a_fd);
    auto &strin = ctx->m_strin;
    auto &strout = ctx->m_strout;
    strin.append(a_buf, a_size);
    while(1) {
        const size_t n = strin.find('\n');
        if(n == std::string::npos) {
            break;
        }
        try {
            const std::string cmd(strin.substr(0, n));
            if(cmd.compare(0, 3, "seq") == 0) {
                ctx->m_seqFactory.createSeq(cmd);
                strout += "OK\r\n";
            } else if(cmd.compare(0, 10, "export seq") == 0) {
                strout += ctx->m_seqFactory.getOneRowSeq();
                strout += "\r\n";
                ctx->m_exportSeq = true;
            } else {
                throw std::runtime_error("Unknown command");
            }
        } catch(const std::exception& err) {
            strout += "ERR: ";
            strout += err.what();
            strout += "\r\n";
        }
        ctx->m_readyWrite = true;
        strin.erase(0, n + 1);
    }
}

void TcpServer::onWrite(int a_fd, size_t a_size)
{
    auto &ctx = m_context.at(a_fd);
    auto &strout = ctx->m_strout;
#ifdef DEBUG
    std::cout << "onWrite: " << a_fd << " : " << a_size << " : " << strout.size() << std::endl;
#endif
    strout.erase(0, a_size);
    if(ctx->m_exportSeq) {
        for(int i=0; i<5000; ++i) {
            auto rowseq = ctx->m_seqFactory.getOneRowSeq();
            if(rowseq.empty()) {
                ctx->m_exportSeq = false;
                break;
            } else {
                strout += rowseq;
                strout += "\r\n";
            }
        }
    }
    if(strout.empty()) {
        ctx->m_readyWrite = false;
    }
}
