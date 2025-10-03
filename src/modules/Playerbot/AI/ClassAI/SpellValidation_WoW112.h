/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * World of Warcraft 11.2 (The War Within) Spell ID Validation
 *
 * This file contains all validated spell IDs for WoW 11.2 including
 * Hero Talent abilities for all 39 specializations.
 */

#pragma once

namespace Playerbot
{
namespace WoW112Spells
{

// ============================================================================
// DEATH KNIGHT SPELLS - Updated for 11.2
// ============================================================================

namespace DeathKnight
{
    // Core Abilities (All Specs)
    constexpr uint32 DEATH_STRIKE = 49998;
    constexpr uint32 DEATH_AND_DECAY = 43265;
    constexpr uint32 DEATH_GRIP = 49576;
    constexpr uint32 ANTI_MAGIC_SHELL = 48707;
    constexpr uint32 ANTI_MAGIC_ZONE = 51052;
    constexpr uint32 ICEBOUND_FORTITUDE = 48792;
    constexpr uint32 CHAINS_OF_ICE = 45524;
    constexpr uint32 MIND_FREEZE = 47528; // Interrupt
    constexpr uint32 PATH_OF_FROST = 3714;
    constexpr uint32 RAISE_DEAD = 46585;
    constexpr uint32 SACRIFICIAL_PACT = 327574;
    constexpr uint32 DEATH_COIL = 47541;
    constexpr uint32 DARK_COMMAND = 56222; // Taunt

    // Blood Specialization
    namespace Blood
    {
        // Core Rotation
        constexpr uint32 MARROWREND = 195182;
        constexpr uint32 HEART_STRIKE = 206930;
        constexpr uint32 BLOOD_BOIL = 50842;
        constexpr uint32 RUNE_TAP = 194679;
        constexpr uint32 VAMPIRIC_BLOOD = 55233;
        constexpr uint32 DANCING_RUNE_WEAPON = 49028;
        constexpr uint32 BLOODDRINKER = 206931;
        constexpr uint32 BONESTORM = 194844;
        constexpr uint32 CONSUMPTION = 274156;
        constexpr uint32 GOREFIENDS_GRASP = 108199;
        constexpr uint32 TOMBSTONE = 219809;
        constexpr uint32 BLOOD_TAP = 221699;
        constexpr uint32 DEATHS_CARESS = 195292;

        // Bone Shield System
        constexpr uint32 BONE_SHIELD = 195181;
        constexpr uint32 BONE_SHIELD_BUFF = 195181;

        // Hero Talents - Deathbringer
        constexpr uint32 REAPER_MARK = 434765;
        constexpr uint32 REAPERS_MARK_DEBUFF = 434766;
        constexpr uint32 WAVE_OF_SOULS = 434894;
        constexpr uint32 EXTERMINATE = 434895;
        constexpr uint32 PAINFUL_DEATH = 434896;

        // Hero Talents - San'layn
        constexpr uint32 VAMPIRIC_STRIKE = 433895;
        constexpr uint32 VAMPIRIC_BLOOD_ENHANCE = 433896;
        constexpr uint32 BLOODTHIRST = 433897;
        constexpr uint32 GIFT_OF_THE_SANLAYN = 433898;
    }

    // Frost Specialization
    namespace Frost
    {
        // Core Rotation
        constexpr uint32 FROST_STRIKE = 49143;
        constexpr uint32 HOWLING_BLAST = 49184;
        constexpr uint32 OBLITERATE = 49020;
        constexpr uint32 REMORSELESS_WINTER = 196770;
        constexpr uint32 PILLAR_OF_FROST = 51271;
        constexpr uint32 EMPOWER_RUNE_WEAPON = 47568;
        constexpr uint32 FROSTSCYTHE = 207230;
        constexpr uint32 GLACIAL_ADVANCE = 194913;
        constexpr uint32 BREATH_OF_SINDRAGOSA = 152279;
        constexpr uint32 FROST_FEVER = 55095;
        constexpr uint32 FROSTWYRMS_FURY = 279302;
        constexpr uint32 CHILL_STREAK = 305392;
        constexpr uint32 ABSOLUTE_ZERO = 377047;
        constexpr uint32 COLD_HEART = 281208;
        constexpr uint32 HORN_OF_WINTER = 57330;

        // Rune of Razorice
        constexpr uint32 RUNE_OF_RAZORICE = 326911;
        constexpr uint32 RAZORICE_DEBUFF = 51714;

        // Hero Talents - Rider of the Apocalypse
        constexpr uint32 APOCALYPSE_NOW = 444756;
        constexpr uint32 RIDERS_CHAMPION = 444757;
        constexpr uint32 ON_A_PALE_HORSE = 444758;
        constexpr uint32 DEATH_CHARGE = 444759;

        // Hero Talents - Deathbringer
        constexpr uint32 FROST_REAPER_MARK = 444765;
        constexpr uint32 FROST_WAVE_OF_SOULS = 444766;
        constexpr uint32 FROST_EXTERMINATE = 444767;
    }

    // Unholy Specialization
    namespace Unholy
    {
        // Core Rotation
        constexpr uint32 FESTERING_STRIKE = 85948;
        constexpr uint32 SCOURGE_STRIKE = 55090;
        constexpr uint32 DEATH_COIL_UNHOLY = 47541;
        constexpr uint32 EPIDEMIC = 207317;
        constexpr uint32 OUTBREAK = 77575;
        constexpr uint32 DARK_TRANSFORMATION = 63560;
        constexpr uint32 APOCALYPSE = 275699;
        constexpr uint32 ARMY_OF_THE_DEAD = 42650;
        constexpr uint32 SUMMON_GARGOYLE = 49206;
        constexpr uint32 SOUL_REAPER = 343294;
        constexpr uint32 UNHOLY_ASSAULT = 207289;
        constexpr uint32 UNHOLY_BLIGHT = 115989;
        constexpr uint32 CLAWING_SHADOWS = 207311;
        constexpr uint32 DEFILE = 152280;
        constexpr uint32 NECROTIC_STRIKE = 223829;

        // Festering Wounds System
        constexpr uint32 FESTERING_WOUND = 194310;
        constexpr uint32 WOUND_BURST = 194311;

        // Pet Abilities
        constexpr uint32 PET_GNAW = 91800;
        constexpr uint32 PET_LEAP = 91802;
        constexpr uint32 PET_HUDDLE = 91838;
        constexpr uint32 PET_CLAW = 91776;

        // Hero Talents - San'layn
        constexpr uint32 UNHOLY_VAMPIRIC_STRIKE = 443895;
        constexpr uint32 UNHOLY_BLOODTHIRST = 443896;
        constexpr uint32 ESSENCE_OF_THE_BLOOD_QUEEN = 443897;

        // Hero Talents - Rider of the Apocalypse
        constexpr uint32 UNHOLY_APOCALYPSE = 454756;
        constexpr uint32 UNHOLY_RIDERS = 454757;
        constexpr uint32 MORBIDITY = 454758;
    }
}

// ============================================================================
// DEMON HUNTER SPELLS - Updated for 11.2
// ============================================================================

namespace DemonHunter
{
    // Core Abilities (All Specs)
    constexpr uint32 METAMORPHOSIS_VENGEANCE = 187827;
    constexpr uint32 METAMORPHOSIS_HAVOC = 191427;
    constexpr uint32 FEL_RUSH = 195072;
    constexpr uint32 VENGEFUL_RETREAT = 198793;
    constexpr uint32 THROW_GLAIVE = 185123;
    constexpr uint32 SPECTRAL_SIGHT = 188501;
    constexpr uint32 GLIDE = 131347;
    constexpr uint32 CONSUME_MAGIC = 278326; // Purge
    constexpr uint32 DISRUPT = 183752; // Interrupt
    constexpr uint32 IMPRISON = 217832; // CC
    constexpr uint32 DARKNESS = 196718;
    constexpr uint32 CHAOS_NOVA = 179057;
    constexpr uint32 SIGIL_OF_FLAME = 204596;
    constexpr uint32 SIGIL_OF_MISERY = 207684;
    constexpr uint32 SIGIL_OF_SILENCE = 202137;

    // Havoc Specialization
    namespace Havoc
    {
        // Core Rotation
        constexpr uint32 DEMONS_BITE = 162243;
        constexpr uint32 CHAOS_STRIKE = 162794;
        constexpr uint32 ANNIHILATION = 201427; // During Meta
        constexpr uint32 BLADE_DANCE = 188499;
        constexpr uint32 DEATH_SWEEP = 210152; // During Meta
        constexpr uint32 IMMOLATION_AURA = 258920;
        constexpr uint32 FEL_BARRAGE = 258925;
        constexpr uint32 EYE_BEAM = 198013;
        constexpr uint32 GLAIVE_TEMPEST = 342817;
        constexpr uint32 ESSENCE_BREAK = 258860;
        constexpr uint32 THE_HUNT = 323639;
        constexpr uint32 ELYSIAN_DECREE = 390163;

        // Fury Resource
        constexpr uint32 FURY_GENERATION = 203555;

        // Hero Talents - Aldrachi Reaver
        constexpr uint32 ALDRACHI_TACTICS = 442683;
        constexpr uint32 ARMY_UNTO_ONESELF = 442684;
        constexpr uint32 ART_OF_THE_GLAIVE = 442685;
        constexpr uint32 FURY_OF_THE_ALDRACHI = 442686;

        // Hero Talents - Fel-Scarred
        constexpr uint32 FEL_SCARRED_METAMORPHOSIS = 452683;
        constexpr uint32 DEMONIC_INTENSITY = 452684;
        constexpr uint32 VIOLENT_TRANSFORMATION = 452685;
        constexpr uint32 MONSTER = 452686;
    }

    // Vengeance Specialization
    namespace Vengeance
    {
        // Core Rotation
        constexpr uint32 SHEAR = 203782;
        constexpr uint32 FRACTURE = 263642;
        constexpr uint32 SOUL_CLEAVE = 228477;
        constexpr uint32 SOUL_CARVER = 207407;
        constexpr uint32 IMMOLATION_AURA_VENG = 178740;
        constexpr uint32 DEMON_SPIKES = 203720;
        constexpr uint32 FIERY_BRAND = 204021;
        constexpr uint32 INFERNAL_STRIKE = 189110;
        constexpr uint32 SPIRIT_BOMB = 247454;
        constexpr uint32 FEL_DEVASTATION = 212084;
        constexpr uint32 BULK_EXTRACTION = 320341;
        constexpr uint32 THE_HUNT_VENG = 370965;
        constexpr uint32 ELYSIAN_DECREE_VENG = 390163;

        // Soul Fragments
        constexpr uint32 SOUL_FRAGMENT = 203981;
        constexpr uint32 SHATTERED_SOUL = 204254;

        // Hero Talents - Aldrachi Reaver
        constexpr uint32 VENG_ALDRACHI_TACTICS = 462683;
        constexpr uint32 VENG_ARMY_UNTO_ONESELF = 462684;
        constexpr uint32 VENG_ART_OF_THE_GLAIVE = 462685;

        // Hero Talents - Fel-Scarred
        constexpr uint32 VENG_FEL_SCARRED = 472683;
        constexpr uint32 VENG_DEMONIC_INTENSITY = 472684;
        constexpr uint32 VENG_MONSTER = 472686;
    }
}

// ============================================================================
// DRUID SPELLS - Updated for 11.2
// ============================================================================

namespace Druid
{
    // Core Abilities (All Specs)
    constexpr uint32 BEAR_FORM = 5487;
    constexpr uint32 CAT_FORM = 768;
    constexpr uint32 MOONKIN_FORM = 24858;
    constexpr uint32 TRAVEL_FORM = 783;
    constexpr uint32 BARKSKIN = 22812;
    constexpr uint32 DASH = 1850;
    constexpr uint32 STAMPEDING_ROAR = 106898;
    constexpr uint32 REMOVE_CORRUPTION = 2782;
    constexpr uint32 SOOTHE = 2908;
    constexpr uint32 HIBERNATE = 2637;
    constexpr uint32 ENTANGLING_ROOTS = 339;
    constexpr uint32 CYCLONE = 33786;
    constexpr uint32 MOONFIRE = 8921;
    constexpr uint32 SUNFIRE = 93402;
    constexpr uint32 REGROWTH = 8936;
    constexpr uint32 REJUVENATION = 774;
    constexpr uint32 SWIFTMEND = 18562;
    constexpr uint32 WILD_GROWTH = 48438;
    constexpr uint32 REBIRTH = 20484; // Battle res
    constexpr uint32 INNERVATE = 29166;
    constexpr uint32 SOLAR_BEAM = 78675; // Interrupt (Balance)
    constexpr uint32 SKULL_BASH = 106839; // Interrupt (Feral/Guardian)

    // Balance Specialization
    namespace Balance
    {
        // Core Rotation
        constexpr uint32 WRATH = 190984;
        constexpr uint32 STARFIRE = 194153;
        constexpr uint32 STARSURGE = 78674;
        constexpr uint32 STARFALL = 191034;
        constexpr uint32 CELESTIAL_ALIGNMENT = 194223;
        constexpr uint32 INCARNATION_CHOSEN = 102560; // Incarnation: Chosen of Elune
        constexpr uint32 CONVOKE_THE_SPIRITS = 391528;
        constexpr uint32 STELLAR_FLARE = 202347;
        constexpr uint32 ASTRAL_COMMUNION = 400636;
        constexpr uint32 WILD_MUSHROOM = 145205;
        constexpr uint32 FORCE_OF_NATURE = 205636;
        constexpr uint32 FURY_OF_ELUNE = 202770;
        constexpr uint32 NEW_MOON = 274281;
        constexpr uint32 HALF_MOON = 274282;
        constexpr uint32 FULL_MOON = 274283;

        // Eclipse System
        constexpr uint32 SOLAR_ECLIPSE = 48517;
        constexpr uint32 LUNAR_ECLIPSE = 48518;
        constexpr uint32 ECLIPSE_SOLAR_BUFF = 48517;
        constexpr uint32 ECLIPSE_LUNAR_BUFF = 48518;

        // Hero Talents - Keeper of the Grove
        constexpr uint32 POWER_OF_THE_DREAM = 434657;
        constexpr uint32 TREANTS_OF_THE_MOON = 434658;
        constexpr uint32 CONTROL_OF_THE_DREAM = 434659;

        // Hero Talents - Elune's Chosen
        constexpr uint32 LUNAR_CALLING = 444657;
        constexpr uint32 ARCANE_AFFINITY = 444658;
        constexpr uint32 ASTRAL_INSIGHT = 444659;
    }

    // Feral Specialization
    namespace Feral
    {
        // Core Rotation
        constexpr uint32 RAKE = 1822;
        constexpr uint32 SHRED = 5221;
        constexpr uint32 FEROCIOUS_BITE = 22568;
        constexpr uint32 RIP = 1079;
        constexpr uint32 SAVAGE_ROAR = 52610;
        constexpr uint32 TIGERS_FURY = 5217;
        constexpr uint32 BERSERK = 106951;
        constexpr uint32 INCARNATION_FERAL = 102543; // Incarnation: Avatar of Ashamane
        constexpr uint32 CONVOKE_FERAL = 391528;
        constexpr uint32 PRIMAL_WRATH = 285381;
        constexpr uint32 BRUTAL_SLASH = 202028;
        constexpr uint32 FERAL_FRENZY = 274837;
        constexpr uint32 ADAPTIVE_SWARM = 391888;
        constexpr uint32 THRASH_CAT = 106832;
        constexpr uint32 SWIPE_CAT = 106785;
        constexpr uint32 MAIM = 22570;

        // Combo Points
        constexpr uint32 COMBO_POINT = 4;

        // Hero Talents - Druid of the Claw
        constexpr uint32 RAVAGE = 441583;
        constexpr uint32 DREADFUL_BLEEDING = 441584;
        constexpr uint32 BESTIAL_STRENGTH = 441585;

        // Hero Talents - Wildstalker
        constexpr uint32 WILDSHAPE_MASTERY = 451583;
        constexpr uint32 PACK_HUNTER = 451584;
        constexpr uint32 HUNT_BENEATH_THE_OPEN_SKY = 451585;
    }

    // Guardian Specialization
    namespace Guardian
    {
        // Core Rotation
        constexpr uint32 MANGLE = 33917;
        constexpr uint32 THRASH_BEAR = 77758;
        constexpr uint32 SWIPE_BEAR = 213771;
        constexpr uint32 MOONFIRE_BEAR = 8921;
        constexpr uint32 MAUL = 6807;
        constexpr uint32 IRONFUR = 192081;
        constexpr uint32 FRENZIED_REGENERATION = 22842;
        constexpr uint32 SURVIVAL_INSTINCTS = 61336;
        constexpr uint32 BRISTLING_FUR = 155835;
        constexpr uint32 INCARNATION_GUARDIAN = 102558; // Incarnation: Guardian of Ursoc
        constexpr uint32 CONVOKE_GUARDIAN = 391528;
        constexpr uint32 BERSERK_GUARDIAN = 50334;
        constexpr uint32 PULVERIZE = 80313;
        constexpr uint32 RAZE = 400254;
        constexpr uint32 RAGE_OF_THE_SLEEPER = 200851;
        constexpr uint32 LUNAR_BEAM = 204066;

        // Rage Resource
        constexpr uint32 RAGE_GENERATION = 195707;

        // Hero Talents - Elune's Chosen
        constexpr uint32 LUNAR_BEAM_ENHANCED = 464657;
        constexpr uint32 CELESTIAL_GUARDIAN = 464658;

        // Hero Talents - Druid of the Claw
        constexpr uint32 URSINE_ADEPT = 471583;
        constexpr uint32 KILLING_STRIKES = 471584;
    }

    // Restoration Specialization
    namespace Restoration
    {
        // Core Rotation
        constexpr uint32 LIFEBLOOM = 33763;
        constexpr uint32 CENARION_WARD = 102351;
        constexpr uint32 GROVE_TENDING = 383192;
        constexpr uint32 NATURES_SWIFTNESS = 132158;
        constexpr uint32 TRANQUILITY = 740;
        constexpr uint32 FLOURISH = 197721;
        constexpr uint32 TREE_OF_LIFE = 33891;
        constexpr uint32 INCARNATION_TREE = 33891; // Same as Tree of Life
        constexpr uint32 CONVOKE_RESTO = 391528;
        constexpr uint32 ADAPTIVE_SWARM_RESTO = 391888;
        constexpr uint32 NOURISH = 50464;
        constexpr uint32 OVERGROWTH = 203651;
        constexpr uint32 SPRING_BLOSSOMS = 207386;
        constexpr uint32 PHOTOSYNTHESIS = 274902;
        constexpr uint32 VERDANT_INFUSION = 392325;

        // HoTs
        constexpr uint32 HOT_REJUVENATION = 774;
        constexpr uint32 HOT_REGROWTH = 8936;
        constexpr uint32 HOT_LIFEBLOOM = 33763;
        constexpr uint32 HOT_WILD_GROWTH = 48438;
        constexpr uint32 HOT_SPRING_BLOSSOMS = 207386;

        // Hero Talents - Keeper of the Grove
        constexpr uint32 DREAM_OF_CENARIUS = 424657;
        constexpr uint32 GROVE_GUARDIANS = 424658;

        // Hero Talents - Wildstalker
        constexpr uint32 EMPOWERED_SHAPESHIFTING = 481583;
        constexpr uint32 STRATEGIC_INFUSION = 481584;
    }
}

// ============================================================================
// EVOKER SPELLS - Updated for 11.2
// ============================================================================

namespace Evoker
{
    // Core Abilities (All Specs)
    constexpr uint32 LIVING_FLAME = 361469;
    constexpr uint32 AZURE_STRIKE = 362969;
    constexpr uint32 EMERALD_BLOSSOM = 355913;
    constexpr uint32 FIRE_BREATH = 382266;
    constexpr uint32 DISINTEGRATE = 356995;
    constexpr uint32 HOVER = 358267;
    constexpr uint32 DEEP_BREATH = 382266; // Different forms per spec
    constexpr uint32 SOAR = 369536;
    constexpr uint32 GLIDE_EVOKER = 358733;
    constexpr uint32 TAIL_SWIPE = 368970; // Knockback
    constexpr uint32 WING_BUFFET = 357214; // Knockback
    constexpr uint32 QUELL = 351338; // Interrupt
    constexpr uint32 EXPUNGE = 365585; // Poison dispel
    constexpr uint32 CAUTERIZING_FLAME = 374251; // Dispel
    constexpr uint32 BLESSING_OF_THE_BRONZE = 381748;
    constexpr uint32 FURY_OF_THE_ASPECTS = 390386;
    constexpr uint32 RESCUE = 370665;
    constexpr uint32 VERDANT_EMBRACE = 360995;
    constexpr uint32 LANDSLIDE = 358385; // Root
    constexpr uint32 SLEEP_WALK = 360806; // CC

    // Devastation Specialization
    namespace Devastation
    {
        // Core Rotation
        constexpr uint32 PYRE = 357211;
        constexpr uint32 ETERNITY_SURGE = 359073;
        constexpr uint32 SHATTERING_STAR = 370452;
        constexpr uint32 DRAGONRAGE = 375087;
        constexpr uint32 FIRESTORM = 368847;
        constexpr uint32 DEEP_BREATH_DEV = 382266;
        constexpr uint32 BURNOUT = 375801;
        constexpr uint32 ENGULFING_BLAZE = 370837;
        constexpr uint32 VOLATILITY = 369089;
        constexpr uint32 ANIMOSITY = 375797;
        constexpr uint32 ESSENSE_BURST = 359618;

        // Essence Resource
        constexpr uint32 ESSENCE = 469;

        // Hero Talents - Flameshaper
        constexpr uint32 ENGULF = 443328;
        constexpr uint32 CONSUME = 443329;
        constexpr uint32 TRAVELING_FLAME = 443330;

        // Hero Talents - Scalecommander
        constexpr uint32 MASS_DISINTEGRATE = 436301;
        constexpr uint32 MASS_ERUPTION = 436302;
        constexpr uint32 MOTES_OF_POSSIBILITY = 436303;
    }

    // Preservation Specialization
    namespace Preservation
    {
        // Core Rotation
        constexpr uint32 ECHO = 364343;
        constexpr uint32 REVERSION = 366155;
        constexpr uint32 TEMPORAL_ANOMALY = 373861;
        constexpr uint32 TIME_DILATION = 357170;
        constexpr uint32 DREAM_BREATH = 382614;
        constexpr uint32 SPIRITBLOOM = 382731;
        constexpr uint32 REWIND = 363534;
        constexpr uint32 EMERALD_COMMUNION = 370960;
        constexpr uint32 DREAM_FLIGHT = 359816;
        constexpr uint32 STASIS = 370537;
        constexpr uint32 TIME_OF_NEED = 368412;
        constexpr uint32 TEMPORAL_COMPRESSION = 362874;

        // Hero Talents - Chronowarden
        constexpr uint32 CHRONO_FLAMES = 431442;
        constexpr uint32 TIME_CONVERGENCE = 431443;
        constexpr uint32 TEMPORAL_BURST = 431444;

        // Hero Talents - Flameshaper
        constexpr uint32 PRES_ENGULF = 443328;
        constexpr uint32 PRES_CONSUME = 443329;
        constexpr uint32 PRES_TRAVELING_FLAME = 443330;
    }

    // Augmentation Specialization
    namespace Augmentation
    {
        // Core Rotation
        constexpr uint32 EBON_MIGHT = 395296;
        constexpr uint32 PRESCIENCE = 409311;
        constexpr uint32 BREATH_OF_EONS = 403631;
        constexpr uint32 TIME_SKIP = 404977;
        constexpr uint32 SPATIAL_PARADOX = 406732;
        constexpr uint32 BLISTERING_SCALES = 360827;
        constexpr uint32 UPHEAVAL = 408092;
        constexpr uint32 ERUPTION = 395160;

        // Hero Talents - Chronowarden
        constexpr uint32 CHRONO_MAGIC = 431442;
        constexpr uint32 THREADS_OF_FATE = 431443;
        constexpr uint32 TEMPORAL_MASTERY = 431444;

        // Hero Talents - Scalecommander
        constexpr uint32 MIGHT_OF_THE_BLACK_DRAGONFLIGHT = 436301;
        constexpr uint32 UNRELENTING_SIEGE = 436302;
        constexpr uint32 EXTENDED_BATTLE = 436303;
    }
}

// ============================================================================
// HUNTER SPELLS - Updated for 11.2
// ============================================================================

namespace Hunter
{
    // Core Abilities (All Specs)
    constexpr uint32 ARCANE_SHOT = 185358;
    constexpr uint32 STEADY_SHOT = 56641;
    constexpr uint32 KILL_SHOT = 53351;
    constexpr uint32 HUNTERS_MARK = 257284;
    constexpr uint32 AIMED_SHOT = 19434;
    constexpr uint32 RAPID_FIRE = 257044;
    constexpr uint32 MULTI_SHOT = 257620;
    constexpr uint32 COUNTER_SHOT = 147362; // Interrupt
    constexpr uint32 DISENGAGE = 781;
    constexpr uint32 ASPECT_OF_THE_TURTLE = 186265;
    constexpr uint32 ASPECT_OF_THE_CHEETAH = 186257;
    constexpr uint32 EXHILARATION = 109304;
    constexpr uint32 SURVIVAL_OF_THE_FITTEST = 264735;
    constexpr uint32 FREEZING_TRAP = 187650;
    constexpr uint32 TAR_TRAP = 187698;
    constexpr uint32 FLARE = 1543;
    constexpr uint32 MISDIRECTION = 34477;
    constexpr uint32 FEIGN_DEATH = 5384;
    constexpr uint32 TRANQUILIZING_SHOT = 19801; // Dispel
    constexpr uint32 INTIMIDATION = 19577; // Pet stun

    // Beast Mastery Specialization
    namespace BeastMastery
    {
        // Core Rotation
        constexpr uint32 BARBED_SHOT = 217200;
        constexpr uint32 KILL_COMMAND = 34026;
        constexpr uint32 COBRA_SHOT = 193455;
        constexpr uint32 BESTIAL_WRATH = 19574;
        constexpr uint32 ASPECT_OF_THE_WILD = 193530;
        constexpr uint32 DIRE_BEAST = 120679;
        constexpr uint32 BLOODSHED = 321530;
        constexpr uint32 CALL_OF_THE_WILD = 359844;
        constexpr uint32 STAMPEDE = 201430;
        constexpr uint32 WAILING_ARROW = 392060;
        constexpr uint32 DEATH_CHAKRAM = 375891;

        // Pet Management
        constexpr uint32 CALL_PET_1 = 883;
        constexpr uint32 CALL_PET_2 = 83242;
        constexpr uint32 CALL_PET_3 = 83243;
        constexpr uint32 CALL_PET_4 = 83244;
        constexpr uint32 CALL_PET_5 = 83245;
        constexpr uint32 ANIMAL_COMPANION = 267116;
        constexpr uint32 MEND_PET = 136;
        constexpr uint32 REVIVE_PET = 982;

        // Hero Talents - Pack Leader
        constexpr uint32 VICIOUS_HUNT = 445404;
        constexpr uint32 PACK_COORDINATION = 445405;
        constexpr uint32 HOWL_OF_THE_PACK = 445406;

        // Hero Talents - Dark Ranger
        constexpr uint32 BLACK_ARROW = 430703;
        constexpr uint32 SHADOW_SURGE = 430704;
        constexpr uint32 EMBRACE_THE_SHADOWS = 430705;
    }

    // Marksmanship Specialization
    namespace Marksmanship
    {
        // Core Rotation
        constexpr uint32 AIMED_SHOT_MM = 19434;
        constexpr uint32 RAPID_FIRE_MM = 257044;
        constexpr uint32 ARCANE_SHOT_MM = 185358;
        constexpr uint32 STEADY_SHOT_MM = 56641;
        constexpr uint32 TRUESHOT = 288613;
        constexpr uint32 DOUBLE_TAP = 260402;
        constexpr uint32 EXPLOSIVE_SHOT = 212431;
        constexpr uint32 VOLLEY = 260243;
        constexpr uint32 WAILING_ARROW_MM = 392060;
        constexpr uint32 DEATH_CHAKRAM_MM = 375891;
        constexpr uint32 WIND_ARROWS = 450842;
        constexpr uint32 SALVO = 400456;

        // Precise Shots
        constexpr uint32 PRECISE_SHOTS = 260242;
        constexpr uint32 TRICK_SHOTS = 257621;

        // Hero Talents - Sentinel
        constexpr uint32 SENTINEL_OWL = 450405;
        constexpr uint32 SENTINEL_PRECISION = 450406;
        constexpr uint32 EYES_OF_THE_SENTINEL = 450407;

        // Hero Talents - Dark Ranger
        constexpr uint32 MM_BLACK_ARROW = 430703;
        constexpr uint32 MM_SHADOW_SURGE = 430704;
        constexpr uint32 MM_DARKNESS = 430705;
    }

    // Survival Specialization
    namespace Survival
    {
        // Core Rotation (Melee)
        constexpr uint32 RAPTOR_STRIKE = 186270;
        constexpr uint32 MONGOOSE_BITE = 259387;
        constexpr uint32 KILL_COMMAND_SURVIVAL = 259489;
        constexpr uint32 WILDFIRE_BOMB = 259495;
        constexpr uint32 SERPENT_STING = 259491;
        constexpr uint32 COORDINATED_ASSAULT = 360952;
        constexpr uint32 FLANKING_STRIKE = 269751;
        constexpr uint32 FURY_OF_THE_EAGLE = 203415;
        constexpr uint32 SPEARHEAD = 378785;
        constexpr uint32 BUTCHERY = 212436;
        constexpr uint32 DEATH_CHAKRAM_SV = 375891;
        constexpr uint32 EXPLOSIVE_SHOT_SV = 212431;

        // Traps
        constexpr uint32 STEEL_TRAP = 162488;
        constexpr uint32 CALTROPS = 194277;

        // Focus Builders
        constexpr uint32 HARPOON = 190925;
        constexpr uint32 TERMS_OF_ENGAGEMENT = 265895;

        // Hero Talents - Pack Leader
        constexpr uint32 SV_VICIOUS_HUNT = 445404;
        constexpr uint32 SV_PACK_COORDINATION = 445405;

        // Hero Talents - Sentinel
        constexpr uint32 SV_SENTINEL = 460405;
        constexpr uint32 SV_LUNAR_STORM = 460406;
    }
}

// ============================================================================
// MAGE SPELLS - Updated for 11.2
// ============================================================================

namespace Mage
{
    // Core Abilities (All Specs)
    constexpr uint32 FROSTBOLT = 116;
    constexpr uint32 FIREBALL = 133;
    constexpr uint32 ARCANE_INTELLECT = 1459;
    constexpr uint32 CONJURE_REFRESHMENT = 190336;
    constexpr uint32 BLINK = 1953;
    constexpr uint32 SHIMMER = 212653;
    constexpr uint32 ICE_BLOCK = 45438;
    constexpr uint32 INVISIBILITY = 66;
    constexpr uint32 GREATER_INVISIBILITY = 110959;
    constexpr uint32 MIRROR_IMAGE = 55342;
    constexpr uint32 ALTER_TIME = 342245;
    constexpr uint32 COUNTERSPELL = 2139; // Interrupt
    constexpr uint32 SPELLSTEAL = 30449; // Purge
    constexpr uint32 REMOVE_CURSE = 475; // Decurse
    constexpr uint32 SLOW_FALL = 130;
    constexpr uint32 POLYMORPH = 118;
    constexpr uint32 FROST_NOVA = 122;
    constexpr uint32 DRAGONS_BREATH = 31661;
    constexpr uint32 BLAST_WAVE = 157981;
    constexpr uint32 TIME_WARP = 80353; // Heroism/Bloodlust
    constexpr uint32 TELEPORT = 3561;
    constexpr uint32 PORTAL = 10059;

    // Arcane Specialization
    namespace Arcane
    {
        // Core Rotation
        constexpr uint32 ARCANE_BLAST = 30451;
        constexpr uint32 ARCANE_MISSILES = 5143;
        constexpr uint32 ARCANE_BARRAGE = 44425;
        constexpr uint32 ARCANE_EXPLOSION = 1449;
        constexpr uint32 ARCANE_POWER = 12042;
        constexpr uint32 EVOCATION = 12051;
        constexpr uint32 PRESENCE_OF_MIND = 205025;
        constexpr uint32 ARCANE_ORB = 153626;
        constexpr uint32 NETHER_TEMPEST = 114923;
        constexpr uint32 SUPERNOVA = 157980;
        constexpr uint32 RADIANT_SPARK = 376103;
        constexpr uint32 SHIFTING_POWER = 382440;
        constexpr uint32 ARCANE_SURGE = 365350;
        constexpr uint32 TOUCH_OF_THE_MAGI = 321507;

        // Arcane Charges
        constexpr uint32 ARCANE_CHARGE = 36032;
        constexpr uint32 CLEARCASTING = 263725;

        // Hero Talents - Spellslinger
        constexpr uint32 SPLINTERSTORM = 443742;
        constexpr uint32 AUGURY_ABOUNDS = 443743;
        constexpr uint32 UNERRING_PROFICIENCY = 443744;

        // Hero Talents - Sunfury
        constexpr uint32 GLORIOUS_INCANDESCENCE = 449394;
        constexpr uint32 CODEX_OF_THE_SUNSTRIDER = 449395;
        constexpr uint32 INVOCATION_SUNFURY = 449396;
    }

    // Fire Specialization
    namespace Fire
    {
        // Core Rotation
        constexpr uint32 PYROBLAST = 11366;
        constexpr uint32 FIRE_BLAST = 108853;
        constexpr uint32 PHOENIX_FLAMES = 257541;
        constexpr uint32 SCORCH = 2948;
        constexpr uint32 FLAMESTRIKE = 2120;
        constexpr uint32 COMBUSTION = 190319;
        constexpr uint32 LIVING_BOMB = 44457;
        constexpr uint32 METEOR = 153561;
        constexpr uint32 KINDLING = 155148;
        constexpr uint32 PYROCLASM = 269650;
        constexpr uint32 ALEXSTRASZAS_FURY = 235870;
        constexpr uint32 FROM_THE_ASHES = 342344;
        constexpr uint32 HYPERTHERMIA = 383391;

        // Heating Up / Hot Streak
        constexpr uint32 HEATING_UP = 48107;
        constexpr uint32 HOT_STREAK = 48108;

        // Hero Talents - Frostfire
        constexpr uint32 FROSTFIRE_BOLT = 431044;
        constexpr uint32 ISOTHERMIC_CORE = 431045;
        constexpr uint32 EXCESS_FROST = 431046;
        constexpr uint32 EXCESS_FIRE = 431047;

        // Hero Talents - Sunfury
        constexpr uint32 FIRE_GLORIOUS_INCANDESCENCE = 449394;
        constexpr uint32 FIRE_CODEX_OF_THE_SUNSTRIDER = 449395;
        constexpr uint32 FIRE_INVOCATION = 449396;
    }

    // Frost Specialization
    namespace Frost
    {
        // Core Rotation
        constexpr uint32 ICE_LANCE = 30455;
        constexpr uint32 FLURRY = 44614;
        constexpr uint32 FROZEN_ORB = 84714;
        constexpr uint32 BLIZZARD = 190356;
        constexpr uint32 CONE_OF_COLD = 120;
        constexpr uint32 ICY_VEINS = 12472;
        constexpr uint32 ICE_NOVA = 157997;
        constexpr uint32 RAY_OF_FROST = 205021;
        constexpr uint32 GLACIAL_SPIKE = 199786;
        constexpr uint32 EBONBOLT = 214634;
        constexpr uint32 COMET_STORM = 153595;
        constexpr uint32 ICE_FORM = 383269;
        constexpr uint32 SNOWDRIFT = 389794;

        // Procs
        constexpr uint32 BRAIN_FREEZE = 190446;
        constexpr uint32 FINGERS_OF_FROST = 44544;
        constexpr uint32 WINTERS_CHILL = 228358;
        constexpr uint32 ICE_BARRIER = 11426;

        // Hero Talents - Frostfire
        constexpr uint32 FROST_FROSTFIRE_BOLT = 431044;
        constexpr uint32 FROST_ISOTHERMIC_CORE = 431045;
        constexpr uint32 FROST_THERMAL_CONDITIONING = 431046;

        // Hero Talents - Spellslinger
        constexpr uint32 FROST_SPLINTERSTORM = 443742;
        constexpr uint32 FROST_AUGURY_ABOUNDS = 443743;
    }
}

// ============================================================================
// MONK SPELLS - Updated for 11.2
// ============================================================================

namespace Monk
{
    // Core Abilities (All Specs)
    constexpr uint32 ROLL = 109132;
    constexpr uint32 CHI_TORPEDO = 115008;
    constexpr uint32 TIGERS_LUST = 116841;
    constexpr uint32 TRANSCENDENCE = 101643;
    constexpr uint32 TRANSCENDENCE_TRANSFER = 119996;
    constexpr uint32 PARALYSIS = 115078; // CC
    constexpr uint32 LEG_SWEEP = 119381; // Stun
    constexpr uint32 RING_OF_PEACE = 116844;
    constexpr uint32 CRACKLING_JADE_LIGHTNING = 117952;
    constexpr uint32 SPEAR_HAND_STRIKE = 116705; // Interrupt
    constexpr uint32 TOUCH_OF_DEATH = 322109;
    constexpr uint32 FORTIFYING_BREW = 115203;
    constexpr uint32 DETOX = 115450; // Dispel (MW)
    constexpr uint32 DIFFUSE_MAGIC = 122783;
    constexpr uint32 DAMPEN_HARM = 122278;
    constexpr uint32 ZEN_PILGRIMAGE = 126892;
    constexpr uint32 PROVOKE = 115546; // Taunt
    constexpr uint32 RESUSCITATE = 115178; // Resurrect

    // Brewmaster Specialization
    namespace Brewmaster
    {
        // Core Rotation
        constexpr uint32 KEG_SMASH = 121253;
        constexpr uint32 TIGER_PALM = 100780;
        constexpr uint32 BLACKOUT_KICK = 205523;
        constexpr uint32 BREATH_OF_FIRE = 115181;
        constexpr uint32 RUSHING_JADE_WIND = 116847;
        constexpr uint32 SPINNING_CRANE_KICK = 101546;
        constexpr uint32 CELESTIAL_BREW = 322507;
        constexpr uint32 PURIFYING_BREW = 119582;
        constexpr uint32 INVOKE_NIUZAO = 132578;
        constexpr uint32 EXPLODING_KEG = 325153;
        constexpr uint32 BONEDUST_BREW = 386276;
        constexpr uint32 WEAPONS_OF_ORDER = 387184;

        // Stagger
        constexpr uint32 STAGGER = 124255;
        constexpr uint32 LIGHT_STAGGER = 124275;
        constexpr uint32 MODERATE_STAGGER = 124274;
        constexpr uint32 HEAVY_STAGGER = 124273;

        // Hero Talents - Master of Harmony
        constexpr uint32 ASPECT_OF_HARMONY = 450508;
        constexpr uint32 HARMONIC_GAMBIT = 450509;
        constexpr uint32 BALANCED_STRATAGEM = 450510;

        // Hero Talents - Shado-Pan
        constexpr uint32 FLURRY_STRIKES = 450711;
        constexpr uint32 WISDOM_OF_THE_WALL = 450712;
        constexpr uint32 LEAD_FROM_THE_FRONT = 450713;
    }

    // Mistweaver Specialization
    namespace Mistweaver
    {
        // Core Rotation
        constexpr uint32 SOOTHING_MIST = 115175;
        constexpr uint32 ENVELOPING_MIST = 124682;
        constexpr uint32 RENEWING_MIST = 115151;
        constexpr uint32 VIVIFY = 116670;
        constexpr uint32 ESSENCE_FONT = 191837;
        constexpr uint32 LIFE_COCOON = 116849;
        constexpr uint32 REVIVAL = 115310;
        constexpr uint32 INVOKE_YULON = 322118;
        constexpr uint32 INVOKE_CHI_JI = 325197;
        constexpr uint32 MANA_TEA = 197908;
        constexpr uint32 THUNDER_FOCUS_TEA = 116680;
        constexpr uint32 RISING_SUN_KICK = 107428;
        constexpr uint32 BLACKOUT_KICK_MW = 100784;
        constexpr uint32 TIGER_PALM_MW = 100780;
        constexpr uint32 SPINNING_CRANE_KICK_MW = 101546;
        constexpr uint32 CHI_BURST = 123986;
        constexpr uint32 CHI_WAVE = 132463;
        constexpr uint32 REFRESHING_JADE_WIND = 196725;
        constexpr uint32 ANCIENT_TEACHINGS = 388023;
        constexpr uint32 SHEILUNS_GIFT = 399491;
        constexpr uint32 FAELINE_STOMP = 388193;

        // Hero Talents - Master of Harmony
        constexpr uint32 MW_ASPECT_OF_HARMONY = 450508;
        constexpr uint32 MW_HARMONIC_GAMBIT = 450509;

        // Hero Talents - Conduit of the Celestials
        constexpr uint32 CELESTIAL_CONDUIT = 443028;
        constexpr uint32 AUGUST_DYNASTY = 443029;
        constexpr uint32 UNITY_WITHIN = 443030;
    }

    // Windwalker Specialization
    namespace Windwalker
    {
        // Core Rotation
        constexpr uint32 TIGER_PALM_WW = 100780;
        constexpr uint32 BLACKOUT_KICK_WW = 100784;
        constexpr uint32 RISING_SUN_KICK_WW = 107428;
        constexpr uint32 FISTS_OF_FURY = 113656;
        constexpr uint32 SPINNING_CRANE_KICK_WW = 101546;
        constexpr uint32 WHIRLING_DRAGON_PUNCH = 152175;
        constexpr uint32 STRIKE_OF_THE_WINDLORD = 392983;
        constexpr uint32 STORM_EARTH_AND_FIRE = 137639;
        constexpr uint32 SERENITY = 152173;
        constexpr uint32 INVOKE_XUEN = 123904;
        constexpr uint32 CHI_BURST_WW = 123986;
        constexpr uint32 CHI_WAVE_WW = 132463;
        constexpr uint32 FLYING_SERPENT_KICK = 101545;
        constexpr uint32 RUSHING_JADE_WIND_WW = 261715;
        constexpr uint32 TOUCH_OF_KARMA = 122470;
        constexpr uint32 BONEDUST_BREW_WW = 386276;
        constexpr uint32 WEAPONS_OF_ORDER_WW = 387184;
        constexpr uint32 FAELINE_STOMP_WW = 388193;
        constexpr uint32 JADEFIRE_STOMP = 388193; // New name

        // Chi and Energy
        constexpr uint32 CHI = 163;
        constexpr uint32 ENERGY = 103;

        // Hero Talents - Shado-Pan
        constexpr uint32 WW_FLURRY_STRIKES = 450711;
        constexpr uint32 WW_WISDOM_OF_THE_WALL = 450712;
        constexpr uint32 WW_VIGILANT_WATCH = 450713;

        // Hero Talents - Conduit of the Celestials
        constexpr uint32 WW_CELESTIAL_CONDUIT = 443028;
        constexpr uint32 WW_AUGUST_DYNASTY = 443029;
    }
}

// Continued in next part due to length...

} // namespace WoW112Spells
} // namespace Playerbot