#include "DeliverySystem.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <GLFW/glfw3.h>
#include "../../engine/graphics/Mesh.h"
#include "../shop/ShopManager.h"

#define STB_EASY_FONT_IMPLEMENTATION
#include "../../../third_party/stb/stb_easy_font.h"

DeliverySystem::DeliverySystem()
    : pickupDistanceThreshold(3.0f)
    , deliveryDistanceThreshold(3.0f)
    , maxPickupSpeedKmh(5.0f)
    , packageAnimationTime(0.0f)
    , packageAnimationDuration(0.5f)
    , isAnimatingPackage(false)
    , walletBalance(0.0f)
    , orderElapsedTime(0.0f)
    , collisionCount(0)
    , totalLoss(0.0f)
    , initialReward(0.0f)
    , finalElapsedTime(0.0f)
    , zoneShader("res/shaders/zone.vert", "res/shaders/zone.frag")
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

void DeliverySystem::Initialize()
{
    LoadDeliveryZones();
    
    // Use the real generator for the first mission
    if (!deliveryZones.empty())
    {
        GenerateNewOrder();
    }
}

void DeliverySystem::Update(float deltaTime, const CarState& car, bool eKeyPressed)
{
    // Update package animation
    if (isAnimatingPackage)
    {
        packageAnimationTime += deltaTime;
        if (packageAnimationTime >= packageAnimationDuration)
        {
            packageAnimationTime = packageAnimationDuration;
            isAnimatingPackage = false;
        }
        
        float t = packageAnimationTime / packageAnimationDuration;
        // Smooth step interpolation
        float smoothT = t * t * (3.0f - 2.0f * t);
        glm::vec3 currentPos = glm::mix(packageAnimationStart, packageAnimationEnd, smoothT);
        package.SetPosition(currentPos);
    }
    
    // Update order state based on car position and speed
    if (currentOrder.state == OrderState::WAITING)
    {
        if (TryPickupOrder(car) && eKeyPressed)
        {
            // Start pickup animation
            packageAnimationStart = currentOrder.originPosition;
            packageAnimationEnd = car.position + glm::vec3(0.0f, 1.0f, 0.0f);
            packageAnimationTime = 0.0f;
            isAnimatingPackage = true;
            
            currentOrder.state = OrderState::PICKED_UP;
            orderElapsedTime = 0.0f;
            collisionCount = 0;
        }
    }
    else if (currentOrder.state == OrderState::PICKED_UP)
    {
        // FIX BUG 5: The internal clock ONLY advances if not yet delivered
        orderElapsedTime += deltaTime;

        // FIX BUG 4: Update star tracking for HUD - respect time thresholds
        if (orderElapsedTime <= timeStar3) { currentStars = 3; currentTargetTime = timeStar3; }
        else if (orderElapsedTime <= timeStar2) { currentStars = 2; currentTargetTime = timeStar2; }
        else if (orderElapsedTime <= timeStar1) { currentStars = 1; currentTargetTime = timeStar1; }
        else { currentStars = 0; currentTargetTime = timeStar0; }
        
        // FIX BUG 4: Fragile health ONLY reduces from physical collisions, NEVER from time
        static bool wasColliding = false;
        float speedKmh = std::abs(car.speed) * 3.6f;
        static float lastSpeed = speedKmh;
        float speedChange = std::abs(lastSpeed - speedKmh);
        
        if (speedChange > 15.0f && lastSpeed > 15.0f) // Significant deceleration at speed
        {
            if (mIgnoreNextCollision)
            {
                // Was a water respawn, not real collision
                mIgnoreNextCollision = false;
            }
            else
            {
                collisionCount++;
                if (currentOrder.type == OrderType::FRAGILE)
                {
                    currentOrder.fragileHealth -= 15.0f;
                    if (currentOrder.fragileHealth <= 0.0f)
                    {
                        currentOrder.fragileHealth = 0.0f;
                        currentOrder.state = OrderState::FAILED;
                        std::cout << "[DELIVERY] Paquete destruido por choque!" << std::endl;
                        GenerateNewOrder();
                    }
                }
            }
        }
        lastSpeed = speedKmh;
        
        // Sync Health with Economy (for non-fragile packages)
        if (currentOrder.type != OrderType::FRAGILE)
        {
            float b, bon, pC, pW, pT;
            CalculateDetailedRewards(b, bon, pC, pW, pT);
            float totalPenalties = pC + pW + pT;
            
            // Health drops exactly in the same percentage as money lost
            currentOrder.fragileHealth = 100.0f * (1.0f - (totalPenalties / b));
            if (currentOrder.fragileHealth < 0.0f) currentOrder.fragileHealth = 0.0f;
            
            if (currentOrder.fragileHealth <= 0.0f)
            {
                currentOrder.state = OrderState::FAILED;
                std::cout << "[DELIVERY] Paquete destruido!" << std::endl;
                GenerateNewOrder();
            }
        }
        
        if (TryDeliverOrder(car) && eKeyPressed)
        {
            float base, bonus, penCol, penWat, penTime;
            CalculateDetailedRewards(base, bonus, penCol, penWat, penTime);
            
            // Save breakdown for final screen
            finalBonusAmount = bonus;
            finalLossCollision = penCol;
            finalLossWater = penWat;
            finalLossTime = penTime;
            finalElapsedTime = orderElapsedTime; // FIX BUG 5: Save frozen elapsed time
            
            // Apply PayPerDelivery upgrade multiplier from shop
            ShopManager* shop = ShopManager::GetInstance();
            float payMultiplier = shop->GetUpgradeMultiplier(UpgradeType::PayPerDelivery);
            
            float finalReward = (base + bonus - penCol - penWat - penTime) * payMultiplier;
            totalLoss = initialReward - finalReward;
            AddToWallet(finalReward);
            
            // Add money to shop system and save progress
            shop->AddMoney(static_cast<int>(finalReward));
            shop->SaveShopData();
            
            // Save final reward to display on screen
            currentOrder.reward = finalReward;
            currentOrder.state = OrderState::DELIVERED;
            orderElapsedTime = 0.0f; // FIX 2: Reset to use as timer for screen
            
            std::cout << "[DELIVERY] Order completed! Reward: $" << finalReward << " (x" << payMultiplier << ") Loss: $" << totalLoss << std::endl;
        }
        
        // Update fragile health based on speed for fragile packages only
        if (currentOrder.type == OrderType::FRAGILE)
        {
            if (speedKmh > 30.0f)
            {
                currentOrder.fragileHealth -= deltaTime * 5.0f;
                if (currentOrder.fragileHealth <= 0.0f)
                {
                    currentOrder.state = OrderState::FAILED;
                    std::cout << "[DELIVERY] Package destroyed by high speed!" << std::endl;
                    GenerateNewOrder();
                }
            }
        }
        
        // Check time limit (don't auto-cancel, just penalize)
        if (orderElapsedTime > currentOrder.timeLimit)
        {
            // Time expired - will be penalized in reward calculation
        }
    }
    // Handle waiting after delivery
    else if (currentOrder.state == OrderState::DELIVERED)
    {
        orderElapsedTime += deltaTime;
        // Minimum 1.5s before can close (prevents same E from delivery from closing it)
        // Then only closes if player presses E
        if (orderElapsedTime > 1.5f && eKeyPressed)
        {
            GenerateNewOrder();
        }
    }
}

void DeliverySystem::Render(Shader& shader, Camera& camera)
{
    // Don't render if coordinates are invalid (no mission)
    if (currentOrder.originPosition.x > 10000.0f || currentOrder.destinationPosition.x > 10000.0f)
        return;
    
    // Create zone marker mesh locally
    Mesh zoneMarkerMesh = CreateZoneMarkerMesh();
    
    // Get current time for animation
    float currentTime = static_cast<float>(glfwGetTime());
    
    // Determine zone color based on state
    glm::vec3 pickupColor = glm::vec3(1.0f, 0.8f, 0.0f); // Yellow for pickup
    glm::vec3 deliveryColor = glm::vec3(0.0f, 1.0f, 0.3f); // Green for delivery
    
    // Determine zone color based on state
    glm::vec3 zoneColor;
    if (currentOrder.state == OrderState::WAITING)
    {
        zoneColor = glm::vec3(0.0f, 1.0f, 0.3f); // Bright green for pickup
    }
    else
    {
        zoneColor = glm::vec3(0.0f, 0.7f, 1.0f); // Neon blue for delivery
    }
    
    // Render zone marker at origin position when waiting
    if (currentOrder.state == OrderState::WAITING)
    {
        zoneShader.Activate();
        
        // Set uniforms
        glm::mat4 zoneModel = glm::translate(glm::mat4(1.0f), currentOrder.originPosition);
        glUniformMatrix4fv(glGetUniformLocation(zoneShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(zoneModel));
        glUniform1f(glGetUniformLocation(zoneShader.ID, "uTime"), currentTime);
        glUniform3fv(glGetUniformLocation(zoneShader.ID, "uZoneColor"), 1, glm::value_ptr(zoneColor));
        
        // Use camera's matrix method to set cameraMatrix uniform
        camera.Matrix(zoneShader, "cameraMatrix");
        
        zoneMarkerMesh.Draw(zoneShader, camera, zoneModel);
        
        // Render "MISIÓN" text above the pillar
        RenderZoneText(shader, camera, currentOrder.originPosition, "MISION", pickupColor);
        
        // Render package at origin position with default shader
        shader.Activate();
        package.Render(shader, camera);
    }
    // Render zone marker at destination position when picked up
    else if (currentOrder.state == OrderState::PICKED_UP)
    {
        zoneShader.Activate();
        
        // Set uniforms
        glm::mat4 zoneModel = glm::translate(glm::mat4(1.0f), currentOrder.destinationPosition);
        glUniformMatrix4fv(glGetUniformLocation(zoneShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(zoneModel));
        glUniform1f(glGetUniformLocation(zoneShader.ID, "uTime"), currentTime);
        glUniform3fv(glGetUniformLocation(zoneShader.ID, "uZoneColor"), 1, glm::value_ptr(zoneColor));
        
        // Use camera's matrix method to set cameraMatrix uniform
        camera.Matrix(zoneShader, "cameraMatrix");
        
        zoneMarkerMesh.Draw(zoneShader, camera, zoneModel);
        
        // Render "ENTREGAR" text above the pillar
        RenderZoneText(shader, camera, currentOrder.destinationPosition, "ENTREGAR", deliveryColor);
        
        // Render package (animated or following car) with default shader
        shader.Activate();
        if (isAnimatingPackage)
        {
            package.Render(shader, camera);
        }
    }
}

bool DeliverySystem::TryPickupOrder(const CarState& car)
{
    if (currentOrder.state != OrderState::WAITING)
        return false;
    
    // Check if car is near origin point
    if (!IsCarNearPoint(car.position, currentOrder.originPosition, pickupDistanceThreshold))
        return false;
    
    // Check if car speed is low enough (< 5 km/h)
    if (!IsCarSpeedLow(car.speed, maxPickupSpeedKmh))
        return false;
    
    return true;
}

bool DeliverySystem::TryDeliverOrder(const CarState& car)
{
    if (currentOrder.state != OrderState::PICKED_UP)
        return false;
    
    // Check if car is near destination point
    if (!IsCarNearPoint(car.position, currentOrder.destinationPosition, deliveryDistanceThreshold))
        return false;
    
    // Check if car speed is low enough
    if (!IsCarSpeedLow(car.speed, maxPickupSpeedKmh))
        return false;
    
    return true;
}

void DeliverySystem::LoadDeliveryZones()
{
    deliveryZones.clear();
    
    std::ifstream file("res/delivery_zones.txt");
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open res/delivery_zones.txt. No delivery zones loaded." << std::endl;
        return;
    }
    
    std::string line;
    DeliveryZone* currentZone = nullptr;
    
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        
        std::istringstream iss(line);
        std::string type;
        
        if (iss >> type)
        {
            if (type == "PICKUP")
            {
                DeliveryZone newZone;
                iss >> newZone.position.x >> newZone.position.y >> newZone.position.z;
                newZone.name = "Pickup Zone";
                deliveryZones.push_back(newZone);
                currentZone = &deliveryZones.back();
            }
            else if (type == "DELIVERY" && currentZone != nullptr)
            {
                glm::vec3 deliveryPos;
                iss >> deliveryPos.x >> deliveryPos.y >> deliveryPos.z;
                currentZone->deliveryPositions.push_back(deliveryPos);
            }
            else if (type == "END_PICKUP")
            {
                currentZone = nullptr;
            }
        }
    }
    
    file.close();
    
    if (deliveryZones.empty())
    {
        std::cerr << "Error: No delivery zones found in file. Cannot start game." << std::endl;
    }
}

bool DeliverySystem::IsCarNearPoint(const glm::vec3& carPos, const glm::vec3& point, float threshold) const
{
    float distance = glm::length(carPos - point);
    return distance <= threshold;
}

bool DeliverySystem::IsCarSpeedLow(float speed, float maxSpeedKmh) const
{
    // Convert speed from m/s to km/h
    float speedKmh = std::abs(speed) * 3.6f;
    return speedKmh <= maxSpeedKmh;
}

Mesh DeliverySystem::CreateZoneMarkerMesh()
{
    // Create a ring of 3D arrows pointing inward + vertical pillar
    const float innerRadius = 1.2f;
    const float outerRadius = 1.8f;
    const float height = 0.05f;
    const int numArrows = 12;
    // Create a vertical pillar (6x larger: 2x current 3x)
    const float pillarHeight = 18.0f;  // 6x larger
    const float pillarWidth = 0.9f;   // 6x wider
    
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    
    GLuint vertexOffset = 0;
    
    // Create vertical pillar (thin glowing line)
    // Front face
    vertices.push_back({{-pillarWidth/2, 0.0f, -pillarWidth/2}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
    vertices.push_back({{pillarWidth/2, 0.0f, -pillarWidth/2}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
    vertices.push_back({{pillarWidth/2, pillarHeight, -pillarWidth/2}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
    vertices.push_back({{-pillarWidth/2, pillarHeight, -pillarWidth/2}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
    indices.push_back(vertexOffset + 0); indices.push_back(vertexOffset + 1); indices.push_back(vertexOffset + 2);
    indices.push_back(vertexOffset + 0); indices.push_back(vertexOffset + 2); indices.push_back(vertexOffset + 3);
    vertexOffset += 4;
    
    // Back face
    vertices.push_back({{pillarWidth/2, 0.0f, pillarWidth/2}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
    vertices.push_back({{-pillarWidth/2, 0.0f, pillarWidth/2}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
    vertices.push_back({{-pillarWidth/2, pillarHeight, pillarWidth/2}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
    vertices.push_back({{pillarWidth/2, pillarHeight, pillarWidth/2}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
    indices.push_back(vertexOffset + 0); indices.push_back(vertexOffset + 1); indices.push_back(vertexOffset + 2);
    indices.push_back(vertexOffset + 0); indices.push_back(vertexOffset + 2); indices.push_back(vertexOffset + 3);
    vertexOffset += 4;
    
    // Left face
    vertices.push_back({{-pillarWidth/2, 0.0f, pillarWidth/2}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
    vertices.push_back({{-pillarWidth/2, 0.0f, -pillarWidth/2}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
    vertices.push_back({{-pillarWidth/2, pillarHeight, -pillarWidth/2}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
    vertices.push_back({{-pillarWidth/2, pillarHeight, pillarWidth/2}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
    indices.push_back(vertexOffset + 0); indices.push_back(vertexOffset + 1); indices.push_back(vertexOffset + 2);
    indices.push_back(vertexOffset + 0); indices.push_back(vertexOffset + 2); indices.push_back(vertexOffset + 3);
    vertexOffset += 4;
    
    // Right face
    vertices.push_back({{pillarWidth/2, 0.0f, -pillarWidth/2}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
    vertices.push_back({{pillarWidth/2, 0.0f, pillarWidth/2}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
    vertices.push_back({{pillarWidth/2, pillarHeight, pillarWidth/2}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
    vertices.push_back({{pillarWidth/2, pillarHeight, -pillarWidth/2}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
    indices.push_back(vertexOffset + 0); indices.push_back(vertexOffset + 1); indices.push_back(vertexOffset + 2);
    indices.push_back(vertexOffset + 0); indices.push_back(vertexOffset + 2); indices.push_back(vertexOffset + 3);
    vertexOffset += 4;
    
    // Top face
    vertices.push_back({{-pillarWidth/2, pillarHeight, -pillarWidth/2}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
    vertices.push_back({{pillarWidth/2, pillarHeight, -pillarWidth/2}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
    vertices.push_back({{pillarWidth/2, pillarHeight, pillarWidth/2}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
    vertices.push_back({{-pillarWidth/2, pillarHeight, pillarWidth/2}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
    indices.push_back(vertexOffset + 0); indices.push_back(vertexOffset + 1); indices.push_back(vertexOffset + 2);
    indices.push_back(vertexOffset + 0); indices.push_back(vertexOffset + 2); indices.push_back(vertexOffset + 3);
    vertexOffset += 4;
    
    // Create arrow shapes arranged in a circle
    for (int i = 0; i < numArrows; i++)
    {
        float angle = (float)i / numArrows * 6.28318f;
        float nextAngle = (float)(i + 1) / numArrows * 6.28318f;
        float midAngle = (angle + nextAngle) / 2.0f;
        
        // Arrow position on the ring
        float ringX = std::cos(midAngle) * ((innerRadius + outerRadius) / 2.0f);
        float ringZ = std::sin(midAngle) * ((innerRadius + outerRadius) / 2.0f);
        
        // Arrow shape (triangle pointing inward toward center)
        float arrowSize = 0.25f;
        
        // Calculate arrow direction (pointing toward center)
        glm::vec3 arrowDir = glm::normalize(glm::vec3(-ringX, 0.0f, -ringZ));
        glm::vec3 arrowPerp = glm::normalize(glm::vec3(-arrowDir.z, 0.0f, arrowDir.x));
        
        // Arrow tip (pointing inward)
        glm::vec3 tip = glm::vec3(ringX, height, ringZ) + arrowDir * arrowSize;
        
        // Arrow base (widest part, pointing outward)
        glm::vec3 baseCenter = glm::vec3(ringX, height, ringZ) - arrowDir * arrowSize * 0.5f;
        glm::vec3 baseLeft = baseCenter + arrowPerp * arrowSize * 0.5f;
        glm::vec3 baseRight = baseCenter - arrowPerp * arrowSize * 0.5f;
        
        // Create arrow triangle (top view)
        vertices.push_back({tip, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.0f}});
        vertices.push_back({baseLeft, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}});
        vertices.push_back({baseRight, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}});
        
        // Create indices for this arrow
        indices.push_back(vertexOffset + 0);
        indices.push_back(vertexOffset + 1);
        indices.push_back(vertexOffset + 2);
        
        vertexOffset += 3;
    }
    
    // Create a simple emissive texture (white for shader coloring)
    std::vector<unsigned char> textureData(64 * 64 * 4);
    for (int y = 0; y < 64; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            int idx = (y * 64 + x) * 4;
            textureData[idx + 0] = 255;
            textureData[idx + 1] = 255;
            textureData[idx + 2] = 255;
            textureData[idx + 3] = 255;
        }
    }
    
    std::vector<Texture> textures;
    textures.emplace_back(textureData.data(), 64, 64, GL_RGBA, "diffuse", 0);
    
    return Mesh(vertices, indices, textures);
}

glm::vec3 DeliverySystem::GetObjectivePosition() const
{
    if (currentOrder.state == OrderState::WAITING)
        return currentOrder.originPosition;
    else if (currentOrder.state == OrderState::PICKED_UP)
        return currentOrder.destinationPosition;
    else
        return glm::vec3(0.0f);
}

float DeliverySystem::GetDistanceToObjective(const CarState& car) const
{
    glm::vec3 objective = GetObjectivePosition();
    return glm::length(car.position - objective);
}

float DeliverySystem::CalculateReward() const
{
    // Use the same detailed calculation system for consistency
    float base, bonus, penCol, penWat, penTime;
    CalculateDetailedRewards(base, bonus, penCol, penWat, penTime);
    float finalReward = base + bonus - penCol - penWat - penTime;
    
    // Also apply health percentage for fragile packages
    float healthPercentage = currentOrder.fragileHealth / 100.0f;
    finalReward *= healthPercentage;
    
    return std::max(finalReward, 0.0f); // Ensure reward is not negative
}

float DeliverySystem::CalculateEstimatedReward() const
{
    float base, bonus, penCol, penWat, penTime;
    CalculateDetailedRewards(base, bonus, penCol, penWat, penTime);
    float reward = base + bonus - penCol - penWat - penTime;
    return std::max(reward, 0.0f);
}

float DeliverySystem::CalculateCollisionLoss() const
{
    // Use the same function as final calculation for consistency
    float base, bonus, penCol, penWat, penTime;
    CalculateDetailedRewards(base, bonus, penCol, penWat, penTime);
    float totalLoss = penCol + penWat + penTime;
    float maxLoss   = base + bonus;
    return std::min(totalLoss, maxLoss); // never exceeds maximum gain
}

void DeliverySystem::GenerateNewOrder()
{
    if (deliveryZones.empty())
    {
        std::cerr << "[DELIVERY] Error: No delivery zones available. Cannot generate order." << std::endl;
        return;
    }
    
    // Find a pickup zone that has delivery positions
    int pickupIndex = -1;
    int attempts = 0;
    const int maxAttempts = 100;
    
    while (pickupIndex == -1 && attempts < maxAttempts)
    {
        pickupIndex = std::rand() % deliveryZones.size();
        if (!deliveryZones[pickupIndex].deliveryPositions.empty())
        {
            break;
        }
        pickupIndex = -1;
        attempts++;
    }
    
    if (pickupIndex == -1)
    {
        std::cerr << "[DELIVERY] Error: No pickup zone has delivery positions. Cannot generate order." << std::endl;
        return;
    }
    
    DeliveryZone& pickupZone = deliveryZones[pickupIndex];
    
    // Choose a random delivery zone from the pickup zone's exclusive delivery positions
    int deliveryIndex = std::rand() % pickupZone.deliveryPositions.size();
    glm::vec3 deliveryPosition = pickupZone.deliveryPositions[deliveryIndex];
    
    currentOrder.originPosition = pickupZone.position;
    currentOrder.destinationPosition = deliveryPosition;
    currentOrder.state = OrderState::WAITING;
    
    // Store which pickup zone this order belongs to
    currentOrderZoneIndex = pickupIndex;
    
    // Random difficulty (0-3 for 4 difficulty levels)
    int difficultyRoll = std::rand() % 4;
    currentOrder.difficulty = static_cast<OrderDifficulty>(difficultyRoll);
    
    // Calculate distance for reward calculation
    float distance = glm::length(currentOrder.destinationPosition - currentOrder.originPosition);
    
    // Calculate time thresholds for star system based on distance
    float baseTime = distance / 5.0f;
    if (baseTime < 20.0f) baseTime = 20.0f; // Minimum guaranteed time
    
    timeStar3 = baseTime;
    timeStar2 = timeStar3 * 1.5f;
    timeStar1 = timeStar2 * 1.5f;
    timeStar0 = timeStar1 * 1.5f; // Maximum limit to fail
    
    currentOrder.timeLimit = timeStar0;
    
    // Base reward (reduced for balance)
    initialReward = 35.0f + (distance * 0.35f);
    if (currentOrder.difficulty == OrderDifficulty::HARD) initialReward *= 1.8f;
    
    // Set reward and type based on difficulty
    switch (currentOrder.difficulty)
    {
        case OrderDifficulty::EASY:
            currentOrder.type = OrderType::STANDARD;
            currentOrder.reward = initialReward; // Base reward for inspection
            break;
        case OrderDifficulty::MEDIUM:
            currentOrder.type = OrderType::STANDARD;
            currentOrder.reward = initialReward; // Base reward for inspection
            break;
        case OrderDifficulty::HARD:
            currentOrder.type = OrderType::EXPRESS;
            currentOrder.reward = initialReward; // Base reward for inspection
            break;
        case OrderDifficulty::SPECIAL:
            currentOrder.type = OrderType::FRAGILE; // Special orders are always fragile
            currentOrder.reward = initialReward * 1.5f; // Higher base for special
            initialReward = currentOrder.reward; // Sync initialReward with the higher value
            break;
    }
    
    // Reset fragile health
    currentOrder.fragileHealth = 100.0f;
    
    // Reset counters
    waterCount = 0;
    collisionCount = 0;
    orderElapsedTime = 0.0f;
    currentStars = 3;
    currentTargetTime = timeStar3;
    totalLoss = 0.0f;
    
    // Initialize package at origin position
    package = Paquete(currentOrder.originPosition);
    package.SetScale(glm::vec3(0.5f));
    
    std::cout << "[DELIVERY] New order generated. Distance: " << distance << "m, Reward: $" << currentOrder.reward << std::endl;
}

void DeliverySystem::RejectOrder(const CarState& car)
{
    std::cout << "[DELIVERY] Order rejected. Generating new order..." << std::endl;
    GenerateNewOrder();
}

void DeliverySystem::OnWaterRespawn()
{
    mIgnoreNextCollision = true; // FIX 3: Set flag BEFORE the rest

    if (currentOrder.state == OrderState::PICKED_UP)
    {
        waterCount++;
        if (currentOrder.type == OrderType::FRAGILE)
        {
            currentOrder.fragileHealth -= 50.0f;
            if (currentOrder.fragileHealth <= 0.0f)
            {
                currentOrder.fragileHealth = 0.0f;
                currentOrder.state = OrderState::FAILED;
                std::cout << "[DELIVERY] Paquete destruido por agua!" << std::endl;
                GenerateNewOrder();
            }
        }
    }
}

void DeliverySystem::CalculateDetailedRewards(float& base, float& bonus, float& penCol, float& penWat, float& penTime) const
{
    base = initialReward;
    bonus = 0.0f;
    penCol = 0.0f;
    penWat = 0.0f;
    penTime = 0.0f;
    
    // Strict bonus assignment based on time (without penalizing health or money yet)
    if (orderElapsedTime <= timeStar3) bonus = base * 0.50f;
    else if (orderElapsedTime <= timeStar2) bonus = base * 0.25f;
    else if (orderElapsedTime <= timeStar1) bonus = 0.0f;
    
    // Calculate real damage based on counters and package type
    // Use same percentages for fragile or standard, since Fragile has added risk of failing mission if health reaches 0, but economically loses the same
    penCol = collisionCount * (base * 0.15f); // 15% per collision
    penWat = waterCount * (base * 0.30f);     // 30% per water fall
    
    // Add speed damage penalty for fragile packages
    if (currentOrder.type == OrderType::FRAGILE)
    {
        float healthLost = 100.0f - currentOrder.fragileHealth;
        if (healthLost > 0.0f)
        {
            // Calculate how much of the health loss is from speed (not from collisions or water)
            // Assume each collision does ~15 damage and water does 50, so remaining is from speed
            float expectedHealthLoss = collisionCount * 15.0f + waterCount * 50.0f;
            float speedHealthLoss = std::max(0.0f, healthLost - expectedHealthLoss);
            
            // Convert speed health loss to monetary penalty (1% health loss = 1% of base reward)
            penCol += (speedHealthLoss / 100.0f) * base;
        }
    }
    
    // Time penalty ONLY applies if passes DOUBLE the time of 1 star
    float limiteDoble = timeStar1 * 2.0f;
    if (orderElapsedTime > limiteDoble) {
        penTime = base * 0.50f; // Penalize 50% extra only if took too long
    }

    // STRICT CLAMP: Losses can NEVER exceed maximum gain
    float gananciaTotal = base + bonus;
    float perdidasTotales = penCol + penWat + penTime;
    
    if (perdidasTotales > gananciaTotal) {
        // Make proportional reduction so visual loss matches what was deducted
        float factor = gananciaTotal / perdidasTotales;
        penCol *= factor;
        penWat *= factor;
        penTime *= factor;
    }
}

void DeliverySystem::RenderZoneText(Shader& shader, Camera& camera, const glm::vec3& position, const char* text, const glm::vec3& color)
{
    // Generate text geometry using stb_easy_font
    static float textBuffer[60000];
    int quads = stb_easy_font_print(0, 0, (char*)text, NULL, textBuffer, sizeof(textBuffer));
    
    if (quads <= 0) return;
    
    // Convert quads to triangles
    static float triangleBuffer[90000];
    for (int i = 0; i < quads; i++)
    {
        float* q = textBuffer + i * 16;
        memcpy(triangleBuffer + i * 24 + 0,  q,      12 * sizeof(float));
        memcpy(triangleBuffer + i * 24 + 12, q,       4 * sizeof(float));
        memcpy(triangleBuffer + i * 24 + 16, q + 8,   4 * sizeof(float));
        memcpy(triangleBuffer + i * 24 + 20, q + 12,  4 * sizeof(float));
    }
    
    // Create VAO and VBO for text
    static unsigned int textVAO = 0, textVBO = 0;
    if (textVAO == 0)
    {
        glGenVertexArrays(1, &textVAO);
        glGenBuffers(1, &textVBO);
    }
    
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, quads * 6 * 16, triangleBuffer, GL_DYNAMIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 16, (void*)0);
    
    // Position text above the pillar (adjusted for larger pillar)
    float textHeight = 18.5f;
    glm::vec3 textPos = position + glm::vec3(0.0f, textHeight, 0.0f);
    
    // Make text rotate above the pillar
    float currentTime = static_cast<float>(glfwGetTime());
    float textRotationAngle = currentTime * 1.0f; // Rotate once per second
    
    // Make text always face the camera (billboard effect)
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, textPos);
    model = glm::rotate(model, textRotationAngle, glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate around Y axis
    model = glm::scale(model, glm::vec3(0.15f, 0.15f, 0.15f)); // Scale down the text
    
    // Use the default shader for text
    shader.Activate();
    
    // Set shader uniforms for vertex shader
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "translation"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "rotation"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID, "scale"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    camera.Matrix(shader, "camMatrix");
    
    // Set color uniform for fragment shader
    glUniform3f(glGetUniformLocation(shader.ID, "lightColor"), color.r, color.g, color.b);
    
    // Disable depth writing for transparent text
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Draw text
    glBindVertexArray(textVAO);
    glDrawArrays(GL_TRIANGLES, 0, quads * 6);
    
    // Restore state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}
