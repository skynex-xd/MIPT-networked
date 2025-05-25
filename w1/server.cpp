#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <set>
#include <map>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include "socket_tools.h"

int main(int argc, const char **argv)
{
  const char *port = "2025";

  int sfd = create_dgram_socket(nullptr, port, nullptr);
  if (sfd == -1)
  {
    printf("cannot create socket\n");
    return 1;
  }
  printf("listening!\n");

  std::set<std::string> known_clients;
  std::map<std::string, sockaddr_in> clients;

  std::set<std::string> duel_queue;
  std::string duel_client1, duel_client2;
  int correct_answer = 0;
  bool duel_active = false;

  srand(time(NULL));

  while (true)
  {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(sfd, &readSet);

    timeval timeout = { 0, 100000 };
    select(sfd + 1, &readSet, NULL, NULL, &timeout);

    if (FD_ISSET(sfd, &readSet))
    {
      constexpr size_t buf_size = 1000;
      static char buffer[buf_size];
      memset(buffer, 0, buf_size);

      sockaddr_in sin;
      socklen_t slen = sizeof(sockaddr_in);
      ssize_t numBytes = recvfrom(sfd, buffer, buf_size - 1, 0, (sockaddr*)&sin, &slen);

      if (numBytes > 0)
      {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(sin.sin_addr), ip_str, INET_ADDRSTRLEN);
        int port = ntohs(sin.sin_port);

        std::ostringstream oss;
        oss << ip_str << ":" << port;
        std::string client_id = oss.str();
        std::string msg(buffer);

        clients[client_id] = sin;

        if (msg.compare(0, 5, "HELLO") == 0)
        {
          if (known_clients.find(client_id) == known_clients.end())
          {
            known_clients.insert(client_id);
            std::cout << "New client: " << client_id << " -> " << msg << std::endl;
          }
          else
          {
            std::cout << "Known client again: " << client_id << std::endl;
          }
        }
        else if (msg.rfind("/c ", 0) == 0)
        {
          std::string to_send = msg;
          for (const auto& [id, addr] : clients)
          {
            sendto(sfd, to_send.c_str(), to_send.size(), 0, (sockaddr*)&addr, sizeof(addr));
          }
        }
        else if (msg == "/mathduel")
        {
          if (duel_active)
          {
            std::string already = "Дуэль уже идёт!";
            sendto(sfd, already.c_str(), already.size(), 0, (sockaddr*)&sin, sizeof(sin));
          }
          else
          {
            duel_queue.insert(client_id);
            std::string wait = "Ожидаем второго участника...";
            sendto(sfd, wait.c_str(), wait.size(), 0, (sockaddr*)&sin, sizeof(sin));

            if (duel_queue.size() == 2)
            {
              auto it = duel_queue.begin();
              duel_client1 = *it++;
              duel_client2 = *it;
              duel_queue.clear();
              duel_active = true;

              int a = rand() % 50 + 10;
              int b = rand() % 10 + 1;
              int c = rand() % 20;
              correct_answer = a * b - c;

              std::ostringstream task;
              task << "Math Duel Started! Solve: " << a << " * " << b << " - " << c << " = ?";
              std::string task_str = task.str();

              sendto(sfd, task_str.c_str(), task_str.size(), 0, (sockaddr*)&clients[duel_client1], sizeof(sockaddr_in));
              sendto(sfd, task_str.c_str(), task_str.size(), 0, (sockaddr*)&clients[duel_client2], sizeof(sockaddr_in));
            }
          }
        }
        else if (msg.rfind("/ans ", 0) == 0 && duel_active)
        {
          int ans = std::stoi(msg.substr(5));
          if ((client_id == duel_client1 || client_id == duel_client2) && ans == correct_answer)
          {
            std::string winner_msg = "Победитель дуэли: " + client_id;
            for (const auto& [id, addr] : clients)
            {
              sendto(sfd, winner_msg.c_str(), winner_msg.size(), 0, (sockaddr*)&addr, sizeof(addr));
            }
            duel_active = false;
          }
        }
        else
        {
          std::cout << "Message from " << client_id << ": " << msg << std::endl;
        }
      }
    }
  }

  return 0;
}
