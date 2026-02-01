#include "game/mech.h"
#include "assets/fit_parser.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace mcgng {

bool Mech::initialize(const MechChassis& chassis) {
    m_chassis = chassis;

    // Initialize components
    m_components[static_cast<int>(MechLocation::Head)] = {
        chassis.headArmor, chassis.headArmor,
        chassis.headStructure, chassis.headStructure, false
    };
    m_components[static_cast<int>(MechLocation::CenterTorso)] = {
        chassis.centerTorsoArmor, chassis.centerTorsoArmor,
        chassis.centerTorsoStructure, chassis.centerTorsoStructure, false
    };
    m_components[static_cast<int>(MechLocation::LeftTorso)] = {
        chassis.sideTorsoArmor, chassis.sideTorsoArmor,
        chassis.sideTorsoStructure, chassis.sideTorsoStructure, false
    };
    m_components[static_cast<int>(MechLocation::RightTorso)] = {
        chassis.sideTorsoArmor, chassis.sideTorsoArmor,
        chassis.sideTorsoStructure, chassis.sideTorsoStructure, false
    };
    m_components[static_cast<int>(MechLocation::LeftArm)] = {
        chassis.armArmor, chassis.armArmor,
        chassis.armStructure, chassis.armStructure, false
    };
    m_components[static_cast<int>(MechLocation::RightArm)] = {
        chassis.armArmor, chassis.armArmor,
        chassis.armStructure, chassis.armStructure, false
    };
    m_components[static_cast<int>(MechLocation::LeftLeg)] = {
        chassis.legArmor, chassis.legArmor,
        chassis.legStructure, chassis.legStructure, false
    };
    m_components[static_cast<int>(MechLocation::RightLeg)] = {
        chassis.legArmor, chassis.legArmor,
        chassis.legStructure, chassis.legStructure, false
    };

    // Set heat sinks
    m_heatSinks = chassis.heatSinks;
    m_heat = 0.0f;
    m_destroyed = false;

    return true;
}

void Mech::update(float deltaTime) {
    if (m_destroyed || m_shutdown) {
        return;
    }

    // Update weapon cooldowns
    for (auto& weapon : m_weapons) {
        if (weapon.cooldownTimer > 0) {
            weapon.cooldownTimer -= deltaTime;
        }
    }

    // Dissipate heat
    dissipateHeat(deltaTime);

    // Check for shutdown from overheating
    if (m_heat >= m_maxHeat) {
        m_shutdown = true;
        std::cout << m_callsign << " shutdown from overheating!\n";
    }

    // Process movement
    if (m_moving) {
        float dx = m_targetX - m_x;
        float dy = m_targetY - m_y;
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < 1.0f) {
            m_moving = false;
            m_currentSpeed = 0.0f;
        } else {
            // Turn towards target
            float targetHeading = std::atan2(dy, dx) * 180.0f / 3.14159f;
            float headingDiff = targetHeading - m_heading;

            // Normalize to -180 to 180
            while (headingDiff > 180.0f) headingDiff -= 360.0f;
            while (headingDiff < -180.0f) headingDiff += 360.0f;

            // Turn rate based on tonnage
            float turnRate = 90.0f - m_chassis.tonnage * 0.5f;  // Heavier = slower turning
            turnRate = std::max(turnRate, 30.0f);

            if (std::abs(headingDiff) > turnRate * deltaTime) {
                m_heading += (headingDiff > 0 ? 1 : -1) * turnRate * deltaTime;
            } else {
                m_heading = targetHeading;
            }

            // Accelerate/decelerate
            float maxSpeed = static_cast<float>(m_chassis.maxSpeed);
            if (m_currentSpeed < maxSpeed) {
                m_currentSpeed = std::min(m_currentSpeed + maxSpeed * deltaTime, maxSpeed);
            }

            // Move forward
            float moveDistance = m_currentSpeed * deltaTime / 3.6f;  // kph to m/s
            float moveX = std::cos(m_heading * 3.14159f / 180.0f) * moveDistance;
            float moveY = std::sin(m_heading * 3.14159f / 180.0f) * moveDistance;

            m_x += moveX;
            m_y += moveY;
        }
    }
}

void Mech::moveTo(float x, float y) {
    m_targetX = x;
    m_targetY = y;
    m_moving = true;
}

void Mech::stop() {
    m_moving = false;
}

bool Mech::fireWeapon(int weaponIndex, float targetX, float targetY) {
    if (weaponIndex < 0 || weaponIndex >= static_cast<int>(m_weapons.size())) {
        return false;
    }

    auto& mounted = m_weapons[weaponIndex];
    if (!mounted.canFire()) {
        return false;
    }

    // Check range
    float dx = targetX - m_x;
    float dy = targetY - m_y;
    float range = std::sqrt(dx * dx + dy * dy);

    if (range < mounted.weapon->minRange || range > mounted.weapon->maxRange) {
        return false;
    }

    // Check heat
    if (m_heat + mounted.weapon->heat > m_maxHeat * 1.5f) {
        return false;  // Too hot, refuse to fire
    }

    // Fire!
    mounted.cooldownTimer = mounted.weapon->cooldown;

    if (mounted.weapon->ammoPerTon > 0) {
        mounted.ammo--;
    }

    m_heat += mounted.weapon->heat;

    return true;
}

void Mech::applyDamage(MechLocation location, int damage) {
    auto& component = m_components[static_cast<int>(location)];

    if (component.destroyed) {
        // Transfer to adjacent location
        if (location == MechLocation::LeftArm || location == MechLocation::RightArm) {
            location = (location == MechLocation::LeftArm) ?
                       MechLocation::LeftTorso : MechLocation::RightTorso;
        } else {
            location = MechLocation::CenterTorso;
        }
        applyDamage(location, damage);
        return;
    }

    // Apply to armor first
    if (component.armor > 0) {
        int armorDamage = std::min(component.armor, damage);
        component.armor -= armorDamage;
        damage -= armorDamage;
    }

    // Remaining damage goes to internal structure
    if (damage > 0 && component.internalStructure > 0) {
        component.internalStructure -= damage;

        if (component.internalStructure <= 0) {
            component.internalStructure = 0;
            component.destroyed = true;

            // Destroy weapons in this location
            for (auto& weapon : m_weapons) {
                if (weapon.location == location) {
                    weapon.destroyed = true;
                }
            }

            // Check for mech destruction
            checkDestruction();
        }
    }
}

void Mech::dissipateHeat(float deltaTime) {
    // Each heat sink dissipates 1 heat per second
    float dissipation = static_cast<float>(m_heatSinks) * deltaTime;
    m_heat = std::max(0.0f, m_heat - dissipation);

    // Can recover from shutdown if heat drops enough
    if (m_shutdown && m_heat < m_maxHeat * 0.5f) {
        m_shutdown = false;
        std::cout << m_callsign << " systems back online.\n";
    }
}

void Mech::checkDestruction() {
    // Mech is destroyed if center torso or head is destroyed
    if (m_components[static_cast<int>(MechLocation::CenterTorso)].destroyed ||
        m_components[static_cast<int>(MechLocation::Head)].destroyed) {
        m_destroyed = true;
        std::cout << m_callsign << " destroyed!\n";
        return;
    }

    // Also destroyed if both legs are gone
    if (m_components[static_cast<int>(MechLocation::LeftLeg)].destroyed &&
        m_components[static_cast<int>(MechLocation::RightLeg)].destroyed) {
        m_destroyed = true;
        std::cout << m_callsign << " crippled - both legs destroyed!\n";
    }
}

// MechDatabase implementation

MechDatabase& MechDatabase::instance() {
    static MechDatabase instance;
    return instance;
}

bool MechDatabase::loadFromFile(const std::string& path) {
    FitParser parser;
    if (!parser.parseFile(path)) {
        std::cerr << "MechDatabase: Failed to load: " << path << "\n";
        return false;
    }

    for (const auto& block : parser.getBlocks()) {
        MechChassis chassis;
        chassis.name = block.name;

        if (auto val = block.getString("Variant")) chassis.variant = *val;
        if (auto val = block.getInt("Tonnage")) chassis.tonnage = static_cast<int>(*val);
        if (auto val = block.getInt("MaxSpeed")) chassis.maxSpeed = static_cast<int>(*val);
        if (auto val = block.getInt("JumpJets")) chassis.jumpJets = static_cast<int>(*val);
        if (auto val = block.getInt("HeatSinks")) chassis.heatSinks = static_cast<int>(*val);

        // Armor values
        if (auto val = block.getInt("HeadArmor")) chassis.headArmor = static_cast<int>(*val);
        if (auto val = block.getInt("CenterTorsoArmor")) chassis.centerTorsoArmor = static_cast<int>(*val);
        if (auto val = block.getInt("SideTorsoArmor")) chassis.sideTorsoArmor = static_cast<int>(*val);
        if (auto val = block.getInt("ArmArmor")) chassis.armArmor = static_cast<int>(*val);
        if (auto val = block.getInt("LegArmor")) chassis.legArmor = static_cast<int>(*val);

        // Structure values
        if (auto val = block.getInt("HeadStructure")) chassis.headStructure = static_cast<int>(*val);
        if (auto val = block.getInt("CenterTorsoStructure")) chassis.centerTorsoStructure = static_cast<int>(*val);
        if (auto val = block.getInt("SideTorsoStructure")) chassis.sideTorsoStructure = static_cast<int>(*val);
        if (auto val = block.getInt("ArmStructure")) chassis.armStructure = static_cast<int>(*val);
        if (auto val = block.getInt("LegStructure")) chassis.legStructure = static_cast<int>(*val);

        m_chassis.push_back(chassis);
    }

    std::cout << "MechDatabase: Loaded " << m_chassis.size() << " chassis definitions\n";
    return true;
}

const MechChassis* MechDatabase::getChassis(const std::string& name) const {
    for (const auto& chassis : m_chassis) {
        if (chassis.name == name) {
            return &chassis;
        }
    }
    return nullptr;
}

std::vector<std::string> MechDatabase::getChassisNames() const {
    std::vector<std::string> names;
    names.reserve(m_chassis.size());
    for (const auto& chassis : m_chassis) {
        names.push_back(chassis.name);
    }
    return names;
}

} // namespace mcgng
