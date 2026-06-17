#include "ShopManager.h"
#include <fstream>
#include <iostream>

ShopManager *ShopManager::instance = nullptr;

ShopManager::ShopManager() : walletBalance(3000)
{
    upgrades[UpgradeType::Speed] = Upgrade{"Speed", 0, 5, 150, 1.6f};
    upgrades[UpgradeType::PayPerDelivery] = Upgrade{"Pay Per Order", 0, 5, 100, 1.5f};
    upgrades[UpgradeType::FuelEfficiency] = Upgrade{"Durability", 0, 5, 120, 1.4f};
    upgrades[UpgradeType::Handling] = Upgrade{"Handling", 0, 5, 130, 1.55f};
    upgrades[UpgradeType::Acceleration] = Upgrade{"Acceleration", 0, 5, 140, 1.5f};

    abilities[AbilityType::Teleport] = Ability{"Teleport (Key 1)", false, 800};
    abilities[AbilityType::Jump] = Ability{"Jump (Key Z)", false, 600};
    abilities[AbilityType::Turbo] = Ability{"Turbo (Shift)", false, 1000};
}

ShopManager *ShopManager::GetInstance()
{
    if (!instance)
    {
        instance = new ShopManager();
    }
    return instance;
}

void ShopManager::Destroy()
{
    delete instance;
    instance = nullptr;
}

bool ShopManager::PurchaseUpgrade(UpgradeType type)
{
    auto it = upgrades.find(type);
    if (it == upgrades.end())
        return false;

    Upgrade &upgrade = it->second;
    if (!upgrade.CanUpgrade())
        return false;

    int cost = upgrade.GetCost();
    if (walletBalance < cost)
        return false;

    walletBalance -= cost;
    upgrade.currentLevel++;
    
    std::cout << "[SHOP] Mejora comprada: " << upgrade.name << " Nivel " << upgrade.currentLevel 
              << " | Multiplicador: x" << GetUpgradeMultiplier(type) << std::endl;
    
    return true;
}

int ShopManager::GetUpgradeLevel(UpgradeType type) const
{
    auto it = upgrades.find(type);
    return (it != upgrades.end()) ? it->second.currentLevel : 0;
}

float ShopManager::GetUpgradeMultiplier(UpgradeType type) const
{
    int level = GetUpgradeLevel(type);
    return 1.0f + (level * 0.2f);
}

const Upgrade *ShopManager::GetUpgrade(UpgradeType type) const
{
    auto it = upgrades.find(type);
    return (it != upgrades.end()) ? &it->second : nullptr;
}

bool ShopManager::PurchaseAbility(AbilityType type)
{
    auto it = abilities.find(type);
    if (it == abilities.end())
        return false;

    Ability &ability = it->second;
    if (ability.unlocked)
        return false;

    if (walletBalance < ability.cost)
        return false;

    walletBalance -= ability.cost;
    ability.unlocked = true;
    
    std::cout << "[SHOP] Habilidad desbloqueada: " << ability.name << std::endl;
    
    return true;
}

bool ShopManager::IsAbilityUnlocked(AbilityType type) const
{
    auto it = abilities.find(type);
    return (it != abilities.end()) ? it->second.unlocked : false;
}

const Ability *ShopManager::GetAbility(AbilityType type) const
{
    auto it = abilities.find(type);
    return (it != abilities.end()) ? &it->second : nullptr;
}

void ShopManager::SaveShopData(const std::string &filepath)
{
    std::ofstream file(filepath);
    if (!file.is_open())
    {
        std::cerr << "Error: No se pudo guardar " << filepath << std::endl;
        return;
    }

    file << walletBalance << "\n";

    file << upgrades.size() << "\n";
    for (const auto &pair : upgrades)
    {
        file << static_cast<int>(pair.first) << " " << pair.second.currentLevel << "\n";
    }

    file << abilities.size() << "\n";
    for (const auto &pair : abilities)
    {
        file << static_cast<int>(pair.first) << " " << pair.second.unlocked << "\n";
    }

    file.close();
}

void ShopManager::LoadShopData(const std::string &filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        std::cout << "No se encontró save, usando valores por defecto" << std::endl;
        return;
    }

    file >> walletBalance;

    int upgradeCount;
    file >> upgradeCount;
    for (int i = 0; i < upgradeCount; i++)
    {
        int typeInt, level;
        file >> typeInt >> level;
        UpgradeType type = static_cast<UpgradeType>(typeInt);
        if (upgrades.find(type) != upgrades.end())
        {
            upgrades[type].currentLevel = level;
        }
    }

    int abilityCount;
    file >> abilityCount;
    for (int i = 0; i < abilityCount; i++)
    {
        int typeInt, unlocked;
        file >> typeInt >> unlocked;
        AbilityType type = static_cast<AbilityType>(typeInt);
        if (abilities.find(type) != abilities.end())
        {
            abilities[type].unlocked = static_cast<bool>(unlocked);
        }
    }

    file.close();
}
