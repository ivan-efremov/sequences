/**
 * @file TcpServer.h
 */

#pragma once

#include <sys/epoll.h>
#include <memory>
#include <string>


/**
 * @short Базовый класс TCP сервера.
 */
class BaseTcpServer
{
    enum {
        BUFFERSIZE = 1024,
        MAXEVENTS = 16384
    };
public:
    /// Открытые методы
                        BaseTcpServer(const std::string& a_addr,
                                      const std::string& a_port);
    virtual            ~BaseTcpServer();
    virtual void        run();
    virtual void        stop();
protected:
    /// Инициализация
    void                sockInit();
    void                listenInit();
    void                epollInit();
protected:
    /// Хэндлеры событий
    virtual void        onAccept(int a_fd);
    virtual void        onRead(int a_fd, const char *a_buf, int a_size);
    virtual void        onWrite(int a_fd, const char *a_buf, int a_size);
    virtual void        onClose(int a_fd);
protected:
    static void         setNonBlocking(int a_fd);
protected:
    std::string         m_address;
    std::string         m_port;
    struct epoll_event  m_event;
    struct epoll_event *m_events;
    int                 m_sfd;
    int                 m_efd;
    volatile bool       m_running;
};


/**
 * @short TCP сервер.
 */
class TcpServer:
    public BaseTcpServer
{
public:
                        TcpServer(const std::string& a_addr,
                                  const std::string& a_port);
protected:
    virtual void        onAccept(int a_fd);
    virtual void        onRead(int a_fd, const char *a_buf, int a_size);
    virtual void        onWrite(int a_fd, const char *a_buf, int a_size);
    virtual void        onClose(int a_fd);
};

typedef std::shared_ptr<TcpServer>  PTcpServer;
