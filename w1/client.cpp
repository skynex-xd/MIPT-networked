#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <unistd.h>
#include "socket_tools.h"

int main(int argc, const char **argv)
{
  const char *port = "2025";

  addrinfo resAddrInfo;
  int sfd = create_dgram_socket("localhost", port, &resAddrInfo);

  if (sfd == -1)
  {
    printf("Cannot create a socket\n");
    return 1;
  }

  std::string hello = "HELLO";
  sendto(sfd, hello.c_str(), hello.size(), 0, resAddrInfo.ai_addr, resAddrInfo.ai_addrlen);

  while (true)
  {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(sfd, &readSet);
    FD_SET(STDIN_FILENO, &readSet);

    int maxfd = std::max(sfd, STDIN_FILENO);
    select(maxfd + 1, &readSet, nullptr, nullptr, nullptr);

    if (FD_ISSET(sfd, &readSet))
    {
      char buf[1024];
      sockaddr_in from;
      socklen_t fromlen = sizeof(from);
      ssize_t len = recvfrom(sfd, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fromlen);
      if (len > 0)
      {
        buf[len] = '\0';
        std::cout << "\n[Server]: " << buf << "\n>";
        std::cout.flush();
      }
    }

    if (FD_ISSET(STDIN_FILENO, &readSet))
    {
      std::string input;
      std::getline(std::cin, input);
      sendto(sfd, input.c_str(), input.size(), 0, resAddrInfo.ai_addr, resAddrInfo.ai_addrlen);
    }
  }

  return 0;
}
