#include "game/combat.h"
#include "assets/fit_parser.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace mcgng {

CombatSystem& CombatSystem::instance() {
    static CombatSystem instance;
    return instance;
}

CombatSystem::CombatSystem() {
    std::random_device rd;
    m_rng.seed(rd());
}

bool CombatSystem::initialize() {
    m_projectiles.clear();
    return true;
}

void CombatSystem::update(float deltaTime) {
    // Update all projectiles
    for (auto& proj : m_projectiles) {
        if (proj.active) {
            updateProjectile(proj, deltaTime);
        }
    }

    // Remove inactive projectiles
    m_projectiles.erase(
        std::remove_if(m_projectiles.begin(), m_projectiles.end(),
            [](const Projectile& p) { return !p.active; }),
        m_projectiles.end());
}

bool CombatSystem::attack(Mech* attacker, int weaponIndex, Mech* target) {
    if (!attacker || !target || attacker->isDestroyed() || target->isDestroyed()) {
        return false;
    }

    const auto& weapons = attacker->getWeapons();
    if (weaponIndex < 0 || weaponIndex >= static_cast<int>(weapons.size())) {
        return false;
    }

    const auto& mounted = weapons[weaponIndex];
    if (!mounted.canFire() || !mounted.weapon) {
        return false;
    }

    float range = calculateRange(attacker, target);
    if (range < mounted.weapon->minRange || range > mounted.weapon->maxRange) {
        return false;
    }

    // Fire the weapon
    if (!attacker->fireWeapon(weaponIndex, target->getX(), target->getY())) {
        return false;
    }

    // Create projectile or instant hit depending on weapon type
    if (mounted.weapon->projectileSpeed > 0) {
        // Ballistic/missile weapon - create projectile
        Projectile proj;
        proj.weapon = mounted.weapon;
        proj.damage = mounted.weapon->damage;
        proj.x = attacker->getX();
        proj.y = attacker->getY();
        proj.targetX = target->getX();
        proj.targetY = target->getY();
        proj.speed = mounted.weapon->projectileSpeed;
        proj.source = attacker;
        proj.target = target;
        proj.lifetime = range / proj.speed * 2.0f;  // Generous lifetime
        proj.active = true;

        m_projectiles.push_back(proj);
    } else {
        // Energy weapon - instant hit
        Projectile proj;
        proj.weapon = mounted.weapon;
        proj.damage = mounted.weapon->damage;
        proj.source = attacker;
        proj.target = target;
        proj.x = target->getX();
        proj.y = target->getY();
        proj.active = true;

        resolveHit(proj);
    }

    // Fire weapon fired event
    CombatEvent event;
    event.type = CombatEventType::WeaponFired;
    event.attacker = attacker;
    event.target = target;
    event.weapon = mounted.weapon;
    event.x = attacker->getX();
    event.y = attacker->getY();
    fireEvent(event);

    return true;
}

bool CombatSystem::attackGround(Mech* attacker, int weaponIndex, float x, float y) {
    if (!attacker || attacker->isDestroyed()) {
        return false;
    }

    const auto& weapons = attacker->getWeapons();
    if (weaponIndex < 0 || weaponIndex >= static_cast<int>(weapons.size())) {
        return false;
    }

    const auto& mounted = weapons[weaponIndex];
    if (!mounted.canFire() || !mounted.weapon) {
        return false;
    }

    float range = calculateRange(attacker, x, y);
    if (range < mounted.weapon->minRange || range > mounted.weapon->maxRange) {
        return false;
    }

    // Fire the weapon
    if (!attacker->fireWeapon(weaponIndex, x, y)) {
        return false;
    }

    // For ground attacks, just create projectile (could hit mechs in area)
    if (mounted.weapon->projectileSpeed > 0) {
        Projectile proj;
        proj.weapon = mounted.weapon;
        proj.damage = mounted.weapon->damage;
        proj.x = attacker->getX();
        proj.y = attacker->getY();
        proj.targetX = x;
        proj.targetY = y;
        proj.speed = mounted.weapon->projectileSpeed;
        proj.source = attacker;
        proj.target = nullptr;
        proj.lifetime = range / proj.speed * 2.0f;
        proj.active = true;

        m_projectiles.push_back(proj);
    }

    return true;
}

float CombatSystem::calculateHitChance(const Mech* attacker, const Weapon* weapon, const Mech* target) const {
    if (!attacker || !weapon || !target) {
        return 0.0f;
    }

    float hitChance = m_baseHitChance;

    // Range modifier
    float range = calculateRange(attacker, target);
    float optimalRange = (weapon->minRange + weapon->maxRange) / 2.0f;
    float rangePenalty = std::abs(range - optimalRange) / 100.0f * m_rangeModifier;
    hitChance -= rangePenalty;

    // Movement modifiers
    if (attacker->isMoving()) {
        hitChance -= m_movementModifier;
    }
    if (target->isMoving()) {
        hitChance -= m_movementModifier;
    }

    // Target size (tonnage based)
    float sizeMod = (100.0f - target->getChassis().tonnage) / 200.0f;
    hitChance -= sizeMod;

    return std::clamp(hitChance, 0.05f, 0.95f);
}

MechLocation CombatSystem::determineHitLocation(const Mech* target) const {
    if (!target) {
        return MechLocation::CenterTorso;
    }

    // Standard hit location table (simplified)
    std::uniform_int_distribution<int> dist(1, 100);
    int roll = dist(const_cast<std::mt19937&>(m_rng));

    // Weighted hit locations (roughly based on tabletop BattleTech)
    if (roll <= 10) return MechLocation::Head;
    if (roll <= 20) return MechLocation::CenterTorso;
    if (roll <= 30) return MechLocation::LeftTorso;
    if (roll <= 40) return MechLocation::RightTorso;
    if (roll <= 55) return MechLocation::LeftArm;
    if (roll <= 70) return MechLocation::RightArm;
    if (roll <= 85) return MechLocation::LeftLeg;
    return MechLocation::RightLeg;
}

bool CombatSystem::checkCritical(Mech* target, MechLocation location) {
    if (!target) return false;

    const auto& component = target->getComponent(location);

    // Higher crit chance when armor is gone
    float critChance = m_criticalChance;
    if (component.armor <= 0) {
        critChance *= 3.0f;  // Triple crit chance with no armor
    }

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(m_rng) < critChance;
}

void CombatSystem::fireEvent(const CombatEvent& event) {
    if (m_eventCallback) {
        m_eventCallback(event);
    }
}

void CombatSystem::updateProjectile(Projectile& proj, float deltaTime) {
    // Update lifetime
    proj.lifetime -= deltaTime;
    if (proj.lifetime <= 0) {
        proj.active = false;
        return;
    }

    // Move projectile
    float dx = proj.targetX - proj.x;
    float dy = proj.targetY - proj.y;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < proj.speed * deltaTime) {
        // Reached target
        proj.x = proj.targetX;
        proj.y = proj.targetY;
        resolveHit(proj);
        proj.active = false;
    } else {
        // Move towards target
        float factor = proj.speed * deltaTime / dist;
        proj.x += dx * factor;
        proj.y += dy * factor;
    }
}

void CombatSystem::resolveHit(Projectile& proj) {
    if (!proj.target || proj.target->isDestroyed()) {
        // Ground hit or target already dead
        return;
    }

    // Calculate hit chance
    float hitChance = calculateHitChance(proj.source, proj.weapon, proj.target);

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    bool hit = dist(m_rng) < hitChance;

    if (!hit) {
        // Miss
        CombatEvent event;
        event.type = CombatEventType::Miss;
        event.attacker = proj.source;
        event.target = proj.target;
        event.weapon = proj.weapon;
        fireEvent(event);
        return;
    }

    // Determine hit location
    MechLocation location = determineHitLocation(proj.target);

    // Apply damage
    int damage = proj.damage;

    // Check for critical hit
    bool isCritical = checkCritical(proj.target, location);
    if (isCritical) {
        damage = static_cast<int>(damage * 1.5f);

        CombatEvent critEvent;
        critEvent.type = CombatEventType::CriticalHit;
        critEvent.attacker = proj.source;
        critEvent.target = proj.target;
        critEvent.weapon = proj.weapon;
        critEvent.hitLocation = location;
        critEvent.damage = damage;
        fireEvent(critEvent);
    }

    // Apply damage to target
    bool wasDestroyed = proj.target->isDestroyed();
    proj.target->applyDamage(location, damage);

    // Fire hit event
    CombatEvent hitEvent;
    hitEvent.type = CombatEventType::Hit;
    hitEvent.attacker = proj.source;
    hitEvent.target = proj.target;
    hitEvent.weapon = proj.weapon;
    hitEvent.hitLocation = location;
    hitEvent.damage = damage;
    fireEvent(hitEvent);

    // Check for destruction
    if (!wasDestroyed && proj.target->isDestroyed()) {
        CombatEvent destroyEvent;
        destroyEvent.type = CombatEventType::MechDestroyed;
        destroyEvent.attacker = proj.source;
        destroyEvent.target = proj.target;
        fireEvent(destroyEvent);
    }
}

float CombatSystem::calculateRange(const Mech* a, const Mech* b) const {
    if (!a || !b) return 0.0f;
    return calculateRange(a, b->getX(), b->getY());
}

float CombatSystem::calculateRange(const Mech* a, float x, float y) const {
    if (!a) return 0.0f;
    float dx = x - a->getX();
    float dy = y - a->getY();
    return std::sqrt(dx * dx + dy * dy);
}

// WeaponDatabase implementation

WeaponDatabase& WeaponDatabase::instance() {
    static WeaponDatabase instance;
    return instance;
}

WeaponDatabase::WeaponDatabase() {
    loadDefaultWeapons();
}

void WeaponDatabase::loadDefaultWeapons() {
    // Default weapon definitions (based on BattleTech)
    m_weapons = {
        // Energy weapons
        {"Medium Laser", WeaponType::Laser, 5, 3, 0, 270, 1.0f, 0, 0, 1},
        {"Large Laser", WeaponType::LargeLaser, 8, 8, 0, 450, 1.5f, 0, 0, 1},
        {"Small Laser", WeaponType::Laser, 3, 1, 0, 90, 0.5f, 0, 0, 1},
        {"PPC", WeaponType::PPC, 10, 10, 90, 540, 2.0f, 0, 0, 1},

        // Ballistic weapons
        {"Machine Gun", WeaponType::MachineGun, 2, 0, 0, 90, 0.25f, 200, 500, 1},
        {"AC/2", WeaponType::Autocannon, 2, 1, 120, 720, 1.0f, 45, 300, 1},
        {"AC/5", WeaponType::Autocannon, 5, 1, 90, 540, 1.0f, 20, 250, 1},
        {"AC/10", WeaponType::Autocannon, 10, 3, 0, 450, 1.5f, 10, 200, 1},
        {"AC/20", WeaponType::Autocannon, 20, 7, 0, 270, 2.0f, 5, 150, 1},
        {"Gauss Rifle", WeaponType::Gauss, 15, 1, 90, 660, 2.0f, 8, 600, 1},

        // Missile weapons
        {"SRM 2", WeaponType::SRM, 4, 2, 0, 270, 1.5f, 50, 150, 2},
        {"SRM 4", WeaponType::SRM, 8, 3, 0, 270, 1.5f, 25, 150, 4},
        {"SRM 6", WeaponType::SRM, 12, 4, 0, 270, 1.5f, 15, 150, 6},
        {"LRM 5", WeaponType::LRM, 5, 2, 180, 630, 2.0f, 24, 120, 5},
        {"LRM 10", WeaponType::LRM, 10, 4, 180, 630, 2.0f, 12, 120, 10},
        {"LRM 15", WeaponType::LRM, 15, 5, 180, 630, 2.0f, 8, 120, 15},
        {"LRM 20", WeaponType::LRM, 20, 6, 180, 630, 2.0f, 6, 120, 20},
        {"Streak SRM 2", WeaponType::Streak, 4, 2, 0, 270, 1.5f, 50, 150, 2},
    };
}

bool WeaponDatabase::loadFromFile(const std::string& path) {
    FitParser parser;
    if (!parser.parseFile(path)) {
        return false;
    }

    for (const auto& block : parser.getBlocks()) {
        Weapon weapon;
        weapon.name = block.name;

        // Parse weapon type
        if (auto val = block.getString("Type")) {
            std::string type = *val;
            if (type == "Laser") weapon.type = WeaponType::Laser;
            else if (type == "PulseLaser") weapon.type = WeaponType::PulseLaser;
            else if (type == "LargeLaser") weapon.type = WeaponType::LargeLaser;
            else if (type == "PPC") weapon.type = WeaponType::PPC;
            else if (type == "MachineGun") weapon.type = WeaponType::MachineGun;
            else if (type == "Autocannon") weapon.type = WeaponType::Autocannon;
            else if (type == "Gauss") weapon.type = WeaponType::Gauss;
            else if (type == "SRM") weapon.type = WeaponType::SRM;
            else if (type == "LRM") weapon.type = WeaponType::LRM;
            else if (type == "Streak") weapon.type = WeaponType::Streak;
        }

        if (auto val = block.getInt("Damage")) weapon.damage = static_cast<int>(*val);
        if (auto val = block.getInt("Heat")) weapon.heat = static_cast<int>(*val);
        if (auto val = block.getFloat("MinRange")) weapon.minRange = static_cast<float>(*val);
        if (auto val = block.getFloat("MaxRange")) weapon.maxRange = static_cast<float>(*val);
        if (auto val = block.getFloat("Cooldown")) weapon.cooldown = static_cast<float>(*val);
        if (auto val = block.getInt("AmmoPerTon")) weapon.ammoPerTon = static_cast<int>(*val);
        if (auto val = block.getFloat("ProjectileSpeed")) weapon.projectileSpeed = static_cast<float>(*val);
        if (auto val = block.getInt("SalvoSize")) weapon.salvoSize = static_cast<int>(*val);

        m_weapons.push_back(weapon);
    }

    return true;
}

const Weapon* WeaponDatabase::getWeapon(const std::string& name) const {
    for (const auto& weapon : m_weapons) {
        if (weapon.name == name) {
            return &weapon;
        }
    }
    return nullptr;
}

std::vector<const Weapon*> WeaponDatabase::getWeaponsByType(WeaponType type) const {
    std::vector<const Weapon*> result;
    for (const auto& weapon : m_weapons) {
        if (weapon.type == type) {
            result.push_back(&weapon);
        }
    }
    return result;
}

} // namespace mcgng
