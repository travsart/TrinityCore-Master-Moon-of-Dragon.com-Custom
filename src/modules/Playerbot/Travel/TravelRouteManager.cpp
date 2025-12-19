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
#include "../Interaction/FlightMasterManager.h"
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
    // Map IDs
    constexpr uint32 MAP_EASTERN_KINGDOMS = 0;
    constexpr uint32 MAP_KALIMDOR = 1;
    constexpr uint32 MAP_OUTLAND = 530;
    constexpr uint32 MAP_NORTHREND = 571;
    constexpr uint32 MAP_PANDARIA = 870;
    constexpr uint32 MAP_DRAENOR = 1116;
    constexpr uint32 MAP_BROKEN_ISLES = 1220;
    constexpr uint32 MAP_ZANDALAR = 1642;
    constexpr uint32 MAP_KUL_TIRAS = 1643;
    constexpr uint32 MAP_DRAGON_ISLES = 2222;
    constexpr uint32 MAP_KHAZ_ALGAR = 2552;

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
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Stormwind to Dragon Isles Portal";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8838.0f, 626.0f, 94.0f);
            conn.arrivalMapId = MAP_DRAGON_ISLES;
            conn.arrivalPosition.Relocate(-2512.0f, -376.0f, 201.0f);  // Valdrakken
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.requiresLevel = true;
            conn.minLevel = 60;
            connections.push_back(conn);
        }

        // Capital city -> Khaz Algar (The War Within)
        {
            TransportConnection conn;
            conn.connectionId = nextId++;
            conn.type = TransportType::PORTAL;
            conn.name = "Dornogal Portal";
            conn.departureMapId = MAP_EASTERN_KINGDOMS;
            conn.departurePosition.Relocate(-8838.0f, 626.0f, 94.0f);
            conn.arrivalMapId = MAP_KHAZ_ALGAR;
            conn.arrivalPosition.Relocate(1287.0f, -2252.0f, 176.0f);  // Dornogal
            conn.waitTimeSeconds = PORTAL_WAIT_TIME;
            conn.travelTimeSeconds = PORTAL_TRAVEL_TIME;
            conn.requiresLevel = true;
            conn.minLevel = 70;
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
    // For now, simplified - assume taxi is available
    // Full implementation would use FlightMasterManager to find actual route

    TravelLeg leg;
    leg.legIndex = static_cast<uint32>(route.legs.size());
    leg.type = TransportType::TAXI_FLIGHT;
    leg.description = "Flight path";
    leg.startMapId = mapId;
    leg.startPosition = from;
    leg.endMapId = mapId;
    leg.endPosition = to;
    leg.currentState = TravelState::IDLE;

    float distance = from.GetExactDist(to);
    leg.estimatedTimeSeconds = static_cast<uint32>(distance / 50.0f); // ~50 yards/sec flight speed
    leg.estimatedCostCopper = static_cast<uint32>(distance * 0.1f);   // Rough cost estimate

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
