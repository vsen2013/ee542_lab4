#include<sys/types.h> 
#include<sys/socket.h> 
#include<unistd.h> 
#include<netinet/in.h> 
#include<arpa/inet.h> 
#include<stdio.h> 
#include<stdlib.h> 
#include<errno.h> 
#include<netdb.h> 
#include<stdarg.h> 
#include<string.h> 
#include<string>
#include<iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <chrono>

using namespace std;
#define BUFFER_SIZE 8972
#define PACKET_SIZE 8968
#define DATA_SIZE 4484

inline size_t GetFileSize(const std::string& filename) {
	struct stat statbuf;
	stat(filename.c_str(), &statbuf);
	return statbuf.st_size;
}

int main(int argc, char **argv) 
{ 
	if (argc < 3) {
		cout << "Usage: " << argv[0] << " <server ip> <server port> "<< endl;
		exit(0);
	}
	string server_ip = argv[1];
	unsigned short port = atoi(argv[2]);
	std::cout << "server_ip: " << server_ip << " " << port << std::endl;

	std::string filepath = argv[3];

 	struct sockaddr_in server_addr, client_addr; 
 
 	socklen_t client_addr_length = sizeof(client_addr); 
 	bzero(&server_addr, sizeof(server_addr)); 
 	server_addr.sin_family = AF_INET; 
 	server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str()); 
 	server_addr.sin_port = htons(port); 
  
 	int client_socket_fd = socket(AF_INET, SOCK_DGRAM, 0); 
 	if(client_socket_fd < 0) 
 	{ 
  		perror("Create Socket Failed:"); 
  		exit(1); 
 	} 

	uint32_t n = 2048*1024*1024;  // set send buffer to 1G
	setsockopt(client_socket_fd, SOL_SOCKET, SO_SNDBUFFORCE, &n, sizeof(n));

	// int curRcvBufSize = -1;
	// socklen_t sz = sizeof(curRcvBufSize);
    // if (getsockopt(client_socket_fd, SOL_SOCKET, SO_SNDBUF, &curRcvBufSize, &sz) < 0)
    // {
    //     printf("getsockopt error=%d(%s)!!!\n", errno, strerror(errno));
    // }
	// std::cout << curRcvBufSize << std::endl;
  
 	char buf[BUFFER_SIZE]; 
 	int len = BUFFER_SIZE;
	int i = 1;
	int max = 10;
	// get file length
	size_t filesize = GetFileSize(filepath);
	std::string size_str = std::to_string(filesize);

	// file open and validation
	int fd_ = open(filepath.c_str(), O_RDONLY);
	if (fd_ == -1) { return -1; }
	char* file_addr = reinterpret_cast<char*>(mmap(0, filesize, PROT_READ, MAP_SHARED, fd_, 0));

	uint32_t offset = 0;
	uint32_t total_offset = filesize / DATA_SIZE;
	socklen_t servaddr_len = sizeof(server_addr); 
	auto start_time = std::chrono::high_resolution_clock::now();
	int cnt = 0;
	while(cnt <= 1) {
		offset = 0;
		while(offset <= total_offset){
			int length = (offset * DATA_SIZE + PACKET_SIZE > filesize) ? filesize - (offset * DATA_SIZE) : PACKET_SIZE;
			memcpy(buf, &offset, sizeof(uint32_t));
			memcpy(buf + sizeof(uint32_t), file_addr + offset * DATA_SIZE, length);
			int res = sendto(client_socket_fd, buf, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr,sizeof(server_addr));
			if(res == -1) 
			{ 
				perror("send to relay fail:"); 
				exit(1); 
			}
			// ack specific
			// {
			// 	int ack_index = 0;
			// 	res = recvfrom(client_socket_fd, &ack_index, sizeof(ack_index), MSG_WAITALL, (struct sockaddr*)&server_addr, &servaddr_len);
			// 	if(res == -1) {
			// 		std::cout << "recvfrom failed" << std::endl;
			// 		continue;
			// 	}
			// 	if(ack_index != offset) {
			// 		std::cout << "ack_index mismatch " << ack_index << " " << offset << std::endl;
			// 		continue;
			// 	}
			// }
			++offset;
		}
		++cnt;
	}
	auto end_time = std::chrono::high_resolution_clock::now(); // Stop measuring time
	std::chrono::duration<double> duration = end_time - start_time;
	std::cout << "Time taken for file transfer: " << duration.count() << " seconds" << std::endl;
	munmap(file_addr, filesize);
 	close(client_socket_fd); 
 	return 0; 
} 