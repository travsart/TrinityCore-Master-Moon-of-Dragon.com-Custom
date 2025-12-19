/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * MULTI-STATION TRAVEL PLANNING SYSTEM - Implementation
 */

#include "TravelRouteManager.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "Log.h"
#include "GameTime.h"
#include "SpellHistory.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "ObjectMgr.h"
#include "Transport.h"
#include "TransportMgr.h"
#include "Map.h"
#include "WorldSession.h"
#include "../Game/FlightMasterManager.h"
#include <queue>
#include <algorithm>
#include <cmath>

namespace Playerbot
{

// Static member initialization
std::vector<TransportConnection> TravelRouteManager::s_transportConnections;
std::unordered_map<uint32, std::vector<size_t>> TravelRouteManager::s_connectionsByDepartureMap;
std::unordered_map<uint32, std::vector<size_t>> TravelRouteManager::s_connectionsByArrivalMap;
std::unordered_map<uint32, std::vector<std::pair<uint32, TransportConnection const*>>> TravelRouteManager::s_mapConnectivityGraph;
bool TravelRouteManager::s_connectionsInitialized = false;

// ============================================================================
// TRANSPORT CONNECTIONS DATABASE
// ============================================================================
// World of Warcraft 11.x transport connections (ships, zeppelins, portals)

namespace TransportDB
{
    // Map IDs - Classic
    constexpr uint32 MAP_EASTERN_KINGDOMS = 0;
    constexpr uint32 MAP_KALIMDOR = 1;

    // Map IDs - Expansions
    constexpr uint32 MAP_OUTLAND = 530;
    constexpr uint32 MAP_NORTHREND = 571;
    constexpr uint32 MAP_DEEPHOLM = 646;           // Cataclysm
    constexpr uint32 MAP_PANDARIA = 870;
    constexpr uint32 MAP_DRAENOR = 1116;
    constexpr uint32 MAP_BROKEN_ISLES = 1220;
    constexpr uint32 MAP_ARGUS = 1669;             // Legion 7.3
    constexpr uint32 MAP_ZANDALAR = 1642;
    constexpr uint32 MAP_KUL_TIRAS = 1643;
    constexpr uint32 MAP_DRAGON_ISLES = 2222;

    // Map IDs - Shadowlands
    constexpr uint32 MAP_SHADOWLANDS = 2222;       // Note: Shares ID with Dragon Isles in some contexts
    constexpr uint32 MAP_ORIBOS = 2364;            // Shadowlands hub city
    constexpr uint32 MAP_MALDRAXXUS = 2286;        // Shadowlands zone
    constexpr uint32 MAP_BASTION = 2287;           // Shadowlands zone
    constexpr uint32 MAP_ARDENWEALD = 2288;        // Shadowlands zone
    constexpr uint32 MAP_REVENDRETH = 2289;        // Shadowlands zone
    constexpr uint32 MAP_THE_MAW = 2290;           // Shadowlands endgame zone
    constexpr uint32 MAP_ZERETH_MORTIS = 2291;     // Shadowlands 9.2 zone

    // Map IDs - The War Within (TWW 11.x)
    constexpr uint32 MAP_KHAZ_ALGAR = 2552;        // Main TWW continent ID
    constexpr uint32 MAP_ISLE_OF_DORN = 2444;      // TWW surface zone (Isle of Dorn)
    constexpr uint32 MAP_RINGING_DEEPS = 2214;     // TWW underground zone 1
    constexpr uint32 MAP_HALLOWFALL = 2215;        // TWW underground zone 2
    constexpr uint32 MAP_AZJ_KAHET = 2255;         // TWW underground zone 3
    constexpr uint32 MAP_CITY_OF_THREADS = 2213;   // TWW Nerubian city
    constexpr uint32 MAP_DORNOGAL = 2339;          // TWW capital city instance
    constexpr uint32 MAP_SCENARIO_TWW = 2601;      // TWW scenario/instance map

    // Map IDs - Special locations
    constexpr uint32 MAP_CAVERNS_OF_TIME = 1;      // Tanaris (same as Kalimdor, different area)
    constexpr uint32 MAP_SILITHUS = 1;             // Same as Kalimdor
    constexpr uint32 MAP_EMERALD_DREAM = 2200;     // Dragonflight 10.2 zone

    // Transport wait times (in seconds)
    constexpr uint32 SHIP_WAIT_TIME = 180;      // 3 minutes average wait
    constexpr uint32 ZEPPELIN_WAIT_TIME = 180;  // 3 minutes average wait
    constexpr uint32 PORTAL_WAIT_TIME = 0;      // Instant

    // Transport travel times (in seconds)
    constexpr uint32 SHIP_TRAVEL_TIME = 60;     // 1 minute travel
    constexpr uint32 ZEPPELIN_TRAVEL_TIME = 60; // 1 minute travel
    constexpr uint32 PORTAL_TRAVEL_TIME = 5;    // Near instant

    /**
     * @brief Initialize all transport connections
     * This is the master database of transport routes in WoW
     */
    void InitializeConnections(std::vector<TransportConnection>& connections)
    {
        connections.clear();
        connections.reserve(64);
        uint32 nextId = 1;

        // ====================================================================
        // EASTERN KINGDOMS <-> KALIMDOR CONNECTIONS
        // ====================================================================

        // Booty Bay <-> Ratchet (Neutral ship)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::SHIP;
            conn.name = "Booty Bay to Ratchet Ship";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-14281.0f, 556.0f, 8.9f);  // Booty Bay dock
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(-994.0f, -3827.0f, 5.5f);    // Ratchet dock
            conn.transportEntry = 20808;  // The Maiden's Fancy
            conn.waitTimeSeconds = SHIP_WAIT_TIME;
            conn.travelTimeSeconds = SHIP_TRAVEL_TIME;
            conn.allianceOnly = false;
            conn.hordeOnly = false;
            connections.push_back(conn);

            // Reverse connection
            conn.connectionId = nextId++;
            conn.name = "Ratchet to Booty Bay Ship";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // Menethil Harbor <-> Theramore (Alliance ship)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::SHIP;
            conn.name = "Menethil Harbor to Theramore Ship";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-3670.0f, -609.0f, 5.4f);  // Menethil dock
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(-3838.0f, -4527.0f, 8.7f);   // Theramore dock
            conn.transportEntry = 176231; // The Lady Mehley
            conn.waitTimeSeconds = SHIP_WAIT_TIME;
            conn.travelTimeSeconds = SHIP_TRAVEL_TIME;
            conn.allianceOnly = true;
            conn.hordeOnly = false;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Theramore to Menethil Harbor Ship";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // Stormwind Harbor <-> Auberdine/Rut'theran (Alliance ship)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::SHIP;
            conn.name = "Stormwind to Rut'theran Ship";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8650.0f, 1345.0f, 5.2f);  // Stormwind Harbor
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(8181.0f, 1005.0f, 0.2f);     // Rut'theran Village
            conn.transportEntry = 181646; // Alliance ship
            conn.waitTimeSeconds = SHIP_WAIT_TIME;
            conn.travelTimeSeconds = SHIP_TRAVEL_TIME;
            conn.allianceOnly = true;
            conn.hordeOnly = false;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Rut'theran to Stormwind Ship";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // Undercity <-> Orgrimmar Zeppelin (Horde)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::ZEPPELIN;
            conn.name = "Undercity to Orgrimmar Zeppelin";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(2066.0f, 285.0f, 97.0f);   // Undercity zeppelin tower
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1177.0f, -4291.0f, 21.3f);   // Orgrimmar zeppelin tower
            conn.transportEntry = 186238; // Horde zeppelin
            conn.waitTimeSeconds = ZEPPELIN_WAIT_TIME;
            conn.travelTimeSeconds = ZEPPELIN_TRAVEL_TIME;
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Orgrimmar to Undercity Zeppelin";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // Stranglethorn <-> Orgrimmar Zeppelin (Horde)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::ZEPPELIN;
            conn.name = "Grom'gol to Orgrimmar Zeppelin";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-12415.0f, 208.0f, 31.5f); // Grom'gol zeppelin
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1177.0f, -4291.0f, 21.3f);   // Orgrimmar
            conn.transportEntry = 186238;
            conn.waitTimeSeconds = ZEPPELIN_WAIT_TIME;
            conn.travelTimeSeconds = ZEPPELIN_TRAVEL_TIME;
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Orgrimmar to Grom'gol Zeppelin";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // Grom'gol <-> Undercity Zeppelin (Horde)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::ZEPPELIN;
            conn.name = "Grom'gol to Undercity Zeppelin";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-12415.0f, 208.0f, 31.5f); // Grom'gol
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;  // Same map!
            conn.arrivalPosition.Relocate(2066.0f, 285.0f, 97.0f);     // Undercity
            conn.transportEntry = 186238;
            conn.waitTimeSeconds = ZEPPELIN_WAIT_TIME;
            conn.travelTimeSeconds = ZEPPELIN_TRAVEL_TIME;
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Undercity to Grom'gol Zeppelin";
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // ====================================================================
        // OUTLAND CONNECTIONS
        // ====================================================================

        // Dark Portal (Eastern Kingdoms <-> Outland)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Dark Portal (Azeroth to Outland)";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-11903.0f, -3206.0f, -14.9f); // Blasted Lands
            conn.arrivalMapId = MAP_OUTLAND;
            conn.arrivalPosition.Relocate(-248.0f, 934.0f, 84.4f);        // Hellfire Peninsula
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.requiresLevel = true;
            conn.minLevel = 58;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Dark Portal (Outland to Azeroth)";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // Shattrath Portals to capital cities
        {
            // Shattrath -> Stormwind
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Shattrath to Stormwind Portal";
            conn.departureMapId = MAP_OUTLAND;
            conn.departurePosition.Relocate(-1889.0f, 5395.0f, -12.4f); // Shattrath
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);     // Stormwind
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            connections.push_back(conn);

            // Shattrath -> Orgrimmar
            conn.connectionId = nextId++;
            conn.name = "Shattrath to Orgrimmar Portal";
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1676.0f, -4315.0f, 61.2f);    // Orgrimmar
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // ====================================================================
        // NORTHREND CONNECTIONS
        // ====================================================================

        // Stormwind -> Borean Tundra (Alliance ship)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::SHIP;
            conn.name = "Stormwind to Valiance Keep Ship";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8650.0f, 1345.0f, 5.2f);  // Stormwind Harbor
            conn.arrivalMapId = MAP_NORTHREND;
            conn.arrivalPosition.Relocate(2236.0f, 5140.0f, 5.3f);     // Valiance Keep
            conn.waitTimeSeconds = SHIP_WAIT_TIME;
            conn.travelTimeSeconds = SHIP_TRAVEL_TIME * 2; // Longer journey
            conn.allianceOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 68;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Valiance Keep to Stormwind Ship";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // Orgrimmar -> Borean Tundra (Horde zeppelin)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::ZEPPELIN;
            conn.name = "Orgrimmar to Warsong Hold Zeppelin";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(1177.0f, -4291.0f, 21.3f); // Orgrimmar
            conn.arrivalMapId = MAP_NORTHREND;
            conn.arrivalPosition.Relocate(2836.0f, 6180.0f, 104.0f);   // Warsong Hold
            conn.waitTimeSeconds = ZEPPELIN_WAIT_TIME;
            conn.travelTimeSeconds = ZEPPELIN_TRAVEL_TIME * 2;
            conn.hordeOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 68;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Warsong Hold to Orgrimmar Zeppelin";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // Menethil -> Howling Fjord (Alliance ship)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::SHIP;
            conn.name = "Menethil to Valgarde Ship";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-3670.0f, -609.0f, 5.4f);  // Menethil
            conn.arrivalMapId = MAP_NORTHREND;
            conn.arrivalPosition.Relocate(588.0f, -5095.0f, 1.6f);     // Valgarde
            conn.waitTimeSeconds = SHIP_WAIT_TIME;
            conn.travelTimeSeconds = SHIP_TRAVEL_TIME * 2;
            conn.allianceOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 68;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Valgarde to Menethil Ship";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // Undercity -> Howling Fjord (Horde zeppelin)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::ZEPPELIN;
            conn.name = "Undercity to Vengeance Landing Zeppelin";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(2066.0f, 285.0f, 97.0f);   // Undercity
            conn.arrivalMapId = MAP_NORTHREND;
            conn.arrivalPosition.Relocate(1974.0f, -6081.0f, 67.0f);   // Vengeance Landing
            conn.waitTimeSeconds = ZEPPELIN_WAIT_TIME;
            conn.travelTimeSeconds = ZEPPELIN_TRAVEL_TIME * 2;
            conn.hordeOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 68;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Vengeance Landing to Undercity Zeppelin";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // Dalaran portals (Northrend)
        {
            // Dalaran -> Stormwind
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Dalaran to Stormwind Portal";
            conn.departureMapId = MAP_NORTHREND;
            conn.departurePosition.Relocate(5719.0f, 719.0f, 641.7f);  // Dalaran
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);    // Stormwind
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            connections.push_back(conn);

            // Dalaran -> Orgrimmar
            conn.connectionId = nextId++;
            conn.name = "Dalaran to Orgrimmar Portal";
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1676.0f, -4315.0f, 61.2f);   // Orgrimmar
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // ====================================================================
        // MODERN EXPANSION CONNECTIONS (Pandaria, Draenor, etc.)
        // ====================================================================

        // Stormwind/Orgrimmar -> Jade Forest (Pandaria)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Stormwind to Jade Forest Portal";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8838.0f, 626.0f, 94.0f);  // Stormwind
            conn.arrivalMapId = MAP_PANDARIA;
            conn.arrivalPosition.Relocate(942.0f, -569.0f, 184.0f);    // Jade Forest
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 85;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Orgrimmar to Jade Forest Portal";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(1676.0f, -4315.0f, 61.2f); // Orgrimmar
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // Stormwind/Orgrimmar -> Draenor
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Stormwind to Draenor Portal";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8838.0f, 626.0f, 94.0f);  // Stormwind
            conn.arrivalMapId = MAP_DRAENOR;
            conn.arrivalPosition.Relocate(2068.0f, 196.0f, 87.0f);     // Shadowmoon Valley
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 90;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Orgrimmar to Draenor Portal";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(1676.0f, -4315.0f, 61.2f); // Orgrimmar
            conn.arrivalPosition.Relocate(5579.0f, 4571.0f, 133.0f);   // Frostfire Ridge
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // Capital city -> Broken Isles (Legion)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Stormwind to Dalaran (Broken Isles) Portal";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8838.0f, 626.0f, 94.0f);
            conn.arrivalMapId = MAP_BROKEN_ISLES;
            conn.arrivalPosition.Relocate(-853.0f, 4491.0f, 729.0f);   // Dalaran
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.requiresLevel = true;
            conn.minLevel = 98;
            connections.push_back(conn);
        }

        // Capital city -> Zandalar/Kul Tiras (BFA)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Stormwind to Boralus Portal";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8838.0f, 626.0f, 94.0f);
            conn.arrivalMapId = MAP_KUL_TIRAS;
            conn.arrivalPosition.Relocate(-1774.0f, -1580.0f, 0.3f);   // Boralus
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 110;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Orgrimmar to Dazar'alor Portal";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(1676.0f, -4315.0f, 61.2f);
            conn.arrivalMapId = MAP_ZANDALAR;
            conn.arrivalPosition.Relocate(-1015.0f, 805.0f, 440.0f);   // Dazar'alor
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // Capital city -> Dragon Isles (Dragonflight)
        {
            // Stormwind -> Valdrakken (Alliance)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Stormwind to Valdrakken Portal";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8838.0f, 626.0f, 94.0f);
            conn.arrivalMapId = MAP_DRAGON_ISLES;
            conn.arrivalPosition.Relocate(-2512.0f, -376.0f, 201.0f);  // Valdrakken
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 60;
            connections.push_back(conn);

            // Orgrimmar -> Valdrakken (Horde)
            conn.connectionId = nextId++;
            conn.name = "Orgrimmar to Valdrakken Portal";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(1676.0f, -4315.0f, 61.2f);  // Orgrimmar
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);

            // Valdrakken -> Stormwind (Alliance) - REVERSE
            conn.connectionId = nextId++;
            conn.name = "Valdrakken to Stormwind Portal";
            conn.departureMapId = MAP_DRAGON_ISLES;
            conn.departurePosition.Relocate(-2512.0f, -376.0f, 201.0f);  // Valdrakken portal room
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);      // Stormwind
            conn.allianceOnly = true;
            conn.hordeOnly = false;
            connections.push_back(conn);

            // Valdrakken -> Orgrimmar (Horde) - REVERSE
            conn.connectionId = nextId++;
            conn.name = "Valdrakken to Orgrimmar Portal";
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1676.0f, -4315.0f, 61.2f);     // Orgrimmar
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // Capital city <-> Khaz Algar (The War Within)
        {
            // Stormwind -> Dornogal (Alliance)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Stormwind to Dornogal Portal";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8838.0f, 626.0f, 94.0f);
            conn.arrivalMapId = MAP_KHAZ_ALGAR;
            conn.arrivalPosition.Relocate(1287.0f, -2252.0f, 176.0f);  // Dornogal
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 70;
            connections.push_back(conn);

            // Orgrimmar -> Dornogal (Horde)
            conn.connectionId = nextId++;
            conn.name = "Orgrimmar to Dornogal Portal";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(1676.0f, -4315.0f, 61.2f);  // Orgrimmar
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);

            // Dornogal -> Stormwind (Alliance) - REVERSE
            conn.connectionId = nextId++;
            conn.name = "Dornogal to Stormwind Portal";
            conn.departureMapId = MAP_KHAZ_ALGAR;
            conn.departurePosition.Relocate(1287.0f, -2252.0f, 176.0f);  // Dornogal portal room
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);      // Stormwind
            conn.allianceOnly = true;
            conn.hordeOnly = false;
            connections.push_back(conn);

            // Dornogal -> Orgrimmar (Horde) - REVERSE
            conn.connectionId = nextId++;
            conn.name = "Dornogal to Orgrimmar Portal";
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1676.0f, -4315.0f, 61.2f);     // Orgrimmar
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // ====================================================================
        // REVERSE ROUTES FROM EXPANSION ZONES (Critical for returning home)
        // ====================================================================

        // Broken Isles (Legion Dalaran) -> Capitals
        {
            // Dalaran (Legion) -> Stormwind (Alliance)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Dalaran (Legion) to Stormwind Portal";
            conn.departureMapId = MAP_BROKEN_ISLES;
            conn.departurePosition.Relocate(-853.0f, 4491.0f, 729.0f);  // Dalaran portal room
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);     // Stormwind
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            connections.push_back(conn);

            // Dalaran (Legion) -> Orgrimmar (Horde)
            conn.connectionId = nextId++;
            conn.name = "Dalaran (Legion) to Orgrimmar Portal";
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1676.0f, -4315.0f, 61.2f);    // Orgrimmar
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // Kul Tiras / Zandalar (BFA) -> Capitals
        {
            // Boralus -> Stormwind (Alliance)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Boralus to Stormwind Portal";
            conn.departureMapId = MAP_KUL_TIRAS;
            conn.departurePosition.Relocate(-1774.0f, -1580.0f, 0.3f);  // Boralus portal room
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);     // Stormwind
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            connections.push_back(conn);

            // Dazar'alor -> Orgrimmar (Horde)
            conn.connectionId = nextId++;
            conn.name = "Dazar'alor to Orgrimmar Portal";
            conn.departureMapId = MAP_ZANDALAR;
            conn.departurePosition.Relocate(-1015.0f, 805.0f, 440.0f);  // Dazar'alor portal room
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1676.0f, -4315.0f, 61.2f);    // Orgrimmar
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // Pandaria (Shrine) -> Capitals
        {
            // Shrine of Seven Stars -> Stormwind (Alliance)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Shrine of Seven Stars to Stormwind Portal";
            conn.departureMapId = MAP_PANDARIA;
            conn.departurePosition.Relocate(942.0f, 249.0f, 520.0f);    // Shrine portal room
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);     // Stormwind
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            connections.push_back(conn);

            // Shrine of Two Moons -> Orgrimmar (Horde)
            conn.connectionId = nextId++;
            conn.name = "Shrine of Two Moons to Orgrimmar Portal";
            conn.departurePosition.Relocate(1641.0f, 931.0f, 471.0f);   // Shrine portal room
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1676.0f, -4315.0f, 61.2f);    // Orgrimmar
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);

            // Pandaria -> Jade Forest (reverse of capital->pandaria)
            conn.connectionId = nextId++;
            conn.name = "Jade Forest to Stormwind Portal";
            conn.departurePosition.Relocate(942.0f, -569.0f, 184.0f);   // Jade Forest
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);
            conn.allianceOnly = true;
            conn.hordeOnly = false;
            connections.push_back(conn);
        }

        // Draenor (Garrison/Ashran) -> Capitals
        {
            // Alliance Garrison -> Stormwind
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Alliance Garrison to Stormwind Portal";
            conn.departureMapId = MAP_DRAENOR;
            conn.departurePosition.Relocate(2068.0f, 196.0f, 87.0f);    // Shadowmoon garrison
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);     // Stormwind
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            connections.push_back(conn);

            // Horde Garrison -> Orgrimmar
            conn.connectionId = nextId++;
            conn.name = "Horde Garrison to Orgrimmar Portal";
            conn.departurePosition.Relocate(5579.0f, 4571.0f, 133.0f);  // Frostfire garrison
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1676.0f, -4315.0f, 61.2f);    // Orgrimmar
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);

            // Ashran -> Stormwind (Alliance)
            conn.connectionId = nextId++;
            conn.name = "Stormshield to Stormwind Portal";
            conn.departurePosition.Relocate(-4059.0f, -2271.0f, 51.0f); // Stormshield
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);
            conn.allianceOnly = true;
            conn.hordeOnly = false;
            connections.push_back(conn);

            // Ashran -> Orgrimmar (Horde)
            conn.connectionId = nextId++;
            conn.name = "Warspear to Orgrimmar Portal";
            conn.departurePosition.Relocate(-3998.0f, -2525.0f, 72.0f); // Warspear
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1676.0f, -4315.0f, 61.2f);
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // ====================================================================
        // THE WAR WITHIN INTERNAL CONNECTIONS
        // ====================================================================
        // TWW has multiple map IDs for different zones that need interconnection

        // Isle of Dorn (2444) <-> Khaz Algar Hub (2552) / Dornogal
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Isle of Dorn to Dornogal Portal";
            conn.departureMapId = MAP_ISLE_OF_DORN;
            conn.departurePosition.Relocate(3675.0f, -1833.0f, 2.8f);   // Isle of Dorn
            conn.arrivalMapId = MAP_KHAZ_ALGAR;
            conn.arrivalPosition.Relocate(1287.0f, -2252.0f, 176.0f);   // Dornogal
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            connections.push_back(conn);

            // Reverse
            conn.connectionId = nextId++;
            conn.name = "Dornogal to Isle of Dorn Portal";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // Isle of Dorn (2444) -> Capital cities (for bots stuck on this specific map ID)
        {
            // Isle of Dorn -> Stormwind (Alliance)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Isle of Dorn to Stormwind Portal";
            conn.departureMapId = MAP_ISLE_OF_DORN;
            conn.departurePosition.Relocate(3675.0f, -1833.0f, 2.8f);   // Isle of Dorn (via Dornogal)
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);     // Stormwind
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            connections.push_back(conn);

            // Isle of Dorn -> Orgrimmar (Horde)
            conn.connectionId = nextId++;
            conn.name = "Isle of Dorn to Orgrimmar Portal";
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1676.0f, -4315.0f, 61.2f);    // Orgrimmar
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // Underground zones <-> Dornogal hub
        // These are elevator/portal connections within TWW
        {
            // Ringing Deeps <-> Dornogal
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Ringing Deeps to Dornogal Elevator";
            conn.departureMapId = MAP_RINGING_DEEPS;
            conn.departurePosition.Relocate(0.0f, 0.0f, 0.0f);          // TODO: Get actual coords
            conn.arrivalMapId = MAP_KHAZ_ALGAR;
            conn.arrivalPosition.Relocate(1287.0f, -2252.0f, 176.0f);   // Dornogal
            conn.waitTimeSeconds = 30;  // Elevator wait time
            conn.travelTimeSeconds = 30;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Dornogal to Ringing Deeps Elevator";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);

            // Hallowfall <-> Dornogal
            conn.connectionId = nextId++;
            conn.name = "Hallowfall to Dornogal Portal";
            conn.departureMapId = MAP_HALLOWFALL;
            conn.departurePosition.Relocate(0.0f, 0.0f, 0.0f);          // TODO: Get actual coords
            conn.arrivalMapId = MAP_KHAZ_ALGAR;
            conn.arrivalPosition.Relocate(1287.0f, -2252.0f, 176.0f);
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Dornogal to Hallowfall Portal";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);

            // Azj-Kahet <-> Dornogal
            conn.connectionId = nextId++;
            conn.name = "Azj-Kahet to Dornogal Portal";
            conn.departureMapId = MAP_AZJ_KAHET;
            conn.departurePosition.Relocate(0.0f, 0.0f, 0.0f);          // TODO: Get actual coords
            conn.arrivalMapId = MAP_KHAZ_ALGAR;
            conn.arrivalPosition.Relocate(1287.0f, -2252.0f, 176.0f);
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Dornogal to Azj-Kahet Portal";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // Dragon Isles internal connections (Valdrakken is the hub)
        // Emerald Dream (map 2200) was added in 10.2
        {
            // Valdrakken -> Emerald Dream
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Valdrakken to Emerald Dream Portal";
            conn.departureMapId = MAP_DRAGON_ISLES;
            conn.departurePosition.Relocate(-2512.0f, -376.0f, 201.0f); // Valdrakken
            conn.arrivalMapId = 2200;  // Emerald Dream
            conn.arrivalPosition.Relocate(4525.0f, -2265.0f, 34.0f);    // Central Encampment
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.requiresLevel = true;
            conn.minLevel = 70;
            connections.push_back(conn);

            // Emerald Dream -> Valdrakken
            conn.connectionId = nextId++;
            conn.name = "Emerald Dream to Valdrakken Portal";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // ====================================================================
        // CATACLYSM CONNECTIONS (Deepholm, Twilight Highlands, etc.)
        // ====================================================================
        {
            // Stormwind -> Deepholm (via portal in Earthen Ring enclave)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Stormwind to Deepholm Portal";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8178.0f, 823.0f, 72.0f);   // Stormwind Earthen Ring
            conn.arrivalMapId = MAP_DEEPHOLM;
            conn.arrivalPosition.Relocate(980.0f, 523.0f, -44.0f);      // Temple of Earth
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.requiresLevel = true;
            conn.minLevel = 82;
            connections.push_back(conn);

            // Orgrimmar -> Deepholm
            conn.connectionId = nextId++;
            conn.name = "Orgrimmar to Deepholm Portal";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(1778.0f, -4341.0f, -7.5f);  // Orgrimmar Earthen Ring
            connections.push_back(conn);

            // Deepholm -> Stormwind
            conn.connectionId = nextId++;
            conn.name = "Deepholm to Stormwind Portal";
            conn.departureMapId = MAP_DEEPHOLM;
            conn.departurePosition.Relocate(980.0f, 523.0f, -44.0f);
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8178.0f, 823.0f, 72.0f);
            conn.allianceOnly = true;
            connections.push_back(conn);

            // Deepholm -> Orgrimmar
            conn.connectionId = nextId++;
            conn.name = "Deepholm to Orgrimmar Portal";
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1778.0f, -4341.0f, -7.5f);
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // ====================================================================
        // ARGUS CONNECTIONS (Legion 7.3)
        // ====================================================================
        {
            // Dalaran (Legion) -> Vindicaar (Argus)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Dalaran to Argus Portal";
            conn.departureMapId = MAP_BROKEN_ISLES;
            conn.departurePosition.Relocate(-853.0f, 4491.0f, 729.0f);  // Dalaran
            conn.arrivalMapId = MAP_ARGUS;
            conn.arrivalPosition.Relocate(-3033.0f, 9023.0f, -168.0f);  // Vindicaar
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.requiresLevel = true;
            conn.minLevel = 110;
            connections.push_back(conn);

            // Argus -> Dalaran
            conn.connectionId = nextId++;
            conn.name = "Argus to Dalaran Portal";
            std::swap(conn.departureMapId, conn.arrivalMapId);
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // ====================================================================
        // SHADOWLANDS CONNECTIONS (Oribos hub)
        // ====================================================================
        {
            // Stormwind -> Oribos (Alliance)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Stormwind to Oribos Portal";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8838.0f, 626.0f, 94.0f);   // Stormwind portal room
            conn.arrivalMapId = MAP_ORIBOS;
            conn.arrivalPosition.Relocate(-1758.0f, 1257.0f, 5453.0f);  // Oribos Ring of Transference
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 48;
            connections.push_back(conn);

            // Orgrimmar -> Oribos (Horde)
            conn.connectionId = nextId++;
            conn.name = "Orgrimmar to Oribos Portal";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(1676.0f, -4315.0f, 61.2f);  // Orgrimmar portal room
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);

            // Oribos -> Stormwind (Alliance)
            conn.connectionId = nextId++;
            conn.name = "Oribos to Stormwind Portal";
            conn.departureMapId = MAP_ORIBOS;
            conn.departurePosition.Relocate(-1758.0f, 1257.0f, 5453.0f);
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);
            conn.allianceOnly = true;
            conn.hordeOnly = false;
            connections.push_back(conn);

            // Oribos -> Orgrimmar (Horde)
            conn.connectionId = nextId++;
            conn.name = "Oribos to Orgrimmar Portal";
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1676.0f, -4315.0f, 61.2f);
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // ====================================================================
        // CAVERNS OF TIME CONNECTIONS
        // ====================================================================
        {
            // Stormwind -> Caverns of Time (via portal room)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Stormwind to Caverns of Time Portal";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8838.0f, 626.0f, 94.0f);
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(-8173.0f, -4746.0f, 33.8f);   // Caverns of Time entrance
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            connections.push_back(conn);

            // Orgrimmar -> Caverns of Time
            conn.connectionId = nextId++;
            conn.name = "Orgrimmar to Caverns of Time Portal";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(1676.0f, -4315.0f, 61.2f);
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);

            // Caverns of Time -> Stormwind (Alliance)
            conn.connectionId = nextId++;
            conn.name = "Caverns of Time to Stormwind Portal";
            conn.departurePosition.Relocate(-8173.0f, -4746.0f, 33.8f);
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);
            conn.allianceOnly = true;
            conn.hordeOnly = false;
            connections.push_back(conn);

            // Caverns of Time -> Orgrimmar (Horde)
            conn.connectionId = nextId++;
            conn.name = "Caverns of Time to Orgrimmar Portal";
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1676.0f, -4315.0f, 61.2f);
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // ====================================================================
        // VALDRAKKEN HUB CONNECTIONS (Portals within Valdrakken)
        // ====================================================================
        {
            // Valdrakken -> New Dalaran (Broken Isles)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Valdrakken to New Dalaran Portal";
            conn.departureMapId = MAP_DRAGON_ISLES;
            conn.departurePosition.Relocate(-2512.0f, -376.0f, 201.0f);  // Valdrakken
            conn.arrivalMapId = MAP_BROKEN_ISLES;
            conn.arrivalPosition.Relocate(-853.0f, 4491.0f, 729.0f);     // Dalaran
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            connections.push_back(conn);

            // Valdrakken -> Jade Forest (Pandaria)
            conn.connectionId = nextId++;
            conn.name = "Valdrakken to Jade Forest Portal";
            conn.arrivalMapId = MAP_PANDARIA;
            conn.arrivalPosition.Relocate(942.0f, -569.0f, 184.0f);      // Jade Forest
            connections.push_back(conn);

            // Valdrakken -> Shadowmoon Valley (Draenor) - Alliance only
            conn.connectionId = nextId++;
            conn.name = "Valdrakken to Shadowmoon Valley Portal";
            conn.arrivalMapId = MAP_DRAENOR;
            conn.arrivalPosition.Relocate(2068.0f, 196.0f, 87.0f);       // Shadowmoon
            conn.allianceOnly = true;
            connections.push_back(conn);

            // Valdrakken -> Frostfire Ridge (Draenor) - Horde only
            conn.connectionId = nextId++;
            conn.name = "Valdrakken to Frostfire Ridge Portal";
            conn.arrivalPosition.Relocate(5579.0f, 4571.0f, 133.0f);     // Frostfire
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // ====================================================================
        // ADDITIONAL ALLIANCE CAPITAL CONNECTIONS
        // ====================================================================
        {
            // Stormwind <-> Ironforge (Deeprun Tram - instant)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;  // Tram acts like portal
            conn.name = "Stormwind to Ironforge Tram";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8366.0f, 615.0f, 91.7f);   // Stormwind Tram entrance
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-4841.0f, -1323.0f, 502.0f);  // Ironforge Tram exit
            conn.waitTimeSeconds = 30;
            conn.travelTimeSeconds = 60;
            conn.allianceOnly = true;
            connections.push_back(conn);

            // Reverse
            conn.connectionId = nextId++;
            conn.name = "Ironforge to Stormwind Tram";
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);

            // Boralus -> Ironforge Portal
            conn.connectionId = nextId++;
            conn.name = "Boralus to Ironforge Portal";
            conn.type = TransportType::PORTAL;
            conn.departureMapId = MAP_KUL_TIRAS;
            conn.departurePosition.Relocate(-1774.0f, -1580.0f, 0.3f);  // Boralus portal room
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-4841.0f, -1323.0f, 502.0f);  // Ironforge
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            connections.push_back(conn);

            // Boralus -> Exodar Portal
            conn.connectionId = nextId++;
            conn.name = "Boralus to Exodar Portal";
            conn.arrivalMapId = 530;  // Outland (Exodar is technically on same map system)
            conn.arrivalPosition.Relocate(-4014.0f, -11897.0f, -1.3f);   // Exodar
            connections.push_back(conn);
        }

        // ====================================================================
        // ADDITIONAL HORDE CAPITAL CONNECTIONS
        // ====================================================================
        {
            // Orgrimmar <-> Undercity Zeppelin is already defined above

            // Orgrimmar <-> Thunder Bluff (not currently defined - long flight)
            // This is typically handled by taxi/flight paths rather than portal

            // Dazar'alor -> Thunder Bluff Portal (BFA addition)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Dazar'alor to Thunder Bluff Portal";
            conn.departureMapId = MAP_ZANDALAR;
            conn.departurePosition.Relocate(-1015.0f, 805.0f, 440.0f);  // Dazar'alor
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(-1274.0f, 124.0f, 131.3f);    // Thunder Bluff
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.hordeOnly = true;
            connections.push_back(conn);

            // Dazar'alor -> Silvermoon Portal
            conn.connectionId = nextId++;
            conn.name = "Dazar'alor to Silvermoon Portal";
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(9492.0f, -7281.0f, 14.3f);    // Silvermoon (translocation)
            connections.push_back(conn);
        }

        // ====================================================================
        // SILVERMOON <-> UNDERCITY (Orb of Translocation - Horde only)
        // ====================================================================
        {
            // Silvermoon -> Undercity (via Orb of Translocation)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Silvermoon to Undercity (Translocation Orb)";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(9492.0f, -7281.0f, 14.3f);   // Silvermoon Sunfury Spire
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(1811.0f, 274.0f, 75.0f);       // Undercity (Ruins of Lordaeron)
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.hordeOnly = true;
            connections.push_back(conn);

            // Undercity -> Silvermoon
            conn.connectionId = nextId++;
            conn.name = "Undercity to Silvermoon (Translocation Orb)";
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);
        }

        // ====================================================================
        // EXODAR CONNECTIONS (Draenei starting area)
        // ====================================================================
        {
            // Exodar -> Stormwind (direct portal in Vault of Lights)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Exodar to Stormwind Portal";
            conn.departureMapId = MAP_OUTLAND;  // Exodar shares Outland map ID
            conn.departurePosition.Relocate(-4014.0f, -11897.0f, -1.3f);  // Exodar
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);       // Stormwind
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            connections.push_back(conn);

            // Exodar -> Darnassus (portal in Vault of Lights)
            conn.connectionId = nextId++;
            conn.name = "Exodar to Darnassus Portal";
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(9949.0f, 2412.0f, 1327.0f);     // Darnassus
            connections.push_back(conn);

            // Exodar -> Hellfire Peninsula (for Outland access)
            conn.connectionId = nextId++;
            conn.name = "Exodar to Hellfire Peninsula Portal";
            conn.arrivalMapId = MAP_OUTLAND;
            conn.arrivalPosition.Relocate(-248.0f, 934.0f, 84.4f);        // Hellfire Peninsula
            conn.requiresLevel = true;
            conn.minLevel = 58;
            connections.push_back(conn);

            // Darnassus -> Exodar (portal in Temple of the Moon)
            conn.connectionId = nextId++;
            conn.name = "Darnassus to Exodar Portal";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(9949.0f, 2412.0f, 1327.0f);   // Darnassus Temple of Moon
            conn.arrivalMapId = MAP_OUTLAND;  // Exodar shares Outland map ID
            conn.arrivalPosition.Relocate(-4014.0f, -11897.0f, -1.3f);    // Exodar
            conn.requiresLevel = false;
            conn.minLevel = 0;
            connections.push_back(conn);
        }

        // ====================================================================
        // DARNASSUS / RUT'THERAN CONNECTIONS
        // ====================================================================
        {
            // Darnassus <-> Rut'theran Village (teleporter)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Darnassus to Rut'theran Portal";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(9949.0f, 2412.0f, 1327.0f);   // Darnassus
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(8181.0f, 1005.0f, 0.2f);        // Rut'theran Village
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            connections.push_back(conn);

            conn.connectionId = nextId++;
            conn.name = "Rut'theran to Darnassus Portal";
            std::swap(conn.departurePosition, conn.arrivalPosition);
            connections.push_back(conn);

            // Darnassus -> Blasted Lands (Dark Portal access for Night Elves)
            conn.connectionId = nextId++;
            conn.name = "Darnassus to Blasted Lands Portal";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(9949.0f, 2412.0f, 1327.0f);   // Temple of the Moon
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-11903.0f, -3206.0f, -14.9f);   // Blasted Lands (Dark Portal)
            conn.requiresLevel = true;
            conn.minLevel = 58;
            connections.push_back(conn);
        }

        // ====================================================================
        // AZSUNA PORTAL (Legion starter zone)
        // ====================================================================
        {
            // Stormwind -> Azsuna (Broken Isles)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Stormwind to Azsuna Portal";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8838.0f, 626.0f, 94.0f);     // Stormwind portal room
            conn.arrivalMapId = MAP_BROKEN_ISLES;
            conn.arrivalPosition.Relocate(-155.0f, 6673.0f, 0.5f);        // Azsuna (Crumbled Palace)
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 98;
            connections.push_back(conn);

            // Orgrimmar -> Azsuna
            conn.connectionId = nextId++;
            conn.name = "Orgrimmar to Azsuna Portal";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(1676.0f, -4315.0f, 61.2f);    // Orgrimmar portal room
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);

            // Azsuna -> Stormwind
            conn.connectionId = nextId++;
            conn.name = "Azsuna to Stormwind Portal";
            conn.departureMapId = MAP_BROKEN_ISLES;
            conn.departurePosition.Relocate(-155.0f, 6673.0f, 0.5f);
            conn.arrivalMapId = MAP_EASTERN_KINGDOMS;
            conn.arrivalPosition.Relocate(-8838.0f, 626.0f, 94.0f);
            conn.allianceOnly = true;
            conn.hordeOnly = false;
            conn.requiresLevel = false;
            connections.push_back(conn);

            // Azsuna -> Orgrimmar
            conn.connectionId = nextId++;
            conn.name = "Azsuna to Orgrimmar Portal";
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(1676.0f, -4315.0f, 61.2f);
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // ====================================================================
        // SILITHUS PORTAL (BFA - Sword of Sargeras location)
        // ====================================================================
        {
            // Boralus -> Silithus (Alliance - via BFA portal hub)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Boralus to Silithus Portal";
            conn.departureMapId = MAP_KUL_TIRAS;
            conn.departurePosition.Relocate(-1774.0f, -1580.0f, 0.3f);    // Boralus
            conn.arrivalMapId = MAP_KALIMDOR;
            conn.arrivalPosition.Relocate(-6948.0f, 1037.0f, 5.9f);       // Silithus (Magni's camp)
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 50;
            connections.push_back(conn);

            // Dazar'alor -> Silithus (Horde)
            conn.connectionId = nextId++;
            conn.name = "Dazar'alor to Silithus Portal";
            conn.departureMapId = MAP_ZANDALAR;
            conn.departurePosition.Relocate(-1015.0f, 805.0f, 440.0f);    // Dazar'alor
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);

            // Silithus -> Boralus
            conn.connectionId = nextId++;
            conn.name = "Silithus to Boralus Portal";
            conn.departureMapId = MAP_KALIMDOR;
            conn.departurePosition.Relocate(-6948.0f, 1037.0f, 5.9f);
            conn.arrivalMapId = MAP_KUL_TIRAS;
            conn.arrivalPosition.Relocate(-1774.0f, -1580.0f, 0.3f);
            conn.allianceOnly = true;
            conn.hordeOnly = false;
            connections.push_back(conn);

            // Silithus -> Dazar'alor
            conn.connectionId = nextId++;
            conn.name = "Silithus to Dazar'alor Portal";
            conn.arrivalMapId = MAP_ZANDALAR;
            conn.arrivalPosition.Relocate(-1015.0f, 805.0f, 440.0f);
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // ====================================================================
        // NAZJATAR PORTAL (BFA 8.2)
        // ====================================================================
        {
            // Boralus -> Nazjatar (Alliance)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Boralus to Nazjatar Portal";
            conn.departureMapId = MAP_KUL_TIRAS;
            conn.departurePosition.Relocate(-1774.0f, -1580.0f, 0.3f);
            conn.arrivalMapId = 1355;  // Nazjatar map ID
            conn.arrivalPosition.Relocate(-925.0f, 698.0f, 0.8f);         // Mezzamere (Alliance hub)
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 50;
            connections.push_back(conn);

            // Dazar'alor -> Nazjatar (Horde)
            conn.connectionId = nextId++;
            conn.name = "Dazar'alor to Nazjatar Portal";
            conn.departureMapId = MAP_ZANDALAR;
            conn.departurePosition.Relocate(-1015.0f, 805.0f, 440.0f);
            conn.arrivalPosition.Relocate(-985.0f, 435.0f, 0.8f);         // Newhome (Horde hub)
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);

            // Nazjatar -> Boralus
            conn.connectionId = nextId++;
            conn.name = "Nazjatar to Boralus Portal";
            conn.departureMapId = 1355;
            conn.departurePosition.Relocate(-925.0f, 698.0f, 0.8f);
            conn.arrivalMapId = MAP_KUL_TIRAS;
            conn.arrivalPosition.Relocate(-1774.0f, -1580.0f, 0.3f);
            conn.allianceOnly = true;
            conn.hordeOnly = false;
            connections.push_back(conn);

            // Nazjatar -> Dazar'alor
            conn.connectionId = nextId++;
            conn.name = "Nazjatar to Dazar'alor Portal";
            conn.departurePosition.Relocate(-985.0f, 435.0f, 0.8f);
            conn.arrivalMapId = MAP_ZANDALAR;
            conn.arrivalPosition.Relocate(-1015.0f, 805.0f, 440.0f);
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        // ====================================================================
        // MECHAGON PORTAL (BFA 8.2)
        // ====================================================================
        {
            // Boralus -> Mechagon (Alliance)
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Boralus to Mechagon Portal";
            conn.departureMapId = MAP_KUL_TIRAS;
            conn.departurePosition.Relocate(-1774.0f, -1580.0f, 0.3f);
            conn.arrivalMapId = 1462;  // Mechagon map ID
            conn.arrivalPosition.Relocate(617.0f, 1418.0f, 45.0f);        // Rustbolt
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.allianceOnly = true;
            conn.requiresLevel = true;
            conn.minLevel = 50;
            connections.push_back(conn);

            // Dazar'alor -> Mechagon (Horde)
            conn.connectionId = nextId++;
            conn.name = "Dazar'alor to Mechagon Portal";
            conn.departureMapId = MAP_ZANDALAR;
            conn.departurePosition.Relocate(-1015.0f, 805.0f, 440.0f);
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);

            // Mechagon -> Boralus
            conn.connectionId = nextId++;
            conn.name = "Mechagon to Boralus Portal";
            conn.departureMapId = 1462;
            conn.departurePosition.Relocate(617.0f, 1418.0f, 45.0f);
            conn.arrivalMapId = MAP_KUL_TIRAS;
            conn.arrivalPosition.Relocate(-1774.0f, -1580.0f, 0.3f);
            conn.allianceOnly = true;
            conn.hordeOnly = false;
            connections.push_back(conn);

            // Mechagon -> Dazar'alor
            conn.connectionId = nextId++;
            conn.name = "Mechagon to Dazar'alor Portal";
            conn.arrivalMapId = MAP_ZANDALAR;
            conn.arrivalPosition.Relocate(-1015.0f, 805.0f, 440.0f);
            conn.allianceOnly = false;
            conn.hordeOnly = true;
            connections.push_back(conn);
        }

        TC_LOG_INFO("module.playerbot.travel", "TravelRouteManager: Initialized {} transport connections",
                    connections.size());
    }
}

// ============================================================================
// TRAVEL ROUTE MANAGER IMPLEMENTATION
// ============================================================================

TravelRouteManager::TravelRouteManager(Player* bot)
    : m_bot(bot)
    , m_lastRoutePlanTime(0)
    , m_lastStateUpdateTime(0)
{
    // Initialize static transport connections on first instance
    if (!s_connectionsInitialized)
    {
        InitializeTransportConnections();
        s_connectionsInitialized = true;
    }

    TC_LOG_DEBUG("module.playerbot.travel", "TravelRouteManager: Created for bot {}",
                 bot ? bot->GetName() : "NULL");
}

TravelRouteManager::~TravelRouteManager()
{
    if (m_activeRoute && m_activeRoute->IsActive())
    {
        TC_LOG_DEBUG("module.playerbot.travel", "TravelRouteManager: Destroying with active route");
    }
}

void TravelRouteManager::InitializeTransportConnections()
{
    TransportDB::InitializeConnections(s_transportConnections);

    // Build lookup indices
    s_connectionsByDepartureMap.clear();
    s_connectionsByArrivalMap.clear();
    s_mapConnectivityGraph.clear();

    for (size_t i = 0; i < s_transportConnections.size(); ++i)
    {
        TransportConnection const& conn = s_transportConnections[i];
        s_connectionsByDepartureMap[conn.departureMapId].push_back(i);
        s_connectionsByArrivalMap[conn.arrivalMapId].push_back(i);

        // Build connectivity graph
        s_mapConnectivityGraph[conn.departureMapId].emplace_back(conn.arrivalMapId, &conn);
    }

    TC_LOG_INFO("module.playerbot.travel", "TravelRouteManager: Built connectivity graph for {} maps",
                s_mapConnectivityGraph.size());
}

// ============================================================================
// ROUTE PLANNING
// ============================================================================

TravelRoute TravelRouteManager::PlanRoute(uint32 destinationMapId, Position const& destination)
{
    TravelRoute route;
    if (!m_bot)
    {
        route.overallState = TravelState::FAILED;
        return route;
    }

    uint32 currentMapId = m_bot->GetMapId();
    Position currentPos = m_bot->GetPosition();

    TC_LOG_DEBUG("module.playerbot.travel", "TravelRouteManager: Planning route for {} from MAP {} to MAP {} at ({:.1f}, {:.1f}, {:.1f})",
                 m_bot->GetName(), currentMapId, destinationMapId,
                 destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());

    // Initialize route
    route.routeId = static_cast<uint32>(GameTime::GetGameTimeMS());
    route.originMapId = currentMapId;
    route.originPosition = currentPos;
    route.destinationMapId = destinationMapId;
    route.destinationPosition = destination;
    route.currentLegIndex = 0;
    route.overallState = TravelState::IDLE;

    // Same map - just taxi or walk
    if (currentMapId == destinationMapId)
    {
        float distance = currentPos.GetExactDist(destination);

        if (distance < 50.0f)
        {
            // Just walk
            AddWalkingLeg(route, currentMapId, currentPos, destination);
        }
        else
        {
            // Try taxi, fallback to walk
            if (!AddTaxiLeg(route, currentMapId, currentPos, destination))
            {
                AddWalkingLeg(route, currentMapId, currentPos, destination);
            }
        }

        route.description = "Same-map travel";
        route.totalLegs = static_cast<uint32>(route.legs.size());
        m_stats.routesPlanned++;
        return route;
    }

    // Different map - need transport connections
    if (!BuildRoute(route, currentMapId, currentPos, destinationMapId, destination))
    {
        TC_LOG_WARN("module.playerbot.travel", "TravelRouteManager: Failed to build route from MAP {} to MAP {} for {}",
                    currentMapId, destinationMapId, m_bot->GetName());
        route.overallState = TravelState::FAILED;
        return route;
    }

    // Calculate totals
    route.totalLegs = static_cast<uint32>(route.legs.size());
    route.totalEstimatedTimeSeconds = 0;
    route.totalEstimatedCostCopper = 0;

    for (auto const& leg : route.legs)
    {
        route.totalEstimatedTimeSeconds += leg.estimatedTimeSeconds;
        route.totalEstimatedCostCopper += leg.estimatedCostCopper;
    }

    // Build description
    std::string desc = "";
    for (size_t i = 0; i < route.legs.size(); ++i)
    {
        if (i > 0) desc += " -> ";
        switch (route.legs[i].type)
        {
            case TransportType::TAXI_FLIGHT: desc += "Taxi"; break;
            case TransportType::SHIP: desc += "Ship"; break;
            case TransportType::ZEPPELIN: desc += "Zeppelin"; break;
            case TransportType::PORTAL: desc += "Portal"; break;
            case TransportType::BOAT: desc += "Boat"; break;
            case TransportType::HEARTHSTONE: desc += "Hearthstone"; break;
            case TransportType::WALK: desc += "Walk"; break;
            default: desc += "Unknown"; break;
        }
    }
    route.description = desc;

    TC_LOG_INFO("module.playerbot.travel", "TravelRouteManager: Planned {}-leg route for {}: {} ({}s, {}c)",
                route.totalLegs, m_bot->GetName(), route.description,
                route.totalEstimatedTimeSeconds, route.totalEstimatedCostCopper);

    m_stats.routesPlanned++;
    return route;
}

bool TravelRouteManager::BuildRoute(TravelRoute& route, uint32 fromMapId, Position const& fromPos,
                                     uint32 toMapId, Position const& toPos)
{
    // Use BFS to find shortest path through transport graph
    struct PathNode
    {
        uint32 mapId;
        TransportConnection const* connection;  // Connection used to reach this map
        size_t parentIndex;
    };

    std::queue<size_t> bfsQueue;
    std::vector<PathNode> visited;
    std::unordered_map<uint32, size_t> visitedMaps;

    // Start node
    visited.push_back({fromMapId, nullptr, SIZE_MAX});
    visitedMaps[fromMapId] = 0;
    bfsQueue.push(0);

    bool found = false;
    size_t destinationIndex = SIZE_MAX;

    while (!bfsQueue.empty() && !found)
    {
        size_t currentIndex = bfsQueue.front();
        bfsQueue.pop();

        uint32 currentMapId = visited[currentIndex].mapId;

        // Check all connections from current map
        auto it = s_mapConnectivityGraph.find(currentMapId);
        if (it == s_mapConnectivityGraph.end())
            continue;

        for (auto const& [neighborMapId, connection] : it->second)
        {
            // Skip if already visited
            if (visitedMaps.find(neighborMapId) != visitedMaps.end())
                continue;

            // Skip if bot can't use this connection
            if (!CanUseConnection(connection))
                continue;

            // Add to visited
            size_t newIndex = visited.size();
            visited.push_back({neighborMapId, connection, currentIndex});
            visitedMaps[neighborMapId] = newIndex;

            // Check if destination reached
            if (neighborMapId == toMapId)
            {
                found = true;
                destinationIndex = newIndex;
                break;
            }

            bfsQueue.push(newIndex);
        }
    }

    if (!found)
    {
        TC_LOG_DEBUG("module.playerbot.travel", "TravelRouteManager: No path found from MAP {} to MAP {}",
                     fromMapId, toMapId);
        return false;
    }

    // Reconstruct path
    std::vector<TransportConnection const*> pathConnections;
    size_t current = destinationIndex;
    while (visited[current].connection != nullptr)
    {
        pathConnections.push_back(visited[current].connection);
        current = visited[current].parentIndex;
    }

    // Reverse to get correct order
    std::reverse(pathConnections.begin(), pathConnections.end());

    // Build route legs
    Position legStartPos = fromPos;
    uint32 legStartMapId = fromMapId;

    for (TransportConnection const* conn : pathConnections)
    {
        // Walk to transport departure if needed
        if (legStartMapId == conn->departureMapId)
        {
            float distToTransport = legStartPos.GetExactDist(conn->departurePosition);
            if (distToTransport > 10.0f)
            {
                // Try taxi to transport, otherwise walk
                if (distToTransport > 100.0f)
                {
                    AddTaxiLeg(route, legStartMapId, legStartPos, conn->departurePosition);
                }
                else
                {
                    AddWalkingLeg(route, legStartMapId, legStartPos, conn->departurePosition);
                }
            }
        }

        // Add transport leg
        AddTransportLeg(route, conn);

        // Update position for next leg
        legStartPos = conn->arrivalPosition;
        legStartMapId = conn->arrivalMapId;
    }

    // Final leg - taxi/walk to destination
    float distToDestination = legStartPos.GetExactDist(toPos);
    if (distToDestination > 10.0f)
    {
        if (distToDestination > 100.0f)
        {
            if (!AddTaxiLeg(route, toMapId, legStartPos, toPos))
            {
                AddWalkingLeg(route, toMapId, legStartPos, toPos);
            }
        }
        else
        {
            AddWalkingLeg(route, toMapId, legStartPos, toPos);
        }
    }

    return true;
}

bool TravelRouteManager::AddTaxiLeg(TravelRoute& route, uint32 mapId, Position const& from, Position const& to)
{
    // Find taxi nodes for start and end positions
    uint32 startNode = FlightMasterManager::FindNearestTaxiNode(from, mapId);
    uint32 endNode = FlightMasterManager::FindNearestTaxiNode(to, mapId);

    // If we can't find valid taxi nodes, leg creation fails
    if (startNode == 0 || endNode == 0)
    {
        TC_LOG_DEBUG("module.playerbot.travel",
            "TravelRouteManager::AddTaxiLeg - Cannot find taxi nodes for map {}: start={}, end={}",
            mapId, startNode, endNode);
        return false;
    }

    // Same node - no taxi needed
    if (startNode == endNode)
    {
        TC_LOG_DEBUG("module.playerbot.travel",
            "TravelRouteManager::AddTaxiLeg - Start and end are same taxi node {}, skipping taxi leg",
            startNode);
        return false;
    }

    TravelLeg leg;
    leg.legIndex = static_cast<uint32>(route.legs.size());
    leg.type = TransportType::TAXI_FLIGHT;
    leg.description = "Flight path";
    leg.startMapId = mapId;
    leg.startPosition = from;
    leg.endMapId = mapId;
    leg.endPosition = to;
    leg.currentState = TravelState::IDLE;

    // Set the taxi node fields - CRITICAL for QuestStrategy to use FlyToTaxiNode
    leg.taxiStartNode = startNode;
    leg.taxiEndNode = endNode;

    float distance = from.GetExactDist(to);
    leg.estimatedTimeSeconds = static_cast<uint32>(distance / 50.0f); // ~50 yards/sec flight speed
    leg.estimatedCostCopper = static_cast<uint32>(distance * 0.1f);   // Rough cost estimate

    TC_LOG_DEBUG("module.playerbot.travel",
        "TravelRouteManager::AddTaxiLeg - Added taxi leg: node {} -> node {} on map {}",
        startNode, endNode, mapId);

    route.legs.push_back(leg);
    return true;
}

bool TravelRouteManager::AddTransportLeg(TravelRoute& route, TransportConnection const* connection)
{
    if (!connection)
        return false;

    TravelLeg leg;
    leg.legIndex = static_cast<uint32>(route.legs.size());
    leg.type = connection->type;
    leg.description = connection->name;
    leg.startMapId = connection->departureMapId;
    leg.startPosition = connection->departurePosition;
    leg.endMapId = connection->arrivalMapId;
    leg.endPosition = connection->arrivalPosition;
    leg.connection = connection;
    leg.currentState = TravelState::IDLE;
    leg.estimatedTimeSeconds = connection->waitTimeSeconds + connection->travelTimeSeconds;
    leg.estimatedCostCopper = connection->costCopper;

    route.legs.push_back(leg);
    return true;
}

void TravelRouteManager::AddWalkingLeg(TravelRoute& route, uint32 mapId, Position const& from, Position const& to)
{
    TravelLeg leg;
    leg.legIndex = static_cast<uint32>(route.legs.size());
    leg.type = TransportType::WALK;
    leg.description = "Walking";
    leg.startMapId = mapId;
    leg.startPosition = from;
    leg.endMapId = mapId;
    leg.endPosition = to;
    leg.currentState = TravelState::IDLE;

    float distance = from.GetExactDist(to);
    leg.estimatedTimeSeconds = static_cast<uint32>(distance / 7.0f); // ~7 yards/sec run speed
    leg.estimatedCostCopper = 0;

    route.legs.push_back(leg);
}

bool TravelRouteManager::AddHearthstoneLeg(TravelRoute& route)
{
    if (!m_bot)
        return false;

    WorldLocation const& homebind = m_bot->m_homebind;

    TravelLeg leg;
    leg.legIndex = static_cast<uint32>(route.legs.size());
    leg.type = TransportType::HEARTHSTONE;
    leg.description = "Hearthstone";
    leg.startMapId = m_bot->GetMapId();
    leg.startPosition = m_bot->GetPosition();
    leg.endMapId = homebind.GetMapId();
    leg.endPosition.Relocate(homebind.GetPositionX(), homebind.GetPositionY(), homebind.GetPositionZ());
    leg.currentState = TravelState::IDLE;
    leg.estimatedTimeSeconds = 10; // Cast time
    leg.estimatedCostCopper = 0;

    route.legs.push_back(leg);
    return true;
}

// ============================================================================
// ROUTE EXECUTION
// ============================================================================

bool TravelRouteManager::StartRoute(TravelRoute&& route)
{
    if (route.legs.empty())
    {
        TC_LOG_WARN("module.playerbot.travel", "TravelRouteManager: Cannot start empty route");
        return false;
    }

    m_activeRoute = std::make_unique<TravelRoute>(std::move(route));
    m_activeRoute->overallState = TravelState::WALKING_TO_TRANSPORT;
    m_activeRoute->routeStartTime = static_cast<uint32>(GameTime::GetGameTimeMS());
    m_activeRoute->currentLegIndex = 0;

    if (!m_activeRoute->legs.empty())
    {
        m_activeRoute->legs[0].currentState = TravelState::WALKING_TO_TRANSPORT;
        m_activeRoute->legs[0].stateStartTime = m_activeRoute->routeStartTime;
    }

    TC_LOG_INFO("module.playerbot.travel", "TravelRouteManager: Started route for {} - {} legs: {}",
                m_bot ? m_bot->GetName() : "NULL",
                m_activeRoute->totalLegs,
                m_activeRoute->description);

    return true;
}

bool TravelRouteManager::Update(uint32 diff)
{
    if (!m_activeRoute || !m_activeRoute->IsActive())
        return false;

    uint32 now = static_cast<uint32>(GameTime::GetGameTimeMS());
    if (now - m_lastStateUpdateTime < STATE_UPDATE_INTERVAL_MS)
        return true;

    m_lastStateUpdateTime = now;

    TravelLeg* currentLeg = m_activeRoute->GetCurrentLeg();
    if (!currentLeg)
    {
        // All legs completed
        m_activeRoute->overallState = TravelState::COMPLETED;
        m_stats.routesCompleted++;
        m_stats.totalTravelTimeMs += now - m_activeRoute->routeStartTime;

        TC_LOG_INFO("module.playerbot.travel", "TravelRouteManager: Route completed for {}",
                    m_bot ? m_bot->GetName() : "NULL");

        if (m_activeRoute->onCompleted)
            m_activeRoute->onCompleted(*m_activeRoute);

        return false;
    }

    // Update current leg state
    UpdateLegState(*currentLeg, diff);

    // Check if current leg completed
    if (currentLeg->currentState == TravelState::COMPLETED)
    {
        m_stats.totalLegsCompleted++;
        AdvanceToNextLeg();
    }
    else if (currentLeg->currentState == TravelState::FAILED)
    {
        m_activeRoute->overallState = TravelState::FAILED;
        m_stats.routesFailed++;

        TC_LOG_WARN("module.playerbot.travel", "TravelRouteManager: Route failed for {} at leg {}",
                    m_bot ? m_bot->GetName() : "NULL", currentLeg->legIndex);

        if (m_activeRoute->onFailed)
            m_activeRoute->onFailed(*m_activeRoute, "Leg failed");

        return false;
    }

    return true;
}

void TravelRouteManager::CancelRoute()
{
    if (m_activeRoute)
    {
        TC_LOG_INFO("module.playerbot.travel", "TravelRouteManager: Route cancelled for {}",
                    m_bot ? m_bot->GetName() : "NULL");
        m_activeRoute->overallState = TravelState::FAILED;
        m_activeRoute.reset();
    }
}

void TravelRouteManager::UpdateLegState(TravelLeg& leg, uint32 /*diff*/)
{
    if (!m_bot)
        return;

    switch (leg.type)
    {
        case TransportType::WALK:
        case TransportType::TAXI_FLIGHT:
        {
            // Check if we've arrived
            if (IsNearPosition(leg.endPosition, 15.0f))
            {
                leg.currentState = TravelState::COMPLETED;
            }
            else if (leg.currentState == TravelState::IDLE || leg.currentState == TravelState::WALKING_TO_TRANSPORT)
            {
                // Start movement
                if (leg.type == TransportType::WALK)
                {
                    m_bot->GetMotionMaster()->MovePoint(0, leg.endPosition);
                }
                leg.currentState = (leg.type == TransportType::TAXI_FLIGHT) ? TravelState::TAXI_FLIGHT : TravelState::WALKING_TO_TRANSPORT;
            }
            break;
        }

        case TransportType::SHIP:
        case TransportType::ZEPPELIN:
        case TransportType::BOAT:
        {
            HandleOnTransport(leg);
            break;
        }

        case TransportType::PORTAL:
        {
            HandlePortal(leg);
            break;
        }

        case TransportType::HEARTHSTONE:
        {
            HandleHearthstone(leg);
            break;
        }

        default:
            leg.currentState = TravelState::FAILED;
            break;
    }
}

void TravelRouteManager::AdvanceToNextLeg()
{
    if (!m_activeRoute)
        return;

    m_activeRoute->currentLegIndex++;

    if (m_activeRoute->currentLegIndex >= m_activeRoute->legs.size())
    {
        m_activeRoute->overallState = TravelState::COMPLETED;
        return;
    }

    TravelLeg& nextLeg = m_activeRoute->legs[m_activeRoute->currentLegIndex];
    nextLeg.currentState = TravelState::WALKING_TO_TRANSPORT;
    nextLeg.stateStartTime = static_cast<uint32>(GameTime::GetGameTimeMS());

    TC_LOG_DEBUG("module.playerbot.travel", "TravelRouteManager: Advanced to leg {} for {}",
                 m_activeRoute->currentLegIndex, m_bot ? m_bot->GetName() : "NULL");
}

void TravelRouteManager::HandleOnTransport(TravelLeg& leg)
{
    // Simplified transport handling
    // Full implementation would track actual transport position

    switch (leg.currentState)
    {
        case TravelState::IDLE:
        case TravelState::WALKING_TO_TRANSPORT:
        {
            // Walk to departure point
            if (!IsNearPosition(leg.startPosition, 30.0f))
            {
                m_bot->GetMotionMaster()->MovePoint(0, leg.startPosition);
                leg.currentState = TravelState::WALKING_TO_TRANSPORT;
            }
            else
            {
                leg.currentState = TravelState::WAITING_FOR_TRANSPORT;
            }
            break;
        }

        case TravelState::WAITING_FOR_TRANSPORT:
        {
            // Wait for transport (simplified - just wait the estimated time)
            uint32 elapsed = static_cast<uint32>(GameTime::GetGameTimeMS()) - leg.stateStartTime;
            if (elapsed > leg.connection->waitTimeSeconds * 1000)
            {
                leg.currentState = TravelState::ON_TRANSPORT;
                leg.stateStartTime = static_cast<uint32>(GameTime::GetGameTimeMS());
            }
            break;
        }

        case TravelState::ON_TRANSPORT:
        {
            // On transport (simplified - teleport after travel time)
            uint32 elapsed = static_cast<uint32>(GameTime::GetGameTimeMS()) - leg.stateStartTime;
            if (elapsed > leg.connection->travelTimeSeconds * 1000)
            {
                // Teleport to arrival
                m_bot->TeleportTo(leg.endMapId,
                                  leg.endPosition.GetPositionX(),
                                  leg.endPosition.GetPositionY(),
                                  leg.endPosition.GetPositionZ(),
                                  leg.endPosition.GetOrientation());
                leg.currentState = TravelState::COMPLETED;
            }
            break;
        }

        default:
            break;
    }
}

void TravelRouteManager::HandlePortal(TravelLeg& leg)
{
    if (leg.currentState == TravelState::IDLE || leg.currentState == TravelState::WALKING_TO_TRANSPORT)
    {
        // Walk to portal if not close
        if (!IsNearPosition(leg.startPosition, 10.0f))
        {
            m_bot->GetMotionMaster()->MovePoint(0, leg.startPosition);
            leg.currentState = TravelState::WALKING_TO_TRANSPORT;
        }
        else
        {
            // Use portal (teleport)
            m_bot->TeleportTo(leg.endMapId,
                              leg.endPosition.GetPositionX(),
                              leg.endPosition.GetPositionY(),
                              leg.endPosition.GetPositionZ(),
                              leg.endPosition.GetOrientation());
            leg.currentState = TravelState::COMPLETED;
        }
    }
}

void TravelRouteManager::HandleHearthstone(TravelLeg& leg)
{
    static constexpr uint32 HEARTHSTONE_SPELL_ID = 8690;

    switch (leg.currentState)
    {
        case TravelState::IDLE:
        case TravelState::WALKING_TO_TRANSPORT:
        {
            // Check cooldown
            SpellHistory* spellHistory = m_bot->GetSpellHistory();
            if (spellHistory && spellHistory->HasCooldown(HEARTHSTONE_SPELL_ID))
            {
                leg.currentState = TravelState::FAILED;
                return;
            }

            // Cast hearthstone
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(HEARTHSTONE_SPELL_ID, DIFFICULTY_NONE);
            if (spellInfo)
            {
                Spell* spell = new Spell(m_bot, spellInfo, TRIGGERED_NONE);
                SpellCastTargets targets;
                targets.SetUnitTarget(m_bot);
                spell->prepare(targets);
            }

            leg.currentState = TravelState::CASTING_HEARTHSTONE;
            leg.stateStartTime = static_cast<uint32>(GameTime::GetGameTimeMS());
            break;
        }

        case TravelState::CASTING_HEARTHSTONE:
        {
            // Check if cast completed (bot is now at homebind)
            if (m_bot->GetMapId() == leg.endMapId && IsNearPosition(leg.endPosition, 50.0f))
            {
                leg.currentState = TravelState::COMPLETED;
            }
            else
            {
                // Check timeout
                uint32 elapsed = static_cast<uint32>(GameTime::GetGameTimeMS()) - leg.stateStartTime;
                if (elapsed > 15000) // 15 second timeout
                {
                    leg.currentState = TravelState::FAILED;
                }
            }
            break;
        }

        default:
            break;
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

bool TravelRouteManager::CanReachMap(uint32 fromMapId, uint32 toMapId) const
{
    if (fromMapId == toMapId)
        return true;

    // BFS to check reachability
    std::queue<uint32> bfsQueue;
    std::unordered_set<uint32> visited;

    bfsQueue.push(fromMapId);
    visited.insert(fromMapId);

    while (!bfsQueue.empty())
    {
        uint32 currentMap = bfsQueue.front();
        bfsQueue.pop();

        auto it = s_mapConnectivityGraph.find(currentMap);
        if (it == s_mapConnectivityGraph.end())
            continue;

        for (auto const& [neighborMap, conn] : it->second)
        {
            if (neighborMap == toMapId)
                return true;

            if (visited.find(neighborMap) == visited.end() && CanUseConnection(conn))
            {
                visited.insert(neighborMap);
                bfsQueue.push(neighborMap);
            }
        }
    }

    return false;
}

uint32 TravelRouteManager::GetEstimatedTravelTime(uint32 fromMapId, Position const& /*fromPos*/,
                                                   uint32 toMapId, Position const& /*toPos*/) const
{
    if (fromMapId == toMapId)
        return 0;

    // Simplified estimate - count hops and multiply by average transport time
    // Full implementation would plan actual route

    if (!CanReachMap(fromMapId, toMapId))
        return UINT32_MAX;

    // Rough estimate: 5 minutes per map hop
    return 300;
}

std::vector<TransportConnection const*> TravelRouteManager::GetConnectionsFromMap(uint32 mapId) const
{
    std::vector<TransportConnection const*> result;

    auto it = s_connectionsByDepartureMap.find(mapId);
    if (it != s_connectionsByDepartureMap.end())
    {
        for (size_t idx : it->second)
        {
            if (idx < s_transportConnections.size())
                result.push_back(&s_transportConnections[idx]);
        }
    }

    return result;
}

std::vector<TransportConnection const*> TravelRouteManager::GetConnectionsToMap(uint32 mapId) const
{
    std::vector<TransportConnection const*> result;

    auto it = s_connectionsByArrivalMap.find(mapId);
    if (it != s_connectionsByArrivalMap.end())
    {
        for (size_t idx : it->second)
        {
            if (idx < s_transportConnections.size())
                result.push_back(&s_transportConnections[idx]);
        }
    }

    return result;
}

TransportConnection const* TravelRouteManager::FindDirectConnection(uint32 fromMapId, uint32 toMapId) const
{
    auto it = s_connectionsByDepartureMap.find(fromMapId);
    if (it == s_connectionsByDepartureMap.end())
        return nullptr;

    for (size_t idx : it->second)
    {
        if (idx < s_transportConnections.size() && s_transportConnections[idx].arrivalMapId == toMapId)
        {
            if (CanUseConnection(&s_transportConnections[idx]))
                return &s_transportConnections[idx];
        }
    }

    return nullptr;
}

bool TravelRouteManager::IsNearPosition(Position const& pos, float range) const
{
    if (!m_bot)
        return false;

    return m_bot->GetExactDist(pos) < range;
}

bool TravelRouteManager::IsOnTransport() const
{
    if (!m_bot)
        return false;

    return m_bot->GetTransport() != nullptr;
}

bool TravelRouteManager::CanUseConnection(TransportConnection const* connection) const
{
    if (!connection || !m_bot)
        return false;

    // Faction check
    bool isAlliance = (m_bot->GetTeam() == ALLIANCE);
    if (connection->allianceOnly && !isAlliance)
        return false;
    if (connection->hordeOnly && isAlliance)
        return false;

    // Level check
    if (connection->requiresLevel && m_bot->GetLevel() < connection->minLevel)
        return false;

    return true;
}

TravelState TravelRouteManager::GetCurrentState() const
{
    if (!m_activeRoute)
        return TravelState::IDLE;

    TravelLeg const* leg = m_activeRoute->GetCurrentLeg();
    if (leg)
        return leg->currentState;

    return m_activeRoute->overallState;
}

std::string TravelRouteManager::GetStatusString() const
{
    if (!m_activeRoute)
        return "No active route";

    if (!m_activeRoute->IsActive())
    {
        if (m_activeRoute->IsComplete())
            return "Route completed";
        if (m_activeRoute->IsFailed())
            return "Route failed";
        return "Route idle";
    }

    TravelLeg const* leg = m_activeRoute->GetCurrentLeg();
    if (!leg)
        return "No current leg";

    std::string status = "Leg " + std::to_string(leg->legIndex + 1) + "/" +
                         std::to_string(m_activeRoute->totalLegs) + ": ";

    switch (leg->currentState)
    {
        case TravelState::WALKING_TO_TRANSPORT: status += "Walking to transport"; break;
        case TravelState::WAITING_FOR_TRANSPORT: status += "Waiting for transport"; break;
        case TravelState::ON_TRANSPORT: status += "On transport"; break;
        case TravelState::TAXI_FLIGHT: status += "Flying"; break;
        case TravelState::CASTING_HEARTHSTONE: status += "Hearthing"; break;
        case TravelState::USING_PORTAL: status += "Using portal"; break;
        default: status += "Unknown state"; break;
    }

    return status;
}

} // namespace Playerbot
