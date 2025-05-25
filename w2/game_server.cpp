#include <enet/enet.h>
#include "raylib.h"
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <random>
#include <chrono>

struct Player {
  int id;
  float x, y;
  int ping;
  ENetPeer* peer;
  std::string name;
};

int generatePlayerID() {
  static int nextID = 1;
  return nextID++;
}

void sendPlayerList(const std::vector<Player>& players, ENetPeer* targetPeer) {
  std::stringstream ss;
  ss << "PLAYERS ";
  for (const auto& player : players)
    ss << player.id << " " << player.x << " " << player.y << " " << player.ping << ";";

  std::string str = ss.str();
  ENetPacket* packet = enet_packet_create(str.c_str(), str.size() + 1, ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(targetPeer, 0, packet);
}

void broadcastNewPlayer(const Player& newPlayer, const std::vector<Player>& players) {
  std::stringstream ss;
  ss << "NEWPLAYER " << newPlayer.id << " " << newPlayer.name;
  std::string str = ss.str();

  for (const auto& player : players) {
    if (player.id == newPlayer.id) continue;
    ENetPacket* packet = enet_packet_create(str.c_str(), str.length() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(player.peer, 0, packet);
  }
}

void broadcastPositions(const std::vector<Player>& players) {
  for (const auto& sender : players) {
    std::stringstream ss;
    ss << "POS " << sender.id << " " << sender.x << " " << sender.y;
    std::string str = ss.str();

    for (const auto& receiver : players) {
      if (receiver.id == sender.id) continue;
      ENetPacket* packet = enet_packet_create(str.c_str(), str.length() + 1, ENET_PACKET_FLAG_UNSEQUENCED);
      enet_peer_send(receiver.peer, 0, packet);
    }
  }
}

void broadcastPings(const std::vector<Player>& players) {
  for (const auto& player : players) {
    std::stringstream ss;
    ss << "PINGS ";
    for (const auto& p : players)
      ss << p.id << " " << p.ping << ";";

    std::string str = ss.str();
    ENetPacket* packet = enet_packet_create(str.c_str(), str.length() + 1, 0);
    enet_peer_send(player.peer, 0, packet);
  }
}

int main(int argc, const char **argv) {
  if (enet_initialize() != 0) {
    std::cerr << "Cannot init ENet\n";
    return 1;
  }

  int port = (argc > 1) ? std::atoi(argv[1]) : 10888;

  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = port;

  ENetHost *server = enet_host_create(&address, 32, 2, 0, 0);
  if (!server) {
    std::cerr << "Cannot create ENet game server\n";
    return 1;
  }

  std::cout << "Game server started on port " << port << "\n";

  std::vector<Player> players;
  uint32_t lastBroadcastTime = enet_time_get();
  uint32_t lastPingTime = enet_time_get();

  while (true) {
    ENetEvent event;
    while (enet_host_service(server, &event, 10) > 0) {
      switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
          std::cout << "Player connected from " << event.peer->address.host
                    << ":" << event.peer->address.port << "\n";

          Player newPlayer;
          newPlayer.id = generatePlayerID();
          newPlayer.name = "Player_" + std::to_string(newPlayer.id);
          newPlayer.x = GetRandomValue(100, 500);
          newPlayer.y = GetRandomValue(100, 300);
          newPlayer.ping = event.peer->roundTripTime;
          newPlayer.peer = event.peer;

          event.peer->data = new int(newPlayer.id);

          std::string welcome = "WELCOME " + std::to_string(newPlayer.id) + " " + newPlayer.name;
          ENetPacket* welcomePacket = enet_packet_create(welcome.c_str(), welcome.length() + 1, ENET_PACKET_FLAG_RELIABLE);
          enet_peer_send(event.peer, 0, welcomePacket);

          sendPlayerList(players, event.peer);
          players.push_back(newPlayer);
          broadcastNewPlayer(newPlayer, players);
          break;
        }

        case ENET_EVENT_TYPE_RECEIVE: {
          int* playerID = static_cast<int*>(event.peer->data);
          if (playerID) {
            auto it = std::find_if(players.begin(), players.end(), 
              [=](const Player& p) { return p.id == *playerID; });

            if (it != players.end()) {
              it->ping = event.peer->roundTripTime;

              std::string msg((const char*)event.packet->data);
              if (msg.rfind("POS ", 0) == 0) {
                std::istringstream ss(msg.substr(4));
                float x, y;
                if (ss >> x >> y) {
                  it->x = x;
                  it->y = y;
                }
              }
            }
          }

          enet_packet_destroy(event.packet);
          break;
        }

        case ENET_EVENT_TYPE_DISCONNECT: {
          std::cout << "Player disconnected from " << event.peer->address.host
                    << ":" << event.peer->address.port << "\n";

          int* playerID = static_cast<int*>(event.peer->data);
          if (playerID) {
            auto it = std::find_if(players.begin(), players.end(),
              [=](const Player& p) { return p.id == *playerID; });

            if (it != players.end()) {
              int id = *playerID;
              players.erase(it);

              std::string leftMsg = "PLAYERLEFT " + std::to_string(id);
              for (const auto& p : players) {
                ENetPacket* packet = enet_packet_create(leftMsg.c_str(), leftMsg.length() + 1, ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(p.peer, 0, packet);
              }
            }

            delete playerID;
            event.peer->data = nullptr;
          }
          break;
        }

        default: break;
      }
    }

    uint32_t now = enet_time_get();
    if (now - lastBroadcastTime > 50) {
      lastBroadcastTime = now;
      if (!players.empty()) broadcastPositions(players);
    }

    if (now - lastPingTime > 500) {
      lastPingTime = now;
      if (!players.empty()) broadcastPings(players);
    }
  }

  for (auto& p : players) {
    if (p.peer->data) {
      delete static_cast<int*>(p.peer->data);
      p.peer->data = nullptr;
    }
  }

  enet_host_destroy(server);
  enet_deinitialize();
  return 0;
}
