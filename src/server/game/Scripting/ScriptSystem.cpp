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

#include "ScriptSystem.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "StopWatch.h"

ScriptPointVector const SystemMgr::_empty;

SystemMgr* SystemMgr::instance()
{
    static SystemMgr instance;
    return &instance;
}

void SystemMgr::LoadScriptWaypoints()
{
    StopWatch sw;

    // Drop Existing Waypoint list
    m_mPointMoveMap.clear();

    uint64 uiCreatureCount = 0;

    // Load Waypoints
    QueryResult result = WorldDatabase.Query("SELECT COUNT(entry) FROM script_waypoint GROUP BY entry");
    if (result)
        uiCreatureCount = result->GetRowCount();

    LOG_INFO("server.loading", "> Loading Script Waypoints for {} creature(s)...", uiCreatureCount);

    //                                     0       1         2           3           4           5
    result = WorldDatabase.Query("SELECT entry, pointid, location_x, location_y, location_z, waittime FROM script_waypoint ORDER BY pointid");

    if (!result)
    {
        LOG_WARN("server.loading", ">> Loaded 0 Script Waypoints. DB table `script_waypoint` is empty.");
        LOG_INFO("server.loading", "");
        return;
    }

    uint32 count = 0;

    do
    {
        auto pFields = result->Fetch();
        ScriptPointMove temp;

        temp.uiCreatureEntry   = pFields[0].Get<uint32>();
        uint32 uiEntry          = temp.uiCreatureEntry;
        temp.uiPointId         = pFields[1].Get<uint32>();
        temp.fX                = pFields[2].Get<float>();
        temp.fY                = pFields[3].Get<float>();
        temp.fZ                = pFields[4].Get<float>();
        temp.uiWaitTime        = pFields[5].Get<uint32>();

        CreatureTemplate const* pCInfo = sObjectMgr->GetCreatureTemplate(temp.uiCreatureEntry);

        if (!pCInfo)
        {
            LOG_ERROR("db.query", "DB table script_waypoint has waypoint for non-existant creature entry {}", temp.uiCreatureEntry);
            continue;
        }

        if (!pCInfo->ScriptID)
            LOG_ERROR("db.query", "DB table script_waypoint has waypoint for creature entry {}, but creature does not have ScriptName defined and then useless.", temp.uiCreatureEntry);

        m_mPointMoveMap[uiEntry].push_back(temp);
        ++count;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} Script Waypoint nodes in {}", count, sw);
    LOG_INFO("server.loading", "");
}
