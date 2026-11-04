/*
 * GM Stats for AzerothCore (master)
 * ------------------------------------------------
 * GMStats.cpp — расширенная статистика для ГМов.
 *
 * Команды (базовый .gstats):
 *   .gstats server          — аптайм, онлайн, пик, сессии
 *   .gstats player <имя>    — сводка по игроку (онлайн/офлайн)
 *   .gstats top             — топы (время в игре, богатейшие, убийства)
 *   .gstats zone            — статистика текущей зоны ГМа
 *   .gstats log [дней]      — просмотр аудита ГМ-команд
 *
 * Архитектура:
 *  - ChatCommandScript: современный механизм команд (ChatCommandTable).
 *  - WorldScript: отслеживание пика онлайна раз в минуту (лёгкая проверка).
 *  - Аудит: запись в mod_gm_stats_audit при каждом вызове команды.
 *  - Heavy-запросы к БД — только по требованию ГМа (никогда в OnUpdate).
 */

#include "Config_GMStats.h"

#include "Chat.h"
#include "World.h"
#include "WorldScript.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "MapMgr.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"

#include <chrono>
#include <ctime>

using namespace GMStats;
using namespace Acore::ChatCommands;

namespace
{
    // ============ Пик онлайна (WorldScript) ============
    uint32 s_peakOnline = 0;

    void UpdatePeakOnline()
    {
        uint32 const online = World::GetWorldInstance()->GetActiveSessionCount();
        if (online > s_peakOnline)
        {
            s_peakOnline = online;
            LOG_DEBUG("module.gmstats", "New online peak: {}", online);
        }
    }

    // ============ Форматирование аптайма ============
    std::string FormatUptime(uint32 secs)
    {
        uint32 const days = secs / 86400;
        uint32 const hours = (secs % 86400) / 3600;
        uint32 const mins = (secs % 3600) / 60;
        return fmt::format("{}d {}h {}m", days, hours, mins);
    }

    // ============ Аудит ============
    void AuditCommand(Player* gm, std::string const& subCommand, std::string const& args = "")
    {
        if (!Audit::Enabled || !gm)
            return;

        std::time_t now = std::time(nullptr);
        std::tm localNow = *std::localtime(&now);
        char dateKey[20];
        std::strftime(dateKey, sizeof(dateKey), "%Y-%m-%d %H:%M:%S", &localNow);

        CharacterDatabase.Execute(
            "INSERT INTO mod_gm_stats_audit (gm_guid, gm_name, command, arguments, used_at) "
            "VALUES ({}, '{}', '{}', '{}', '{}')",
            gm->GetGUID().GetCounter(), gm->GetName(), subCommand,
            Audit::LogArguments ? args : "", dateKey);
    }

    // ============ .gstats server ============
    bool HandleServerCommand(ChatHandler* handler)
    {
        Player* gm = handler->GetSession()->GetPlayer();

        uint32 const uptime = World::GetWorldInstance()->GetUptime();
        uint32 const sessions = World::GetWorldInstance()->GetActiveSessionCount();
        uint32 const players = World::GetWorldInstance()->GetPlayerCount();
        uint32 const queued = World::GetWorldInstance()->GetQueueSize();

        handler->PSendSysMessage("|cff00ff00[GStats]|r Server:");
        handler->PSendSysMessage("  Uptime: {}", FormatUptime(uptime));
        handler->PSendSysMessage("  Sessions: {} | Players: {} | Queued: {}",
                                 sessions, players, queued);
        handler->PSendSysMessage("  Online peak (since restart): {}", s_peakOnline);

        AuditCommand(gm, "server");
        return true;
    }

    // ============ .gstats player <имя> ============
    bool HandlePlayerCommand(ChatHandler* handler, std::string const& playerName)
    {
        Player* gm = handler->GetSession()->GetPlayer();

        Player* target = ObjectAccessor::FindPlayerByName(playerName, false);
        if (!target)
        {
            // Игрок офлайн — базовая сводка из characters DB.
            uint32 guid = 0;
            if (QueryResult res = CharacterDatabase.Query(
                    "SELECT guid FROM characters WHERE name = '{}'", playerName))
            {
                guid = (*res)[0].Get<uint32>();
            }

            if (!guid)
            {
                handler->SendSysMessage("|cffff3030[GStats]|r Player not found.");
                return true;
            }

            handler->PSendSysMessage("|cffffcc00[GStats]|r {} (offline):", playerName);
            if (QueryResult res = CharacterDatabase.Query(
                    "SELECT totaltime, money, totalKills FROM characters WHERE guid = {}",
                    guid))
            {
                handler->PSendSysMessage("  Total played: {}h | Money: {}g | Honor kills: {}",
                                         (*res)[0].Get<uint32>() / 3600,
                                         (*res)[1].Get<uint64>() / 10000,
                                         (*res)[2].Get<uint32>());
            }
            AuditCommand(gm, "player", playerName);
            return true;
        }

        handler->PSendSysMessage("|cffffcc00[GStats]|r {} (online):", target->GetName());
        handler->PSendSysMessage("  Level {} | Zone: {}",
                                 target->GetLevel(),
                                 target->GetZoneId());
        handler->PSendSysMessage("  Played total: {}h | This level: {}h",
                                 target->GetTotalPlayedTime() / 3600,
                                 target->GetLevelPlayedTime() / 3600);
        handler->PSendSysMessage("  Money: {}g | Honor kills: {}",
                                 target->GetMoney() / 10000,
                                 target->GetHonorKills());

        AuditCommand(gm, "player", playerName);
        return true;
    }

    // ============ .gstats top ============
    bool HandleTopCommand(ChatHandler* handler)
    {
        Player* gm = handler->GetSession()->GetPlayer();
        uint32 const limit = Top::Limit;

        handler->PSendSysMessage("|cffffcc00[GStats]|r Top {} by played time:", limit);
        if (QueryResult res = CharacterDatabase.Query(
                "SELECT name, totaltime FROM characters ORDER BY totaltime DESC LIMIT {}", limit))
        {
            uint32 i = 1;
            do
            {
                handler->PSendSysMessage("  {}. {} ({}h)",
                                         i++, (*res)[0].Get<std::string>(),
                                         (*res)[1].Get<uint32>() / 3600);
            } while (res->NextRow());
        }

        handler->PSendSysMessage("|cffffcc00[GStats]|r Top {} richest:", limit);
        if (QueryResult res = CharacterDatabase.Query(
                "SELECT name, money FROM characters ORDER BY money DESC LIMIT {}", limit))
        {
            uint32 i = 1;
            do
            {
                handler->PSendSysMessage("  {}. {} ({}g)",
                                         i++, (*res)[0].Get<std::string>(),
                                         (*res)[1].Get<uint64>() / 10000);
            } while (res->NextRow());
        }

        handler->PSendSysMessage("|cffffcc00[GStats]|r Top {} by honor kills:", limit);
        if (QueryResult res = CharacterDatabase.Query(
                "SELECT name, totalKills FROM characters ORDER BY totalKills DESC LIMIT {}", limit))
        {
            uint32 i = 1;
            do
            {
                handler->PSendSysMessage("  {}. {} ({})",
                                         i++, (*res)[0].Get<std::string>(),
                                         (*res)[1].Get<uint32>());
            } while (res->NextRow());
        }

        AuditCommand(gm, "top");
        return true;
    }

    // ============ .gstats zone ============
    bool HandleZoneCommand(ChatHandler* handler)
    {
        Player* gm = handler->GetSession()->GetPlayer();
        Map* map = gm->GetMap();
        uint32 const zoneId = gm->GetZoneId();

        uint32 players = 0, creaturesAlive = 0, creaturesDead = 0, gameObjects = 0;

        // Игроки: перебор сессий карты (лёгкий, без grid-обхода).
        for (auto const& [guid, player] : map->GetPlayers())
        {
            if (player->GetZoneId() == zoneId)
                ++players;
        }

        // Существа и GO: кэш карты (без grid-visit — статичный подсчёт,
        // safe для зоны с большим числом объектов).
        for (auto const& [guid, creature] : map->GetCreatureMap())
        {
            if (creature->GetZoneId() != zoneId)
                continue;

            creature->IsAlive() ? ++creaturesAlive : ++creaturesDead;
        }

        for (auto const& [guid, go] : map->GetGameObjectMap())
        {
            if (go->GetZoneId() == zoneId)
                ++gameObjects;
        }

        handler->PSendSysMessage("|cffffcc00[GStats]|r Zone {}:", zoneId);
        handler->PSendSysMessage("  Players: {} | Creatures: {} alive / {} dead | GOs: {}",
                                 players, creaturesAlive, creaturesDead, gameObjects);

        AuditCommand(gm, "zone");
        return true;
    }

    // ============ .gstats log [дней] ============
    bool HandleLogCommand(ChatHandler* handler, uint32 days)
    {
        Player* gm = handler->GetSession()->GetPlayer();

        if (days == 0)
            days = 1;
        if (days > 30)
            days = 30;

        handler->PSendSysMessage("|cffffcc00[GStats]|r Audit log, last {} day(s):", days);
        if (QueryResult res = CharacterDatabase.Query(
                "SELECT gm_name, command, used_at FROM mod_gm_stats_audit "
                "WHERE used_at >= NOW() - INTERVAL {} DAY ORDER BY used_at DESC LIMIT 50",
                days))
        {
            do
            {
                handler->PSendSysMessage("  [{}] {} -> .gstats {}",
                                         (*res)[2].Get<std::string>(),
                                         (*res)[0].Get<std::string>(),
                                         (*res)[1].Get<std::string>());
            } while (res->NextRow());
        }
        else
        {
            handler->SendSysMessage("  (empty)");
        }

        AuditCommand(gm, "log", std::to_string(days));
        return true;
    }
}

// ============ ChatCommandScript ============
class GStatsCommandScript : public CommandScript
{
public:
    GStatsCommandScript() : CommandScript("GStatsCommandScript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "server", HandleServerCommand, SEC_GAMEMASTER, Console::No },
            { "player", HandlePlayerCommand, SEC_GAMEMASTER, Console::No },
            { "top",    HandleTopCommand,    SEC_GAMEMASTER, Console::No },
            { "zone",   HandleZoneCommand,   SEC_GAMEMASTER, Console::No },
            { "log",    HandleLogCommand,    SEC_GAMEMASTER, Console::No },
        };
        return commandTable;
    }
};

// ============ WorldScript: пик онлайна ============
class GStatsWorldScript : public WorldScript
{
public:
    GStatsWorldScript() : WorldScript("GStatsWorldScript") {}

    void OnUpdate(uint32 diff) override
    {
        static std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - last >= std::chrono::minutes(1))
        {
            last = now;
            UpdatePeakOnline();
        }
    }
};

void AddSC_GMStatsScripts()
{
    new GStatsCommandScript();
    new GStatsWorldScript();
}
