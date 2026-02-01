#ifndef MCGNG_MISSION_H
#define MCGNG_MISSION_H

#include "game/mech.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace mcgng {

/**
 * Mission objective status.
 */
enum class ObjectiveStatus {
    Incomplete,
    Complete,
    Failed
};

/**
 * Mission objective type.
 */
enum class ObjectiveType {
    Destroy,            // Destroy specific target(s)
    DestroyAll,         // Destroy all enemies
    Capture,            // Capture building/area
    Defend,             // Defend for time or until condition
    Escort,             // Escort unit to destination
    Reach,              // Reach specific location
    Survive,            // Survive for time period
    Custom              // Script-defined objective
};

/**
 * Mission objective.
 */
struct MissionObjective {
    std::string id;
    std::string name;
    std::string description;
    ObjectiveType type = ObjectiveType::Custom;
    ObjectiveStatus status = ObjectiveStatus::Incomplete;
    bool primary = true;        // Primary vs secondary objective
    bool hidden = false;        // Not shown until triggered
    int targetCount = 0;        // For destroy objectives
    int currentCount = 0;
    float timeLimit = 0.0f;     // For timed objectives
    float x = 0, y = 0;         // For location-based objectives
    float radius = 0;           // Area radius
};

/**
 * Mission spawn point.
 */
struct SpawnPoint {
    std::string id;
    float x = 0, y = 0;
    float heading = 0;
    int team = 0;
    std::string mechType;
    std::string pilot;
};

/**
 * Mission trigger condition.
 */
enum class TriggerCondition {
    OnStart,            // Mission start
    OnTime,             // After time elapsed
    OnUnitDestroyed,    // Specific unit destroyed
    OnAreaEntered,      // Unit enters area
    OnObjectiveComplete,// Objective completed
    OnCustom            // Script condition
};

/**
 * Mission trigger.
 */
struct MissionTrigger {
    std::string id;
    TriggerCondition condition = TriggerCondition::OnStart;
    std::string targetId;       // Unit/area/objective ID
    float delay = 0.0f;         // Delay after condition met
    bool fired = false;
    std::string action;         // Script action to execute
};

/**
 * Mission state.
 */
enum class MissionState {
    NotLoaded,
    Loading,
    Briefing,
    InProgress,
    Paused,
    Success,
    Failure,
    Aborted
};

/**
 * Mission class - represents a single game mission.
 */
class Mission {
public:
    Mission() = default;
    ~Mission() = default;

    /**
     * Load mission from file.
     * @param path Path to mission file
     * @return true on success
     */
    bool load(const std::string& path);

    /**
     * Initialize mission (spawn units, set up triggers).
     */
    bool initialize();

    /**
     * Update mission state.
     * @param deltaTime Time since last update
     */
    void update(float deltaTime);

    /**
     * Start the mission.
     */
    void start();

    /**
     * Pause the mission.
     */
    void pause();

    /**
     * Resume from pause.
     */
    void resume();

    /**
     * End the mission.
     */
    void end(MissionState result);

    // Getters

    MissionState getState() const { return m_state; }
    const std::string& getName() const { return m_name; }
    const std::string& getDescription() const { return m_description; }
    const std::string& getBriefing() const { return m_briefing; }
    float getElapsedTime() const { return m_elapsedTime; }

    const std::vector<MissionObjective>& getObjectives() const { return m_objectives; }
    const std::vector<std::shared_ptr<Mech>>& getMechs() const { return m_mechs; }

    /**
     * Get player mechs.
     */
    std::vector<Mech*> getPlayerMechs() const;

    /**
     * Get enemy mechs.
     */
    std::vector<Mech*> getEnemyMechs() const;

    /**
     * Find mech by ID.
     */
    Mech* findMech(const std::string& id);

    // Objective management

    /**
     * Complete an objective.
     */
    void completeObjective(const std::string& id);

    /**
     * Fail an objective.
     */
    void failObjective(const std::string& id);

    /**
     * Update objective progress.
     */
    void updateObjectiveProgress(const std::string& id, int count);

    // Events

    using StateChangeCallback = std::function<void(MissionState)>;
    using ObjectiveCallback = std::function<void(const MissionObjective&)>;

    void setOnStateChange(StateChangeCallback callback) {
        m_onStateChange = std::move(callback);
    }
    void setOnObjectiveComplete(ObjectiveCallback callback) {
        m_onObjectiveComplete = std::move(callback);
    }
    void setOnObjectiveFail(ObjectiveCallback callback) {
        m_onObjectiveFail = std::move(callback);
    }

private:
    MissionState m_state = MissionState::NotLoaded;

    std::string m_name;
    std::string m_description;
    std::string m_briefing;
    std::string m_debriefing;

    float m_elapsedTime = 0.0f;
    float m_timeLimit = 0.0f;  // 0 = no limit

    std::vector<MissionObjective> m_objectives;
    std::vector<SpawnPoint> m_spawnPoints;
    std::vector<MissionTrigger> m_triggers;
    std::vector<std::shared_ptr<Mech>> m_mechs;

    // Callbacks
    StateChangeCallback m_onStateChange;
    ObjectiveCallback m_onObjectiveComplete;
    ObjectiveCallback m_onObjectiveFail;

    void checkTriggers();
    void checkObjectives();
    void checkMissionEnd();
    void fireTrigger(MissionTrigger& trigger);
    void setState(MissionState state);
};

/**
 * Mission manager - handles loading and progression through campaign.
 */
class MissionManager {
public:
    static MissionManager& instance();

    /**
     * Initialize mission manager with assets path.
     */
    bool initialize(const std::string& assetsPath);

    /**
     * Load a mission by name.
     */
    bool loadMission(const std::string& name);

    /**
     * Get current mission.
     */
    Mission* getCurrentMission() { return m_currentMission.get(); }

    /**
     * Get list of available missions.
     */
    std::vector<std::string> getAvailableMissions() const;

    /**
     * Save mission progress.
     */
    bool saveProgress(const std::string& savePath);

    /**
     * Load mission progress.
     */
    bool loadProgress(const std::string& savePath);

private:
    MissionManager() = default;

    std::string m_assetsPath;
    std::unique_ptr<Mission> m_currentMission;
    std::vector<std::string> m_missionList;
    int m_currentMissionIndex = 0;
};

} // namespace mcgng

#endif // MCGNG_MISSION_H
