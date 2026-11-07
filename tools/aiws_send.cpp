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

std::string default_socket_path(){ //Used to determine the default unix socket location
    const char* user = std::getenv("USER");
    if(user != nullptr && user[0] != '\0'){
        return "/tmp/aiws-" + std::string(user) + ".sock";
    }
    return "/tmp/aiws-" + std::to_string(getuid()) + ".sock";
}
bool send_all(int socket_fd, const void* data, std::size_t length){
    const char* bytes = static_cast<const char*>(data); //msg treated as a sequence of bytes
    std::size_t  total_sent = 0;
    while(total_sent < length){ //continues until the complete message is fully transmitted
        ssize_t sent = send(socket_fd, bytes + total_sent, length - total_sent, MSG_NOSIGNAL);
        if(sent < 0){
            if(errno == EINTR){ //retry if interrupted by signal
                continue;
            }
            return false;
        }
        if(sent == 0){ //no forward progress has been made
            return false;
        }
        total_sent += static_cast<std::size_t>(sent);
    }
    return true;
}
bool recv_all(int socket_fd, void* data, std::size_t length){
    //Treating the received message as sequence of bytes
    char* bytes = static_cast<char*>(data);
    std::size_t total_received = 0;
    while(total_received < length){
        ssize_t received = recv(socket_fd, bytes + total_received, length - total_received, 0);
        if(received < 0){
            if(errno == EINTR){
                continue;
            }
            return false;
        }
        if(received == 0){
            return false;
        }
        total_received += static_cast<std::size_t>(received);
    }
    return true;
}

int main(int argc, char* argv[]){
    std::string socket_path = default_socket_path();
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
    //connecting to the running AI workspace
    if(connect(socket_fd, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == -1){
        std::cerr << "Unable to connect: " << std::strerror(errno) << '\n';
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
    if(!send_all(socket_fd, &path_length, sizeof(path_length))){
        std::cerr << "Failed to send path length.\n";
        close(socket_fd);
        return 2;
    }
    if(!send_all(socket_fd, path_string.data(), path_string.size())){
        std::cerr << "Failed to send the document path.\n";
        close(socket_fd);
        return 2;
    }
    //receiving the one byte response status
    std::uint8_t response_status = 0;
    if(!recv_all(socket_fd, &response_status, sizeof(response_status))){
        std::cerr << "Failed to receive server status. \n";
        close(socket_fd);
        return 2;
    }
    //recieving the four byte error message length
    std::uint32_t reason_length_network = 0;
    if(!recv_all(socket_fd, &reason_length_network, sizeof(reason_length_network))){
        std::cerr << "Failed to receive reason length. \n";
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
    if(!recv_all(socket_fd, reason.data(), reason.size())){
        std::cerr << "Failed to receive server response.\n";
        close(socket_fd);
        return 2;
    }
    //closing socket after response received
    close(socket_fd);

    //interpretation of server status
    if(response_status == 0){
        std::cout << "Document request accepted.\n";
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