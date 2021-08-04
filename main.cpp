/**
 * @file main.cpp
 */

#include <iostream>
#include <stdexcept>
#include <csignal>
#include "TcpServer.h"


static PTcpServer g_server;


static void stop(int a_signal)
{
    if(g_server) {
        std::cout << std::endl;
        g_server->stop();
    }
}


int main(int argc, const char *argv[])
{
    const std::string host = "0.0.0.0";
    const std::string port = "4000";
    try {
        g_server = std::make_shared<TcpServer>(host, port);
        
        std::signal(SIGTERM, stop);
        std::signal(SIGINT,  stop);
        std::signal(SIGHUP,  stop);
        std::signal(SIGQUIT, stop);
        std::signal(SIGPIPE, SIG_IGN);
        
        std::cout << "Server is started at "
                  << host << ':' << port << ".\n"
                  << "Use CTRL-C to quit." << std::endl;
        
        g_server->run();
    } catch(const std::exception& err) {
        std::cerr << "Server error: " << err.what() << std::endl;
    }
    std::cout << "Shutdown server." << std::endl;
    return 0;
}
