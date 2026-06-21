#pragma once

#include <string>
#include <unordered_map>
#include <cmath>

enum class UpgradeType {
    Speed,
    PayPerDelivery,
    FuelEfficiency,
    Handling,
    Acceleration
};

enum class AbilityType {
    Teleport,
    Jump,
    Turbo,
    NeonUnderglow,
    TireDrift,
    TireGrip
};

struct Upgrade {
    std::string name;
    int currentLevel = 0;
    int maxLevel = 5;
    int baseCost = 100;
    float multiplier = 1.5f;

    Upgrade() = default;
    Upgrade(std::string n, int lvl, int maxLvl, int cost, float mult)
        : name(n), currentLevel(lvl), maxLevel(maxLvl), baseCost(cost), multiplier(mult) {}
    
    int GetCost() const { return baseCost * std::pow(multiplier, currentLevel); }
    bool CanUpgrade() const { return currentLevel < maxLevel; }
};

struct Ability {
    std::string name;
    bool unlocked = false;
    int cost = 500;

    Ability() = default;
    Ability(std::string n, bool unlockState, int c)
        : name(n), unlocked(unlockState), cost(c) {}
};

class ShopManager {
private:
    static ShopManager* instance;
    
    int walletBalance;
    std::unordered_map<UpgradeType, Upgrade> upgrades;
    std::unordered_map<AbilityType, Ability> abilities;
    
    ShopManager();
    
public:
    static ShopManager* GetInstance();
    static void Destroy();
    
    int GetBalance() const { return walletBalance; }
    void AddMoney(int amount) { walletBalance += amount; }
    
    bool PurchaseUpgrade(UpgradeType type);
    int GetUpgradeLevel(UpgradeType type) const;
    float GetUpgradeMultiplier(UpgradeType type) const;
    const Upgrade* GetUpgrade(UpgradeType type) const;
    
    bool PurchaseAbility(AbilityType type);
    bool IsAbilityUnlocked(AbilityType type) const;
    const Ability* GetAbility(AbilityType type) const;
    
    void SaveShopData(const std::string& filepath = "shop_save.txt");
    void LoadShopData(const std::string& filepath = "shop_save.txt");
    
    const std::unordered_map<UpgradeType, Upgrade>& GetAllUpgrades() const { return upgrades; }
    const std::unordered_map<AbilityType, Ability>& GetAllAbilities() const { return abilities; }
};
