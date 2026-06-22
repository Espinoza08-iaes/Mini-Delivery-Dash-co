#include "TrafficSystem.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

// ─── Tuning constants ───────────────────────────────────────────────────────
constexpr float MAX_SPAWN_DIST   = 90.0f;
constexpr float MIN_SPAWN_DIST   = 25.0f;
constexpr float DESPAWN_DIST     = 130.0f;
constexpr size_t MAX_NPCS        = 35;

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

void TrafficSystem::InitializePathfinder(const City& city) {
    mPathfinder.Initialize(city.GetPhysics().GetRoadTriangles(), city.GetWorldMinBounds(), city.GetWorldMaxBounds());
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

    // Strategy: Use the NavMesh to find a valid spawn point directly on the street.
    // This guarantees the NPC starts on the actual road, not on a sidewalk.
    glm::vec3 candidate;
    bool foundSpawn = false;
    
    for (int attempt = 0; attempt < 30; ++attempt) {
        glm::vec3 navPoint;
        if (mPathfinder.IsInitialized()) {
            navPoint = mPathfinder.GetRandomNavPoint();
        } else {
            // Fallback if no NavMesh
            float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
            float dist  = MIN_SPAWN_DIST + (float)(rand() % (int)(MAX_SPAWN_DIST - MIN_SPAWN_DIST));
            navPoint = playerPos + glm::vec3(cos(angle) * dist, 0.0f, sin(angle) * dist);
        }
        
        // Check distance to player (not too close, not too far)
        float distToPlayer = glm::distance(glm::vec2(navPoint.x, navPoint.z), glm::vec2(playerPos.x, playerPos.z));
        if (distToPlayer < MIN_SPAWN_DIST || distToPlayer > MAX_SPAWN_DIST) continue;
        
        // Get proper ground height
        game::GroundSample sample;
        bool onRoad = city.GetGroundSample(navPoint, navPoint.y + 10.0f, sample, 20.0f, 5.0f);
        if (!onRoad || !sample.found) continue;
        
        navPoint.y = sample.height + kGroundClearance;
        
        // Make sure it's not inside a building
        if (city.CheckCollision(navPoint, NPC_COLLISION_RADIUS)) continue;
        
        candidate = navPoint;
        foundSpawn = true;
        break;
    }
    
    if (!foundSpawn) return;
    
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
    if (bestRoad < 1.0f) return; // No good direction
    
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
    }
    
    npc.position = candidate;
    npc.yaw = bestYaw;
    npc.pitch = 0.0f;
    npc.roll = 0.0f;
    npc.cruiseSpeed = NPC_MAX_SPEED * (0.6f + (rand() % 30) / 100.0f);
    npc.desiredSpeed = npc.cruiseSpeed;
    npc.speed = npc.cruiseSpeed * 0.5f; // start moving
    npc.verticalSpeed = 0.0f;
    npc.steering = 0.0f;
    npc.wheelSpin = 0.0f;
    npc.steerInput = 0.0f;
    npc.roadCheckTimer = 0.0f;
    npc.stuckTimer = 0.0f;
    npc.stuckCount = 0;
    npc.recovering = false;
    npc.recoverTimer = 0.0f;
    npc.recoverTurnDir = (rand() % 2 == 0) ? 1.0f : -1.0f;
    
    npc.currentPathIndex = 0;
    npc.failedPathCount = 0;
    if (mPathfinder.IsInitialized()) {
        glm::vec3 dest = mPathfinder.GetRandomNavPoint();
        npc.currentPath = mPathfinder.FindPath(candidate, dest);
    }
    
    mActiveNPCs.push_back(npc);
    std::cout << "[TRAFFIC] Spawned NPC at (" << candidate.x << ", " << candidate.y << ", " << candidate.z << ") road=" << bestRoad << "m" << std::endl;
    return; // one per call
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
        
        // 3. Off-road correction: if NPC drifted off the NavMesh, snap it back
        if (mPathfinder.IsInitialized() && !npc.recovering) {
            if (!mPathfinder.IsOnNavMesh(npc.position, 4.0f)) {
                // NPC has wandered off the road - snap back to nearest NavMesh point
                glm::vec3 corrected = mPathfinder.FindNearestPointOnNavMesh(npc.position);
                float snapDist = glm::distance(glm::vec2(npc.position.x, npc.position.z), glm::vec2(corrected.x, corrected.z));
                if (snapDist < 15.0f && snapDist > 0.1f) {
                    // Smoothly pull back toward the road
                    npc.position.x = glm::mix(npc.position.x, corrected.x, 0.15f);
                    npc.position.z = glm::mix(npc.position.z, corrected.z, 0.15f);
                    // Also slow down to avoid overshooting again
                    npc.desiredSpeed = std::min(npc.desiredSpeed, npc.cruiseSpeed * 0.3f);
                } else if (snapDist >= 15.0f) {
                    // Way too far off - mark for despawn by moving far away
                    npc.position.y = 10000.0f;
                }
                
                // Force repath after correction
                if (npc.currentPath.empty() || npc.currentPathIndex >= (int)npc.currentPath.size()) {
                    glm::vec3 dest = mPathfinder.GetRandomNavPoint();
                    npc.currentPath = mPathfinder.FindPath(npc.position, dest);
                    npc.currentPathIndex = 0;
                }
            }
        }
    }

    // --- Hard Collision Resolution ---
    // 1. NPC vs Player
    for (auto& npc : mActiveNPCs) {
        glm::vec2 pPos(playerCar.position.x, playerCar.position.z);
        glm::vec2 nPos(npc.position.x, npc.position.z);
        float dist = glm::distance(pPos, nPos);
        float hitRadius = 2.8f; // Car collision boundary
        if (dist < hitRadius && dist > 0.01f) {
            glm::vec2 pushDir = glm::normalize(pPos - nPos);
            float overlap = hitRadius - dist;
            
            // Push player and NPC away from each other
            pPos += pushDir * (overlap * 0.5f);
            nPos -= pushDir * (overlap * 0.5f);
            
            // Keep Y coordinates
            playerCar.position.x = pPos.x;
            playerCar.position.z = pPos.y;
            npc.position.x = nPos.x;
            npc.position.z = nPos.y;
            
            // Crash speed penalty
            playerCar.speed *= 0.8f;
            npc.speed *= 0.5f;
        }
    }

    // 2. NPC vs NPC
    for (size_t i = 0; i < mActiveNPCs.size(); ++i) {
        for (size_t j = i + 1; j < mActiveNPCs.size(); ++j) {
            glm::vec2 n1(mActiveNPCs[i].position.x, mActiveNPCs[i].position.z);
            glm::vec2 n2(mActiveNPCs[j].position.x, mActiveNPCs[j].position.z);
            float dist = glm::distance(n1, n2);
            float hitRadius = 2.5f;
            if (dist < hitRadius && dist > 0.01f) {
                glm::vec2 pushDir = glm::normalize(n1 - n2);
                float overlap = hitRadius - dist;
                
                n1 += pushDir * (overlap * 0.5f);
                n2 -= pushDir * (overlap * 0.5f);
                
                mActiveNPCs[i].position.x = n1.x;
                mActiveNPCs[i].position.z = n1.y;
                mActiveNPCs[j].position.x = n2.x;
                mActiveNPCs[j].position.z = n2.y;
                
                mActiveNPCs[i].speed *= 0.8f;
                mActiveNPCs[j].speed *= 0.8f;
            }
        }
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

    // Recovery mode overrides standard AI completely
    if (npc.recovering) {
        npc.recoverTimer -= dt;
        npc.desiredSpeed = -NPC_MAX_SPEED * 0.4f; // Reverse slowly
        npc.steerInput = npc.recoverTurnDir;      // Turn wheel hard while reversing
        if (npc.recoverTimer <= 0.0f) {
            npc.recovering = false;
            npc.stuckTimer = 0.0f;
            // Force repath so they don't drive right back into the wall
            if (mPathfinder.IsInitialized()) {
                glm::vec3 dest = mPathfinder.GetRandomNavPoint();
                npc.currentPath = mPathfinder.FindPath(npc.position, dest);
                npc.currentPathIndex = 0;
            }
        }
        return; // Skip normal steering
    }

    // Follow path
    if (mPathfinder.IsInitialized()) {
        if (npc.currentPath.empty() || npc.currentPathIndex >= (int)npc.currentPath.size()) {
            glm::vec3 dest = mPathfinder.GetRandomNavPoint();
            npc.currentPath = mPathfinder.FindPath(npc.position, dest);
            npc.currentPathIndex = 0;
            npc.cruiseSpeed = NPC_MAX_SPEED * (0.6f + (rand() % 30) / 100.0f);
            npc.desiredSpeed = npc.cruiseSpeed;
            
            if (npc.currentPath.empty()) {
                npc.failedPathCount++;
            } else {
                npc.failedPathCount = 0;
            }
        }
        
        if (!npc.currentPath.empty() && npc.currentPathIndex < (int)npc.currentPath.size()) {
            // === NavMesh Pure Pursuit & Lane Keeping ===
            
            // 1. Advance waypoint if close
            while (npc.currentPathIndex < (int)npc.currentPath.size()) {
                float d = glm::distance(glm::vec2(npc.position.x, npc.position.z), 
                                        glm::vec2(npc.currentPath[npc.currentPathIndex].x, npc.currentPath[npc.currentPathIndex].z));
                if (d < 4.0f) {
                    npc.currentPathIndex++;
                } else {
                    break;
                }
            }
            
            if (npc.currentPathIndex < (int)npc.currentPath.size()) {
                // 2. Pure Pursuit: Find a lookahead point
                float lookahead = std::max(6.0f, npc.speed * 1.2f);
                glm::vec3 target = npc.currentPath[npc.currentPathIndex];
                
                for (int i = npc.currentPathIndex; i < (int)npc.currentPath.size(); ++i) {
                    float d = glm::distance(glm::vec2(npc.position.x, npc.position.z), 
                                            glm::vec2(npc.currentPath[i].x, npc.currentPath[i].z));
                    if (d >= lookahead) {
                        target = npc.currentPath[i];
                        break;
                    }
                }
                
                // 3. Lane Keeping: Offset the target to the right side of the path segment
                glm::vec3 pathDir;
                if (npc.currentPathIndex < (int)npc.currentPath.size() - 1) {
                    pathDir = glm::normalize(npc.currentPath[npc.currentPathIndex + 1] - npc.currentPath[npc.currentPathIndex]);
                } else if (npc.currentPathIndex > 0) {
                    pathDir = glm::normalize(npc.currentPath[npc.currentPathIndex] - npc.currentPath[npc.currentPathIndex - 1]);
                } else {
                    pathDir = forward;
                }
                pathDir.y = 0.0f;
                glm::vec3 pathRight = glm::vec3(pathDir.z, 0.0f, -pathDir.x);
                
                // Offset 2.5 meters to the right, but snap back to NavMesh to avoid hitting walls
                glm::vec3 idealLaneTarget = target - pathRight * 2.5f;
                target = mPathfinder.FindNearestPointOnNavMesh(idealLaneTarget);
                
                glm::vec3 toTarget = target - npc.position;
                toTarget.y = 0.0f;
                if (glm::length(toTarget) > 0.01f) {
                    glm::vec3 dirToTarget = glm::normalize(toTarget);
                    
                    // 4. Steer smoothly towards the offset target
                    float cross = forward.x * dirToTarget.z - forward.z * dirToTarget.x;
                    npc.steerInput = glm::clamp(-cross * 3.0f, -1.0f, 1.0f);
                    
                    // 5. Look-ahead Corner Braking
                    float targetSpeed = npc.cruiseSpeed;
                    int checkIdx = std::min(npc.currentPathIndex + 2, (int)npc.currentPath.size() - 1);
                    if (checkIdx > npc.currentPathIndex) {
                        glm::vec3 futureDir = glm::normalize(npc.currentPath[checkIdx] - npc.currentPath[npc.currentPathIndex]);
                        futureDir.y = 0.0f;
                        float turnDot = glm::dot(forward, futureDir);
                        if (turnDot < 0.6f) {
                            targetSpeed = npc.cruiseSpeed * 0.3f; // Sharp turn, brake hard
                        } else if (turnDot < 0.85f) {
                            targetSpeed = npc.cruiseSpeed * 0.6f; // Mild turn, slow down
                        }
                    }
                    
                    // Also slow down if we are severely misaligned right now
                    if (glm::dot(forward, dirToTarget) < 0.7f) {
                        targetSpeed = std::min(targetSpeed, npc.cruiseSpeed * 0.5f);
                    }
                    
                    npc.desiredSpeed = targetSpeed;
                }
            }
        } else {
            // === Sensor fallback mode (FindPath failed) ===
            // Keep driving using road sensors instead of stopping dead.
            npc.desiredSpeed = npc.cruiseSpeed * 0.5f;
            // Steer toward the most open road direction
            float roadLeft  = SenseRoad(city, npc.position, glm::normalize(forward - right * 0.5f), SENSE_DIST);
            float roadRight = SenseRoad(city, npc.position, glm::normalize(forward + right * 0.5f), SENSE_DIST);
            float bias = (roadLeft - roadRight) / SENSE_DIST;
            npc.steerInput = glm::clamp(bias * 2.0f, -1.0f, 1.0f);
            
            // If we've failed to find a path too many times, mark for despawn
            if (npc.failedPathCount > 5) {
                std::cout << "[TRAFFIC] NPC despawned due to failed paths. Marking obstacle at " << npc.position.x << "," << npc.position.z << std::endl;
                const_cast<Pathfinder&>(mPathfinder).MarkAreaAsObstacle(npc.position, 6.0f);
                npc.position.y = 10000.0f; // Will be despawned by distance check
            }
        }
    }

    // Collision avoidance overlay
    npc.roadCheckTimer += dt;
    if (npc.roadCheckTimer >= SENSE_INTERVAL)
    {
        npc.roadCheckTimer = 0.0f;
        
        float roadAhead = SenseRoad(city, npc.position, forward, SENSE_DIST);
        float roadLeft  = SenseRoad(city, npc.position, glm::normalize(forward - right * 0.5f), SENSE_DIST);
        float roadRight = SenseRoad(city, npc.position, glm::normalize(forward + right * 0.5f), SENSE_DIST);
        
        if (roadAhead < 3.0f) {
            if (roadLeft < 1.0f && roadRight < 1.0f) {
                // Completely blocked - initiate recovery (reverse)
                npc.recovering = true;
                npc.recoverTimer = 1.5f + ((rand() % 10) / 10.0f); 
                npc.recoverTurnDir = (rand() % 2 == 0) ? 1.0f : -1.0f;
            } else if (roadLeft > roadRight) {
                npc.steerInput = 1.0f;
            } else {
                npc.steerInput = -1.0f;
            }
            npc.desiredSpeed = std::min(npc.desiredSpeed, NPC_MAX_SPEED * 0.2f);
        } else if (roadAhead < SENSE_DIST * 0.6f) {
            float bias = (roadLeft - roadRight) / SENSE_DIST;
            npc.steerInput = glm::clamp(npc.steerInput + bias * 2.0f, -1.0f, 1.0f);
            npc.desiredSpeed = std::min(npc.desiredSpeed, NPC_MAX_SPEED * 0.4f);
        }
    }

    // General Stuck Detection (speed near 0 for too long)
    if (std::abs(npc.speed) < 0.5f && npc.desiredSpeed > 0.3f) {
        npc.stuckTimer += dt;
        if (npc.stuckTimer > 2.0f) {
            npc.stuckCount++;
            if (npc.stuckCount > 3) {
                // Hopelessly stuck for multiple recovery attempts
                std::cout << "[TRAFFIC] Civilian hopelessly stuck. Marking obstacle at " << npc.position.x << "," << npc.position.z << std::endl;
                const_cast<Pathfinder&>(mPathfinder).MarkAreaAsObstacle(npc.position, 8.0f);
                npc.position.y = 10000.0f; // despawn
            } else {
                npc.recovering = true;
                npc.recoverTimer = 1.5f;
                npc.recoverTurnDir = (rand() % 2 == 0) ? 1.0f : -1.0f;
                npc.stuckTimer = 0.0f;
            }
        }
    } else {
        npc.stuckTimer = 0.0f;
        if (npc.speed > 2.0f) npc.stuckCount = 0; // Reset if driving fine
    }
    
    // Brake for player car if ahead
    float distToPlayer = glm::distance(npc.position, playerCar.position);
    if (distToPlayer < 12.0f) {
        glm::vec3 dirToPlayer = glm::normalize(playerCar.position - npc.position);
        if (glm::dot(forward, dirToPlayer) > 0.7f) {
            // Don't go below 0.4 so the stuck detector can still trigger and they reverse
            npc.desiredSpeed = std::max(0.4f, npc.desiredSpeed * 0.1f); 
        }
    }
    
    // Brake for other NPCs ahead & Avoid Traffic Jams
    for (const auto& other : mActiveNPCs) {
        if (&other == &npc) continue;
        float d = glm::distance(npc.position, other.position);
        if (d < 12.0f && d > 0.5f) {
            glm::vec3 dirToOther = glm::normalize(other.position - npc.position);
            if (glm::dot(forward, dirToOther) > 0.7f) {
                glm::vec3 otherForward = glm::vec3(sin(other.yaw), 0.0f, cos(other.yaw));
                
                // If facing head-on, don't brake, just swerve!
                if (glm::dot(forward, otherForward) < -0.5f) {
                    float cross = forward.x * dirToOther.z - forward.z * dirToOther.x;
                    if (cross > 0.0f) npc.steerInput = glm::clamp(npc.steerInput + 1.0f, -1.0f, 1.0f);
                    else npc.steerInput = glm::clamp(npc.steerInput - 1.0f, -1.0f, 1.0f);
                } else {
                    // Traffic jam (same direction) - brake, but keep a minimum desiredSpeed 
                    // so the stuck detector will eventually trigger and make them reverse/repath
                    npc.desiredSpeed = std::max(0.4f, std::min(npc.desiredSpeed, other.speed * 0.8f));
                    
                    // Nudge slightly to the side to try to go around
                    float cross = forward.x * dirToOther.z - forward.z * dirToOther.x;
                    npc.steerInput += (cross > 0.0f) ? 0.3f : -0.3f;
                    npc.steerInput = glm::clamp(npc.steerInput, -1.0f, 1.0f);
                }
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
        // Recovery logic for police cars
        if (npc.recovering) {
            npc.recoverTimer -= dt;
            npc.desiredSpeed = -NPC_MAX_SPEED * 0.5f; // Police reverses slightly faster
            npc.steerInput = npc.recoverTurnDir;
            
            // Siren still flashes while reversing
            npc.stateTimer += dt;
            npc.color = (fmod(npc.stateTimer, 0.4f) < 0.2f) ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
            
            if (npc.recoverTimer <= 0.0f) {
                npc.recovering = false;
                npc.stuckTimer = 0.0f;
                // Force path re-calc immediately
                if (mPathfinder.IsInitialized()) {
                    npc.currentPath = mPathfinder.FindPath(npc.position, playerCar.position);
                    npc.currentPathIndex = 0;
                }
            }
            return; // Skip normal steering
        }

        // Siren flash
        npc.stateTimer += dt;
        npc.color = (fmod(npc.stateTimer, 0.4f) < 0.2f)
            ? glm::vec3(1.0f, 0.0f, 0.0f)
            : glm::vec3(0.0f, 0.0f, 1.0f);

        glm::vec3 forward = glm::vec3(sin(npc.yaw), 0.0f, cos(npc.yaw));
        glm::vec3 toPlayer = playerCar.position - npc.position;
        toPlayer.y = 0.0f;
        float dist = glm::length(toPlayer);

        if (dist > 15.0f && mPathfinder.IsInitialized()) {
            // Pathfind to player if far
            if (npc.currentPath.empty() || npc.roadCheckTimer > 1.0f) {
                npc.currentPath = mPathfinder.FindPath(npc.position, playerCar.position);
                npc.currentPathIndex = 0;
                npc.roadCheckTimer = 0.0f; // Reuse timer for repath interval
            }
            npc.roadCheckTimer += dt;

            if (!npc.currentPath.empty() && npc.currentPathIndex < (int)npc.currentPath.size()) {
                glm::vec3 target = npc.currentPath[npc.currentPathIndex];
                float distToTarget = glm::distance(glm::vec2(npc.position.x, npc.position.z), glm::vec2(target.x, target.z));
                if (distToTarget < 5.0f) npc.currentPathIndex++;
                
                if (npc.currentPathIndex < (int)npc.currentPath.size()) {
                    target = npc.currentPath[npc.currentPathIndex];
                    glm::vec3 toTarget = target - npc.position;
                    toTarget.y = 0.0f;
                    if (glm::length(toTarget) > 0.01f) {
                        glm::vec3 dirToTarget = glm::normalize(toTarget);
                        float cross = forward.x * dirToTarget.z - forward.z * dirToTarget.x;
                        npc.steerInput = glm::clamp(-cross * 4.0f, -1.0f, 1.0f);
                        npc.desiredSpeed = NPC_MAX_SPEED * POLICE_SPEED_MULT + wantedLevel * 1.0f;
                    }
                }
            } else {
                // Path failed - drive toward player directly using sensors
                if (dist > 0.1f) {
                    glm::vec3 dirToPlayer = glm::normalize(toPlayer);
                    float cross = forward.x * dirToPlayer.z - forward.z * dirToPlayer.x;
                    npc.steerInput = glm::clamp(-cross * 3.0f, -1.0f, 1.0f);
                    npc.desiredSpeed = NPC_MAX_SPEED * POLICE_SPEED_MULT * 0.5f;
                }
            }
        } else if (dist > 2.0f) {
            // Direct steering if close
            glm::vec3 dirToPlayer = glm::normalize(toPlayer);
            float cross = forward.x * dirToPlayer.z - forward.z * dirToPlayer.x;
            npc.steerInput = glm::clamp(-cross * 3.0f, -1.0f, 1.0f);
            npc.desiredSpeed = NPC_MAX_SPEED * POLICE_SPEED_MULT + wantedLevel * 1.0f;
        } else {
            npc.steerInput = 0.0f;
            npc.desiredSpeed = 0.0f;
        }
        
        // Still respect walls — sense ahead and override if wall
        float roadAhead = SenseRoad(city, npc.position, forward, 3.0f);
        glm::vec3 right = glm::vec3(cos(npc.yaw), 0.0f, -sin(npc.yaw));
        float roadLeft  = SenseRoad(city, npc.position, glm::normalize(forward - right * 0.5f), 3.0f);
        float roadRight = SenseRoad(city, npc.position, glm::normalize(forward + right * 0.5f), 3.0f);
        
        if (roadAhead < 1.5f) {
            if (roadLeft < 1.0f && roadRight < 1.0f) {
                npc.recovering = true;
                npc.recoverTimer = 1.0f;
                npc.recoverTurnDir = (rand() % 2 == 0) ? 1.0f : -1.0f;
            } else {
                npc.steerInput = (roadLeft > roadRight) ? 1.0f : -1.0f;
                npc.desiredSpeed = std::min(npc.desiredSpeed, NPC_MAX_SPEED * 0.4f);
            }
        }
        
        // General Stuck Detection for police
        if (std::abs(npc.speed) < 0.5f && npc.desiredSpeed > 0.3f) {
            npc.stuckTimer += dt;
            if (npc.stuckTimer > 1.0f) { // Police is less patient than civilians
                npc.stuckCount++;
                if (npc.stuckCount > 4) {
                    std::cout << "[TRAFFIC] Police hopelessly stuck. Marking obstacle at " << npc.position.x << "," << npc.position.z << std::endl;
                    const_cast<Pathfinder&>(mPathfinder).MarkAreaAsObstacle(npc.position, 8.0f);
                    npc.position.y = 10000.0f; // despawn
                } else {
                    npc.recovering = true;
                    npc.recoverTimer = 1.2f;
                    npc.recoverTurnDir = (rand() % 2 == 0) ? 1.0f : -1.0f;
                    npc.stuckTimer = 0.0f;
                }
            }
        } else {
            npc.stuckTimer = 0.0f;
            if (npc.speed > 2.0f) npc.stuckCount = 0;
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
