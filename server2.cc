#include <bits/stdc++.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <assert.h>

#include "util.h"

class Server {
public:
  Server() = default;
  Status Init() {
    if((sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      return Status::IO_ERROR;
    }

    memset(&servaddr_, 0, sizeof(servaddr_));
    memset(&clientaddr_, 0, sizeof(clientaddr_));

    servaddr_.sin_family    = AF_INET; // IPv4
    servaddr_.sin_addr.s_addr = INADDR_ANY;
    servaddr_.sin_port = htons(PORT);

    if (bind(sock_fd_, (const struct sockaddr *)&servaddr_, sizeof(servaddr_)) < 0 )
    {
      return Status::IO_ERROR;
    }

    return Status::Ok;
  }

  Status Run() {
    char buffer[MAXLINE];
    socklen_t len = sizeof(clientaddr_);

    // Get filename
    int filename_len = recvfrom(sock_fd_, (char*) buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &clientaddr_, &len);
    if (filename_len == -1) {
      return Status::IO_ERROR;
    }
    buffer[filename_len] = '\0';
    std::string filename(buffer);
    filename = "./uploaded_" + filename;

    // open/create file
    int fd = open(filename.c_str(), O_CREAT | O_RDWR);
    if(fd == -1) {
      return Status::IO_ERROR;
    }
    memset(buffer, 0, MAXLINE);

    // Get filesize
    int recvd_len = recvfrom(sock_fd_, (char*) buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &clientaddr_, &len);
    if(recvd_len == -1) return Status::IO_ERROR;
    buffer[recvd_len] = '\0';
    ssize_t filesize = stoi(std::string(buffer));
    std::cout << "file size is: " << filesize << std::endl;
    if (fallocate(fd, FALLOC_FL_KEEP_SIZE, 0, filesize) == -1) {
      return Status::IO_ERROR;
    }
    memset(buffer, 0, MAXLINE);

    int cur = 0;
    while(cur < filesize) {
      int recvd_len = recvfrom(sock_fd_, (char*) buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &clientaddr_, &len);
      if(recvd_len != -1) {
        // decode to get block index
        // check if its an ACK packet
        if (recvd_len == sizeof(uint32_t)) {
          // extract the block index from ack packet
          uint32_t ack_index;
          memcpy(&ack_index, buffer, sizeof(uint32_t));
          std::cout << "\r Received ACK for block index: " << ack_index << std::endl;
        }
        else{
        uint32_t index;
        memcpy(&index, buffer, sizeof(uint32_t));
        // send back ACK
        int index_send_len = sendto(sock_fd_, (const char*)&index, sizeof(uint32_t), MSG_CONFIRM, (struct sockaddr *) &clientaddr_, sizeof(clientaddr_));
        assert(index_send_len == sizeof(uint32_t));
        // write data to offset
        int written_len = pwrite(fd, buffer + sizeof(uint32_t), recvd_len - sizeof(uint32_t), index * 16384);
        assert(written_len != -1);
        cur += written_len;
        std::cout << "\rcursize: " << cur << ", get block index: " << index << std::endl;
        }
      }
    }
    close(fd);
    return Status::Ok;
  }
private:
  int sock_fd_, fd_;
  struct sockaddr_in servaddr_, clientaddr_;
};

int main() {
  Server server;
  Status s = server.Init();
  if(s != Status::Ok) {
    exit(EXIT_FAILURE);
  }
  s = server.Run();
  if(s != Status::Ok) {
    exit(EXIT_FAILURE);
  }
}
