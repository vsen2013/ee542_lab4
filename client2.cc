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

#include "util.h"

// Define the maximum number of retries for packet retransmission.
#define MAX_RETRIES 10

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
        std::cout << "filename: " << filename << std::endl;
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
        int block_per_thread = index_count / opts_.thread_num;
        int size_per_thread = block_per_thread * opts_.block_size;
        int last_size_per_thread = filesize - (opts_.thread_num - 1) * size_per_thread;
        std::vector<std::thread> threads(opts_.thread_num);
        for(int i = 0; i < opts_.thread_num; ++i) {
            int read_size = size_per_thread;
            if(i == opts_.thread_num - 1) { // treat last size_per_thread carefully
                read_size = last_size_per_thread;
            }
            threads[i] = std::thread(&Client::thread_func, this, sock_fd_, i, fd_, servaddr_, i * size_per_thread, read_size, i * block_per_thread);
        }

        for(int i = 0; i < opts_.thread_num; ++i) {
            threads[i].join();
        }
        close(fd_);
        return Status::Ok;
    }
private:
    Options opts_;
    int sock_fd_, fd_;
    std::string server_addr_;
    struct sockaddr_in servaddr_;
private:
    void thread_func(int sock_fd, int tid, int fd, sockaddr_in servaddr, ssize_t read_offset, ssize_t read_size, int start_index) {
        std::cout << "thread id: " << tid << " reads start at " << read_offset << " for " << read_size << std::endl;
        int cnt = 0;
        char read_buf[16388];
        uint32_t packet_no = start_index; // Initialize packet number.
        int retries = 0; // Initialize retry count.
        while(cnt < read_size) {
            std::cout << "tid: " << tid << ", cnt " << cnt + read_offset << ", read size: " << read_size << ", index: " << packet_no << std::endl;
            int length = (cnt + opts_.block_size > read_size) ? read_size - cnt : opts_.block_size;
            // add data to read_buf
            int n = pread(fd, read_buf + sizeof(uint32_t), length, read_offset + cnt);
            assert(n == length);
            // add block index to read_buf header
            memcpy(read_buf, &packet_no, sizeof(uint32_t));
            
            bool sent = false;
            while (!sent && retries < MAX_RETRIES) {
                n = sendto(sock_fd, (const char*)read_buf, length + sizeof(uint32_t), 
                            MSG_CONFIRM, (struct sockaddr *) &servaddr, sizeof(servaddr));
                
                if (n == -1) {
                    std::cerr << "Failed to send packet. Retrying..." << std::endl;
                    retries++;
                } else {
                    retries = 0; // Reset retry count upon successful send.
                    int index_confirm;
                    socklen_t len = sizeof(servaddr);
                    // Wait for ACK
                    n = recvfrom(sock_fd, (char*)&index_confirm, sizeof(uint32_t), MSG_WAITALL, ( struct sockaddr *) &servaddr, &len);
                    if (n == -1) {
                        std::cerr << "Failed to receive ACK. Retrying..." << std::endl;
                    } else {
                        if (index_confirm == packet_no) {
                            sent = true;
                            packet_no++; // Increment packet number.
                            cnt += opts_.block_size;
                        } else {
                            std::cerr << "Received incorrect ACK. Retrying..." << std::endl;
                        }
                    }
                }
            }
            
            if (retries >= MAX_RETRIES) {
                std::cerr << "Max retries exceeded. Aborting thread." << std::endl;
                break;
            }
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
    options.thread_num = 1;
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
