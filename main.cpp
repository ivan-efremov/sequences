/**
 * @file main.cpp
 */

#include <iostream>
#include <stdexcept>
#include <csignal>
#include "Sequence.h"
#include "TcpServer.h"


static PTcpServer g_server;


static void stop(int a_signal)
{
    if(g_server) {
        g_server->stop();
    }
}

int main(int argc, const char *argv[])
{
/*
    g_sequences.emplace_back(1, 2);
    g_sequences.emplace_back(2, 3);
    g_sequences.emplace_back(3, 4);
    
    for(;;) {
        std::cout << g_sequences.at(0).next() << '\t'
                  << g_sequences.at(1).next() << '\t'
                  << g_sequences.at(2).next() << std::endl;
        std::cin.get();
    }
*/
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
    std::cout << "\nShutdown server." << std::endl;
    return 0;
}
