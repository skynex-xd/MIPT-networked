#include <enet/enet.h>
#include <iostream>
#include <vector>
#include <map>
#include <chrono>
#include <cstdlib>

#include "entity.h"
#include "protocol.h"
#include "mathUtils.h"

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

static std::vector<Entity> entities;
static std::map<uint16_t, ENetPeer*> controlledMap;

static uint32_t frameCounter = 0;
static TimePoint serverStartTime;
constexpr int DELAY = 200000;

void on_join(ENetPacket *packet, ENetPeer *peer, ENetHost *host)
{
  for (const Entity &ent : entities)
    send_new_entity(peer, ent);

  uint16_t maxEid = invalid_entity;
  for (const Entity &e : entities)
    maxEid = std::max(maxEid, e.eid);

  uint16_t newEid = maxEid + 1;
  uint32_t color = 0xff000000 +
                   0x00440000 * (rand() % 5) +
                   0x00004400 * (rand() % 5) +
                   0x00000044 * (rand() % 5);
  float x = (rand() % 4) * 5.f;
  float y = (rand() % 4) * 5.f;

  Entity ent;
  ent.color = color;
  ent.x = x;
  ent.y = y;
  ent.vx = 0.f;
  ent.vy = 0.f;
  ent.ori = (rand() / (float)RAND_MAX) * 3.141592654f;
  ent.omega = 0.f;
  ent.thr = 0.f;
  ent.steer = 0.f;
  ent.eid = newEid;

  entities.push_back(ent);
  controlledMap[newEid] = peer;

  for (size_t i = 0; i < host->peerCount; ++i)
    send_new_entity(&host->peers[i], ent);

  send_set_controlled_entity(peer, newEid);
}

void on_input(ENetPacket *packet)
{
  uint16_t eid;
  float thr, steer;
  deserialize_entity_input(packet, eid, thr, steer);
  for (Entity &e : entities)
    if (e.eid == eid) {
      e.thr = thr;
      e.steer = steer;
    }
}

void update_net(ENetHost* server)
{
  ENetEvent event;
  while (enet_host_service(server, &event, 0) > 0)
  {
    switch (event.type)
    {
    case ENET_EVENT_TYPE_CONNECT:
      printf("Client connected from %x:%u\n", event.peer->address.host, event.peer->address.port);
      break;
    case ENET_EVENT_TYPE_RECEIVE:
      switch (get_packet_type(event.packet))
      {
        case MessageType::ClientJoin:
          on_join(event.packet, event.peer, server);
          break;
        case MessageType::ClientInput:
          on_input(event.packet);
          break;
      }
      enet_packet_destroy(event.packet);
      break;
    default:
      break;
    }
  }
}

void simulate_world(ENetHost* server, float dt)
{
  TimePoint now = Clock::now();
  for (Entity &e : entities)
  {
    simulate_entity(e, dt);
    for (size_t i = 0; i < server->peerCount; ++i)
    {
      ENetPeer* peer = &server->peers[i];
      send_snapshot(peer, e.eid, e.x, e.y, e.ori, e.vx, e.vy, e.omega, now, frameCounter);
    }
  }
}

void update_time(ENetHost* server, uint32_t curTime)
{
  for (size_t i = 0; i < server->peerCount; ++i)
    send_time_msec(&server->peers[i], curTime);
}

int main(int argc, const char** argv)
{
  if (enet_initialize() != 0)
  {
    std::cerr << "Failed to initialize ENet." << std::endl;
    return 1;
  }

  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = 10131;
  ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
  if (!server)
  {
    std::cerr << "Failed to create ENet server." << std::endl;
    return 1;
  }

  serverStartTime = Clock::now();
  frameCounter = 0;

  uint32_t lastTime = enet_time_get();
  float accumulated = 0.f;

  while (true)
  {
    uint32_t curTime = enet_time_get();
    float elapsed = static_cast<float>(curTime - lastTime);
    lastTime = curTime;
    accumulated += elapsed;

    if (accumulated >= FIXED_DT * 1000.f)
    {
      simulate_world(server, FIXED_DT);
      update_net(server);
      update_time(server, curTime);
      frameCounter++;
      accumulated -= FIXED_DT * 1000.f;
    }

    usleep(DELAY);
  }

  enet_host_destroy(server);
  atexit(enet_deinitialize);
  return 0;
}
