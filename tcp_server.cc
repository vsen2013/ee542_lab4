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
#include <sys/stat.h>
#include <chrono>

using namespace std;

//缓存大小设置不能超过2M
#define BUFF_SIZE (1024 * 1024)
#define FILE_NAME_LENGTH 1024


// off64_t getFileSize(char *filePath) {
//     FILE *f;
//     f = fopen(filePath, "rb");
//     if (NULL == f) {
//         printf("getFileSize fopen error\n");
//         return -1;
//     }

//     if (0 != fseeko64(f, 0, SEEK_END)) {
//         printf("getFileSize fseek error\n");
//         return -1;
//     }

//     off64_t fileSize = ftello64(f);
//     if (fileSize < 0) {
//         printf("ftell error\n");
//     }
//     printf("fileSize:%lld\n", fileSize);
//     fclose(f);
//     return fileSize;
// }


inline size_t GetFileSize(const std::string& filename) {
    struct stat statbuf;
    stat(filename.c_str(), &statbuf);
    return statbuf.st_size;
}

char *getFileName(char *filePath) {
    bool bFound = false;
    char *buff = new char[1024];
    memset(buff, 0, 1024);
    while (!bFound) {
        int lastIndex = 0;
        for (int i = 0; i < strlen(filePath); ++i) {
            if (filePath[i] == '\\' || filePath[i] == '/') {
                lastIndex = i;
            }
        }
        for (int i = lastIndex + 1; i < strlen(filePath); ++i) {
            buff[i - lastIndex - 1] = filePath[i];
        }
        bFound = true;
    }
    return buff;
}

void thread_func(int sock_fd, int fd, uint32_t read_size, uint32_t offset) {
    char buff[BUFF_SIZE];              /* buffer for sending & receiving data */
    size_t sendSize = 0;
    // send threadlocal size info
    std::string size_str = std::to_string(read_size);
    size_str += ",";
    size_str += std::to_string(offset);
    std::cout << "metadata: " << size_str << std::endl; 
    if (send(sock_fd, (const char*)size_str.c_str(), size_str.length(), 0) < 0) {
        printf("send fileSize to client error\n");
        exit(7);
    }
    // // printf("sizeof:%ld strlen:%ld\n", sizeof(fileName), strlen(fileName));
    // if (send(ns, filename.c_str(), filename.length(), 0) < 0) {
    //     printf("send fileName to client error\n");
    //     exit(7);
    // }
    while (sendSize < read_size) {
        memset(buff, 0, 1024 * 1024);
        size_t iread = pread(fd, buff, BUFF_SIZE, offset + sendSize);
        // size_t iread = fread(buff, sizeof(char), BUFF_SIZE, f);
        if (iread < 0) {
            printf("fread error\n");
            break;
        }
        int iSend = send(sock_fd, buff, iread, 0);
        if (iSend < 0) {
            printf("send error\n");
            break;
        }
        sendSize += iSend;
        // printf("fileSize:%ld iSend:%d sendSize:%ld\n", fileSize, iSend, sendSize);
    }
}

int server_s;

int main(int argc, char **argv)
{
    unsigned short port;       /* port server binds to                */
    struct sockaddr_in client; /* client address information          */
    struct sockaddr_in server; /* server address information          */
    socklen_t namelen;               /* length of client name               */
    std::string filename;

    //检查是否传入端口参数
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(1);
    }

    port = (unsigned short) atoi(argv[1]);
    filename = argv[2];
    int thread_count = atoi(argv[3]);

    size_t fileSize = GetFileSize(filename);
    printf("fileSize:%ld\n", fileSize);
    std::cout << "fileName: " << filename << std::endl;

    if ((server_s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("socket error\n");
        exit(2);
    }

    server.sin_family = AF_INET;
    server.sin_port   = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_s, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("bind error\n");
        exit(3);
    }

    if (listen(server_s, 5) != 0)
    {
        printf("listen error\n");
        exit(4);
    }

    int fd = open(filename.c_str(), O_RDONLY);
    if(fd == -1) {
       std::cout << "error reading file" << std::endl;
       exit(4);
    }

    std::vector<int> s(thread_count);
    namelen = sizeof(client);
    uint32_t read_size = fileSize / thread_count;
    std::vector<std::thread> t(thread_count);
    auto start_time = std::chrono::steady_clock::now();
    for(int i = 0; i < thread_count; ++i) {
        if ((s[i] = accept(server_s, (struct sockaddr *)&client, &namelen)) == -1)
        {
            printf("accept error\n");
            exit(5);
        }
        std::cout << "ac'd" << std::endl;
        t[i] = std::thread(&thread_func, s[i], fd, read_size, i * read_size);
    }

    for(int i = 0; i < thread_count; ++i) {
        t[i].join();
    }
    auto end_time = std::chrono::steady_clock::now(); // Stop measuring time
    std::chrono::duration<double,std::milli> duration = end_time - start_time;
    std::cout << "Time taken for file transfer: " << duration.count() << " ms" << std::endl;

    printf("Server ended successfully\n");
    close(fd);
    close(server_s);
    for(int i = 0; i < thread_count; ++i) {
        close(s[i]);
    }
    exit(0);
}