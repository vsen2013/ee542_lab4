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

#define PORT 3038

inline size_t GetFileSize(const std::string& filename) {
  struct stat statbuf;
	stat(filename.c_str(), &statbuf);
	return statbuf.st_size;
}

int block_size = 16384;
void thread_func(int sock_fd, int tid, int fd, sockaddr_in servaddr, ssize_t read_offset, ssize_t read_size, int start_index) {
  std::cout << "thread id: " << tid << " reads start at " << read_offset << " for " << read_size << std::endl;
  int cnt = 0;
  char read_buf[16388];
  while(cnt <= read_size) {
    std::cout << "tid: " << tid << ", cnt " << cnt + read_offset << ", read size: " << read_size << ", index: " << start_index << std::endl;
    int length = (cnt + block_size > read_size) ? read_size - cnt : block_size;
    // add data to read_buf
    int n = pread(fd, read_buf + sizeof(uint32_t), length, read_offset + cnt);
    if(n != length) {
      exit(EXIT_FAILURE);
    }
    // add block index to read_buf header
    memcpy(read_buf, &start_index, sizeof(uint32_t));
    start_index++;
    n = sendto(sock_fd, (const char*)read_buf, length + sizeof(uint32_t), 
                    MSG_CONFIRM, (struct sockaddr *) &servaddr, sizeof(servaddr));
    sleep(0.5);
    cnt += block_size;
  }
}

// ./client server_address file_path
int main(int argc, char* argv[]) {
  if(argc < 3) {
    perror("argument less than required");
    exit(EXIT_FAILURE);
  }

  std::string server_addr = argv[1];
  std::string filepath = argv[2];

  // file open and validation
  int fd = open(filepath.c_str(), O_RDONLY);
  if (fd == -1) {
    exit(EXIT_FAILURE);
  }

  // network setup
  int sock_fd;
  struct sockaddr_in servaddr;
  
  if((sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&servaddr, 0, sizeof(servaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(server_addr.c_str());
  servaddr.sin_port = htons(PORT);

  // first send file length
  size_t filesize = GetFileSize(filepath);
  std::string size_str = std::to_string(filesize);
  int n = -1;
  while(n == -1) {
    n = sendto(sock_fd, (const char*)size_str.c_str(), size_str.length(), 
                MSG_CONFIRM, (struct sockaddr *) &servaddr, sizeof(servaddr));
  }

  int thread_num = 3;
  int index_count = filesize / block_size;
  int block_per_thread = index_count / thread_num;
  int size_per_thread = block_per_thread * block_size;
  int last_size_per_thread = filesize - (thread_num - 1) * size_per_thread;
  std::vector<std::thread> threads(thread_num);
  for(int i = 0; i < thread_num; ++i) {
    int read_size = size_per_thread;
    if(i == thread_num - 1) { // treat last size_per_thread carefully
      read_size = last_size_per_thread;
    }
    threads[i] = std::thread(thread_func, sock_fd, i, fd, servaddr, i * size_per_thread, read_size, i * block_per_thread);
  }

  for(int i = 0; i < thread_num; ++i) {
    threads[i].join();
  }

  // send file using multiple threads
  close(fd);
}