#ifndef TRAFFIC_SYSTEM_H
#define TRAFFIC_SYSTEM_H

#include <vector>
#include <glm/glm.hpp>
#include <string>
#include "../../engine/rendering/Model.h"
#include "../../engine/graphics/Shader.h"
#include "../../engine/graphics/Camera.h"
#include "../vehicle/CarController.h"
#include "../city/City.h"

enum class NPCType {
    CIVILIAN,
    POLICE
};

enum class PoliceState {
    PATROL,
    CHASE,
    RAM
};

struct NPCCar {
    NPCType type;
    
    // Full physics state (mirrors CarState)
    glm::vec3 position;
    float yaw;
    float pitch;
    float roll;
    float speed;       // current forward speed (m/s)
    float verticalSpeed; // for gravity
    float steering;    // current steering angle (rad)
    float wheelSpin;
    
    // AI steering
    float desiredSpeed;
    float steerInput;  // -1..+1 AI steering input
    
    glm::vec3 color;
    
    // For Police AI
    PoliceState policeState;
    float stateTimer;
    
    // Road-following AI
    float roadCheckTimer;
    
    // Stuck recovery
    float stuckTimer;      // how long speed has been ~0
    bool recovering;       // currently doing a reverse+turn maneuver
    float recoverTimer;    // time left in recovery
    float recoverTurnDir;  // -1 or +1
};

class TrafficSystem {
public:
    TrafficSystem(Model* sharedCarModel);
    
    void Update(float dt, CarState& playerCar, const City& city);
    void Render(Shader& shader, Camera& camera);
    
    int GetWantedLevel() const { return wantedLevel; }
    void AddWantedLevel(int amount);
    void ResetWantedLevel();

private:
    void SpawnNPCNearPlayer(const glm::vec3& playerPos, const City& city);
    void DespawnDistantNPCs(const glm::vec3& playerPos);
    
    void UpdateNPCPhysics(NPCCar& npc, float dt, const City& city);
    void SteerCivilian(NPCCar& npc, float dt, const CarState& playerCar, const City& city);
    void SteerPolice(NPCCar& npc, float dt, const CarState& playerCar, const City& city);
    
    // Road-sensing: returns how much road is ahead in the given direction
    float SenseRoad(const City& city, const glm::vec3& pos, const glm::vec3& dir, float maxDist) const;

    Model* mCarModel;
    std::vector<NPCCar> mActiveNPCs;
    
    int wantedLevel;
    float spawnTimer;
};

#endif
