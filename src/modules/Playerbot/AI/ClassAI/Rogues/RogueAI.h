/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef ROGUE_AI_H
#define ROGUE_AI_H

#include "ClassAI.h"
#include "RogueSpecialization.h"
#include <memory>

namespace Playerbot
{

enum class RogueSpec : uint8
{
    ASSASSINATION = 0,
    COMBAT        = 1,
    SUBTLETY      = 2
};

class RogueAI : public ClassAI
{
public:
    explicit RogueAI(Player* bot);
    ~RogueAI() override = default;

    // Core AI Interface
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

    // Resource Management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

    // Specialization Management
    RogueSpec GetCurrentSpecialization() const;

private:
    void DetectSpecialization();
    void InitializeSpecialization();

    std::unique_ptr<RogueSpecialization> _specialization;
    RogueSpec _detectedSpec;
};

} // namespace Playerbot

#endif // ROGUE_AI_H