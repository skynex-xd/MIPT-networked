#pragma once
#include <enet/enet.h>
#include <cstdint>
#include <chrono>
#include "entity.h"

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

constexpr float FIXED_DT = 1.0f / 10.0f;

enum class MessageType : uint8_t
{
  ClientJoin = 0,
  ServerNewEntity,
  ServerSetControlled,
  ClientInput,
  ServerSnapshot,
  ServerTimeSync
};

// Отправка
void send_join(ENetPeer* peer);
void send_new_entity(ENetPeer* peer, const Entity& ent);
void send_set_controlled_entity(ENetPeer* peer, uint16_t eid);
void send_entity_input(ENetPeer* peer, uint16_t eid, float thr, float steer);
void send_snapshot(ENetPeer* peer, uint16_t eid, float x, float y, float ori,
                   float vx, float vy, float omega, TimePoint timestamp, uint32_t frameNumber);
void send_time_msec(ENetPeer* peer, uint32_t timeMsec);

// Получение
MessageType get_packet_type(ENetPacket* packet);

void deserialize_new_entity(ENetPacket* packet, Entity& ent);
void deserialize_set_controlled_entity(ENetPacket* packet, uint16_t& eid);
void deserialize_entity_input(ENetPacket* packet, uint16_t& eid, float& thr, float& steer);
void deserialize_snapshot(ENetPacket* packet, uint16_t& eid, float& x, float& y, float& ori,
                          float& vx, float& vy, float& omega, TimePoint& timestamp, uint32_t& frameNumber);
void deserialize_time_msec(ENetPacket* packet, uint32_t& timeMsec);
