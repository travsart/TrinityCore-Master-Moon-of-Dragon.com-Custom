/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef TRINITY_HUNTERPLAYERAI_H
#define TRINITY_HUNTERPLAYERAI_H

#include "ClassAI.h"
#include <memory>

class Player;

namespace Playerbot
{

class HunterSpecialization;

enum class HunterSpec : uint8
{
    BEAST_MASTERY = 1,
    MARKSMANSHIP = 2,
    SURVIVAL = 3
};

class HunterAI : public ClassAI
{
public:
    explicit HunterAI(Player* bot);
    ~HunterAI() override = default;

    // Core AI interface
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

    // Hunter-specific methods
    HunterSpec GetCurrentSpecialization() const;

private:
    void DetectSpecialization();
    void InitializeSpecialization();

    std::unique_ptr<HunterSpecialization> _specialization;
    HunterSpec _detectedSpec;
};

} // namespace Playerbot

#endif