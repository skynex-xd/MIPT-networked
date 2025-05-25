#include "raylib.h"
#include <enet/enet.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

struct Player {
  int id;
  float x, y;
  int ping;
};

void send_fragmented_packet(ENetPeer *peer) {
  const char *baseMsg = "Stay awhile and listen. ";
  const size_t msgLen = strlen(baseMsg);
  const size_t sendSize = 2500;

  char *hugeMessage = new char[sendSize];
  for (size_t i = 0; i < sendSize; ++i)
    hugeMessage[i] = baseMsg[i % msgLen];
  hugeMessage[sendSize - 1] = '\0';

  ENetPacket *packet = enet_packet_create(hugeMessage, sendSize, ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(peer, 0, packet);
  delete[] hugeMessage;
}

void send_micro_packet(ENetPeer *peer) {
  const char *msg = "dv/dt";
  ENetPacket *packet = enet_packet_create(msg, strlen(msg) + 1, ENET_PACKET_FLAG_UNSEQUENCED);
  enet_peer_send(peer, 1, packet);
}

void send_position(ENetPeer *peer, float x, float y) {
  char posMsg[64];
  snprintf(posMsg, sizeof(posMsg), "POS %.2f %.2f", x, y);
  ENetPacket *packet = enet_packet_create(posMsg, strlen(posMsg) + 1, ENET_PACKET_FLAG_UNSEQUENCED);
  enet_peer_send(peer, 0, packet);
}

int main(int argc, const char **argv) {
  int width = 800, height = 600;
  InitWindow(width, height, "w2 MIPT networked");
  const int scrWidth = GetMonitorWidth(0);
  const int scrHeight = GetMonitorHeight(0);
  if (scrWidth < width || scrHeight < height) {
    width = std::min(scrWidth, width);
    height = std::min(scrHeight - 150, height);
    SetWindowSize(width, height);
  }

  SetTargetFPS(60);

  if (enet_initialize() != 0) {
    std::cerr << "Cannot init ENet" << std::endl;
    return 1;
  }

  ENetHost *client = enet_host_create(nullptr, 2, 2, 0, 0);
  if (!client) {
    std::cerr << "Cannot create ENet client" << std::endl;
    return 1;
  }

  ENetAddress lobbyAddress;
  enet_address_set_host(&lobbyAddress, "localhost");
  lobbyAddress.port = 10887;

  ENetPeer *lobbyPeer = enet_host_connect(client, &lobbyAddress, 2, 0);
  if (!lobbyPeer) {
    std::cerr << "Cannot connect to lobby" << std::endl;
    return 1;
  }

  uint32_t timeStart = enet_time_get();
  uint32_t lastFragmentedSendTime = timeStart;
  uint32_t lastMicroSendTime = timeStart;
  uint32_t lastPositionSendTime = timeStart;

  bool connectedToLobby = false;
  bool connectedToGameServer = false;
  ENetPeer *gamePeer = nullptr;

  std::string gameServerStatus = "Connecting to lobby...";
  std::vector<Player> players;

  float posx = GetRandomValue(100, 500);
  float posy = GetRandomValue(100, 500);
  float velx = 0.f, vely = 0.f;

  int myPlayerId = -1;

  while (!WindowShouldClose()) {
    const float dt = GetFrameTime();
    ENetEvent event;

    while (enet_host_service(client, &event, 10) > 0) {
      switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT:
        if (event.peer == lobbyPeer) {
          connectedToLobby = true;
          gameServerStatus = "Connected to lobby";
        } else if (event.peer == gamePeer) {
          connectedToGameServer = true;
          gameServerStatus = "Connected to game server";
        }
        break;

      case ENET_EVENT_TYPE_RECEIVE:
        {
          const char* data = (const char*)event.packet->data;

          if (event.peer == lobbyPeer && !connectedToGameServer) {
            if (strncmp(data, "GAMESERVER", 10) == 0) {
              char serverIP[256];
              int serverPort;
              if (sscanf(data, "GAMESERVER %s %d", serverIP, &serverPort) == 2) {
                ENetAddress gameAddress;
                enet_address_set_host(&gameAddress, serverIP);
                gameAddress.port = serverPort;
                gamePeer = enet_host_connect(client, &gameAddress, 2, 0);
                gameServerStatus = gamePeer ? "Connecting to game server..." : "Cannot connect to game server";
              }
            }
          }
          else if (event.peer == gamePeer) {
            if (strncmp(data, "WELCOME", 7) == 0) {
              sscanf(data, "WELCOME %d", &myPlayerId);
              gameServerStatus = "Playing as player " + std::to_string(myPlayerId);
            } 
            else if (strncmp(data, "PLAYERS", 7) == 0) {
              players.clear();
              std::istringstream ss(std::string(data + 8));
              Player player;
              std::string token;
              while (std::getline(ss, token, ';')) {
                std::istringstream playerStream(token);
                if (playerStream >> player.id >> player.x >> player.y >> player.ping)
                  players.push_back(player);
              }
            }
            else if (strncmp(data, "POS", 3) == 0) {
              int playerId;
              float x, y;
              if (sscanf(data, "POS %d %f %f", &playerId, &x, &y) == 3) {
                for (auto& player : players) {
                  if (player.id == playerId) {
                    player.x = x;
                    player.y = y;
                    break;
                  }
                }
              }
            }
            else if (strncmp(data, "NEWPLAYER", 9) == 0) {
              int playerId;
              if (sscanf(data, "NEWPLAYER %d", &playerId) == 1) {
                bool exists = false;
                for (const auto& p : players)
                  if (p.id == playerId)
                    exists = true;
                if (!exists)
                  players.push_back(Player{playerId, 0.f, 0.f, 0});
              }
            }
            else if (strncmp(data, "PLAYERLEFT", 10) == 0) {
              int playerId;
              if (sscanf(data, "PLAYERLEFT %d", &playerId) == 1) {
                players.erase(std::remove_if(players.begin(), players.end(),
                  [=](const Player& p) { return p.id == playerId; }), players.end());
              }
            }
            else if (strncmp(data, "PINGS", 5) == 0) {
              std::istringstream ss(std::string(data + 6));
              std::string token;
              while (std::getline(ss, token, ';')) {
                if (token.empty()) continue;
                int playerId, ping;
                std::istringstream ts(token);
                if (ts >> playerId >> ping) {
                  bool found = false;
                  for (auto& p : players) {
                    if (p.id == playerId) {
                      p.ping = ping;
                      found = true;
                      break;
                    }
                  }
                  if (!found && playerId != myPlayerId)
                    players.push_back(Player{playerId, 0.f, 0.f, ping});
                }
              }
            }
          }

          enet_packet_destroy(event.packet);
        }
        break;

      case ENET_EVENT_TYPE_DISCONNECT:
        if (event.peer == lobbyPeer) {
          connectedToLobby = false;
          gameServerStatus = "Disconnected from lobby";
          lobbyPeer = nullptr;
        } else if (event.peer == gamePeer) {
          connectedToGameServer = false;
          gameServerStatus = "Disconnected from game server";
          gamePeer = nullptr;
        }
        break;

      default: break;
      }
    }

    if (connectedToGameServer && enet_time_get() - lastPositionSendTime > 50) {
      lastPositionSendTime = enet_time_get();
      send_position(gamePeer, posx, posy);
    }

    if (connectedToLobby) {
      uint32_t cur = enet_time_get();
      if (cur - lastFragmentedSendTime > 1000) lastFragmentedSendTime = cur; // send_fragmented_packet(lobbyPeer);
      if (cur - lastMicroSendTime > 100) lastMicroSendTime = cur; // send_micro_packet(lobbyPeer);
    }

    if (IsKeyPressed(KEY_ESCAPE)) break;
    if (IsKeyPressed(KEY_ENTER) && connectedToLobby) {
      ENetPacket *packet = enet_packet_create("Start!", 7, ENET_PACKET_FLAG_RELIABLE);
      enet_peer_send(lobbyPeer, 0, packet);
    }

    constexpr float accel = 30.f;
    velx += ((IsKeyDown(KEY_LEFT) ? -1.f : 0.f) + (IsKeyDown(KEY_RIGHT) ? 1.f : 0.f)) * dt * accel;
    vely += ((IsKeyDown(KEY_UP) ? -1.f : 0.f) + (IsKeyDown(KEY_DOWN) ? 1.f : 0.f)) * dt * accel;
    posx += velx * dt;
    posy += vely * dt;
    velx *= 0.99f;
    vely *= 0.99f;

    BeginDrawing();
      ClearBackground(BLACK);
      DrawText(TextFormat("Current status: %s", gameServerStatus.c_str()), 20, 20, 20, WHITE);
      DrawText(TextFormat("My position: (%d, %d)", (int)posx, (int)posy), 20, 40, 20, WHITE);
      DrawCircleV(Vector2{posx, posy}, 10.f, WHITE);
      DrawText("List of players:", 20, 60, 20, WHITE);
      int yOffset = 80;
      for (const auto& p : players) {
        DrawText(TextFormat("Player %d: (%d, %d) - Ping: %d ms", p.id, (int)p.x, (int)p.y, p.ping), 20, yOffset, 18, WHITE);
        yOffset += 20;
        if (p.id != myPlayerId) {
          DrawCircleV(Vector2{p.x, p.y}, 10.f, RED);
          DrawText(TextFormat("%d", p.id), p.x - 5, p.y - 5, 16, WHITE);
        }
      }
    EndDrawing();
  }

  if (lobbyPeer) enet_peer_disconnect(lobbyPeer, 0);
  if (gamePeer) enet_peer_disconnect(gamePeer, 0);

  ENetEvent e;
  while (enet_host_service(client, &e, 1000) > 0) {
    if (e.type == ENET_EVENT_TYPE_RECEIVE) enet_packet_destroy(e.packet);
  }

  enet_host_destroy(client);
  enet_deinitialize();
  CloseWindow();
  return 0;
}
