/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#pragma once

#include "PriestSpecialization.h"
#include <map>
#include <set>

namespace Playerbot
{

class TC_GAME_API ShadowSpecialization : public PriestSpecialization
{
public:
    explicit ShadowSpecialization(Player* bot);

    // Core specialization interface
    void UpdateRotation(::Unit* target) override;
    void UpdateBuffs() override;
    void UpdateCooldowns(uint32 diff) override;
    bool CanUseAbility(uint32 spellId) override;

    // Combat callbacks
    void OnCombatStart(::Unit* target) override;
    void OnCombatEnd() override;

    // Resource management
    bool HasEnoughResource(uint32 spellId) override;
    void ConsumeResource(uint32 spellId) override;

    // Positioning
    Position GetOptimalPosition(::Unit* target) override;
    float GetOptimalRange(::Unit* target) override;

    // Healing interface (limited for Shadow)
    void UpdateHealing() override;
    bool ShouldHeal() override;
    ::Unit* GetBestHealTarget() override;
    void HealTarget(::Unit* target) override;

    // Role management
    PriestRole GetCurrentRole() override;
    void SetRole(PriestRole role) override;

    // Specialization info
    PriestSpec GetSpecialization() const override { return PriestSpec::SHADOW; }
    const char* GetSpecializationName() const override { return "Shadow"; }

    // Shadow spell IDs (public for external access)
    enum ShadowSpells
    {
        SHADOW_FORM = 15473,
        VOID_FORM = 194249,
        MIND_BLAST = 8092,
        SHADOW_WORD_PAIN = 589,
        VAMPIRIC_TOUCH = 34914,
        MIND_FLAY = 15407,
        SHADOW_WORD_DEATH = 32379,
        MIND_SPIKE = 73510,
        PSYCHIC_SCREAM = 8122,
        VOID_BOLT = 205448,
        MIND_SEAR = 48045,
        SHADOWFIEND = 34433,
        MIND_CONTROL = 605,
        DISPERSION = 47585,
        VAMPIRIC_EMBRACE = 15286,
        INSANITY = 129197,
        VOIDFORM_BUFF = 194249,
        LINGERING_INSANITY = 197937
    };

private:
    // Shadow-specific mechanics
    void UpdateShadowForm();
    void UpdateVoidForm();
    void UpdateInsanity();
    void UpdateShadowWordPain();
    void UpdateVampiricTouch();
    void UpdateMindFlay();
    bool ShouldEnterShadowForm();
    bool ShouldEnterVoidForm();
    bool ShouldCastMindBlast(::Unit* target);
    bool ShouldCastShadowWordPain(::Unit* target);
    bool ShouldCastVampiricTouch(::Unit* target);
    bool ShouldCastMindFlay(::Unit* target);
    bool ShouldCastShadowWordDeath(::Unit* target);

    // Shadow Form abilities
    void EnterShadowForm();
    void ExitShadowForm();
    void EnterVoidForm();
    void ManageVoidFormStacks();
    uint32 GetVoidFormStacks();

    // Insanity management
    void GenerateInsanity(uint32 amount);
    void ConsumeInsanity(uint32 amount);
    uint32 GetInsanity();
    uint32 GetMaxInsanity();
    float GetInsanityPercent();
    bool HasEnoughInsanity(uint32 amount);

    // DoT management
    void UpdateDoTs();
    void CastShadowWordPain(::Unit* target);
    void CastVampiricTouch(::Unit* target);
    void RefreshDoTs();
    bool HasShadowWordPain(::Unit* target);
    bool HasVampiricTouch(::Unit* target);
    uint32 GetShadowWordPainTimeRemaining(::Unit* target);
    uint32 GetVampiricTouchTimeRemaining(::Unit* target);

    // Core shadow spells
    void CastMindBlast(::Unit* target);
    void CastMindFlay(::Unit* target);
    void CastShadowWordDeath(::Unit* target);
    void CastMindSpike(::Unit* target);
    void CastPsychicScream();
    void CastVoidBolt(::Unit* target);

    // AoE and multi-target
    void UpdateMultiTarget();
    void HandleAoERotation();
    std::vector<::Unit*> GetNearbyEnemies(float range = 10.0f);
    bool ShouldUseAoE();
    void CastMindSear();
    void CastShadowWordPainAoE();

    // Cooldown management
    void UpdateShadowCooldowns();
    void UseShadowCooldowns();
    void CastShadowfiend();
    void CastMindControl(::Unit* target);
    void CastDispersion();
    void CastVampiricEmbrace(::Unit* target);

    // Void Form mechanics
    void UpdateVoidFormRotation(::Unit* target);
    void BuildInsanityForVoidForm();
    void OptimizeVoidFormUptime();
    bool IsInVoidForm();
    uint32 GetVoidFormTimeRemaining();

    // Shadow healing (Vampiric abilities)
    void UpdateVampiricHealing();
    void CastVampiricEmbrace();
    bool ShouldUseVampiricHealing();

    // Mana and resource optimization
    void OptimizeManaUsage();
    void RegenerateMana();
    bool ShouldConserveMana();

    // Target prioritization
    ::Unit* GetBestShadowTarget();
    ::Unit* GetBestDoTTarget();
    ::Unit* GetBestMindControlTarget();
    bool ShouldSwitchTargets();

    // State tracking
    PriestRole _currentRole;
    bool _inShadowForm;
    bool _inVoidForm;
    uint32 _voidFormStacks;
    uint32 _currentInsanity;
    uint32 _lastShadowformToggle;
    uint32 _voidFormStartTime;

    // DoT tracking per target
    std::map<uint64, uint32> _shadowWordPainTimers;
    std::map<uint64, uint32> _vampiricTouchTimers;

    // Cooldown tracking
    std::map<uint32, uint32> _cooldowns;

    // Multi-target tracking
    std::vector<uint64> _dotTargets;
    uint32 _lastTargetScan;

    // Performance optimization
    uint32 _lastDoTCheck;
    uint32 _lastInsanityCheck;
    uint32 _lastHealCheck;
    uint32 _lastMultiTargetCheck;
    uint32 _lastRotationUpdate;

    // Mind control tracking
    std::set<uint64> _mindControlTargets;
    uint32 _lastMindControl;

    // Emergency healing (limited in shadow form)
    std::priority_queue<Playerbot::HealTarget> _emergencyHealQueue;

    // Constants
    static constexpr uint32 SHADOW_WORD_PAIN_DURATION = 18000; // 18 seconds
    static constexpr uint32 VAMPIRIC_TOUCH_DURATION = 15000; // 15 seconds
    static constexpr uint32 VOID_FORM_BASE_DURATION = 25000; // 25 seconds base
    static constexpr uint32 MAX_INSANITY = 100;
    static constexpr uint32 VOID_FORM_INSANITY_COST = 65;
    static constexpr uint32 DOT_REFRESH_THRESHOLD = 3000; // 3 seconds remaining
    static constexpr float MULTI_TARGET_THRESHOLD = 3.0f; // Use AoE with 3+ targets
    static constexpr float VOID_FORM_ENTRY_THRESHOLD = 0.9f; // 90% insanity
    static constexpr float SHADOW_HEAL_THRESHOLD = 30.0f; // Emergency heal threshold
    static constexpr float MANA_CONSERVATION_THRESHOLD = 0.4f;
};

} // namespace Playerbot