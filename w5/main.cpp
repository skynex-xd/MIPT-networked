#include <functional>
#include "raylib.h"
#include <enet/enet.h>
#include <cmath>
#include <vector>
#include <deque>
#include <unordered_map>
#include <chrono>
#include "entity.h"
#include "protocol.h"

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

struct InputCommand {
  uint32_t frame;
  float thr;
  float steer;
};

struct EntityState {
  float x, y, ori, vx, vy, omega;
  uint32_t frame;
};

static std::vector<Entity> entities;
static std::unordered_map<uint16_t, size_t> entityMap;
static std::deque<InputCommand> inputHistory;
static std::deque<EntityState> stateHistory;

static uint16_t my_entity = invalid_entity;
static uint32_t clientFrame = 0;

void on_new_entity_packet(ENetPacket *packet)
{
  Entity newEntity;
  deserialize_new_entity(packet, newEntity);
  if (entityMap.find(newEntity.eid) != entityMap.end())
    return;
  entityMap[newEntity.eid] = entities.size();
  entities.push_back(newEntity);
}

void on_set_controlled_entity(ENetPacket *packet)
{
  deserialize_set_controlled_entity(packet, my_entity);
}

void on_snapshot(ENetPacket *packet)
{
  uint16_t eid;
  float x, y, ori;
  deserialize_snapshot(packet, eid, x, y, ori);
  auto it = entityMap.find(eid);
  if (it != entityMap.end()) {
    Entity &e = entities[it->second];
    e.x = x;
    e.y = y;
    e.ori = ori;
  }
}

int main(int argc, const char **argv)
{
  if (enet_initialize() != 0) {
    printf("Failed to initialize ENet\n");
    return 1;
  }

  ENetHost *client = enet_host_create(nullptr, 1, 2, 0, 0);
  if (!client) {
    printf("Failed to create ENet client\n");
    return 1;
  }

  ENetAddress address;
  enet_address_set_host(&address, "localhost");
  address.port = 10131;
  ENetPeer *serverPeer = enet_host_connect(client, &address, 2, 0);
  if (!serverPeer) {
    printf("Failed to connect to server\n");
    return 1;
  }

  int width = 600, height = 600;
  InitWindow(width, height, "Networked Game Client");

  Camera2D camera = {{0, 0}, {width / 2.0f, height / 2.0f}, 0.f, 10.f};
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    ENetEvent event;
    while (enet_host_service(client, &event, 0) > 0) {
      if (event.type == ENET_EVENT_TYPE_CONNECT) {
        send_join(serverPeer);
      } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
        switch (get_packet_type(event.packet)) {
          case E_SERVER_TO_CLIENT_NEW_ENTITY:
            on_new_entity_packet(event.packet); break;
          case E_SERVER_TO_CLIENT_SET_CONTROLLED_ENTITY:
            on_set_controlled_entity(event.packet); break;
          case E_SERVER_TO_CLIENT_SNAPSHOT:
            on_snapshot(event.packet); break;
          default: break;
        }
        enet_packet_destroy(event.packet);
      }
    }

    if (my_entity != invalid_entity) {
      bool left = IsKeyDown(KEY_LEFT);
      bool right = IsKeyDown(KEY_RIGHT);
      bool up = IsKeyDown(KEY_UP);
      bool down = IsKeyDown(KEY_DOWN);

      float thr = (up ? 1.f : 0.f) - (down ? 1.f : 0.f);
      float steer = (right ? 1.f : 0.f) - (left ? 1.f : 0.f);

      inputHistory.push_back({clientFrame, thr, steer});
      if (inputHistory.size() > 100) inputHistory.pop_front();

      auto it = entityMap.find(my_entity);
      if (it != entityMap.end()) {
        Entity &e = entities[it->second];
        e.thr = thr;
        e.steer = steer;
        simulate_entity(e, dt);

        stateHistory.push_back({e.x, e.y, e.ori, e.vx, e.vy, e.omega, clientFrame});
        if (stateHistory.size() > 200) stateHistory.pop_front();

        send_entity_input(serverPeer, my_entity, thr, steer);
      }

      clientFrame++;
    }

    BeginDrawing();
      ClearBackground(GRAY);
      BeginMode2D(camera);
        for (const Entity &e : entities) {
          Rectangle rect = {e.x, e.y, 3.f, 1.f};
          DrawRectanglePro(rect, {0.f, 0.5f}, e.ori * 180.f / PI, GetColor(e.color));
        }
      EndMode2D();
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
