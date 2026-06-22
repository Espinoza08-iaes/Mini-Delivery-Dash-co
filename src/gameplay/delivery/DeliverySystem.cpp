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
    // Handle waiting after delivery (Success Screen Pause)
    if (lastDeliveredOrder.state == OrderState::DELIVERED)
    {
        lastDeliveredOrder.elapsedTime += deltaTime;
        // Minimum 1.5s before can close
        if (lastDeliveredOrder.elapsedTime > 1.5f && eKeyPressed)
        {
            lastDeliveredOrder.state = OrderState::WAITING; // clear state
            std::cout << "[DELIVERY] Closed success screen, resuming active deliveries." << std::endl;
        }
        // Do not update other timers while paused on success screen
        return;
    }

    if (isAnimatingPackage)
    {
        packageAnimationTime += deltaTime;
        if (packageAnimationTime >= packageAnimationDuration)
        {
            packageAnimationTime = packageAnimationDuration;
            isAnimatingPackage = false;
        }
        float t = packageAnimationTime / packageAnimationDuration;
        float smoothT = t * t * (3.0f - 2.0f * t);
        package.SetPosition(glm::mix(packageAnimationStart, packageAnimationEnd, smoothT));
    }
    
    // Check pickup
    if (activeOrders.size() < 3 && waitingOrder.state == OrderState::WAITING)
    {
        if (TryPickupOrder(car) && eKeyPressed)
        {
            packageAnimationStart = waitingOrder.originPosition;
            packageAnimationEnd = car.position + glm::vec3(0.0f, 1.0f, 0.0f);
            packageAnimationTime = 0.0f;
            isAnimatingPackage = true;
            
            waitingOrder.state = OrderState::PICKED_UP;
            waitingOrder.elapsedTime = 0.0f;
            waitingOrder.collisionCount = 0;
            waitingOrder.waterCount = 0;
            
            activeOrders.push_back(waitingOrder);
        }
    }

    float speedKmh = std::abs(car.speed) * 3.6f;
    static float lastSpeed = speedKmh;
    float speedChange = std::abs(lastSpeed - speedKmh);
    bool hadSignificantCollision = (speedChange > 15.0f && lastSpeed > 15.0f);
    lastSpeed = speedKmh;
    
    if (hadSignificantCollision && mIgnoreNextCollision) {
        mIgnoreNextCollision = false;
        hadSignificantCollision = false;
    }

    for (auto it = activeOrders.begin(); it != activeOrders.end(); )
    {
        DeliveryOrder& order = *it;
        order.elapsedTime += deltaTime;
        
        // Update star tracking for HUD (based on this order's time)
        if (order.elapsedTime <= timeStar3) { currentStars = 3; currentTargetTime = timeStar3; }
        else if (order.elapsedTime <= timeStar2) { currentStars = 2; currentTargetTime = timeStar2; }
        else if (order.elapsedTime <= timeStar1) { currentStars = 1; currentTargetTime = timeStar1; }
        else { currentStars = 0; currentTargetTime = timeStar0; }
        
        if (hadSignificantCollision)
        {
            order.collisionCount++;
            if (order.type == OrderType::FRAGILE) order.fragileHealth -= 15.0f;
        }
        
        if (order.type == OrderType::FRAGILE && speedKmh > 30.0f)
            order.fragileHealth -= deltaTime * 5.0f;
        
        if (order.type != OrderType::FRAGILE)
        {
            float b, bon, pC, pW, pT;
            CalculateDetailedRewards(order, b, bon, pC, pW, pT);
            float totalPenalties = pC + pW + pT;
            order.fragileHealth = 100.0f * (1.0f - (totalPenalties / b));
        }
        
        if (order.fragileHealth < 0.0f) order.fragileHealth = 0.0f;
        
        if (order.fragileHealth <= 0.0f)
        {
            std::cout << "[DELIVERY] Paquete destruido!" << std::endl;
            it = activeOrders.erase(it);
            continue;
        }
        
        if (order.state == OrderState::DROPPING_OFF)
        {
            if (!isAnimatingPackage)
            {
                float base, bonus, penCol, penWat, penTime;
                CalculateDetailedRewards(order, base, bonus, penCol, penWat, penTime);
            
            ShopManager* shop = ShopManager::GetInstance();
            float payMultiplier = shop->GetUpgradeMultiplier(UpgradeType::PayPerDelivery);
            
            base *= payMultiplier;
            bonus *= payMultiplier;
            penCol *= payMultiplier;
            penWat *= payMultiplier;
            penTime *= payMultiplier;
            
            float finalReward = base + bonus - penCol - penWat - penTime;
            if (finalReward < 0.0f) finalReward = 0.0f;
            
            int starsEarned = 0;
            if (order.elapsedTime <= order.timeGold) starsEarned = 3;
            else if (order.elapsedTime <= order.timeSilver) starsEarned = 2;
            else if (order.elapsedTime <= order.timeBronze) starsEarned = 1;
            
            order.starsEarned = starsEarned;
            currentStars = starsEarned;
            
            AddToWallet(finalReward);
            shop->AddMoney(static_cast<int>(finalReward));
            shop->SaveShopData();
            
            order.reward = base;
            order.state = OrderState::DELIVERED;
            
            lastDeliveredOrder = order;
            lastDeliveredOrder.elapsedTime = 0.0f;
            lastDeliveredOrder.starsEarned = starsEarned;
            
            finalBonusAmount = bonus;
            finalLossCollision = penCol;
            finalLossWater = penWat;
            finalLossTime = penTime;
            totalLoss = penCol + penWat + penTime;
            finalElapsedTime = order.elapsedTime;
            
                std::cout << "[DELIVERY] Order completed! Reward: $" << finalReward 
                          << " | Stars: " << starsEarned << std::endl;
                
                it = activeOrders.erase(it);
                continue;
            }
            ++it;
            continue;
        }
        
        bool nearDelivery = IsCarNearPoint(car.position, order.destinationPosition, deliveryDistanceThreshold);
        if (nearDelivery && IsCarSpeedLow(car.speed, maxPickupSpeedKmh) && eKeyPressed)
        {
            packageAnimationStart = car.position + glm::vec3(0.0f, 1.0f, 0.0f);
            packageAnimationEnd = order.destinationPosition;
            packageAnimationTime = 0.0f;
            isAnimatingPackage = true;
            order.state = OrderState::DROPPING_OFF;
            ++it;
            continue;
        }
        
        ++it;
    }
    
    if (activeOrders.empty() && waitingOrder.state != OrderState::WAITING)
    {
        GenerateNewOrder();
    }
}

void DeliverySystem::Render(Shader& shader, Camera& camera)
{
    float currentTime = static_cast<float>(glfwGetTime());
    glm::vec3 pickupColor = glm::vec3(1.0f, 0.8f, 0.0f); // Yellow for pickup
    glm::vec3 deliveryColor = glm::vec3(0.0f, 1.0f, 0.3f); // Green for delivery
    
    // Render waiting order
    if (activeOrders.size() < 3 && waitingOrder.state == OrderState::WAITING)
    {
        Mesh zoneMarkerMesh = CreateZoneMarkerMesh();
        zoneShader.Activate();
        
        glm::mat4 zoneModel = glm::translate(glm::mat4(1.0f), waitingOrder.originPosition);
        glUniformMatrix4fv(glGetUniformLocation(zoneShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(zoneModel));
        glUniform1f(glGetUniformLocation(zoneShader.ID, "uTime"), currentTime);
        
        glm::vec3 zoneColor = glm::vec3(0.0f, 0.8f, 1.0f); // Cyan
        glUniform3fv(glGetUniformLocation(zoneShader.ID, "uZoneColor"), 1, glm::value_ptr(zoneColor));
        camera.Matrix(zoneShader, "cameraMatrix");
        zoneMarkerMesh.Draw(zoneShader, camera, zoneModel);
        
        RenderZoneText(shader, camera, waitingOrder.originPosition, "MISION", zoneColor);
        
        shader.Activate();
        package.SetPosition(waitingOrder.originPosition);
        package.Render(shader, camera);
    }
    
    // Render active orders
    if (!activeOrders.empty())
    {
        glm::vec3 carPos = camera.Position;
        std::vector<std::pair<float, size_t>> dists;
        for (size_t i = 0; i < activeOrders.size(); ++i) {
            dists.push_back({glm::length(carPos - activeOrders[i].destinationPosition), i});
        }
        std::sort(dists.begin(), dists.end(), [](const std::pair<float, size_t>& a, const std::pair<float, size_t>& b) {
            return a.first < b.first;
        });
        
        std::vector<glm::vec3> orderColors(activeOrders.size());
        for (size_t i = 0; i < dists.size(); ++i) {
            if (i == 0) orderColors[dists[i].second] = glm::vec3(0.0f, 1.0f, 0.3f); // Green
            else if (i == 1) orderColors[dists[i].second] = glm::vec3(1.0f, 0.8f, 0.0f); // Yellow
            else orderColors[dists[i].second] = glm::vec3(1.0f, 0.2f, 0.2f); // Red
        }

        for (size_t i = 0; i < activeOrders.size(); ++i)
        {
            Mesh zoneMarkerMesh = CreateZoneMarkerMesh();
            zoneShader.Activate();
            
            glm::mat4 zoneModel = glm::translate(glm::mat4(1.0f), activeOrders[i].destinationPosition);
            glUniformMatrix4fv(glGetUniformLocation(zoneShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(zoneModel));
            glUniform1f(glGetUniformLocation(zoneShader.ID, "uTime"), currentTime);
            
            glm::vec3 zoneColor = orderColors[i];
            glUniform3fv(glGetUniformLocation(zoneShader.ID, "uZoneColor"), 1, glm::value_ptr(zoneColor));
            camera.Matrix(zoneShader, "cameraMatrix");
            zoneMarkerMesh.Draw(zoneShader, camera, zoneModel);
            
            RenderZoneText(shader, camera, activeOrders[i].destinationPosition, "ENTREGAR", zoneColor);
        }
    }
    
    // Render animated package
    if (isAnimatingPackage)
    {
        shader.Activate();
        package.Render(shader, camera);
    }
}

bool DeliverySystem::TryPickupOrder(const CarState& car)
{
    if (activeOrders.size() >= 3 || waitingOrder.state != OrderState::WAITING) return false;
    if (!IsCarNearPoint(car.position, waitingOrder.originPosition, pickupDistanceThreshold)) return false;
    return IsCarSpeedLow(car.speed, maxPickupSpeedKmh);
}

bool DeliverySystem::TryDeliverOrder(const CarState& car)
{
    // Now handled in Update loop iteration
    return false;
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

const DeliveryOrder* DeliverySystem::GetClosestActiveOrder(const glm::vec3& carPos) const
{
    if (activeOrders.empty()) return nullptr;
    
    const DeliveryOrder* closest = &activeOrders[0];
    float minDist = glm::length(carPos - closest->destinationPosition);
    
    for (size_t i = 1; i < activeOrders.size(); ++i)
    {
        float dist = glm::length(carPos - activeOrders[i].destinationPosition);
        if (dist < minDist)
        {
            minDist = dist;
            closest = &activeOrders[i];
        }
    }
    return closest;
}

glm::vec3 DeliverySystem::GetObjectivePosition() const
{
    if (!activeOrders.empty())
    {
        return activeOrders[0].destinationPosition; // For simple objective fallback
    }
    return waitingOrder.originPosition;
}

float DeliverySystem::GetDistanceToObjective(const CarState& car) const
{
    if (!activeOrders.empty())
    {
        const DeliveryOrder* closest = GetClosestActiveOrder(car.position);
        if (closest) return glm::length(car.position - closest->destinationPosition);
    }
    return glm::length(car.position - waitingOrder.originPosition);
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
    
    waitingOrder.originPosition = pickupZone.position;
    waitingOrder.destinationPosition = deliveryPosition;
    waitingOrder.state = OrderState::WAITING;
    
    // Store which pickup zone this order belongs to
    currentOrderZoneIndex = pickupIndex;
    
    // Random difficulty (0-3 for 4 difficulty levels)
    int difficultyRoll = std::rand() % 4;
    waitingOrder.difficulty = static_cast<OrderDifficulty>(difficultyRoll);
    
    // Calculate distance for reward calculation
    float distance = glm::length(waitingOrder.destinationPosition - waitingOrder.originPosition);
    
    // Calculate time thresholds for star system based on distance
    float baseTime = distance / 5.0f;
    if (baseTime < 20.0f) baseTime = 20.0f; // Minimum guaranteed time
    
    timeStar3 = baseTime;
    timeStar2 = timeStar3 * 1.5f;
    timeStar1 = timeStar2 * 1.5f;
    timeStar0 = timeStar1 * 1.5f; // Maximum limit to fail
    
    waitingOrder.timeLimit = timeStar0;
    waitingOrder.timeGold = timeStar3;
    waitingOrder.timeSilver = timeStar2;
    waitingOrder.timeBronze = timeStar1;
    
    // Base reward (reduced for balance)
    initialReward = 35.0f + (distance * 0.35f);
    if (waitingOrder.difficulty == OrderDifficulty::HARD) initialReward *= 1.8f;
    waitingOrder.baseDisplayReward = initialReward;
    
    // Set reward and type based on difficulty
    switch (waitingOrder.difficulty)
    {
        case OrderDifficulty::EASY:
            waitingOrder.type = OrderType::STANDARD;
            waitingOrder.reward = initialReward; // Base reward for inspection
            break;
        case OrderDifficulty::MEDIUM:
            waitingOrder.type = OrderType::STANDARD;
            waitingOrder.reward = initialReward; // Base reward for inspection
            break;
        case OrderDifficulty::HARD:
            waitingOrder.type = OrderType::EXPRESS;
            waitingOrder.reward = initialReward; // Base reward for inspection
            break;
        case OrderDifficulty::SPECIAL:
            waitingOrder.type = OrderType::FRAGILE; // Special orders are always fragile
            waitingOrder.reward = initialReward * 1.5f; // Higher base for special
            initialReward = waitingOrder.reward; // Sync initialReward with the higher value
            break;
    }
    
    // Reset fragile health
    waitingOrder.fragileHealth = 100.0f;
    
    // Reset counters
    waitingOrder.waterCount = 0;
    waitingOrder.collisionCount = 0;
    waitingOrder.elapsedTime = 0.0f;
    currentStars = 3;
    currentTargetTime = timeStar3;
    totalLoss = 0.0f;
    
    // Initialize package at origin position
    package = Paquete(waitingOrder.originPosition);
    package.SetScale(glm::vec3(0.5f));
    
    std::cout << "[DELIVERY] New order generated. Distance: " << distance << "m, Reward: $" << waitingOrder.reward << std::endl;
}

void DeliverySystem::RejectOrder(const CarState& car)
{
    std::cout << "[DELIVERY] Order rejected. Generating new order..." << std::endl;
    GenerateNewOrder();
}

void DeliverySystem::OnWaterRespawn()
{
    mIgnoreNextCollision = true; 

    for (auto& order : activeOrders)
    {
        order.waterCount++;
        if (order.type == OrderType::FRAGILE)
        {
            order.fragileHealth -= 50.0f;
            // The loop in Update will kill it if it reaches 0
        }
    }
}

void DeliverySystem::CalculateDetailedRewards(const DeliveryOrder& order, float& base, float& bonus, float& penCol, float& penWat, float& penTime) const
{
    base = order.reward;
    bonus = 0.0f;
    penCol = 0.0f;
    penWat = 0.0f;
    penTime = 0.0f;
    
    penCol = order.collisionCount * (base * 0.15f);
    penWat = order.waterCount * (base * 0.30f);
    
    if (order.type == OrderType::FRAGILE)
    {
        float healthLost = 100.0f - order.fragileHealth;
        if (healthLost > 0.0f)
        {
            float expectedHealthLoss = order.collisionCount * 15.0f + order.waterCount * 50.0f;
            float speedHealthLoss = std::max(0.0f, healthLost - expectedHealthLoss);
            penCol += (speedHealthLoss / 100.0f) * base;
        }
    }
    
    if (order.elapsedTime <= order.timeGold) {
        bonus = base * 0.40f;
    } else if (order.elapsedTime <= order.timeSilver) {
        bonus = base * 0.20f;
    } else if (order.elapsedTime <= order.timeBronze) {
        bonus = base * 0.10f;
    } else {
        float overtime = order.elapsedTime - order.timeBronze;
        penTime = overtime * 1.5f;
    }

    float limiteDoble = order.timeBronze * 2.0f;
    if (order.elapsedTime > limiteDoble) {
        penTime += base * 0.50f;
    }

    float gananciaTotal = base + bonus;
    float perdidasTotales = penCol + penWat + penTime;
    
    if (perdidasTotales > gananciaTotal) {
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

int DeliverySystem::GetStarsEarned(const DeliveryOrder& order) const
{
    if (order.elapsedTime <= order.timeGold) return 3;
    if (order.elapsedTime <= order.timeSilver) return 2;
    if (order.elapsedTime <= order.timeBronze) return 1;
    return 0;
}
