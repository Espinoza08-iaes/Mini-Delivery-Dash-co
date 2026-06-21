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
    
    // Hierarchical structure: each pickup zone can have multiple delivery zones
    std::vector<glm::vec3> deliveryPositions;
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
    
    // HUD and UI access
    const DeliveryOrder& GetCurrentOrder() const { return lastDeliveredOrder; }
    const DeliveryOrder& GetWaitingOrder() const { return waitingOrder; }
    const std::vector<DeliveryOrder>& GetActiveOrders() const { return activeOrders; }
    
    bool HasActiveOrder() const { return !activeOrders.empty(); }
    bool HasWaitingOrder() const { return waitingOrder.state == OrderState::WAITING; }
    
    const DeliveryOrder* GetClosestActiveOrder(const glm::vec3& carPos) const;
    
    float GetTimeRemaining(const glm::vec3& carPos) const
    {
        const DeliveryOrder* order = GetClosestActiveOrder(carPos);
        if (order) return std::max(0.0f, order->timeLimit - order->elapsedTime);
        return 0.0f;
    }
    
    float GetFragileHealth(const glm::vec3& carPos) const
    {
        const DeliveryOrder* order = GetClosestActiveOrder(carPos);
        if (order && order->type == OrderType::FRAGILE) return order->fragileHealth;
        return 100.0f;
    }
    
    glm::vec3 GetObjectivePosition() const;
    float GetDistanceToObjective(const CarState& car) const;
    const std::vector<DeliveryZone>& GetDeliveryZones() const { return deliveryZones; }
    
    float GetWalletBalance() const { return walletBalance; }
    void AddToWallet(float amount) { walletBalance += amount; }
    float GetTotalLoss() const { return totalLoss; }
    void RejectOrder(const CarState& car);
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
    void CalculateDetailedRewards(const DeliveryOrder& order, float& base, float& bonus, float& penCol, float& penWat, float& penTime) const;
    
    // Track which pickup zone the current waiting order belongs to
    int currentOrderZoneIndex;
    
    // Getters for HUD access
    float GetInitialReward() const { return initialReward; }
    float GetTimeStar3() const { return timeStar3; }
    float GetTimeStar2() const { return timeStar2; }
    float GetTimeStar1() const { return timeStar1; }
    
    // 3D Text rendering for zone markers
    void RenderZoneText(Shader& shader, Camera& camera, const glm::vec3& position, const char* text, const glm::vec3& color);

    OrderState GetOrderState() const
    {
        return activeOrders.empty() ? OrderState::WAITING : OrderState::PICKED_UP;
    }
    
private:
    void LoadDeliveryZones();
    bool IsCarNearPoint(const glm::vec3& carPos, const glm::vec3& point, float threshold) const;
    bool IsCarSpeedLow(float speed, float maxSpeedKmh) const;
    Mesh CreateZoneMarkerMesh();
    void GenerateNewOrder();
    
    std::vector<DeliveryZone> deliveryZones;
    std::vector<DeliveryOrder> activeOrders;
    DeliveryOrder waitingOrder;
    DeliveryOrder lastDeliveredOrder;
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
    float totalLoss;
    float initialReward;
    Shader zoneShader;
    
    // Star system private variables
    float timeStar3, timeStar2, timeStar1, timeStar0;
    
    // Water respawn collision ignore flag
    bool mIgnoreNextCollision = false;
};

#endif
