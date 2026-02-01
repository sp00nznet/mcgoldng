#include "game/mission.h"
#include "assets/fit_parser.h"
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

namespace mcgng {

bool Mission::load(const std::string& path) {
    m_state = MissionState::Loading;

    FitParser parser;
    if (!parser.parseFile(path)) {
        std::cerr << "Mission: Failed to load: " << path << "\n";
        m_state = MissionState::NotLoaded;
        return false;
    }

    // Load mission info
    if (const auto* info = parser.findBlock("MissionInfo")) {
        if (auto val = info->getString("Name")) m_name = *val;
        if (auto val = info->getString("Description")) m_description = *val;
        if (auto val = info->getString("Briefing")) m_briefing = *val;
        if (auto val = info->getString("Debriefing")) m_debriefing = *val;
        if (auto val = info->getFloat("TimeLimit")) m_timeLimit = static_cast<float>(*val);
    }

    // Load objectives
    int objIndex = 0;
    while (const auto* obj = parser.findBlock("Objective" + std::to_string(objIndex))) {
        MissionObjective objective;
        objective.id = "obj_" + std::to_string(objIndex);

        if (auto val = obj->getString("Name")) objective.name = *val;
        if (auto val = obj->getString("Description")) objective.description = *val;
        if (auto val = obj->getBool("Primary")) objective.primary = *val;
        if (auto val = obj->getBool("Hidden")) objective.hidden = *val;
        if (auto val = obj->getInt("TargetCount")) objective.targetCount = static_cast<int>(*val);
        if (auto val = obj->getFloat("X")) objective.x = static_cast<float>(*val);
        if (auto val = obj->getFloat("Y")) objective.y = static_cast<float>(*val);
        if (auto val = obj->getFloat("Radius")) objective.radius = static_cast<float>(*val);
        if (auto val = obj->getFloat("TimeLimit")) objective.timeLimit = static_cast<float>(*val);

        // Parse objective type
        if (auto val = obj->getString("Type")) {
            std::string type = *val;
            if (type == "Destroy") objective.type = ObjectiveType::Destroy;
            else if (type == "DestroyAll") objective.type = ObjectiveType::DestroyAll;
            else if (type == "Capture") objective.type = ObjectiveType::Capture;
            else if (type == "Defend") objective.type = ObjectiveType::Defend;
            else if (type == "Escort") objective.type = ObjectiveType::Escort;
            else if (type == "Reach") objective.type = ObjectiveType::Reach;
            else if (type == "Survive") objective.type = ObjectiveType::Survive;
        }

        m_objectives.push_back(objective);
        objIndex++;
    }

    // Load spawn points
    int spawnIndex = 0;
    while (const auto* spawn = parser.findBlock("Spawn" + std::to_string(spawnIndex))) {
        SpawnPoint point;
        point.id = "spawn_" + std::to_string(spawnIndex);

        if (auto val = spawn->getFloat("X")) point.x = static_cast<float>(*val);
        if (auto val = spawn->getFloat("Y")) point.y = static_cast<float>(*val);
        if (auto val = spawn->getFloat("Heading")) point.heading = static_cast<float>(*val);
        if (auto val = spawn->getInt("Team")) point.team = static_cast<int>(*val);
        if (auto val = spawn->getString("MechType")) point.mechType = *val;
        if (auto val = spawn->getString("Pilot")) point.pilot = *val;

        m_spawnPoints.push_back(point);
        spawnIndex++;
    }

    // Load triggers
    int triggerIndex = 0;
    while (const auto* trig = parser.findBlock("Trigger" + std::to_string(triggerIndex))) {
        MissionTrigger trigger;
        trigger.id = "trigger_" + std::to_string(triggerIndex);

        if (auto val = trig->getString("TargetId")) trigger.targetId = *val;
        if (auto val = trig->getFloat("Delay")) trigger.delay = static_cast<float>(*val);
        if (auto val = trig->getString("Action")) trigger.action = *val;

        // Parse condition
        if (auto val = trig->getString("Condition")) {
            std::string cond = *val;
            if (cond == "OnStart") trigger.condition = TriggerCondition::OnStart;
            else if (cond == "OnTime") trigger.condition = TriggerCondition::OnTime;
            else if (cond == "OnUnitDestroyed") trigger.condition = TriggerCondition::OnUnitDestroyed;
            else if (cond == "OnAreaEntered") trigger.condition = TriggerCondition::OnAreaEntered;
            else if (cond == "OnObjectiveComplete") trigger.condition = TriggerCondition::OnObjectiveComplete;
        }

        m_triggers.push_back(trigger);
        triggerIndex++;
    }

    std::cout << "Mission: Loaded " << m_name << " (" << m_objectives.size()
              << " objectives, " << m_spawnPoints.size() << " spawns)\n";

    return true;
}

bool Mission::initialize() {
    // Spawn mechs
    auto& mechDb = MechDatabase::instance();

    for (const auto& spawn : m_spawnPoints) {
        const MechChassis* chassis = mechDb.getChassis(spawn.mechType);
        if (!chassis) {
            std::cerr << "Mission: Unknown mech type: " << spawn.mechType << "\n";
            continue;
        }

        auto mech = std::make_shared<Mech>();
        mech->initialize(*chassis);
        mech->setName(spawn.id);
        mech->setCallsign(spawn.pilot);
        mech->setTeam(spawn.team);

        // TODO: Set initial position via internal method
        // For now, mechs start at origin

        m_mechs.push_back(mech);
    }

    return true;
}

void Mission::update(float deltaTime) {
    if (m_state != MissionState::InProgress) {
        return;
    }

    m_elapsedTime += deltaTime;

    // Update all mechs
    for (auto& mech : m_mechs) {
        mech->update(deltaTime);
    }

    // Check triggers
    checkTriggers();

    // Check objectives
    checkObjectives();

    // Check for mission end conditions
    checkMissionEnd();
}

void Mission::start() {
    if (m_state == MissionState::Loading || m_state == MissionState::Briefing) {
        setState(MissionState::InProgress);
        m_elapsedTime = 0.0f;

        // Fire OnStart triggers
        for (auto& trigger : m_triggers) {
            if (trigger.condition == TriggerCondition::OnStart) {
                fireTrigger(trigger);
            }
        }
    }
}

void Mission::pause() {
    if (m_state == MissionState::InProgress) {
        setState(MissionState::Paused);
    }
}

void Mission::resume() {
    if (m_state == MissionState::Paused) {
        setState(MissionState::InProgress);
    }
}

void Mission::end(MissionState result) {
    if (result == MissionState::Success || result == MissionState::Failure ||
        result == MissionState::Aborted) {
        setState(result);
    }
}

std::vector<Mech*> Mission::getPlayerMechs() const {
    std::vector<Mech*> result;
    for (const auto& mech : m_mechs) {
        if (mech->getTeam() == 0 && !mech->isDestroyed()) {
            result.push_back(mech.get());
        }
    }
    return result;
}

std::vector<Mech*> Mission::getEnemyMechs() const {
    std::vector<Mech*> result;
    for (const auto& mech : m_mechs) {
        if (mech->getTeam() != 0 && !mech->isDestroyed()) {
            result.push_back(mech.get());
        }
    }
    return result;
}

Mech* Mission::findMech(const std::string& id) {
    for (auto& mech : m_mechs) {
        if (mech->getName() == id) {
            return mech.get();
        }
    }
    return nullptr;
}

void Mission::completeObjective(const std::string& id) {
    for (auto& obj : m_objectives) {
        if (obj.id == id && obj.status == ObjectiveStatus::Incomplete) {
            obj.status = ObjectiveStatus::Complete;
            if (m_onObjectiveComplete) {
                m_onObjectiveComplete(obj);
            }
            break;
        }
    }
}

void Mission::failObjective(const std::string& id) {
    for (auto& obj : m_objectives) {
        if (obj.id == id && obj.status == ObjectiveStatus::Incomplete) {
            obj.status = ObjectiveStatus::Failed;
            if (m_onObjectiveFail) {
                m_onObjectiveFail(obj);
            }
            break;
        }
    }
}

void Mission::updateObjectiveProgress(const std::string& id, int count) {
    for (auto& obj : m_objectives) {
        if (obj.id == id) {
            obj.currentCount = count;
            if (obj.currentCount >= obj.targetCount && obj.targetCount > 0) {
                completeObjective(id);
            }
            break;
        }
    }
}

void Mission::checkTriggers() {
    for (auto& trigger : m_triggers) {
        if (trigger.fired) continue;

        bool conditionMet = false;

        switch (trigger.condition) {
            case TriggerCondition::OnTime:
                conditionMet = m_elapsedTime >= trigger.delay;
                break;

            case TriggerCondition::OnUnitDestroyed:
                if (Mech* mech = findMech(trigger.targetId)) {
                    conditionMet = mech->isDestroyed();
                }
                break;

            case TriggerCondition::OnObjectiveComplete:
                for (const auto& obj : m_objectives) {
                    if (obj.id == trigger.targetId && obj.status == ObjectiveStatus::Complete) {
                        conditionMet = true;
                        break;
                    }
                }
                break;

            default:
                break;
        }

        if (conditionMet) {
            fireTrigger(trigger);
        }
    }
}

void Mission::checkObjectives() {
    for (auto& obj : m_objectives) {
        if (obj.status != ObjectiveStatus::Incomplete) continue;

        // Check time-based objectives
        if (obj.timeLimit > 0 && m_elapsedTime >= obj.timeLimit) {
            if (obj.type == ObjectiveType::Survive) {
                obj.status = ObjectiveStatus::Complete;
                if (m_onObjectiveComplete) {
                    m_onObjectiveComplete(obj);
                }
            } else {
                obj.status = ObjectiveStatus::Failed;
                if (m_onObjectiveFail) {
                    m_onObjectiveFail(obj);
                }
            }
        }

        // Check DestroyAll objective
        if (obj.type == ObjectiveType::DestroyAll) {
            bool allDestroyed = true;
            for (const auto& mech : m_mechs) {
                if (mech->getTeam() != 0 && !mech->isDestroyed()) {
                    allDestroyed = false;
                    break;
                }
            }
            if (allDestroyed) {
                completeObjective(obj.id);
            }
        }
    }
}

void Mission::checkMissionEnd() {
    // Check for mission failure - all player mechs destroyed
    bool anyPlayerAlive = false;
    for (const auto& mech : m_mechs) {
        if (mech->getTeam() == 0 && !mech->isDestroyed()) {
            anyPlayerAlive = true;
            break;
        }
    }

    if (!anyPlayerAlive) {
        end(MissionState::Failure);
        return;
    }

    // Check for primary objective failure
    for (const auto& obj : m_objectives) {
        if (obj.primary && obj.status == ObjectiveStatus::Failed) {
            end(MissionState::Failure);
            return;
        }
    }

    // Check for all primary objectives complete
    bool allPrimaryComplete = true;
    for (const auto& obj : m_objectives) {
        if (obj.primary && obj.status != ObjectiveStatus::Complete) {
            allPrimaryComplete = false;
            break;
        }
    }

    if (allPrimaryComplete) {
        end(MissionState::Success);
    }

    // Check time limit
    if (m_timeLimit > 0 && m_elapsedTime >= m_timeLimit) {
        end(MissionState::Failure);
    }
}

void Mission::fireTrigger(MissionTrigger& trigger) {
    trigger.fired = true;

    // TODO: Execute trigger action (ABL script or built-in action)
    std::cout << "Mission: Trigger fired: " << trigger.id << " -> " << trigger.action << "\n";
}

void Mission::setState(MissionState state) {
    if (m_state != state) {
        m_state = state;
        if (m_onStateChange) {
            m_onStateChange(state);
        }
    }
}

// MissionManager implementation

MissionManager& MissionManager::instance() {
    static MissionManager instance;
    return instance;
}

bool MissionManager::initialize(const std::string& assetsPath) {
    m_assetsPath = assetsPath;

    // Find available missions
    fs::path missionPath = fs::path(assetsPath) / "MISSION.FST";
    if (fs::exists(missionPath)) {
        // TODO: Scan mission files
        std::cout << "MissionManager: Found mission archive\n";
    }

    return true;
}

bool MissionManager::loadMission(const std::string& name) {
    m_currentMission = std::make_unique<Mission>();

    fs::path missionFile = fs::path(m_assetsPath) / "missions" / (name + ".fit");
    if (!m_currentMission->load(missionFile.string())) {
        m_currentMission.reset();
        return false;
    }

    if (!m_currentMission->initialize()) {
        m_currentMission.reset();
        return false;
    }

    return true;
}

std::vector<std::string> MissionManager::getAvailableMissions() const {
    return m_missionList;
}

bool MissionManager::saveProgress(const std::string& savePath) {
    // TODO: Implement save
    (void)savePath;
    return true;
}

bool MissionManager::loadProgress(const std::string& savePath) {
    // TODO: Implement load
    (void)savePath;
    return true;
}

} // namespace mcgng
