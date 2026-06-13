#ifndef DELIVERY_SYSTEM_H
#define DELIVERY_SYSTEM_H

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "DeliveryOrder.h"
#include "Paquete.h"
#include "../vehicle/CarController.h"
#include "../../engine/graphics/Shader.h"
#include "../../engine/graphics/Camera.h"

struct DeliveryZone
{
    glm::vec3 position;
    std::string name;
};

class DeliverySystem
{
public:
    DeliverySystem();
    
    void Initialize();
    void Update(float deltaTime, const CarState& car, bool eKeyPressed);
    void Render(Shader& shader, Camera& camera);
    
    bool TryPickupOrder(const CarState& car);
    bool TryDeliverOrder(const CarState& car);
    
    const DeliveryOrder& GetCurrentOrder() const { return currentOrder; }
    bool HasActiveOrder() const { return currentOrder.state == OrderState::PICKED_UP; }
    bool HasWaitingOrder() const { return currentOrder.state == OrderState::WAITING; }
    
    glm::vec3 GetObjectivePosition() const;
    float GetDistanceToObjective(const CarState& car) const;
    
    float GetWalletBalance() const { return walletBalance; }
    void AddToWallet(float amount) { walletBalance += amount; }
    float GetElapsedTime() const { return orderElapsedTime; }
    float GetEstimatedReward() const { return CalculateEstimatedReward(); }
    float GetCollisionLoss() const { return CalculateCollisionLoss(); }
    float GetTotalLoss() const { return totalLoss; }
    int GetCollisionCount() const { return collisionCount; }
    void RejectOrder();
    void OnWaterRespawn();
    
    // Frozen elapsed time for success screen
    float finalElapsedTime;
    
    // Star system and breakdown variables
    int currentStars;
    float currentTargetTime;
    float finalBonusAmount;
    float finalLossCollision;
    float finalLossWater;
    float finalLossTime;
    void CalculateDetailedRewards(float& base, float& bonus, float& penCol, float& penWat, float& penTime) const;
    
    // Getters for HUD access
    float GetInitialReward() const { return initialReward; }
    float GetTimeStar3() const { return timeStar3; }
    float GetTimeStar2() const { return timeStar2; }
    float GetTimeStar1() const { return timeStar1; }
    
    // 3D Text rendering for zone markers
    void RenderZoneText(Shader& shader, Camera& camera, const glm::vec3& position, const char* text, const glm::vec3& color);
    
private:
    void LoadDeliveryZones();
    DeliveryZone GetRandomDeliveryZone() const;
    bool IsCarNearPoint(const glm::vec3& carPos, const glm::vec3& point, float threshold) const;
    bool IsCarSpeedLow(float speed, float maxSpeedKmh) const;
    Mesh CreateZoneMarkerMesh();
    float CalculateReward() const;
    void GenerateNewOrder();
    float CalculateEstimatedReward() const;
    float CalculateCollisionLoss() const;
    
    std::vector<DeliveryZone> deliveryZones;
    DeliveryOrder currentOrder;
    Paquete package;
    
    float pickupDistanceThreshold;
    float deliveryDistanceThreshold;
    float maxPickupSpeedKmh;
    
    // Animation
    float packageAnimationTime;
    float packageAnimationDuration;
    glm::vec3 packageAnimationStart;
    glm::vec3 packageAnimationEnd;
    bool isAnimatingPackage;
    
    // Economy
    float walletBalance;
    float orderElapsedTime;
    int collisionCount;
    float totalLoss;
    float initialReward;
    Shader zoneShader;
    
    // Star system private variables
    int waterCount;
    float timeStar3, timeStar2, timeStar1, timeStar0;
    
    // Water respawn collision ignore flag
    bool mIgnoreNextCollision = false;
};

#endif
