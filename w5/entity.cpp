#include "entity.h"
#include "mathUtils.h"

constexpr float kWorldLimit = 30.f;

static float wrap_position(float pos, float limit)
{
  if (pos < -limit)
    return pos + 2.f * limit;
  if (pos > limit)
    return pos - 2.f * limit;
  return pos;
}

void simulate_entity(Entity &e, float dt)
{
  const bool braking = sign(e.thr) < 0.f;
  const float accelRate = braking ? 12.f : 3.5f;
  const float appliedForce = clamp(e.thr, -0.3f, 3.f) * accelRate;

  e.vx += dt * appliedForce * cosf(e.ori);
  e.vy += dt * appliedForce * sinf(e.ori);
  e.omega += e.steer * dt * 0.3f;

  e.ori += e.omega * dt;
  e.x += e.vx * dt;
  e.y += e.vy * dt;

  e.x = wrap_position(e.x, kWorldLimit);
  e.y = wrap_position(e.y, kWorldLimit);
}
