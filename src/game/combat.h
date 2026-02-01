#ifndef MCGNG_COMBAT_H
#define MCGNG_COMBAT_H

#include "game/mech.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <random>

namespace mcgng {

/**
 * Projectile in flight.
 */
struct Projectile {
    const Weapon* weapon = nullptr;
    int damage = 0;
    float x = 0, y = 0;
    float targetX = 0, targetY = 0;
    float speed = 0;
    Mech* source = nullptr;
    Mech* target = nullptr;
    float lifetime = 0;
    bool active = true;
};

/**
 * Combat event types.
 */
enum class CombatEventType {
    WeaponFired,
    Hit,
    Miss,
    CriticalHit,
    MechDestroyed,
    ComponentDestroyed,
    Overheat
};

/**
 * Combat event data.
 */
struct CombatEvent {
    CombatEventType type;
    Mech* attacker = nullptr;
    Mech* target = nullptr;
    const Weapon* weapon = nullptr;
    MechLocation hitLocation;
    int damage = 0;
    float x = 0, y = 0;
};

/**
 * Combat event callback.
 */
using CombatEventCallback = std::function<void(const CombatEvent&)>;

/**
 * Combat system - handles weapon fire, damage, and hit resolution.
 */
class CombatSystem {
public:
    static CombatSystem& instance();

    /**
     * Initialize the combat system.
     */
    bool initialize();

    /**
     * Update combat state (projectiles, etc).
     */
    void update(float deltaTime);

    /**
     * Process an attack from one mech to another.
     * @param attacker Attacking mech
     * @param weaponIndex Weapon to fire
     * @param target Target mech
     * @return true if attack was initiated
     */
    bool attack(Mech* attacker, int weaponIndex, Mech* target);

    /**
     * Process an attack at a ground position.
     */
    bool attackGround(Mech* attacker, int weaponIndex, float x, float y);

    /**
     * Calculate hit chance.
     * @param attacker Attacking mech
     * @param weapon Weapon being used
     * @param target Target mech
     * @return Hit probability (0.0 to 1.0)
     */
    float calculateHitChance(const Mech* attacker, const Weapon* weapon, const Mech* target) const;

    /**
     * Determine hit location on target.
     * Uses the standard hit location table.
     */
    MechLocation determineHitLocation(const Mech* target) const;

    /**
     * Check for critical hit.
     * @param target Target mech
     * @param location Hit location
     * @return true if critical hit occurred
     */
    bool checkCritical(Mech* target, MechLocation location);

    /**
     * Get active projectiles (for rendering).
     */
    const std::vector<Projectile>& getProjectiles() const { return m_projectiles; }

    /**
     * Register combat event callback.
     */
    void setEventCallback(CombatEventCallback callback) {
        m_eventCallback = std::move(callback);
    }

    /**
     * Set the mech list for collision detection.
     */
    void setMechList(std::vector<std::shared_ptr<Mech>>* mechs) {
        m_mechs = mechs;
    }

    // Combat modifiers

    /**
     * Set base hit chance modifier.
     */
    void setBaseHitChance(float chance) { m_baseHitChance = chance; }

    /**
     * Set critical hit chance.
     */
    void setCriticalChance(float chance) { m_criticalChance = chance; }

private:
    CombatSystem();

    std::vector<Projectile> m_projectiles;
    std::vector<std::shared_ptr<Mech>>* m_mechs = nullptr;

    CombatEventCallback m_eventCallback;

    // Combat parameters
    float m_baseHitChance = 0.7f;
    float m_rangeModifier = 0.1f;    // Per 100m penalty
    float m_movementModifier = 0.1f;
    float m_criticalChance = 0.1f;

    std::mt19937 m_rng;

    void fireEvent(const CombatEvent& event);
    void updateProjectile(Projectile& proj, float deltaTime);
    void resolveHit(Projectile& proj);
    float calculateRange(const Mech* a, const Mech* b) const;
    float calculateRange(const Mech* a, float x, float y) const;
};

/**
 * Weapon database.
 */
class WeaponDatabase {
public:
    static WeaponDatabase& instance();

    /**
     * Load weapon definitions.
     */
    bool loadFromFile(const std::string& path);

    /**
     * Get weapon by name.
     */
    const Weapon* getWeapon(const std::string& name) const;

    /**
     * Get all weapons of a type.
     */
    std::vector<const Weapon*> getWeaponsByType(WeaponType type) const;

private:
    WeaponDatabase();

    std::vector<Weapon> m_weapons;

    void loadDefaultWeapons();
};

} // namespace mcgng

#endif // MCGNG_COMBAT_H
