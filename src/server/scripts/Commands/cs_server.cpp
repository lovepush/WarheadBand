/*
 * This file is part of the WarheadCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

 /* ScriptData
 Name: server_commandscript
 %Complete: 100
 Comment: All server related commands
 Category: commandscripts
 EndScriptData */

#include "Chat.h"
#include "DatabaseEnv.h"
#include "DatabaseMgr.h"
#include "GameConfig.h"
#include "GameTime.h"
#include "GitRevision.h"
#include "Language.h"
#include "ModuleMgr.h"
#include "Player.h"
#include "Realm.h"
#include "ScriptObject.h"
#include "ServerMotd.h"
#include "StringConvert.h"
#include "Timer.h"
#include "UpdateTime.h"
#include "VMapFactory.h"
#include "VMapMgr2.h"
#include <boost/version.hpp>
#include <filesystem>
#include <numeric>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>

using namespace Warhead::ChatCommands;

class server_commandscript : public CommandScript
{
public:
    server_commandscript() : CommandScript("server_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable serverIdleRestartCommandTable =
        {
            { "cancel",       HandleServerShutDownCancelCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "",             HandleServerIdleRestartCommand,    SEC_CONSOLE,       Console::Yes }
        };

        static ChatCommandTable serverIdleShutdownCommandTable =
        {
            { "cancel",       HandleServerShutDownCancelCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "",             HandleServerIdleShutDownCommand,   SEC_CONSOLE,       Console::Yes }
        };

        static ChatCommandTable serverRestartCommandTable =
        {
            { "cancel",       HandleServerShutDownCancelCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "",             HandleServerRestartCommand,        SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable serverShutdownCommandTable =
        {
            { "cancel",       HandleServerShutDownCancelCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "",             HandleServerShutDownCommand,       SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable serverSetCommandTable =
        {
            { "difftime",     HandleServerSetDiffTimeCommand,    SEC_CONSOLE,       Console::Yes },
            { "motd",         HandleServerSetMotdCommand,        SEC_ADMINISTRATOR, Console::Yes },
            { "closed",       HandleServerSetClosedCommand,      SEC_CONSOLE,       Console::Yes }
        };

        static ChatCommandTable serverCommandTable =
        {
            { "corpses",      HandleServerCorpsesCommand,        SEC_GAMEMASTER,    Console::Yes },
            { "debug",        HandleServerDebugCommand,          SEC_ADMINISTRATOR, Console::Yes },
            { "exit",         HandleServerExitCommand,           SEC_CONSOLE,       Console::Yes },
            { "idlerestart",  serverIdleRestartCommandTable },
            { "idleshutdown", serverIdleShutdownCommandTable },
            { "info",         HandleServerInfoCommand,           SEC_PLAYER,        Console::Yes },
            { "motd",         HandleServerMotdCommand,           SEC_PLAYER,        Console::Yes },
            { "restart",      serverRestartCommandTable },
            { "shutdown",     serverShutdownCommandTable },
            { "set",          serverSetCommandTable }
        };

        static ChatCommandTable commandTable =
        {
            { "server", serverCommandTable }
        };

        return commandTable;
    }

    // Triggering corpses expire check in world
    static bool HandleServerCorpsesCommand(ChatHandler* /*handler*/)
    {
        sWorld->RemoveOldCorpses();
        return true;
    }

    static bool HandleServerDebugCommand(ChatHandler* handler)
    {
        uint16 worldPort = sGameConfig->GetOption<uint16>("WorldServerPort");
        std::string dbPortOutput;

        {
            uint16 dbPort = 0;
            if (QueryResult res = AuthDatabase.Query("SELECT port FROM realmlist WHERE id = {}", realm.Id.Realm))
                dbPort = (*res)[0].Get<uint16>();

            if (dbPort)
                dbPortOutput = Warhead::StringFormat("Realmlist (Realm Id: {}) configured in port %" PRIu16, realm.Id.Realm, dbPort);
            else
                dbPortOutput = Warhead::StringFormat("Realm Id: {} not found in `realmlist` table. Please check your setup", realm.Id.Realm);
        }

        handler->PSendSysMessage("{}", GitRevision::GetFullVersion());
        handler->PSendSysMessage("Using SSL version: {} (library: {})", OPENSSL_VERSION_TEXT, OpenSSL_version(OPENSSL_VERSION));
        handler->PSendSysMessage("Using Boost version: {}.{}.{}", BOOST_VERSION / 100000, BOOST_VERSION / 100 % 1000, BOOST_VERSION % 100);
        handler->PSendSysMessage("Using MySQL version: {}", sDatabaseMgr->GetDatabaseLibraryVersion());
        handler->PSendSysMessage("Using CMake version: {}", GitRevision::GetCMakeVersion());

        handler->PSendSysMessage("Compiled on: {}", GitRevision::GetHostOSVersion());

        handler->PSendSysMessage("Worldserver listening connections on port {}" PRIu16, worldPort);
        handler->PSendSysMessage("{}", dbPortOutput);

        bool vmapIndoorCheck = CONF_GET_BOOL("vmap.enableIndoorCheck");
        bool vmapLOSCheck = VMAP::VMapFactory::createOrGetVMapMgr()->isLineOfSightCalcEnabled();
        bool vmapHeightCheck = VMAP::VMapFactory::createOrGetVMapMgr()->isHeightCalcEnabled();
        bool mmapEnabled = CONF_GET_BOOL("MoveMaps.Enable");

        std::string dataDir = sWorld->GetDataPath();
        std::vector<std::string> subDirs;
        subDirs.emplace_back("maps");
        if (vmapIndoorCheck || vmapLOSCheck || vmapHeightCheck)
        {
            handler->PSendSysMessage("VMAPs status: Enabled. LineOfSight: {}, getHeight: {}, indoorCheck: {}", vmapLOSCheck, vmapHeightCheck, vmapIndoorCheck);
            subDirs.emplace_back("vmaps");
        }
        else
            handler->SendSysMessage("VMAPs status: Disabled");

        if (mmapEnabled)
        {
            handler->SendSysMessage("MMAPs status: Enabled");
            subDirs.emplace_back("mmaps");
        }
        else
            handler->SendSysMessage("MMAPs status: Disabled");

        for (std::string const& subDir : subDirs)
        {
            std::filesystem::path mapPath(dataDir);
            mapPath /= subDir;

            if (!std::filesystem::exists(mapPath))
            {
                handler->PSendSysMessage("{} directory doesn't exist!. Using path: {}", subDir, mapPath.generic_string());
                continue;
            }

            auto end = std::filesystem::directory_iterator();
            std::size_t folderSize = std::accumulate(std::filesystem::directory_iterator(mapPath), end, std::size_t(0), [](std::size_t val, std::filesystem::path const& mapFile)
            {
                if (std::filesystem::is_regular_file(mapFile))
                    val += std::filesystem::file_size(mapFile);
                return val;
            });

            handler->PSendSysMessage("{} directory located in {}. Total size: {} bytes", subDir, mapPath.generic_string(), folderSize);
        }

        LocaleConstant defaultLocale = sWorld->GetDefaultDbcLocale();
        uint32 availableLocalesMask = (1 << defaultLocale);

        for (uint8 i = 0; i < TOTAL_LOCALES; ++i)
        {
            LocaleConstant locale = static_cast<LocaleConstant>(i);
            if (locale == defaultLocale)
                continue;

            if (sWorld->GetAvailableDbcLocale(locale) != defaultLocale)
                availableLocalesMask |= (1 << locale);
        }

        std::string availableLocales;
        for (uint8 i = 0; i < TOTAL_LOCALES; ++i)
        {
            if (!(availableLocalesMask & (1 << i)))
                continue;

            availableLocales += localeNames[i];
            if (i != TOTAL_LOCALES - 1)
                availableLocales += " ";
        }

        handler->PSendSysMessage("Using {} DBC Locale as default. All available DBC locales: {}", localeNames[defaultLocale], availableLocales);

        handler->PSendSysMessage("Using World DB: {}", sWorld->GetDBVersion());

        handler->PSendSysMessage("AuthDatabase queue size: {}", AuthDatabase.GetQueueSize());
        handler->PSendSysMessage("CharacterDatabase queue size: {}", CharacterDatabase.GetQueueSize());
        handler->PSendSysMessage("WorldDatabase queue size: {}", WorldDatabase.GetQueueSize());

        if (Warhead::Module::GetEnableModulesList().empty())
            handler->SendSysMessage("No modules enabled");
        else
            handler->SendSysMessage("> List enable modules:");

        for (auto const& modName : Warhead::Module::GetEnableModulesList())
        {
            handler->PSendSysMessage("- {}", modName);
        }

        return true;
    }

    static bool HandleServerInfoCommand(ChatHandler* handler)
    {
        std::string realmName = sWorld->GetRealmName();
        uint32 playerCount = sWorld->GetPlayerCount();
        uint32 activeSessionCount = sWorld->GetActiveSessionCount();
        uint32 queuedSessionCount = sWorld->GetQueuedSessionCount();
        uint32 connPeak = sWorld->GetMaxActiveSessionCount();
        std::string uptime = Warhead::Time::ToTimeString(GameTime::GetUptime());

        handler->PSendSysMessage("{}", GitRevision::GetFullVersion());

        if (!queuedSessionCount)
            handler->PSendSysMessage("Connected players: {}. Characters in world: {}.", activeSessionCount, playerCount);
        else
            handler->PSendSysMessage("Connected players: {}. Characters in world: {}. Queue: {}.", activeSessionCount, playerCount, queuedSessionCount);

        handler->PSendSysMessage("Connection peak: {}.", connPeak);
        handler->PSendSysMessage(LANG_UPTIME, uptime);
        handler->PSendSysMessage("Update time diff: {}ms, Average: {}ms", sWorldUpdateTime.GetLastUpdateTime(), sWorldUpdateTime.GetAverageUpdateTime());

        //! Can't use sWorld->ShutdownMsg here in case of console command
        if (sWorld->IsShuttingDown())
            handler->PSendSysMessage(LANG_SHUTDOWN_TIMELEFT, Warhead::Time::ToTimeString(Seconds(sWorld->GetShutDownTimeLeft())));

        return true;
    }
    // Display the 'Message of the day' for the realm
    static bool HandleServerMotdCommand(ChatHandler* handler)
    {
        handler->PSendSysMessage(LANG_MOTD_CURRENT, Motd::GetMotd());
        return true;
    }

    static bool HandleServerShutDownCancelCommand(ChatHandler* /*handler*/)
    {
        sWorld->ShutdownCancel();

        return true;
    }

    static bool HandleServerShutDownCommand(ChatHandler* handler, std::string_view time, Optional<int32> exitCode, Tail reason)
    {
        std::wstring wReason   = std::wstring();
        std::string  strReason = std::string();

        if (time.empty())
        {
            handler->SendSysMessage(LANG_BAD_VALUE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!reason.empty())
        {
            if (!Utf8toWStr(reason, wReason))
            {
                return false;
            }

            if (!WStrToUtf8(wReason, strReason))
            {
                return false;
            }
        }

        Seconds delay = Warhead::Time::TimeStringTo(time);
        if (delay == 0s)
        {
            handler->SendSysMessage(LANG_BAD_VALUE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (exitCode && *exitCode >= 0 && *exitCode <= 125)
        {
            sWorld->ShutdownServ(delay.count(), 0, *exitCode);
        }
        else
        {
            sWorld->ShutdownServ(delay.count(), 0, SHUTDOWN_EXIT_CODE, strReason);
        }

        return true;
    }

    static bool HandleServerRestartCommand(ChatHandler* handler, std::string_view time, Optional<int32> exitCode, Tail reason)
    {
        std::wstring wReason = std::wstring();
        std::string strReason    = std::string();

        if (time.empty())
        {
            handler->SendSysMessage(LANG_BAD_VALUE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        Seconds delay = Warhead::Time::TimeStringTo(time);

        if (!reason.empty())
        {
            if (!Utf8toWStr(reason, wReason))
            {
                return false;
            }

            if (!WStrToUtf8(wReason, strReason))
            {
                return false;
            }
        }

        if (delay == 0s)
        {
            handler->SendSysMessage(LANG_BAD_VALUE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (exitCode && *exitCode >= 0 && *exitCode <= 125)
        {
            sWorld->ShutdownServ(delay.count(), SHUTDOWN_MASK_RESTART, *exitCode);
        }
        else
        {
            sWorld->ShutdownServ(delay.count(), SHUTDOWN_MASK_RESTART, RESTART_EXIT_CODE, strReason);
        }

        return true;
    }

    static bool HandleServerIdleRestartCommand(ChatHandler* handler, std::string_view time, Optional<int32> exitCode, Tail reason)
    {
        std::wstring wReason   = std::wstring();
        std::string  strReason = std::string();

        if (time.empty())
        {
            handler->SendSysMessage(LANG_BAD_VALUE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!reason.empty())
        {
            if (!Utf8toWStr(reason, wReason))
            {
                return false;
            }

            if (!WStrToUtf8(wReason, strReason))
            {
                return false;
            }
        }

        Seconds delay = Warhead::Time::TimeStringTo(time);
        if (delay == 0s)
        {
            handler->SendSysMessage(LANG_BAD_VALUE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (exitCode && *exitCode >= 0 && *exitCode <= 125)
        {
            sWorld->ShutdownServ(delay.count(), SHUTDOWN_MASK_RESTART | SHUTDOWN_MASK_IDLE, *exitCode);
        }
        else
        {
            sWorld->ShutdownServ(delay.count(), SHUTDOWN_MASK_RESTART | SHUTDOWN_MASK_IDLE, RESTART_EXIT_CODE, strReason);
        }

        return true;
    }

    static bool HandleServerIdleShutDownCommand(ChatHandler* handler, std::string_view time, Optional<int32> exitCode, Tail reason)
    {
        std::wstring wReason   = std::wstring();
        std::string  strReason = std::string();

        if (time.empty())
        {
            return false;
        }

        if (!reason.empty())
        {
            if (!Utf8toWStr(reason, wReason))
            {
                return false;
            }

            if (!WStrToUtf8(wReason, strReason))
            {
                return false;
            }
        }

        Seconds delay = Warhead::Time::TimeStringTo(time);
        if (delay == 0s)
        {
            handler->SendSysMessage(LANG_BAD_VALUE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (exitCode && *exitCode >= 0 && *exitCode <= 125)
        {
            sWorld->ShutdownServ(delay.count(), SHUTDOWN_MASK_IDLE, *exitCode);
        }
        else
        {
            sWorld->ShutdownServ(delay.count(), SHUTDOWN_MASK_IDLE, SHUTDOWN_EXIT_CODE, strReason);
        }

        return true;
    }

    // Exit the realm
    static bool HandleServerExitCommand(ChatHandler* handler)
    {
        handler->SendSysMessage(LANG_COMMAND_EXIT);
        World::StopNow(SHUTDOWN_EXIT_CODE);
        return true;
    }

    // Define the 'Message of the day' for the realm
    static bool HandleServerSetMotdCommand(ChatHandler* handler, std::string motd)
    {
        Motd::SetMotd(motd);
        handler->PSendSysMessage(LANG_MOTD_NEW, motd);
        return true;
    }

    // Set whether we accept new clients
    static bool HandleServerSetClosedCommand(ChatHandler* handler, Optional<std::string> args)
    {
        if (StringStartsWith("on", *args))
        {
            handler->SendSysMessage(LANG_WORLD_CLOSED);
            sWorld->SetClosed(true);
            return true;
        }
        else if (StringStartsWith("off", *args))
        {
            handler->SendSysMessage(LANG_WORLD_OPENED);
            sWorld->SetClosed(false);
            return true;
        }

        handler->SendSysMessage(LANG_USE_BOL);
        handler->SetSentErrorMessage(true);
        return false;
    }

    // set diff time record interval
    static bool HandleServerSetDiffTimeCommand(ChatHandler* /*handler*/, int32 newTime)
    {
        if (newTime < 0)
            return false;

        sWorldUpdateTime.SetRecordUpdateTimeInterval(newTime);
        printf("Record diff every %u ms\n", newTime);

        return true;
    }
};

void AddSC_server_commandscript()
{
    new server_commandscript();
}
