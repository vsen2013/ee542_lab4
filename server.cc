#include <bits/stdc++.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT 3038
#define MAXLINE 16384

int main() {
  int sock_fd;
  struct sockaddr_in servaddr, clientaddr;
  
  if((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&servaddr, 0, sizeof(servaddr));
  memset(&clientaddr, 0, sizeof(clientaddr));

  servaddr.sin_family    = AF_INET; // IPv4
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(PORT);

  if (bind(sock_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 )
  {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  char buffer[MAXLINE];
  socklen_t len = sizeof(clientaddr);
  // recvfrom
  int n = recvfrom(sock_fd, (char*) buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &clientaddr, &len);
  buffer[n] = '\0';
  ssize_t size = stoi(std::string(buffer));
  std::cout << "file size is: " << size << std::endl;
  memset(buffer, 0, MAXLINE);

  int cur = 0;
  while(cur < size) {
    int n = recvfrom(sock_fd, (char*) buffer, MAXLINE, MSG_WAITALL, ( struct sockaddr *) &clientaddr, &len);
    if(n != -1) {
      cur += n;
      std::cout << "\rcursize: " << cur << ", get file: " << std::string(buffer) << std::endl;
    }
  }
}