#include "TrafficSystem.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

// ─── Tuning constants ───────────────────────────────────────────────────────
constexpr float MAX_SPAWN_DIST   = 120.0f;
constexpr float MIN_SPAWN_DIST   = 40.0f;
constexpr float DESPAWN_DIST     = 160.0f;
constexpr size_t MAX_NPCS        = 12;

// NPC driving params (same physics model as player car)
constexpr float NPC_MAX_SPEED        = 5.5f;   // m/s  (relaxed cruise)
constexpr float NPC_ACCELERATION     = 4.0f;
constexpr float NPC_BRAKE_POWER      = 10.0f;
constexpr float NPC_FRICTION         = 3.5f;
constexpr float NPC_STEER_RESPONSE   = 3.5f;
constexpr float NPC_STEER_RETURN     = 6.0f;
constexpr float NPC_MAX_STEERING     = 0.6108f; // ~35 deg
constexpr float NPC_TURN_RATE        = 2.18f;   // rad/s  (125 deg)
constexpr float NPC_COLLISION_RADIUS = kCarModelScale * 1.3f;
constexpr float NPC_FRONT_OFFSET     = kCarModelScale * 1.1f;

// Road sense
constexpr float SENSE_DIST     = 6.0f;   // how far ahead to probe
constexpr float SENSE_LATERAL  = 1.5f;   // lateral offset for left/right probes
constexpr float SENSE_INTERVAL = 0.08f;  // seconds between road checks (faster = better avoidance)

constexpr float POLICE_SPEED_MULT = 1.6f;

// ─── Constructor ─────────────────────────────────────────────────────────────
TrafficSystem::TrafficSystem(Model* sharedCarModel)
    : mCarModel(sharedCarModel), wantedLevel(0), spawnTimer(0.0f)
{
}

void TrafficSystem::AddWantedLevel(int amount)
{
    wantedLevel = std::min(wantedLevel + amount, 5);
    std::cout << "[POLICE] Wanted level: " << wantedLevel << std::endl;
}

void TrafficSystem::ResetWantedLevel()
{
    wantedLevel = 0;
}

// ─── Road sensing ────────────────────────────────────────────────────────────
// Cast a ray along `dir` from `pos`, sampling the ground every 0.8m.
// Returns the distance at which the road disappears (no ground found).
float TrafficSystem::SenseRoad(const City& city, const glm::vec3& pos,
                                const glm::vec3& dir, float maxDist) const
{
    const float step = 0.8f;
    for (float d = step; d <= maxDist; d += step)
    {
        glm::vec3 probe = pos + dir * d;
        game::GroundSample sample;
        bool ok = city.GetGroundSample(probe, pos.y + 1.0f, sample, 3.0f, 0.5f);
        if (!ok || !sample.found)
            return d - step;          // road ended
        // Also check for wall collision slightly elevated
        glm::vec3 collisionProbe = probe;
        collisionProbe.y += 0.6f;
        if (city.CheckCollision(collisionProbe, NPC_COLLISION_RADIUS * 1.5f))
            return d - step;          // wall ahead
    }
    return maxDist;
}

// ─── Spawn ───────────────────────────────────────────────────────────────────
void TrafficSystem::SpawnNPCNearPlayer(const glm::vec3& playerPos, const City& city)
{
    if (mActiveNPCs.size() >= MAX_NPCS) return;

    // Try random directions around player to find a road spot
    int failGround = 0, failCollision = 0, failRoad = 0;
    for (int attempt = 0; attempt < 40; ++attempt)
    {
        float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
        float dist  = MIN_SPAWN_DIST + (float)(rand() % (int)(MAX_SPAWN_DIST - MIN_SPAWN_DIST));
        
        glm::vec3 candidate = playerPos + glm::vec3(cos(angle) * dist, 0.0f, sin(angle) * dist);
        
        // Check there's actual ground here (very generous search)
        game::GroundSample sample;
        bool onRoad = city.GetGroundSample(candidate, playerPos.y + 10.0f, sample, 20.0f, 5.0f);
        if (!onRoad || !sample.found) { failGround++; continue; }
        
        candidate.y = sample.height + kGroundClearance;
        
        // Make sure it's not inside a building
        if (city.CheckCollision(candidate, NPC_COLLISION_RADIUS)) { failCollision++; continue; }
        
        // Find best direction to face (pick direction with most open road)
        float bestYaw = 0.0f;
        float bestRoad = 0.0f;
        for (int a = 0; a < 8; ++a)
        {
            float testYaw = a * 3.14159f * 0.25f;
            glm::vec3 testDir = glm::vec3(sin(testYaw), 0.0f, cos(testYaw));
            float road = SenseRoad(city, candidate, testDir, 6.0f);
            if (road > bestRoad)
            {
                bestRoad = road;
                bestYaw = testYaw;
            }
        }
        if (bestRoad < 1.5f) { failRoad++; continue; } // relaxed from 3.0 to 1.5
        
        // Good spawn point!
        NPCCar npc;
        
        bool spawnPolice = (wantedLevel > 0) && ((rand() % 100) < (wantedLevel * 10));
        if (spawnPolice) {
            npc.type = NPCType::POLICE;
            npc.color = glm::vec3(0.1f, 0.1f, 0.3f);
            npc.policeState = PoliceState::CHASE;
            npc.stateTimer = 0.0f;
            npc.desiredSpeed = NPC_MAX_SPEED * POLICE_SPEED_MULT;
        } else {
            npc.type = NPCType::CIVILIAN;
            // Nice car colors (not random soup)
            const glm::vec3 palette[] = {
                {0.85f, 0.12f, 0.12f}, // red
                {0.10f, 0.10f, 0.80f}, // blue
                {0.10f, 0.70f, 0.20f}, // green
                {0.90f, 0.85f, 0.10f}, // yellow
                {0.95f, 0.95f, 0.95f}, // white
                {0.15f, 0.15f, 0.15f}, // black
                {0.90f, 0.45f, 0.05f}, // orange
                {0.50f, 0.05f, 0.60f}, // purple
                {0.00f, 0.75f, 0.75f}, // teal
                {0.55f, 0.27f, 0.07f}, // brown
            };
            npc.color = palette[rand() % 10];
            npc.policeState = PoliceState::PATROL;
            npc.stateTimer = 0.0f;
            npc.desiredSpeed = NPC_MAX_SPEED * (0.7f + (rand() % 30) / 100.0f);
        }
        
        npc.position = candidate;
        npc.yaw = bestYaw;
        npc.pitch = 0.0f;
        npc.roll = 0.0f;
        npc.speed = npc.desiredSpeed * 0.5f; // start moving
        npc.verticalSpeed = 0.0f;
        npc.steering = 0.0f;
        npc.wheelSpin = 0.0f;
        npc.steerInput = 0.0f;
        npc.roadCheckTimer = 0.0f;
        npc.stuckTimer = 0.0f;
        npc.recovering = false;
        npc.recoverTimer = 0.0f;
        npc.recoverTurnDir = (rand() % 2 == 0) ? 1.0f : -1.0f;
        
        mActiveNPCs.push_back(npc);
        std::cout << "[TRAFFIC] Spawned NPC at (" << candidate.x << ", " << candidate.y << ", " << candidate.z << ") road=" << bestRoad << "m" << std::endl;
        return; // one per call
    }
    // If we get here, all attempts failed
    static int spawnFailCount = 0;
    if (++spawnFailCount % 10 == 1) {
        std::cout << "[TRAFFIC] Spawn failed (x" << spawnFailCount << ") ground=" << failGround << " collision=" << failCollision << " road=" << failRoad << " playerY=" << playerPos.y << std::endl;
    }
}

void TrafficSystem::DespawnDistantNPCs(const glm::vec3& playerPos)
{
    auto it = std::remove_if(mActiveNPCs.begin(), mActiveNPCs.end(),
        [&playerPos](const NPCCar& npc) {
            return glm::distance(npc.position, playerPos) > DESPAWN_DIST;
        });
    mActiveNPCs.erase(it, mActiveNPCs.end());
}

// ─── Main update ─────────────────────────────────────────────────────────────
void TrafficSystem::Update(float dt, CarState& playerCar, const City& city)
{
    spawnTimer += dt;
    if (spawnTimer > 0.8f) {
        spawnTimer = 0.0f;
        SpawnNPCNearPlayer(playerCar.position, city);
        DespawnDistantNPCs(playerCar.position);
    }

    for (auto& npc : mActiveNPCs) {
        // --- Stuck detection & recovery ---
        if (npc.recovering) {
            npc.recoverTimer -= dt;
            if (npc.recoverTimer <= 0.0f) {
                // Done recovering, pick a new direction
                npc.recovering = false;
                npc.stuckTimer = 0.0f;
                // Turn 90-180 degrees away from wall
                npc.yaw += npc.recoverTurnDir * (1.5f + (rand() % 100) / 100.0f);
                npc.speed = 0.0f;
                npc.steering = 0.0f;
            } else {
                // During recovery: reverse slowly with a turn
                npc.desiredSpeed = 0.0f;
                npc.speed = std::max(npc.speed - NPC_BRAKE_POWER * dt, 0.0f);
                npc.steerInput = npc.recoverTurnDir;
                // Also rotate yaw directly during recovery for a faster escape
                npc.yaw += npc.recoverTurnDir * 2.0f * dt;
            }
        } else {
            // Track how long we've been stuck
            if (npc.speed < 0.3f && npc.desiredSpeed > 1.0f) {
                npc.stuckTimer += dt;
            } else {
                npc.stuckTimer = 0.0f;
            }
            
            // If stuck for 1 second, enter recovery
            if (npc.stuckTimer > 1.0f) {
                npc.recovering = true;
                npc.recoverTimer = 0.8f; // 0.8s to turn around
                npc.recoverTurnDir = (rand() % 2 == 0) ? 1.0f : -1.0f;
                npc.stuckTimer = 0.0f;
            }
        }
        
        // 1. AI decides steerInput and desiredSpeed (unless recovering)
        if (!npc.recovering) {
            if (npc.type == NPCType::CIVILIAN) {
                SteerCivilian(npc, dt, playerCar, city);
            } else {
                SteerPolice(npc, dt, playerCar, city);
            }
        }
        
        // 2. Apply the exact same physics as the player car
        UpdateNPCPhysics(npc, dt, city);
    }
}

// ─── NPC Physics (mirrors CarController::UpdateCar exactly) ──────────────────
void TrafficSystem::UpdateNPCPhysics(NPCCar& npc, float dt, const City& city)
{
    // --- Speed (acceleration / braking / friction) ---
    float throttle = (npc.speed < npc.desiredSpeed) ? 1.0f : 0.0f;
    if (npc.desiredSpeed < 0.1f) throttle = -1.0f; // brake
    
    if (throttle > 0.0f)
        npc.speed += NPC_ACCELERATION * dt;
    else if (throttle < 0.0f)
        npc.speed -= NPC_BRAKE_POWER * dt;
    else {
        if (npc.speed > 0.0f)
            npc.speed = std::max(0.0f, npc.speed - NPC_FRICTION * dt);
    }
    npc.speed = glm::clamp(npc.speed, 0.0f, npc.desiredSpeed * 1.1f);

    // --- Steering (smooth input) ---
    if (std::abs(npc.steerInput) > 0.01f)
        npc.steering += npc.steerInput * NPC_STEER_RESPONSE * dt;
    else {
        if (npc.steering > 0.0f)
            npc.steering = std::max(0.0f, npc.steering - NPC_STEER_RETURN * dt);
        if (npc.steering < 0.0f)
            npc.steering = std::min(0.0f, npc.steering + NPC_STEER_RETURN * dt);
    }
    npc.steering = glm::clamp(npc.steering, -NPC_MAX_STEERING, NPC_MAX_STEERING);

    // --- Rotation (same formula as player) ---
    float turnFactor = 0.0f;
    if (npc.speed > 0.01f) {
        float speedRatio = npc.speed / NPC_MAX_SPEED;
        turnFactor = 0.35f + 0.65f * speedRatio;
    }
    npc.yaw += npc.steering * turnFactor * NPC_TURN_RATE * dt;

    // --- Movement & Collision (3-sphere, same as player) ---
    glm::vec3 forward = glm::vec3(std::sin(npc.yaw), 0.0f, std::cos(npc.yaw));
    glm::vec3 right   = glm::vec3(std::cos(npc.yaw), 0.0f, -std::sin(npc.yaw));
    glm::vec3 prevPos = npc.position;
    glm::vec3 fullMove = forward * npc.speed * dt;
    glm::vec3 nextPos = npc.position + fullMove;

    auto collidesAt = [&](const glm::vec3& pos) -> bool {
        glm::vec3 cPos = pos;
        cPos.y += 0.4f; // raise to hit walls instead of slipping under
        if (city.CheckCollision(cPos, NPC_COLLISION_RADIUS)) return true;
        if (city.CheckCollision(cPos + forward * NPC_FRONT_OFFSET, NPC_COLLISION_RADIUS)) return true;
        if (city.CheckCollision(cPos - forward * NPC_FRONT_OFFSET, NPC_COLLISION_RADIUS)) return true;
        return false;
    };

    if (collidesAt(nextPos)) {
        // Try axis-separated sliding (same as player)
        glm::vec3 tryX = glm::vec3(nextPos.x, prevPos.y, prevPos.z);
        glm::vec3 tryZ = glm::vec3(prevPos.x, prevPos.y, nextPos.z);
        bool xOk = !collidesAt(tryX);
        bool zOk = !collidesAt(tryZ);

        if (xOk && !zOk) nextPos = tryX;
        else if (zOk && !xOk) nextPos = tryZ;
        else if (xOk && zOk)  nextPos = tryX;
        else {
            npc.speed = 0.0f;
            nextPos = prevPos;
        }
    }
    npc.position = nextPos;

    // --- Ground resolve (gravity + hard snap, same as player) ---
    game::GroundSample groundSample;
    if (city.GetGroundSample(npc.position, npc.position.y, groundSample, 15.0f, 0.5f))
    {
        float groundY = groundSample.height;
        float desiredY = groundY + kGroundClearance;
        // Hard snap to ground (same as player)
        if (npc.position.y <= desiredY + 0.15f) {
            npc.position.y = desiredY;
            npc.verticalSpeed = 0.0f;
        } else {
            // Apply gravity
            npc.verticalSpeed -= 18.0f * dt;
            npc.position.y += npc.verticalSpeed * dt;
        }
    } else {
        // Falling into the void
        npc.verticalSpeed -= 18.0f * dt;
        npc.position.y += npc.verticalSpeed * dt;
        
        // If they fall too far, despawn them by moving them far away
        if (npc.position.y < -50.0f) {
            npc.position.y = 10000.0f;
        }
    }

    // --- Tilt (pitch & roll, same 4-point sampling as player) ---
    const float wheelBase  = 1.0f;
    const float trackWidth = 0.7f;
    const float tiltSmooth = 6.0f;

    glm::vec3 fwd = glm::vec3(std::sin(npc.yaw), 0.0f, std::cos(npc.yaw));
    glm::vec3 frontPos = npc.position + fwd * wheelBase;
    glm::vec3 rearPos  = npc.position - fwd * wheelBase;
    glm::vec3 leftPos  = npc.position + right * trackWidth;
    glm::vec3 rightPos = npc.position - right * trackWidth;

    game::GroundSample sample;
    auto sampleH = [&](const glm::vec3& pos) -> float {
        if (city.GetGroundSample(pos, npc.position.y, sample, 8.0f, 0.5f) && sample.found)
            return sample.height;
        return npc.position.y - kGroundClearance;
    };

    float frontH = sampleH(frontPos);
    float rearH  = sampleH(rearPos);
    float leftH  = sampleH(leftPos);
    float rightH = sampleH(rightPos);

    // Pitch
    float pitchDiff = frontH - rearH;
    float targetPitch = -std::atan2(pitchDiff, wheelBase * 2.0f);
    targetPitch = glm::clamp(targetPitch, glm::radians(-20.0f), glm::radians(20.0f));

    // Roll
    float rollDiff = leftH - rightH;
    float targetRoll = -std::atan2(rollDiff, trackWidth * 2.0f);
    targetRoll = glm::clamp(targetRoll, glm::radians(-6.0f), glm::radians(6.0f));

    npc.pitch = glm::mix(npc.pitch, targetPitch, tiltSmooth * dt);
    npc.roll  = glm::mix(npc.roll,  targetRoll,  tiltSmooth * dt);

    // Wheel spin
    float wheelRadius = 0.38f * kCarModelScale;
    npc.wheelSpin += (npc.speed * dt) / wheelRadius;
}

// ─── Civilian AI: follow the road by sensing ─────────────────────────────────
void TrafficSystem::SteerCivilian(NPCCar& npc, float dt, const CarState& playerCar, const City& city)
{
    glm::vec3 forward = glm::vec3(sin(npc.yaw), 0.0f, cos(npc.yaw));
    glm::vec3 right   = glm::vec3(cos(npc.yaw), 0.0f, -sin(npc.yaw));

    // Periodically sense the road ahead
    npc.roadCheckTimer += dt;
    if (npc.roadCheckTimer >= SENSE_INTERVAL)
    {
        npc.roadCheckTimer = 0.0f;
        
        // Sense forward, forward-left, forward-right
        float roadAhead = SenseRoad(city, npc.position, forward, SENSE_DIST);
        
        glm::vec3 leftDir  = glm::normalize(forward - right * 0.5f);
        glm::vec3 rightDir = glm::normalize(forward + right * 0.5f);
        
        float roadLeft  = SenseRoad(city, npc.position, leftDir,  SENSE_DIST);
        float roadRight = SenseRoad(city, npc.position, rightDir, SENSE_DIST);
        
        // Decide steering
        if (roadAhead < 3.5f) {
            // Road ending / wall ahead – hard turn toward the better side
            if (roadLeft < 1.0f && roadRight < 1.0f) {
                // Completely blocked
                npc.steerInput = npc.recoverTurnDir; 
                npc.desiredSpeed = 0.0f;
            } else if (roadLeft > roadRight) {
                npc.steerInput = 1.0f;   // turn left
            } else {
                npc.steerInput = -1.0f;  // turn right
            }
            npc.desiredSpeed = NPC_MAX_SPEED * 0.2f; // slow down for turn
        }
        else if (roadAhead < SENSE_DIST * 0.6f) {
            // Road narrowing – gentle steer toward better side
            float bias = (roadLeft - roadRight) / SENSE_DIST;
            npc.steerInput = glm::clamp(bias * 2.0f, -1.0f, 1.0f);
            npc.desiredSpeed = NPC_MAX_SPEED * 0.6f;
        }
        else {
            // Road is clear – straighten out with slight bias
            float bias = (roadLeft - roadRight) / SENSE_DIST;
            npc.steerInput = glm::clamp(bias * 0.5f, -0.3f, 0.3f);
            npc.desiredSpeed = NPC_MAX_SPEED * (0.7f + (rand() % 30) / 100.0f);
        }
    }
    
    // Brake for player car if ahead
    float distToPlayer = glm::distance(npc.position, playerCar.position);
    if (distToPlayer < 12.0f) {
        glm::vec3 dirToPlayer = glm::normalize(playerCar.position - npc.position);
        if (glm::dot(forward, dirToPlayer) > 0.7f) {
            npc.desiredSpeed = 0.0f;
        }
    }
    
    // Brake for other NPCs ahead
    for (const auto& other : mActiveNPCs) {
        if (&other == &npc) continue;
        float d = glm::distance(npc.position, other.position);
        if (d < 8.0f && d > 0.5f) {
            glm::vec3 dirToOther = glm::normalize(other.position - npc.position);
            if (glm::dot(forward, dirToOther) > 0.7f) {
                npc.desiredSpeed = std::min(npc.desiredSpeed, other.speed * 0.8f);
            }
        }
    }
}

// ─── Police AI ───────────────────────────────────────────────────────────────
void TrafficSystem::SteerPolice(NPCCar& npc, float dt, const CarState& playerCar, const City& city)
{
    if (wantedLevel == 0) {
        npc.policeState = PoliceState::PATROL;
    } else {
        npc.policeState = PoliceState::CHASE;
    }

    if (npc.policeState == PoliceState::PATROL) {
        SteerCivilian(npc, dt, playerCar, city);
        npc.color = glm::vec3(0.1f, 0.1f, 0.3f);
    }
    else if (npc.policeState == PoliceState::CHASE) {
        // Siren flash
        npc.stateTimer += dt;
        npc.color = (fmod(npc.stateTimer, 0.4f) < 0.2f)
            ? glm::vec3(1.0f, 0.0f, 0.0f)
            : glm::vec3(0.0f, 0.0f, 1.0f);

        glm::vec3 forward = glm::vec3(sin(npc.yaw), 0.0f, cos(npc.yaw));
        glm::vec3 toPlayer = playerCar.position - npc.position;
        toPlayer.y = 0.0f;
        float dist = glm::length(toPlayer);

        if (dist > 2.0f) {
            glm::vec3 dirToPlayer = glm::normalize(toPlayer);
            // Cross product gives which side the player is on
            float cross = forward.x * dirToPlayer.z - forward.z * dirToPlayer.x;
            npc.steerInput = glm::clamp(-cross * 3.0f, -1.0f, 1.0f);
            npc.desiredSpeed = NPC_MAX_SPEED * POLICE_SPEED_MULT + wantedLevel * 1.0f;
        } else {
            npc.steerInput = 0.0f;
            npc.desiredSpeed = 0.0f;
        }
        
        // Still respect walls — sense ahead and override if wall
        float roadAhead = SenseRoad(city, npc.position, forward, 3.0f);
        if (roadAhead < 1.5f) {
            glm::vec3 right = glm::vec3(cos(npc.yaw), 0.0f, -sin(npc.yaw));
            float roadLeft  = SenseRoad(city, npc.position, glm::normalize(forward - right * 0.5f), 3.0f);
            float roadRight = SenseRoad(city, npc.position, glm::normalize(forward + right * 0.5f), 3.0f);
            npc.steerInput = (roadLeft > roadRight) ? 1.0f : -1.0f;
            npc.desiredSpeed = NPC_MAX_SPEED * 0.4f;
        }
    }
}

// ─── Render ──────────────────────────────────────────────────────────────────
void TrafficSystem::Render(Shader& shader, Camera& camera)
{
    if (!mCarModel) return;

    for (const auto& npc : mActiveNPCs) {
        CarState dummyCar;
        dummyCar.position = npc.position;
        dummyCar.yaw      = npc.yaw;
        dummyCar.pitch    = npc.pitch;
        dummyCar.roll     = npc.roll;
        dummyCar.speed    = npc.speed;

        glm::mat4 modelMatrix = BuildCarMatrix(dummyCar);

        bool headlights = true;
        bool braking    = (npc.speed < 1.0f);

        mCarModel->Draw(shader, camera, modelMatrix, npc.wheelSpin, npc.steering, headlights, braking, nullptr, npc.color);
    }
}
