#ifndef MCGNG_MECH_H
#define MCGNG_MECH_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace mcgng {

/**
 * Mech component locations.
 */
enum class MechLocation {
    Head,
    CenterTorso,
    LeftTorso,
    RightTorso,
    LeftArm,
    RightArm,
    LeftLeg,
    RightLeg,
    Count
};

/**
 * Weapon types.
 */
enum class WeaponType {
    None,
    // Energy
    Laser,
    PulseLaser,
    LargeLaser,
    PPC,
    // Ballistic
    MachineGun,
    Autocannon,
    Gauss,
    // Missile
    SRM,
    LRM,
    Streak
};

/**
 * Weapon definition.
 */
struct Weapon {
    std::string name;
    WeaponType type = WeaponType::None;
    int damage = 0;
    int heat = 0;
    float minRange = 0.0f;
    float maxRange = 0.0f;
    float cooldown = 0.0f;        // Seconds between shots
    int ammoPerTon = 0;           // 0 = no ammo (energy weapon)
    float projectileSpeed = 0.0f; // For ballistic weapons
    int salvoSize = 1;            // Number of missiles per salvo
};

/**
 * Mounted weapon on a mech.
 */
struct MountedWeapon {
    const Weapon* weapon = nullptr;
    MechLocation location = MechLocation::CenterTorso;
    int ammo = 0;
    float cooldownTimer = 0.0f;
    bool destroyed = false;

    bool canFire() const {
        return weapon && !destroyed && cooldownTimer <= 0 &&
               (weapon->ammoPerTon == 0 || ammo > 0);
    }
};

/**
 * Mech component (body part).
 */
struct MechComponent {
    int armor = 0;
    int maxArmor = 0;
    int internalStructure = 0;
    int maxInternalStructure = 0;
    bool destroyed = false;

    float getDamageRatio() const {
        if (maxArmor + maxInternalStructure == 0) return 0.0f;
        return 1.0f - static_cast<float>(armor + internalStructure) /
                      static_cast<float>(maxArmor + maxInternalStructure);
    }
};

/**
 * Mech chassis definition (base stats).
 */
struct MechChassis {
    std::string name;
    std::string variant;
    int tonnage = 0;
    int maxSpeed = 0;           // In kph
    int jumpJets = 0;
    int heatSinks = 0;

    // Component max values
    int headArmor = 0;
    int centerTorsoArmor = 0;
    int sideTorsoArmor = 0;
    int armArmor = 0;
    int legArmor = 0;

    int headStructure = 0;
    int centerTorsoStructure = 0;
    int sideTorsoStructure = 0;
    int armStructure = 0;
    int legStructure = 0;

    // Hardpoints (weapon slots)
    int energyHardpoints[static_cast<int>(MechLocation::Count)] = {0};
    int ballisticHardpoints[static_cast<int>(MechLocation::Count)] = {0};
    int missileHardpoints[static_cast<int>(MechLocation::Count)] = {0};
};

/**
 * Mech unit in game.
 */
class Mech {
public:
    Mech() = default;
    ~Mech() = default;

    /**
     * Initialize mech from chassis definition.
     */
    bool initialize(const MechChassis& chassis);

    /**
     * Update mech state.
     */
    void update(float deltaTime);

    // Movement

    /**
     * Set target position for movement.
     */
    void moveTo(float x, float y);

    /**
     * Stop movement.
     */
    void stop();

    /**
     * Get current position.
     */
    float getX() const { return m_x; }
    float getY() const { return m_y; }

    /**
     * Get current heading (degrees).
     */
    float getHeading() const { return m_heading; }

    /**
     * Check if mech is moving.
     */
    bool isMoving() const { return m_moving; }

    // Combat

    /**
     * Fire a weapon at target position.
     * @param weaponIndex Index of mounted weapon
     * @param targetX Target X
     * @param targetY Target Y
     * @return true if weapon fired
     */
    bool fireWeapon(int weaponIndex, float targetX, float targetY);

    /**
     * Apply damage to mech.
     * @param location Where damage is applied
     * @param damage Amount of damage
     */
    void applyDamage(MechLocation location, int damage);

    /**
     * Get mounted weapons.
     */
    const std::vector<MountedWeapon>& getWeapons() const { return m_weapons; }

    // Heat management

    /**
     * Get current heat level.
     */
    float getHeat() const { return m_heat; }

    /**
     * Get maximum heat before shutdown.
     */
    float getMaxHeat() const { return m_maxHeat; }

    /**
     * Check if overheating.
     */
    bool isOverheated() const { return m_heat >= m_maxHeat; }

    // Status

    /**
     * Check if mech is destroyed.
     */
    bool isDestroyed() const { return m_destroyed; }

    /**
     * Get component status.
     */
    const MechComponent& getComponent(MechLocation loc) const {
        return m_components[static_cast<int>(loc)];
    }

    /**
     * Get chassis info.
     */
    const MechChassis& getChassis() const { return m_chassis; }

    // Identification
    void setName(const std::string& name) { m_name = name; }
    const std::string& getName() const { return m_name; }

    void setCallsign(const std::string& callsign) { m_callsign = callsign; }
    const std::string& getCallsign() const { return m_callsign; }

    void setTeam(int team) { m_team = team; }
    int getTeam() const { return m_team; }

private:
    std::string m_name;
    std::string m_callsign;
    MechChassis m_chassis;
    int m_team = 0;

    // Position and movement
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_heading = 0.0f;
    float m_targetX = 0.0f;
    float m_targetY = 0.0f;
    bool m_moving = false;
    float m_currentSpeed = 0.0f;

    // Components
    MechComponent m_components[static_cast<int>(MechLocation::Count)];

    // Weapons
    std::vector<MountedWeapon> m_weapons;

    // Heat
    float m_heat = 0.0f;
    float m_maxHeat = 30.0f;
    int m_heatSinks = 10;

    // Status
    bool m_destroyed = false;
    bool m_shutdown = false;

    void dissipateHeat(float deltaTime);
    void checkDestruction();
};

/**
 * Mech database for loading chassis definitions.
 */
class MechDatabase {
public:
    static MechDatabase& instance();

    /**
     * Load mech definitions from FIT files.
     */
    bool loadFromFile(const std::string& path);

    /**
     * Get chassis by name.
     */
    const MechChassis* getChassis(const std::string& name) const;

    /**
     * Get all chassis names.
     */
    std::vector<std::string> getChassisNames() const;

private:
    MechDatabase() = default;

    std::vector<MechChassis> m_chassis;
};

} // namespace mcgng

#endif // MCGNG_MECH_H
