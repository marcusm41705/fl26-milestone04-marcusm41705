#include "aiws/ipc_server.hpp"
#include <cerrno>
#include <cstring>
#include <poll.h>
#include <utility>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <chrono>
#include <cstdint>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <exception>
#include <sys/socket.h>
#include <sys/un.h>
namespace aiws{
    namespace{
        using D_line = std::chrono::steady_clock::time_point;
        //waiting for socket activity while shutdown
        bool wait_4_io(int fd, short events, const std::atomic<bool>& stop, D_line d_line){
            while(!stop.load()){
                if(std::chrono::steady_clock::now() >= d_line){
                    return false;
                }
                pollfd desc{};
                desc.fd = fd;
                desc.events = events;
                int result = poll(&desc, 1, 50);
                if(result > 0){
                    if(desc.revents & events){
                        return true;
                    }
                    if(desc.revents & (POLLERR | POLLHUP | POLLNVAL)){
                        return false;
                    }
                } else if(result < 0 && errno != EINTR){
                    return false;
                }
            }
            return false;
        }
        bool receive_all(int fd, void* data, std::size_t length, const std::atomic<bool>& stop, D_line d_line){
            char* bytes = static_cast<char*>(data);
            std::size_t rec_total = 0;
            while(rec_total < length && !stop.load()){
                ssize_t received = recv(fd, bytes + rec_total, length - rec_total, MSG_DONTWAIT);
                if(received > 0){
                    rec_total += static_cast<std::size_t>(received);
                    continue;
                }
                if(received == 0){
                    return false;
                }
                if(errno == EINTR){
                    continue;
                }
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    if(!wait_4_io(fd, POLLIN, stop, d_line)){
                        return false;
                    }
                    continue;

                }
                return false;
            }
            return rec_total == length;
        }
        //send length bytes exactly, including any partial writes
        bool send_all_response(int fd, const void* data, std::size_t length, const std::atomic<bool>& stop, D_line d_line){
            const char* bytes = static_cast<const char*>(data);
            std::size_t sent_total = 0;
            while(sent_total < length && !stop.load()){
                ssize_t sent = send(fd, bytes + sent_total, length - sent_total, MSG_DONTWAIT | MSG_NOSIGNAL);
                if(sent > 0){
                    sent_total += static_cast<std::size_t>(sent);
                    continue;
                }
                if(sent == 0){
                    return false;
                }
                if(errno == EINTR){
                    continue;
                }
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    if(!wait_4_io(fd,  POLLOUT, stop, d_line)){
                        return false;
                    }
                    continue;
                }
                return false;
            }
            return sent_total == length;
        }
        //reply is 1 status byte + reason length of 4 bytes + reason
        bool send_response(int fd, std::uint8_t status, const std::string& reason, const std::atomic<bool>& stop, D_line d_line){
            std::uint32_t length = htonl(static_cast<std::uint32_t>(reason.size()));
            return send_all_response(fd, &status, sizeof(status), stop, d_line) &&
            send_all_response(fd, &length, sizeof(length), stop, d_line) &&
            send_all_response(fd, reason.data(), reason.size(), stop, d_line);

        }
    } //end of namespace
    IPCServer::IPCServer(const std::string&  socket_path, std::function<void(std::string)> on_add): 
    socket_path_(socket_path), on_add_(std::move(on_add))
    { }
    IPCServer::~IPCServer(){
        stop();
    }
    bool IPCServer::start(std::string& error){
        if(listen_fd_ != -1){
            error = "Server already started.";
            return false;
        }
        sockaddr_un  address{};
        address.sun_family = AF_UNIX;
        if(socket_path_.empty() || socket_path_.size() >= sizeof(address.sun_path)){
            error = "Invalid or excessively long socket path.";
            return false;
        }
        std::memcpy(address.sun_path, socket_path_.c_str(), socket_path_.size()+1);
        //Creating unix domain stream socket
        listen_fd_ = socket(AF_UNIX,  SOCK_STREAM, 0);
        if(listen_fd_ == -1){
            error = std::strerror(errno);
            return false;
        }
        //Binding  socket to filesystem path
        if(bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address))== -1){
            error = std::strerror(errno);
            close(listen_fd_);
            listen_fd_ = -1;
            return false;

        }
        socket_created_ = true;
        //listen for incoming connection
        if(listen(listen_fd_,  10) == -1){
            error = std::strerror(errno);
            stop();
            return false;
        }
        stop_requested_ = false;
        try{ //launcing the listener on standard c++ thread
            worker_ = std::thread(&IPCServer::run,this);
        } catch(const std::exception& except){
            error = except.what();
            stop();
            return false;
        }
        return true;
        
    }

    void IPCServer::run(){
        while(!stop_requested_){
            pollfd descriptor{};
             descriptor.fd = listen_fd_;
            descriptor.events =  POLLIN;
            //Check the connections using 50ms polling interval
            int result = poll(&descriptor, 1, 50);
            if(result < 0){
                if(errno ==EINTR){
                    continue;
                }
                break;
            }
            if(result ==0){
                continue;
            }
            if(descriptor.revents & POLLIN){
                //accepting an incoming client connection
                int client_fd = accept(listen_fd_, nullptr, nullptr);
                if(client_fd >= 0){
                    handle_client(client_fd);
                    close(client_fd);
                }
            }
        }
    }
    void IPCServer::stop(){
        //request listener to stop
        stop_requested_ = true;
        //Join the worker before destroying resources
        if(worker_.joinable()){
            worker_.join();
        }
        //Close listening socket
        if(listen_fd_ != -1){
            close(listen_fd_);
            listen_fd_ = -1;
        }
        //removing socket file that was created
        if(socket_created_){
            unlink(socket_path_.c_str());
            socket_created_ = false;
        }
    }

    void IPCServer::handle_client(int client_fd){
        //overall d_line for handling connection
        auto d_line = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        //recieving the four byte path length
        std::uint32_t netwrk_lth = 0;
        if(!receive_all(client_fd, &netwrk_lth, sizeof(netwrk_lth), stop_requested_, d_line)){
            return;
        }
        //converting from big endian to byte order of host length
        std::uint32_t path_length = ntohl(netwrk_lth);
        //rejecting empty/excessive large path
        if(path_length == 0 || path_length > 4096 ) {
            send_response(client_fd, 1, "Invalid document path length.", stop_requested_, d_line);
            return;
        }
        //allocating space for incoming path
        std::string path(path_length, '\0');
        //collecting complete doc path
        if(!receive_all(client_fd, path.data(), path.size(), stop_requested_, d_line)){
            return;
        }
        if(path.find('\0') != std::string::npos){
            send_response(client_fd, 1, "Invalid document path. ", stop_requested_, d_line);
            return;
        }
        //checking that doc can be opened for reading
        int file_fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);

        if(file_fd == -1){
            std::string reason = std::strerror(errno);
            send_response(client_fd, 1, reason, stop_requested_, d_line);
            return;
        }
        struct stat file_info{};
        //ensure path refers to regular file
        bool file_vld = fstat(file_fd, &file_info) == 0 && S_ISREG(file_info.st_mode);
        close(file_fd);
        if(!file_vld){
            send_response(client_fd, 1, "Not a regular readable file.", stop_requested_, d_line);
            return;
        }
        bool replied = send_response(client_fd, 0, "", stop_requested_, d_line);
        //forwarding accepted request w/o waiting for the gui
        if(replied && !stop_requested_ && on_add_){
            on_add_(std::move(path));
        }
    }



} //end of IPC Server cpp