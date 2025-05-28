#pragma once
#include <cstdint>

constexpr uint16_t kInvalidEntity = static_cast<uint16_t>(-1);

struct Entity
{
  uint32_t color = 0xff00ffff;
  float x = 0.f;
  float y = 0.f;
  float vx = 0.f;
  float vy = 0.f;
  float ori = 0.f;
  float omega = 0.f; 
  float thr = 0.f;
  float steer = 0.f;
  uint16_t eid = kInvalidEntity;
};

void simulate_entity(Entity &e, float dt);
