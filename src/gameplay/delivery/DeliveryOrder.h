#ifndef DELIVERY_ORDER_H
#define DELIVERY_ORDER_H

#include <glm/glm.hpp>

// Order difficulty levels
enum class OrderDifficulty
{
    EASY,
    MEDIUM,
    HARD,
    SPECIAL
};

// Order types
enum class OrderType
{
    STANDARD,
    FRAGILE,
    EXPRESS
};

// Order states
enum class OrderState
{
    WAITING,
    PICKED_UP,
    DROPPING_OFF,
    DELIVERED,
    FAILED
};

// Delivery order structure
struct DeliveryOrder
{
    OrderDifficulty difficulty;
    OrderType type;
    OrderState state;
    
    glm::vec3 originPosition;
    glm::vec3 destinationPosition;
    
    float reward;
    float baseDisplayReward;
    float timeLimit;
    float fragileHealth;
    float timeGold;
    float timeSilver;
    float timeBronze;
    int starsEarned;

    // Tracking variables per order
    float elapsedTime;
    int collisionCount;
    int waterCount;
    
    // Constructor
    DeliveryOrder()
        : difficulty(OrderDifficulty::EASY)
        , type(OrderType::STANDARD)
        , state(OrderState::WAITING)
        , originPosition(glm::vec3(999999.0f))
        , destinationPosition(glm::vec3(999999.0f))
        , reward(0.0f)
        , baseDisplayReward(0.0f)
        , timeLimit(0.0f)
        , fragileHealth(100.0f)
        , timeGold(0.0f)
        , timeSilver(0.0f)
        , timeBronze(0.0f)
        , starsEarned(0)
        , elapsedTime(0.0f)
        , collisionCount(0)
        , waterCount(0)
    {}
};

#endif
