#include <iostream>

#include <stdio.h>
#include <time.h>
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


using namespace std;

#define BUFF_SIZE (1024 * 1024)

#define FILE_NAME_LENGTH 1024

int s;                     /* client socket                       */

void thread_func(int tid, int sock_fd, sockaddr_in server, int fd) {
    std::cout << "ready to connet to server from tid: " << tid << std::endl;;
    if (connect(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("Connect error\n");
        exit(4);
    }

    char buf[BUFF_SIZE];

    memset(buf, 0, BUFF_SIZE);
    int iRecv = recv(sock_fd, buf, BUFF_SIZE, 0);
    if (iRecv < 0) {
        printf("recv fileSize error\n");
        exit(5);
    }
    std::cout << "recv: " << iRecv << std::endl;
    buf[iRecv] = '\0';
    std::string metadata(buf, iRecv);
    size_t delimiter = metadata.find(',');
    size_t read_size = stoi(metadata.substr(0, delimiter));
    size_t offset = stoi(metadata.substr(delimiter + 1));
    std::cout << "offset: " << offset << ", read_size: " << read_size << std::endl;
    // memset(buf, 0, BUFF_SIZE);
    // iRecv = recv(s, buf, BUFF_SIZE, 0);
    // if (iRecv < 0) {
    //     printf("recv fileName error\n");
    //     exit(5);
    // }
    // buf[iRecv] = '\0';
    // std::string fileName(buf);
    // std::cout << "recv fileName: " << fileName;

    uint32_t fileRecv = 0;

    while (fileRecv < read_size) {
        memset(buf, 0, BUFF_SIZE);
        iRecv = recv(sock_fd, buf, BUFF_SIZE, 0);
        if (iRecv < 0)
        {
            printf("Recv error\n");
            exit(6);
        }
        if (iRecv == 0) {
            break;
        }
        
        // std::cout << "tid: " << tid << ", recv: " << fileRecv << std::endl;
        pwrite(fd, buf, iRecv, offset + fileRecv);
        fileRecv += iRecv;
    }
}



/*
 * Client Main.
 */
int main(int argc, char** argv)
{
    printf("start...\n");

    unsigned short port;       
    struct hostent *hostnm;    
    struct sockaddr_in server; 

    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s hostname port thread_count\n", argv[0]);
        exit(1);
    }

    hostnm = gethostbyname(argv[1]);
    if (hostnm == (struct hostent *) 0)
    {
        fprintf(stderr, "Gethostbyname failed\n");
        exit(2);
    }

    port = (unsigned short) atoi(argv[2]);
    int thread_count = atoi(argv[3]);

    //put the server information into the server structure.
    //The port must be put into network byte order.
    server.sin_family      = AF_INET;
    server.sin_port        = htons(port);
    server.sin_addr.s_addr = *((unsigned long *)hostnm->h_addr);

    std::string fileName("result.txt");
    int fd = open(fileName.c_str(), O_CREAT | O_RDWR);
    if (fd == -1) {
        exit(5);
    }

    std::vector<int> s(thread_count);
    for(int i = 0; i < thread_count; ++i) {
        if ((s[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("Socket error\n");
            exit(3);
        }
    }

    std::cout << "socket created" << std::endl;

    std::vector<std::thread> ts(thread_count);
    for(int i = 0; i < thread_count; ++i) {
        ts[i] = std::thread(&thread_func, i, s[i], server, fd);
    }

    for(int i = 0; i < thread_count; ++i) {
        ts[i].join();
    }
    
    for(int i = 0; i < thread_count; ++i) {
        close(s[i]);
    }

    close(fd);

    printf("Client Ended Successfully\n");
    exit(0);
}