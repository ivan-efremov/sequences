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


TcpServer::TcpServer(
    const std::string& a_addr,
    const std::string& a_port)
:
    m_address(a_addr),
    m_port(a_port),
    m_event(),
    m_events(nullptr),
    m_sfd(-1),
    m_efd(-1),
    m_running(true)
{
}

TcpServer::~TcpServer()
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

void TcpServer::sockInit()
{
    struct addrinfo addr, *res = nullptr;

    bzero(&addr, sizeof(addr));
    addr.ai_family = AF_INET;
    addr.ai_socktype = SOCK_STREAM;
    addr.ai_flags = AI_PASSIVE;

    int err = getaddrinfo(m_address.c_str(), m_port.c_str(), &addr, &res);
    if(err != 0) {
        std::string errmsg("getaddrinfo: ");
        errmsg += gai_strerror(err);
        throw std::runtime_error(errmsg);
    }
     
    m_sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(m_sfd == -1) {
        std::string errmsg("socket: ");
        errmsg += strerror(errno);
        freeaddrinfo(res);
        throw std::runtime_error(errmsg);
    }

    int yes = 1;
    if(setsockopt(m_sfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) == -1) {
        std::string errmsg("setsockopt: ");
        errmsg += strerror(errno);
        close(m_sfd);
        m_sfd = -1;
        freeaddrinfo(res);
        throw std::runtime_error(errmsg);
    }

    if(bind(m_sfd, res->ai_addr, res->ai_addrlen) == -1) {
        std::string errmsg("bind: ");
        errmsg += strerror(errno);
        close(m_sfd);
        m_sfd = -1;
        freeaddrinfo(res);
        throw std::runtime_error(errmsg);
    }
    freeaddrinfo(res);
}

void TcpServer::setNonBlocking(int a_fd)
{
    int flags = fcntl(a_fd, F_GETFL, 0);
    if(flags == -1) {
        flags = 0;
    }
    flags |= O_NONBLOCK;
    if(fcntl(a_fd, F_SETFL, flags) == -1) {
        std::string errmsg("fcntl: ");
        errmsg += strerror(errno);
        throw std::runtime_error(errmsg);
    }
}

void TcpServer::listenInit()
{
    if(listen(m_sfd, 10000) == -1) {
        std::string errmsg("listen: ");
        errmsg += strerror(errno);
        throw std::runtime_error(errmsg);
    }
}

void TcpServer::epollInit()
{
    m_efd = epoll_create1(0);
    if(m_efd == -1) {
        std::string errmsg("epoll_create1: ");
        errmsg += strerror(errno);
        throw std::runtime_error(errmsg);
    }
    
    m_event.data.fd = m_sfd;
    m_event.events = EPOLLIN | EPOLLET;
    
    if(epoll_ctl(m_efd, EPOLL_CTL_ADD, m_sfd, &m_event) == -1) {
        std::string errmsg("epoll_ctl: ");
        errmsg += strerror(errno);
        throw std::runtime_error(errmsg);
    }

    m_events = (epoll_event*) calloc(MAXEVENTS, sizeof(m_event));
    if(m_events == nullptr) {
        throw std::runtime_error("m_events: out of memory");
    }
}

void TcpServer::run()
{
    sockInit();
    setNonBlocking(m_sfd);
    listenInit();
    epollInit();
    while(m_running) try {
        int n = epoll_wait(m_efd, m_events, MAXEVENTS, -1);
        if(n == -1) {
            std::string errmsg("epoll_wait: ");
            errmsg += strerror(errno);
            throw std::runtime_error(errmsg);
        }
        for(int i = 0; i < n; ++i) {
            if((m_events[i].events & EPOLLERR) ||
               (m_events[i].events & EPOLLHUP) ||
               (!(m_events[i].events & EPOLLIN)))
            {
                //onClose(m_events[i].data.fd);
                close(m_events[i].data.fd);
                continue;
            }
            else if(m_sfd == m_events[i].data.fd)
            {
                struct sockaddr client;
                socklen_t clientLen = sizeof(client);
                int clientfd = accept(m_sfd, &client, &clientLen);
                if(clientfd == -1) {
                    if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                        continue;
                    } else {
                        std::string errmsg("accept: ");
                        errmsg += strerror(errno);
                        throw std::runtime_error(errmsg);
                    }
                }
                setNonBlocking(clientfd);
                //onAccept(clientfd);
                m_event.data.fd = clientfd;
                m_event.events = EPOLLIN | EPOLLET;
                if(epoll_ctl(m_efd, EPOLL_CTL_ADD, clientfd, &m_event) == -1) {
                    std::string errmsg("epoll_ctl: ");
                    errmsg += strerror(errno);
                    throw std::runtime_error(errmsg);
                }
            }
            else
            {
                int err = 0;
                while(1) {
                    char buf[BUFFERSIZE];
                    ssize_t count = read(m_events[i].data.fd, buf, sizeof(buf));
                    if(count == -1) {
                        if(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                            err = 1;
                        }
                        break;
                    } else if(count == 0) {
                        err = 1;
                        break;
                    }
                    buf[count] = 0;
                    //onRead(m_events[i].data.fd, buf, count);
                }
                if(err) {
                    //onClose(m_events[i].data.fd);
                    close(m_events[i].data.fd);
                }
            } ///< if
        } ///< for
    } catch(const std::exception& err) { ///< while
        std::cerr << "Loop error: " << err.what() << std::endl;
    }
}

void TcpServer::stop()
{
    m_running = false;
}
