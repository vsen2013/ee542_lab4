#include <bits/stdc++.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <openssl/md5.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <chrono>

#include "util.h"
#include "lf_queue.h"

// Implementation of client.
class Client {
  public:
    explicit Client(Options opts, std::string& server_addr) : opts_(opts), server_addr_(server_addr) {}
    Status Init() {
      // network setup      
      if((sock_fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        return Status::IO_ERROR;
      }
      memset(&servaddr_, 0, sizeof(servaddr_));

      // set timeout
      struct timeval timeout;
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      if (setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        return Status::IO_ERROR;
      }

      servaddr_.sin_family = AF_INET;
      servaddr_.sin_addr.s_addr = inet_addr(server_addr_.c_str());
      servaddr_.sin_port = htons(PORT);
      return Status::Ok;
    }

    Status Upload(std::string& filepath) {
      int n = -1;
      // file open and validation
      fd_ = open(filepath.c_str(), O_RDONLY);
      if (fd_ == -1) { return Status::IO_ERROR; }

      std::string filename = ParseFileName(filepath);
      // send filename
      while(n == -1) {
        n = sendto(sock_fd_, (const char*)filename.c_str(), filename.length(), 
                    MSG_CONFIRM, (struct sockaddr *) &servaddr_, sizeof(servaddr_));
      }
      n = -1;

      // send file length
      size_t filesize = GetFileSize(filepath);
      std::string size_str = std::to_string(filesize);
      // use while loop to guard failure.
      while(n == -1) {
        n = sendto(sock_fd_, (const char*)size_str.c_str(), size_str.length(), 
                    MSG_CONFIRM, (struct sockaddr *) &servaddr_, sizeof(servaddr_));
      }

      int index_count = filesize / opts_.block_size;
      std::vector<std::thread> threads(opts_.thread_num);
      
      // create background thread for sweeping block status
      for(int i = 0; i <= index_count; ++i) {
        ack_status[i].store(0);
      }

      std::thread bgack(&Client::sweep, this);

      // start send file
      auto start = std::chrono::steady_clock::now();
      
      for(int i = 0; i < opts_.thread_num; ++i) {
        threads[i] = std::thread(&Client::thread_func, this, i, filesize);
      }

      for(int i = 0; i < opts_.thread_num; ++i) {
        threads[i].join();
      }

      // end send file
      auto end = std::chrono::steady_clock::now(); 

      std::cout << "Elapsed time in milliseconds: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        << " ms" << std::endl;

      bgack.join();

      const char* eof = "eof";
      n = -1;
      while(n == -1) {
        n = sendto(sock_fd_, (const char*)eof, strlen(eof), 
                      MSG_CONFIRM, (struct sockaddr *) &servaddr_, sizeof(servaddr_));
      }
      close(fd_);
      return Status::Ok;
    }
  private:
    Options opts_;
    int sock_fd_, fd_;
    std::string server_addr_;
    struct sockaddr_in servaddr_;
    std::map<int, std::atomic<int>> ack_status;
    MutexQueue task_queue;
  private:
    void thread_func(int tid, int filesize) {
      char read_buf[16388];
      while(true) {
        int idx = task_queue.pop();
        if(idx == -1) return;
        int read_offset = idx * opts_.block_size;
        int length = (read_offset + opts_.block_size > filesize) ? filesize - read_offset : opts_.block_size;
#ifdef DEBUG
        std::cout << "tid: " << tid << ", read_index: " << idx << ", read offset: " << read_offset << std::endl;
#endif
         // add data to read_buf
        int n = pread(fd_, read_buf + sizeof(uint32_t), length, read_offset);
        assert(n == length);
        // add block index to read_buf header
        memcpy(read_buf, &idx, sizeof(uint32_t));
        n = sendto(sock_fd_, (const char*)read_buf, length + sizeof(uint32_t), 
                        MSG_CONFIRM, (struct sockaddr *) &servaddr_, sizeof(servaddr_));
        int ack_index;
        socklen_t len = sizeof(servaddr_);
        // acquire ACK
        int cnt = 0;
        while(cnt < opts_.retry_count) {
          n = recvfrom(sock_fd_, (char*)&ack_index, sizeof(uint32_t), 0,  ( struct sockaddr *) &servaddr_, &len);
          if(n != -1) {
            ack_status[ack_index].store(1);
            break;
          }
          ++cnt;
        }
      }
    }

    void sweep() {
      int status = 1;
      while(true) {
        for(auto it = ack_status.begin(); it != ack_status.end(); ++it) {
          if(it->second.load() == 0) {
            // add to task queue if it hasn't been acked
            task_queue.push(it->first);
            if(status == 1) status = 0;
          }
        }
        if(status == 1) {
          task_queue.kill();
          return;
        }
        status = 1;
        sleep(2);
      }
    }

    inline size_t GetFileSize(const std::string& filename) {
      struct stat statbuf;
      stat(filename.c_str(), &statbuf);
      return statbuf.st_size;
    }

    std::string ParseFileName(const std::string& filepath) {
      int start = filepath.length() - 1;
      while(start >= 0 && filepath[start] != '/') { --start; }
      return filepath.substr(start + 1);
    }
};

// ./client server_address file_path
int main(int argc, char* argv[]) {
  if(argc < 3) {
    perror("argument less than required");
    exit(EXIT_FAILURE);
  }

  std::string server_addr = argv[1];
  std::string filepath = argv[2];

  Options options;
  options.block_size = 16384;
  options.thread_num = 10;
  Client client(options, server_addr);
  Status s = client.Init();
  if(s != Status::Ok) {
    std::cout << "failed to init client" << std::endl;
    exit(EXIT_FAILURE);
  }
  s = client.Upload(filepath);
  if (s != Status::Ok) {
    std::cout << "failed to upload" << std::endl;
    exit(EXIT_FAILURE);
  }
}