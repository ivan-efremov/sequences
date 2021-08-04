/**
 * @file TcpServer.h
 */

#pragma once

#include <sys/epoll.h>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include "Sequence.h"


/**
 * @short Контекст соединения.
 */
struct CtxConnection {
    std::string         m_strin;
    std::string         m_strout;
    SequenceFactory     m_seqFactory;
    bool                m_readyWrite;
    bool                m_exportSeq;
                        CtxConnection();
};

typedef std::shared_ptr<CtxConnection>  PCtxConnection;
typedef std::map<int, PCtxConnection>   MapCtxConnection;
typedef std::vector<std::thread>        VectorThreads;


/**
 * @short Базовый класс TCP сервера.
 */
class BaseTcpServer
{
    enum {
        BUFFERSIZE      = 1024,
        MAXEVENTS       = 4096
    };
public:
    /// Открытые методы
                        BaseTcpServer(const std::string& a_addr,
                                      const std::string& a_port);
    virtual            ~BaseTcpServer();
    virtual void        run();
    virtual void        stop();
private:
    /// Работа с сокетами
    void                sockInit();
    void                listenInit();
    void                epollInit();
    void                doAccept();
    void                doRead(int a_fd);
    void                doWrite(int a_fd);
    void                doClose(int a_fd);
private:
    /// Работа с потоками выполнения
    void                startThread();
    void                runThread();
    void                joinThread();
protected:
    /// Хэндлеры событий
    virtual void        onAccept(int a_fd);
    virtual void        onRead(int a_fd, const char *a_buf, size_t a_size);
    virtual void        onWrite(int a_fd, size_t a_size);
    virtual void        onClose(int a_fd);
protected:
    static void         syserror(const std::string& a_errmsg);
    static void         setNonBlocking(int a_fd);
protected:
    std::string         m_address;
    std::string         m_port;
    MapCtxConnection    m_context;
    VectorThreads       m_poolThreads;
    struct epoll_event *m_events;
    int                 m_sfd;
    int                 m_efd;
    volatile bool       m_running;
    static std::mutex   s_lock;
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
    virtual void        onRead(int a_fd, const char *a_buf, size_t a_size);
    virtual void        onWrite(int a_fd, size_t a_size);
};

typedef std::shared_ptr<TcpServer>  PTcpServer;
