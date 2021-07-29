/**
 * @file TcpServer.h
 */

#pragma once

#include <sys/epoll.h>
#include <memory>
#include <string>


/**
 * @short TCP сервер.
 */
class TcpServer
{
    enum {
        BUFFERSIZE = 1024,
        MAXEVENTS = 16384
    };
public:
    /// Открытые методы
                        TcpServer(const std::string& a_addr,
                                  const std::string& a_port);
                       ~TcpServer();
    void                run();
    void                stop();
private:
    /// Инициализация
    void                sockInit();
    void                listenInit();
    void                epollInit();
protected:
    /// Хэндлеры событий
    void                onAccept(int a_fd);
    void                onRead(int a_fd, const char *a_buf, int a_size);
    void                onWrite(int a_fd, const char *a_buf, int a_size);
    void                onClose(int a_fd);
private:
    static void         setNonBlocking(int a_fd);
private:
    std::string         m_address;
    std::string         m_port;
    struct epoll_event  m_event;
    struct epoll_event *m_events;
    int                 m_sfd;
    int                 m_efd;
    volatile bool       m_running;
};

typedef std::shared_ptr<TcpServer>  PTcpServer;
