#include <iostream>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cerrno>
#include <cstring>
#include <arpa/inet.h>
#include <cstdint>
#include <limits>
#include <fcntl.h>
#include <chrono>
#include <poll.h>
//using this deadline for both sending + receiving so the complete request
// response exchange can't five seconds. 
using D_Line = std::chrono::steady_clock::time_point;


//This function is used to wait for a socket to be ready and does not exceed the d_line.
bool wait_4_io_send(int socket_fd, short events, D_Line d_line){
    while(true){
        auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(d_line - std::chrono::steady_clock::now()).count();
        if(remain <= 0){
            return false;
        }
        pollfd desc{};
        desc.fd = socket_fd;
        desc.events = events;
        int result = poll(&desc, 1,static_cast<int>(remain));
        if(result<0 && errno == EINTR){
            continue;
        }
        if(desc.revents & events){
            return true;
        }
        return false; //This indicates an error, invalid desc, or disconnection
    }
}

std::string def_skt_path(){ //Used to determine the default unix socket location
    const char* user = std::getenv("USER");
    if(user != nullptr && user[0] != '\0'){
        return "/tmp/aiws-" + std::string(user) + ".sock";
    }
    return "/tmp/aiws-" + std::to_string(getuid()) + ".sock";
}
bool send_all(int socket_fd, const void* data, std::size_t length, D_Line d_line){
    //This function sends the entire request and retries all partial writes until
    //all bytes transmitted / communication d_line will expire
    const char* bytes = static_cast<const char*>(data); //msg treated as a sequence of bytes
    std::size_t  total_sent = 0;
    while(total_sent < length){ //continues until the complete message is fully transmitted
        if(std::chrono::steady_clock::now() >= d_line){
                return false;
        }
        ssize_t sent = send(socket_fd, bytes + total_sent, length - total_sent, MSG_NOSIGNAL | MSG_DONTWAIT);
         if(sent >0 ){
            total_sent += static_cast<std::size_t>(sent);
            continue;
         }
         if(sent == 0){
            return false;
         }
         if(errno == EINTR){
            continue;
        
         }
         if(errno == EAGAIN || errno == EWOULDBLOCK){
            if(!wait_4_io_send(socket_fd, POLLOUT, d_line)){
                return false;
            }
            continue;
         }
         return false;
    }
    return true;
}
//This function and send all edited for same approach for recieving 
bool recv_all(int socket_fd, void* data, std::size_t length, D_Line d_line){
    // Receives exactly the requested number of bytes.
    // Handles partial reads and respects the shared communication deadline.
    //Treating the received message as sequence of bytes
    char* bytes = static_cast<char*>(data);
    std::size_t total_received = 0;
    while(total_received < length){
        if(std::chrono::steady_clock::now() >= d_line){
            return false;
        }
        ssize_t received = recv(socket_fd, bytes + total_received, length - total_received, MSG_DONTWAIT);
        if(received > 0){
            total_received += static_cast<std::size_t>(received);
            continue;
            
        }

        if(received ==0){
            return false;
        }
        if(errno == EINTR){
            continue;
        } if (errno ==  EAGAIN || errno == EWOULDBLOCK){
            if(!wait_4_io_send(socket_fd, POLLIN, d_line)){
                return false;
            }
            continue;
        }
        return false;
    }
   return true;
}
bool connect_with_timeout(int socket_fd, const sockaddr_un& server_address, std::string& error){
    //This function attemps to connect to the application within two secs. It uses a nonblocking socket + poll()
    // to avoid waiting indefinitely when server can't be reached
    int flgs = fcntl(socket_fd, F_GETFL, 0);
    if(flgs == -1 || (fcntl(socket_fd, F_SETFL, flgs | O_NONBLOCK) == -1)){
        error = "Unable to configure non-blocking socket.";
        return false;
    }
    auto d_line = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    int result = connect(socket_fd, reinterpret_cast<const sockaddr*>(&server_address), sizeof(server_address));
    if(result == -1){
        if(errno != EINPROGRESS){ //EINPROGRESS = non-block connection is underway
            error = std::strerror(errno);
            return false;
        }
        while(true){
            auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(d_line - std::chrono::steady_clock::now()).count();
            if(remain <= 0){
                error = "Connection timed out.";
                return false;
            }
            pollfd desc{};
            desc.fd = socket_fd;
            desc.events = POLLOUT;
            result = poll(&desc, 1, static_cast<int>(remain));
            if(result == -1 && errno ==  EINTR){
                continue;
            }
            if(result == 0){
                error = "The connection has timed out.";
                return false;
            }
            if(result == -1){
                error = std::strerror(errno);
                return false;
            }
            int socket_error = 0; //checks whether the connection succeeded.
            socklen_t error_size = sizeof(socket_error);
            if(getsockopt(socket_fd, SOL_SOCKET, SO_ERROR,&socket_error,&error_size) == -1){
                error = std::strerror(errno);
                return false;
            }
            if(socket_error != 0){
                error = std::strerror(socket_error);
                return false;
            }
            if(desc.revents & POLLNVAL){
                error = "Invalid socket descriptor.";
                return false;
            }
            if(std::chrono::steady_clock::now() >= d_line){
                error = "The connection has timed out. ";
                return false;
            }
            break;
        }
    }
    
    return true;
}



int main(int argc, char* argv[]){
    auto d_line = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    std::string socket_path = def_skt_path();
    int argument_index = 1;
    //checking for optional custom socket path
    if(argc > 1 && std::string(argv[1]) == "--socket"){
        if(argc < 4){
            std::cerr << "Missing socket path or command.\n";
            return 1;
        }
        socket_path = argv[2];
        argument_index = 3;
    }
    //verifying  the required add file arguments
    if(argc - argument_index != 2 || std::string(argv[argument_index]) != "add"){
        std::cerr << "Usage: aiws_send [--socket PATH] add FILE\n";
        return 1;
    }
    //Resolving document path against the working directory from the sender
    std::filesystem::path document_path(argv[argument_index + 1]);
    std::error_code error;
    document_path = std::filesystem::absolute(document_path, error);
    if(error){
        std::cerr << "Unable to resolve the document path.\n";
        return 1;
    }
    //Creating a unix domain stream socket
    int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(socket_fd == -1){
        std::cerr << "Failed to create the socket: " << std::strerror(errno) << '\n';
        return 2;
    }
    sockaddr_un server_address{};
    server_address.sun_family = AF_UNIX;
    //Ensuring the socket path fits into the addr structure
    if(socket_path.size() >= sizeof(server_address.sun_path)){
        std::cerr << "Socket path is too long.\n";
        close(socket_fd);
        return 2;
    }
    std::memcpy(server_address.sun_path, socket_path.c_str(), socket_path.size() + 1);
    //connecting to the running AI workspace within 2 secs
    std::string connect_err;
    if(!connect_with_timeout(socket_fd, server_address, connect_err)){
        std::cerr << "Unable to connect: " << connect_err <<'\n';
        close(socket_fd);
        return 2; 
    }
   
    std::string path_string = document_path.string(); //converting absolute path into bytes
    //verifying path length will fit into protocol
    if(path_string.size() > std::numeric_limits<std::uint32_t>::max()){
        std::cerr << "Document path is too long.\n";
        close(socket_fd);
        return 1;
    }
    //encodes the path length by using big endian byte order
    std::uint32_t path_length = htonl(static_cast<std::uint32_t>(path_string.size()));
    //sending four byte length first
    if(!send_all(socket_fd, &path_length, sizeof(path_length), d_line)){
        std::cerr << "Failed to send path length.\n";
        close(socket_fd);
        return 2;
    }
    if(!send_all(socket_fd, path_string.data(), path_string.size(), d_line)){
        std::cerr << "Failed to send the document path.\n";
        close(socket_fd);
        return 2;
    }
    //receiving the one byte response status
    std::uint8_t response_status = 0;
    if(!recv_all(socket_fd, &response_status, sizeof(response_status), d_line)){
        std::cerr << "Failed to receive the server status. \n";
        close(socket_fd);
        return 2;
    }
    //recieving the four byte error message length
    std::uint32_t reason_length_network = 0;
    if(!recv_all(socket_fd, &reason_length_network, sizeof(reason_length_network), d_line)){
        std::cerr << "Failed to receive the reason length. \n";
        close(socket_fd);
        return 2;
    }
    //convertine from big endian to byte order for host
    std::uint32_t reason_length = ntohl(reason_length_network);
    //rejecting an unexpectedly large response
    if(reason_length > 4096){
        std::cerr << "Invalid server response length. \n";
        close(socket_fd);
        return 2;
    }
    std::string reason(reason_length, '\0');
    if(!recv_all(socket_fd, reason.data(),reason.size(), d_line)){
        std::cerr << "Failed to receive server response.\n";
        close(socket_fd);
        return 2;
    }
    //closing socket after response received
    close(socket_fd);

    //interpretation of server status
    if(response_status == 0){
        std::cout << "The document has been accepted.\n";
        return 0 ;
    }
    if(response_status == 1){
        std::cerr << "Document rejected: " << reason << '\n';
        return 1;
    }
    //an unknown status = protocol error
    std::cerr << "Invalid server response status.\n";
    return 2;

}