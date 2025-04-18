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

/** \file
    \ingroup world
*/

#include "WeatherMgr.h"
#include "DBCacheMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "MiscPackets.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "StopWatch.h"
#include "Weather.h"
#include <memory>

namespace WeatherMgr
{
    namespace
    {
        typedef std::unordered_map<uint32, std::unique_ptr<Weather>> WeatherMap;
        typedef std::unordered_map<uint32, WeatherData> WeatherZoneMap;

        WeatherMap m_weathers;
        WeatherZoneMap mWeatherZoneMap;

        WeatherData const* GetWeatherData(uint32 zone_id)
        {
            WeatherZoneMap::const_iterator itr = mWeatherZoneMap.find(zone_id);
            return (itr != mWeatherZoneMap.end()) ? &itr->second : nullptr;
        }
    }

    /// Find a Weather object by the given zoneid
    Weather* FindWeather(uint32 id)
    {
        WeatherMap::const_iterator itr = m_weathers.find(id);
        return (itr != m_weathers.end()) ? itr->second.get() : 0;
    }

    /// Remove a Weather object for the given zoneid
    void RemoveWeather(uint32 id)
    {
        // not called at the moment. Kept for completeness
        WeatherMap::iterator itr = m_weathers.find(id);

        if (itr != m_weathers.end())
            m_weathers.erase(itr);
    }

    /// Add a Weather object to the list
    Weather* AddWeather(uint32 zone_id)
    {
        WeatherData const* weatherChances = GetWeatherData(zone_id);

        // zone does not have weather, ignore
        if (!weatherChances)
            return nullptr;

        Weather* w = new Weather(zone_id, weatherChances);
        m_weathers[w->GetZone()].reset(w);
        w->ReGenerate();
        w->UpdateWeather();

        return w;
    }

    void LoadWeatherData()
    {
        StopWatch sw;

        auto result{ sDBCacheMgr->GetResult(DBCacheTable::GameWeather) };
        if (!result)
        {
            LOG_WARN("server.loading", ">> Loaded 0 weather definitions. DB table `game_weather` is empty.");
            LOG_INFO("server.loading", "");
            return;
        }

        uint32 count = 0;

        do
        {
            auto fields = result->Fetch();

            uint32 zone_id = fields[0].Get<uint32>();

            WeatherData& wzc = mWeatherZoneMap[zone_id];

            for (uint8 season = 0; season < WEATHER_SEASONS; ++season)
            {
                wzc.data[season].rainChance  = fields[season * (MAX_WEATHER_TYPE - 1) + 1].Get<uint8>();
                wzc.data[season].snowChance  = fields[season * (MAX_WEATHER_TYPE - 1) + 2].Get<uint8>();
                wzc.data[season].stormChance = fields[season * (MAX_WEATHER_TYPE - 1) + 3].Get<uint8>();

                if (wzc.data[season].rainChance > 100)
                {
                    wzc.data[season].rainChance = 25;
                    LOG_ERROR("db.query", "Weather for zone {} season {} has wrong rain chance > 100%", zone_id, season);
                }

                if (wzc.data[season].snowChance > 100)
                {
                    wzc.data[season].snowChance = 25;
                    LOG_ERROR("db.query", "Weather for zone {} season {} has wrong snow chance > 100%", zone_id, season);
                }

                if (wzc.data[season].stormChance > 100)
                {
                    wzc.data[season].stormChance = 25;
                    LOG_ERROR("db.query", "Weather for zone {} season {} has wrong storm chance > 100%", zone_id, season);
                }
            }

            wzc.ScriptId = sObjectMgr->GetScriptId(fields[13].Get<std::string>());

            ++count;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> Loaded {} Weather Definitions in {}", count, sw);
        LOG_INFO("server.loading", "");
    }

    void SendFineWeatherUpdateToPlayer(Player* player)
    {
        WorldPackets::Misc::Weather weather(WEATHER_STATE_FINE);
        player->SendDirectMessage(weather.Write());
    }

    void Update(uint32 diff)
    {
        ///- Send an update signal to Weather objects
        WeatherMap::iterator itr, next;
        for (itr = m_weathers.begin(); itr != m_weathers.end(); itr = next)
        {
            next = itr;
            ++next;

            ///- and remove Weather objects for zones with no player
            // As interval > WorldTick
            if (!itr->second->Update(diff))
                m_weathers.erase(itr);
        }
    }

} // namespace
