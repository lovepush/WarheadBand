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

#ifndef _VIP_H_
#define _VIP_H_

#include "Duration.h"
#include "TaskScheduler.h"
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

class Player;
class ChatHandler;
class ObjectGuid;
class Creature;

using VipSpelLList = std::vector<uint32>;

struct VipLevelInfo
{
    uint32 Level{};
    uint32 MountSpell{};
    VipSpelLList LearnSpells;
    bool CanUseUnbindCommands{};
};

enum class VipRate
{
    XP,
    Honor,
    ArenaPoint,
    Reputation
};

class WH_GAME_API Vip
{
    Vip() = default;
    ~Vip() = default;

    Vip(Vip const&) = delete;
    Vip(Vip&&) = delete;
    Vip& operator=(Vip const&) = delete;
    Vip& operator=(Vip&&) = delete;

public:
    using WarheadVip = std::tuple<Seconds/*start*/, Seconds/*endtime*/, uint32/*level*/>;
    using WarheadVipRates = std::tuple<float/*XP*/, float/*Honor*/, float/*ArenaPoint*/, float/*Reputation*/>;

    static Vip* Instance();

    void LoadConfig();
    void InitSystem();

    inline bool IsEnable() { return _isEnable; }

    void Update(uint32 diff);
    bool Add(uint32 accountID, Seconds endTime, uint32 level, bool force = false);
    bool Delete(uint32 accountID);

    // For player targer
    void OnLoginPlayer(Player* player);
    void OnLogoutPlayer(Player* player);
    void UnSet(uint32 accountID);
    bool IsVip(Player* player);
    bool IsVip(uint32 accountID);
    uint32 GetLevel(Player* player);
    std::string GetDuration(Player* player);
    std::string GetDuration(uint32 accountID);
    void RemoveColldown(Player* player, uint32 spellID);
    void UnBindInstances(Player* player);
    void SendVipInfo(ChatHandler* handler, ObjectGuid targetGuid);
    void SendVipListRates(ChatHandler* handler);
    bool CanUsingVendor(Player* player, Creature* creature);

    // Creature
    bool IsVipVendor(uint32 entry);
    uint32 GetVendorVipLevel(uint32 entry);
    void AddVendorVipLevel(uint32 entry, uint32 vendorVipLevel);
    void DeleteVendorVipLevel(uint32 entry);

    float GetRateForPlayer(Player* player, VipRate rate);

    // Load system
    void LoadRates();
    void LoadVipVendors();
    void LoadVipLevels();

    // Info
    VipLevelInfo* GetVipLevelInfo(uint32 level);

private:
    void LoadAccounts();
    void LoadUnbinds();

    void LearnSpells(Player* player, uint32 vipLevel);
    void UnLearnSpells(Player* player, bool unlearnMount = true);
    void IterateVipSpellsForPlayer(Player* player, bool isLearn);

    WarheadVip* GetVipInfo(uint32 accountID);
    WarheadVipRates* GetVipRateInfo(uint32 vipLevel);
    Seconds* GetUndindTime(uint64 guid);
    static Player* GetPlayerFromAccount(uint32 accountID);
    static std::string GetDuration(WarheadVip* vipInfo);

    bool _isEnable{};
    Seconds _updateDelay{};
    Seconds _unbindDuration{ 1_days };
    uint32 _maxLevel{};

    std::unordered_map<uint32/*acc id*/, WarheadVip> _store;
    std::unordered_map<uint32/*level*/, WarheadVipRates> _storeRates;
    std::unordered_map<uint64/*guid*/, Seconds/*unbindtime*/> _storeUnbind;
    std::unordered_map<uint32/*creature entry*/, uint32/*vip level*/> _storeVendors;
    std::unordered_map<uint32/*vip level*/, VipLevelInfo> _vipLevelsInfo;

    TaskScheduler scheduler;
};

#define sVip Vip::Instance()

#endif /* _VIP_H_ */
