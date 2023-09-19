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
#include<iostream>
#include<string>
#include<sys/syscall.h>   
#include <thread>
#include <fcntl.h>
#include <vector>
#include <atomic>
#include <sys/mman.h>
#include "lf_queue.h"

using namespace std;
#define BUFFER_SIZE 8973
#define PACKET_SIZE 8969
#define DATA_SIZE 8969
#define THREAD_COUNT 2

string g_ip;
unsigned short g_port;
pid_t gettid()
{      
	return syscall(SYS_gettid); 
} 
inline string IpU32ToString(unsigned ipv4)
{
    char buf[INET_ADDRSTRLEN] = {0};
    struct in_addr in;
    in.s_addr = ipv4;

    if(inet_ntop(AF_INET, &in ,buf, sizeof(buf)))
    {
        return string(buf);
    }
    else
    {
        return string("");
    }

}

// void start_udp_server(string &ip, unsigned port, int fd){
// 	cout << "ip: " << ip << " " << port << " " << gettid() << endl;
// 	uint64_t mmap_size = 1073741824;
// 	uint32_t total_index = mmap_size / DATA_SIZE;
// 	struct sockaddr_in server_addr;  
// 	socklen_t server_addr_length = sizeof(server_addr); 
// 	//bzero(&server_addr, sizeof(server_addr)); 
// 	server_addr.sin_family = AF_INET; 
// 	server_addr.sin_addr.s_addr = inet_addr(ip.c_str()); 
// 	server_addr.sin_port = htons(port); 
// 	int server_socket_fd = socket(AF_INET, SOCK_DGRAM, 0); 
// 	if(server_socket_fd < 0) 
// 	{ 
// 		perror("Create Socket Failed:"); 
// 		exit(1); 
// 	}
// 	int opt_val = 1;
// 	setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val));
// 	uint32_t n = 2048*1024*1024;  // set recv buffer to 1G
// 	setsockopt(server_socket_fd, SOL_SOCKET, SO_RCVBUFFORCE, &n, sizeof(n));

// 	ftruncate(fd, mmap_size);
// 	char* mmap_addr = (char*)mmap(0, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	
//  	if(-1 == (bind(server_socket_fd,(struct sockaddr*)&server_addr,sizeof(server_addr)))) 
//  	{ 
//   		perror("Server Bind Failed:"); 
//   		exit(1); 
//  	}

// 	while(1){  
//   		struct sockaddr_in client_addr; 
//   		socklen_t client_addr_length = sizeof(client_addr); 
  
//   		char buffer[BUFFER_SIZE]; 
//   		bzero(buffer, BUFFER_SIZE); 
// 		int res = recvfrom(server_socket_fd, buffer, BUFFER_SIZE, 0,(struct sockaddr*)&client_addr, &client_addr_length);
//   		if(res == -1) 
//   		{ 
//    			perror("Receive Data Failed:"); 
//    			exit(1); 
//   		} 
// 		int index = 0;
// 		memcpy(&index, buffer, sizeof(uint32_t));
// 		// send back ACK
// 		sendto(server_socket_fd, &index, sizeof(index), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
//   		if(index % 5000 == 0) std::cout << "index: " << index << std::endl;
// 		int length = (index * DATA_SIZE + PACKET_SIZE > mmap_size) ? mmap_size - (index * DATA_SIZE) : PACKET_SIZE;
// 		memcpy(mmap_addr + index * DATA_SIZE, buffer + sizeof(uint32_t), length);
// 		if(index == total_index) break;
//  	}
// 	munmap(mmap_addr, mmap_size);
// }

// void thread_func(int fd){
// 	start_udp_server(g_ip, g_port, fd);
// }

struct sockaddr_in client_addr;
socklen_t client_addr_length = sizeof(client_addr); 

std::vector<MutexQueue*> task_queues(THREAD_COUNT);

void thread_main(int fd) {
	uint32_t cnt = 0;
	while(1){
		char buffer[BUFFER_SIZE]; 
  		// bzero(buffer, BUFFER_SIZE); 
		int res = recvfrom(fd, buffer, BUFFER_SIZE, 0,(struct sockaddr*)&client_addr, &client_addr_length);
		std::string data(buffer, BUFFER_SIZE);
  		if(res == -1) 
  		{ 
   			perror("Receive Data Failed:"); 
   			exit(1); 
  		}
		task_queues[cnt++ % THREAD_COUNT]->push(data);
 	}
}

void thread_worker(int tid, int sock_fd, char* mmap_addr, uint32_t mmap_size) {
	while(1) {
		char* data = const_cast<char*>(task_queues[tid]->pop().c_str());
		int index = 0;
		memcpy(&index, data, sizeof(uint32_t));
		// send back ACK
		sendto(sock_fd, &index, sizeof(index), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
		if(index % 5000 == 0) std::cout << "index: " << index << std::endl;
		int length = (index * DATA_SIZE + PACKET_SIZE > mmap_size) ? mmap_size - (index * DATA_SIZE) : PACKET_SIZE;
		std::cout << "start memcpy" << std::endl;
		memcpy(mmap_addr + index * DATA_SIZE, data + sizeof(uint32_t), length);
		std::cout << "end memcpy" << std::endl;
	}
}

int main(int argc, char **argv){
	
	if (argc < 4) {
		cout << "Usage: " << argv[0] << " <local_ip> <udp_port> <thread_count>"<< endl;
		exit(0);
	}
	g_ip = argv[1];
	g_port = atoi(argv[2]);
	int thread_count = atoi(argv[3]);
	cout << "ip: " << g_ip << " port: " << g_port << " thread_cout: " << thread_count << endl;
	int fd = open("./result.txt", O_CREAT | O_RDWR);

	// setup
	cout << "ip: " << g_ip << " " << g_port << " " << gettid() << endl;
	uint64_t mmap_size = 1073741824;
	uint32_t total_index = mmap_size / DATA_SIZE;
	struct sockaddr_in server_addr;  
	socklen_t server_addr_length = sizeof(server_addr); 
	//bzero(&server_addr, sizeof(server_addr)); 
	server_addr.sin_family = AF_INET; 
	server_addr.sin_addr.s_addr = inet_addr(g_ip.c_str()); 
	server_addr.sin_port = htons(g_port); 
	int server_socket_fd = socket(AF_INET, SOCK_DGRAM, 0); 
	if(server_socket_fd < 0) 
	{ 
		perror("Create Socket Failed:"); 
		exit(1); 
	}
	int opt_val = 1;
	setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val));
	uint32_t n = 2048*1024*1024;  // set recv buffer to 1G
	setsockopt(server_socket_fd, SOL_SOCKET, SO_RCVBUFFORCE, &n, sizeof(n));

	ftruncate(fd, mmap_size);
	char* mmap_addr = (char*)mmap(0, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	
 	if(-1 == (bind(server_socket_fd,(struct sockaddr*)&server_addr,sizeof(server_addr)))) 
 	{ 
  		perror("Server Bind Failed:"); 
  		exit(1); 
 	}

	// open work threads
	std::vector<std::thread> threads(thread_count);
	for(int i = 0; i < thread_count; i++){
		task_queues[i] = new MutexQueue();
		threads[i] = std::thread(&thread_worker, i, server_socket_fd, mmap_addr, mmap_size);
	}

	thread_main(server_socket_fd);

	for(int i = 0; i < thread_count; ++i) {
		threads[i].join();
		delete task_queues[i];
	}
	return 0;
}