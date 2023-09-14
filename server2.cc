#include <bits/stdc++.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>

#define PORT 3038
#define MAXLINE 16388

std::unordered_map<uint32_t, bool> ack_map; // Map to track received packets

void send_acknowledgment(int sock_fd, const struct sockaddr *clientaddr, socklen_t len) {
    // Create an acknowledgment packet
    char ack_buf[4] = "ACK";
    sendto(sock_fd, ack_buf, sizeof(ack_buf), MSG_CONFIRM, clientaddr, len);
}

int main() {
    int sock_fd;
    struct sockaddr_in servaddr, clientaddr;

    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&clientaddr, 0, sizeof(clientaddr));

    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    if (bind(sock_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Create or open the file for writing
    int fd = open("./result.txt", O_CREAT | O_RDWR);

    char buffer[MAXLINE];
    socklen_t len = sizeof(clientaddr);
    // Receive the file size from the client
    int recvd_len = recvfrom(sock_fd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *)&clientaddr, &len);
    buffer[recvd_len] = '\0';
    ssize_t filesize = stoi(std::string(buffer));
    std::cout << "file size is: " << filesize << std::endl;
    if (fallocate(fd, FALLOC_FL_KEEP_SIZE, 0, filesize) == -1) {
        perror("fallocate failed");
        exit(EXIT_FAILURE);
    }
    memset(buffer, 0, MAXLINE);

    int cur = 0;
    while (cur < filesize) {
        int recvd_len = recvfrom(sock_fd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *)&clientaddr, &len);
        if (recvd_len != -1) {
            // Extract the block index from the packet
            uint32_t index;
            memcpy(&index, buffer, sizeof(uint32_t));
            // Check if the packet has not been received before
            if (ack_map.find(index) == ack_map.end()) {
                // Write data to offset
                int written_len = pwrite(fd, buffer + sizeof(uint32_t), recvd_len - sizeof(uint32_t), index * 16384);
                if (written_len == -1) {
                    perror("write data to disk failed");
                    exit(EXIT_FAILURE);
                }
                cur += written_len;
                // Mark the packet as received in the acknowledgment map
                ack_map[index] = true;
                std::cout << "\rcurrent size: " << cur << ", get block index: " << index << std::endl;
            }
            // Send acknowledgment for the received packet
            send_acknowledgment(sock_fd, (struct sockaddr *)&clientaddr, len);
        }
    }

    close(fd);
}

