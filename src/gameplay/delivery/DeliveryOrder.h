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
    float timeLimit;
    float fragileHealth;
    
    // Constructor
    DeliveryOrder()
        : difficulty(OrderDifficulty::EASY)
        , type(OrderType::STANDARD)
        , state(OrderState::WAITING)
        , originPosition(glm::vec3(0.0f))
        , destinationPosition(glm::vec3(0.0f))
        , reward(0.0f)
        , timeLimit(0.0f)
        , fragileHealth(100.0f)
    {}
};

#endif
