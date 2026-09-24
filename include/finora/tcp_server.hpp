#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include "logger.hpp"
#include <string>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

class TcpServer {
protected:
    int port_;
    std::string name_;
    int server_fd_;
    bool is_running_;

public:
    TcpServer(int port, const std::string& name) 
        : port_(port), name_(name), server_fd_(-1), is_running_(false) {}

    virtual ~TcpServer() {
        stop();
    }

    // Initialize server sockets, reuse address and disable Nagle
    void init() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            throw std::runtime_error(name_ + ": Failed to create socket");
        }

        int opt = 1;
        if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            close(server_fd_);
            server_fd_ = -1;
            throw std::runtime_error(name_ + ": setsockopt SO_REUSEADDR failed");
        }

        // Disable Nagle's algorithm for low-latency HFT
        if (setsockopt(server_fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
            logger::log_warn(name_ + ": setsockopt TCP_NODELAY failed (latency tuning degraded)");
        }

        struct sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);

        if (bind(server_fd_, (struct sockaddr *)&address, sizeof(address)) < 0) {
            close(server_fd_);
            server_fd_ = -1;
            throw std::runtime_error(name_ + ": Bind failed on port " + std::to_string(port_));
        }

        if (listen(server_fd_, 128) < 0) {
            close(server_fd_);
            server_fd_ = -1;
            throw std::runtime_error(name_ + ": Listen failed on port " + std::to_string(port_));
        }

        logger::log_info(name_ + " initialized and listening on port " + std::to_string(port_));
    }

    // Starts the blocking accept loop (typically detached or run in separate thread)
    void run() {
        if (server_fd_ < 0) {
            init();
        }

        is_running_ = true;
        logger::log_info(name_ + " loop started.");

        while (is_running_) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = accept(server_fd_, (struct sockaddr *)&client_addr, &addr_len);
            
            if (client_fd < 0) {
                if (is_running_) {
                    logger::log_error(name_ + ": Client accept failed");
                }
                break;
            }

            // Delegate client handling to subclass, running on a separate thread
            std::thread([this, client_fd]() {
                try {
                    this->handle_client(client_fd);
                } catch (const std::exception& e) {
                    logger::log_error(this->name_ + " Exception handling client: " + std::string(e.what()));
                    close(client_fd);
                } catch (...) {
                    logger::log_error(this->name_ + " Unknown exception handling client.");
                    close(client_fd);
                }
            }).detach();
        }
    }

    void stop() {
        is_running_ = false;
        if (server_fd_ >= 0) {
            logger::log_info(name_ + " stopping server socket.");
            close(server_fd_);
            server_fd_ = -1;
        }
    }

    virtual void handle_client(int client_fd) = 0;
};

#endif // TCP_SERVER_HPP
