#pragma once
#include <atomic>
#include <string>
#include <thread>
#include <functional>
namespace aiws{
    class IPCServer{
        public: 
        explicit IPCServer(const std::string& socket_path, std::function<void(std::string)> on_add);
        ~IPCServer();
        //Preventing accidental copies of server
        IPCServer(const IPCServer& ) = delete;
        IPCServer& operator=(const IPCServer&) = delete;
        bool start(std::string& error); //used to start listening for connections
        void stop();
        private:
        void run(); //Function is executed on the listener thread
        std::string socket_path_;
        std::atomic<bool> stop_requested_{false}; //used to indicate whether shutdown was requested
        std::thread worker_; //used as a background listening thread
        int listen_fd_{-1}; //Unix domain listening socket
        bool socket_created_{false}; //used to indicate whether the server created the socket file
        void handle_client(int client_fd);
        std::function<void(std::string)> on_add_;
    };
}