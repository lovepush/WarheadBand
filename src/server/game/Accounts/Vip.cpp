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

#include "Vip.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "ModulesConfig.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "Player.h"
#include "SpellMgr.h"
#include "StopWatch.h"
#include "TaskScheduler.h"
#include "Tokenize.h"
#include <tuple>
#include <unordered_map>

Vip* Vip::Instance()
{
    static Vip instance;
    return &instance;
}

void Vip::LoadConfig()
{
    _isEnable = MOD_CONF_GET_BOOL("VIP.Enable");
    if (!_isEnable)
        return;

    _updateDelay = Seconds(MOD_CONF_GET_UINT("VIP.Update.Delay"));
    if (_updateDelay < 10s)
    {
        LOG_ERROR("module.vip", "> Vip: Delay < 10 seconds. Set to 10 seconds");
        _updateDelay = 10s;
    }

    _unbindDuration = Seconds(MOD_CONF_GET_UINT("VIP.Unbind.Duration"));
}

void Vip::InitSystem()
{
    if (!_isEnable)
        return;

    scheduler.CancelAll();
    scheduler.Schedule(3s, [this](TaskContext context)
    {
        for (auto const& [accountID, vipInfo] : _store)
        {
            auto const& [startTime, endTime, level] = vipInfo;

            if (GameTime::GetGameTime() >= endTime)
                UnSet(accountID);
        }

        context.Repeat(_updateDelay);
    });

    LoadVipLevels();
    LoadAccounts();
    LoadUnbinds();
    LoadRates();
    LoadVipVendors();
}

void Vip::Update(uint32 diff)
{
    if (!_isEnable)
        return;

    scheduler.Update(diff);
}

bool Vip::Add(uint32 accountID, Seconds endTime, uint32 level, bool force /*= false*/)
{
    if (!_isEnable)
        return false;

    if (level > _maxLevel)
        level = _maxLevel;

    auto const& vipInfo = GetVipInfo(accountID);

    if (vipInfo)
    {
        // Set inactive all in DB
        if (force)
            Delete(accountID);
        else // Add error
        {
            LOG_ERROR("module.vip", "> Vip: Account {} is premium. {}", accountID, *vipInfo);
            return false;
        }
    }

    auto timeNow = GameTime::GetGameTime();

    _store.emplace(accountID, std::make_tuple(timeNow, endTime, level));

    // Add DB
    AuthDatabase.Execute("INSERT INTO `account_premium` (`AccountID`, `StartTime`, `EndTime`, `Level`, `IsActive`) VALUES ({}, FROM_UNIXTIME({}), FROM_UNIXTIME({}), {}, 1)",
        accountID, timeNow.count(), endTime.count(), level);

    if (auto player = GetPlayerFromAccount(accountID))
    {
        ChatHandler(player->GetSession()).PSendSysMessage("> У вас обновление премиум статуса. Уровень {}. Окончание через {}", level, Warhead::Time::ToTimeString(endTime - GameTime::GetGameTime(), 4, TimeFormat::FullText));
        LearnSpells(player, level);
    }

    return true;
}

bool Vip::Delete(uint32 accountID)
{
    if (!_isEnable)
        return false;

    auto const& vipInfo = GetVipInfo(accountID);
    if (!vipInfo)
    {
        LOG_ERROR("module.vip", "> Vip: Account {} is not premium", accountID);
        return false;
    }

    if (auto player = GetPlayerFromAccount(accountID))
        UnLearnSpells(player);

    // Set inactive in DB
    AuthDatabase.Execute("UPDATE `account_premium` SET `IsActive` = 0 WHERE `AccountID` = {} AND `IsActive` = 1",
        accountID);

    _store.erase(accountID);
    return true;
}

void Vip::OnLoginPlayer(Player* player)
{
    if (!_isEnable)
        return;

    UnLearnSpells(player);

    if (!IsVip(player))
        return;

    ChatHandler handler(player->GetSession());
    uint8 vipLevel = GetLevel(player);

    handler.PSendSysMessage("|cffff0000#|r --");
    handler.PSendSysMessage("|cffff0000#|r |cff00ff00Привет,|r {}!", player->GetName());
    handler.PSendSysMessage("|cffff0000#|r |cff00ff00Ваш уровень премиум аккаунта:|r {}", vipLevel);
    handler.PSendSysMessage("|cffff0000#|r |cff00ff00Оставшееся время:|r {}", GetDuration(player));

    LearnSpells(player, vipLevel);
}

void Vip::OnLogoutPlayer(Player* player)
{
    if (!_isEnable)
        return;

//    UnLearnSpells(player, false);
}

void Vip::UnSet(uint32 accountID)
{
    if (!_isEnable)
        return;

    if (auto targetSession = sWorld->FindSession(accountID))
        ChatHandler(targetSession).PSendSysMessage("> Вы лишены статуса премиум аккаунта");

    Delete(accountID);
}

bool Vip::IsVip(Player* player)
{
    if (!player)
        return false;

    return GetVipInfo(player->GetSession()->GetAccountId()) != nullptr;
}

bool Vip::IsVip(uint32 accountID)
{
    if (!accountID)
        return false;

    return GetVipInfo(accountID) != nullptr;
}

uint32 Vip::GetLevel(Player* player)
{
    if (!player)
        return 0;

    auto accountID = player->GetSession()->GetAccountId();
    auto const& vipInfo = GetVipInfo(accountID);
    if (!vipInfo)
        return 0;

    return std::get<2>(*vipInfo);
}

float Vip::GetRateForPlayer(Player* player, VipRate rate)
{
    if (!IsVip(player))
        return 1.0f;

    auto const& level = GetLevel(player);
    auto const& vipRateInfo = GetVipRateInfo(level);

    if (!vipRateInfo)
    {
        auto accountID = player->GetSession()->GetAccountId();
        LOG_ERROR("module.vip", "> Vip: Vip Account {} [{}] is incorrect vip level {}. {}", accountID, *GetVipInfo(accountID), level);
        return 1.0f;
    }

    auto const& [rateXP, rateHonor, rateAP, rateRep] = *vipRateInfo;

    switch (rate)
    {
    case VipRate::XP:
        return rateXP;
    case VipRate::Honor:
        return rateHonor;
    case VipRate::ArenaPoint:
        return rateAP;
    case VipRate::Reputation:
        return rateRep;
    default:
        ABORT();
        return 1.0f;
    }
}

std::string Vip::GetDuration(Player* player)
{
    if (!player)
        return "<неизвестно>";

    return GetDuration(player->GetSession()->GetAccountId());
}

std::string Vip::GetDuration(uint32 accountID)
{
    return GetDuration(GetVipInfo(accountID));
}

void Vip::RemoveColldown(Player* player, uint32 spellID)
{
    scheduler.Schedule(1s, [player, spellID](TaskContext /*context*/)
    {
        if (player && player->HasSpellCooldown(spellID))
            player->RemoveSpellCooldown(spellID, true);
    });
}

void Vip::UnBindInstances(Player* player)
{
    if (!player || !_isEnable)
        return;

    ChatHandler handler(player->GetSession());

    if (!IsVip(player))
    {
        handler.PSendSysMessage("> У вас нет премиум статуса");
        return;
    }

    auto level{ GetLevel(player) };

    auto levelInfo{ GetVipLevelInfo(level) };
    if (!levelInfo)
    {
        handler.PSendSysMessage("> Ошибка в уровне премиума: Не найдена информация для уровня {}", level);
        return;
    }

    if (!levelInfo->CanUseUnbindCommands)
    {
        handler.PSendSysMessage("> Ошибка в уровне премиума: Вы не можете использовать эту команду");
        return;
    }

    auto guid = player->GetGUID().GetRawValue();

    if (auto unbindInfo = GetUndindTime(guid))
    {
        auto duration = GameTime::GetGameTime() - *unbindInfo;

        if (duration < _unbindDuration)
        {
            handler.PSendSysMessage("> Это можно сделать через: {}", Warhead::Time::ToTimeString(_unbindDuration - duration));
            return;
        }
    }

    auto GetMapName = [](uint32 mapID, uint8 localeIndex)
    {
        MapEntry const* map = sMapStore.LookupEntry(mapID);
        if (!map)
            return "Неизвестно";

        return map->name[localeIndex];
    };

    auto GetDiffName = [](Difficulty diff, bool isRaid)
    {
        if (isRaid)
        {
            switch (diff)
            {
            case RAID_DIFFICULTY_10MAN_NORMAL:
                return "10 об.";
            case RAID_DIFFICULTY_25MAN_NORMAL:
                return "25 об.";
            case RAID_DIFFICULTY_10MAN_HEROIC:
                return "10 гер.";
            case RAID_DIFFICULTY_25MAN_HEROIC:
                return "25 гер.";
            default:
                break;
            }
        }
        else // if dungeon
        {
            switch (diff)
            {
            case DUNGEON_DIFFICULTY_NORMAL:
                return "5 об.";
            case DUNGEON_DIFFICULTY_HEROIC:
                return "5 гер.";
            default:
                break;
            }
        }

        return "###";
    };

    uint32 count = 0;

    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
    {
        auto diff{ Difficulty(i) };

        for (auto const& [mapID, bind] : sInstanceSaveMgr->PlayerGetBoundInstances(player->GetGUID(), diff))
        {
            if (player->GetMapId() == mapID)
                continue;

            handler.PSendSysMessage("> {} {}", GetMapName(mapID, handler.GetSessionDbLocaleIndex()), GetDiffName(diff, bind.save->GetMapEntry()->IsRaid()));
            sInstanceSaveMgr->PlayerUnbindInstance(player->GetGUID(), mapID, diff, true, player);
            CharacterDatabase.Execute("REPLACE INTO `vip_unbind` (`PlayerGuid`) VALUES ({})", player->GetGUID().GetRawValue());
            count++;
        }
    }

    if (!count)
    {
        handler.PSendSysMessage("> Нечего сбрасывать");
        return;
    }

    _storeUnbind.emplace(guid, GameTime::GetGameTime());
}

void Vip::LoadRates()
{
    StopWatch sw;
    _storeRates.clear(); // for reload case

    LOG_INFO("module.vip", "Loading vip rates...");

    QueryResult result = CharacterDatabase.Query("SELECT VipLevel, RateXp, RateHonor, RateArenaPoint, RateReputation FROM vip_rates");
    if (!result)
    {
        LOG_WARN("module.vip", ">> Loaded 0 Vip rates. DB table `vip_rates` is empty.");
        LOG_INFO("module.vip", "");
        return;
    }

    auto CheckRate = [](uint8 level, std::initializer_list<float> rates)
    {
        for (auto const& rate : rates)
        {
            if (rate <= 1.0f)
            {
                LOG_ERROR("module.vip", "> Vip: Invalid rate value ({}) for level ({})", rate, level);
                return false;
            }
        }

        return true;
    };

    for (auto& row : *result)
    {
        auto const& [level, rateXP, rateHonor, rateArenaPoint, rateReputation] =
            row.FetchTuple<uint8, float, float, float, float>();

        if (!CheckRate(level, { rateXP, rateHonor, rateArenaPoint, rateReputation }))
            continue;

        auto rates = std::make_tuple(rateXP, rateHonor, rateArenaPoint, rateReputation);

        _storeRates.emplace(level, rates);
    }

    LOG_INFO("module.vip", ">> Loaded {} vip rates in {}", _storeRates.size(), sw);
    LOG_INFO("module.vip", "");
}

void Vip::LoadAccounts()
{
    StopWatch sw;
    _store.clear(); // for reload case

    LOG_INFO("module.vip", "Load vip accounts...");

    QueryResult result = AuthDatabase.Query("SELECT AccountID, UNIX_TIMESTAMP(StartTime), UNIX_TIMESTAMP(EndTime), Level FROM account_premium WHERE IsActive = 1");
    if (!result)
    {
        LOG_INFO("module.vip", ">> Loaded 0 vip accounts. DB table `account_premium` is empty.");
        LOG_INFO("module.vip", "");
        return;
    }

    do
    {
        auto const& [accountID, startTime, endTime, level] = result->FetchTuple<uint32, Seconds, Seconds, uint8>();

        if (level > _maxLevel)
        {
            LOG_ERROR("module.vip", "> Account {} has a incorrect vip level of {}. Max vip level {}. Skip", level, _maxLevel);
            continue;
        }

        _store.emplace(accountID, std::make_tuple(startTime, endTime, level));

    } while (result->NextRow());

    LOG_INFO("module.vip", ">> Loaded {} vip accounts in {}", _store.size(), sw);
    LOG_INFO("module.vip", "");
}

void Vip::LoadUnbinds()
{
    StopWatch sw;
    _storeUnbind.clear(); // for reload case

    LOG_INFO("module.vip", "Load vip unbinds...");

    QueryResult result = CharacterDatabase.Query("SELECT PlayerGuid, UNIX_TIMESTAMP(UnbindTime) FROM vip_unbind");
    if (!result)
    {
        LOG_WARN("module.vip", ">> Loaded 0 vip unbinds. DB table `vip_unbind` is empty.");
        LOG_INFO("module.vip", "");
        return;
    }

    for (auto& row : *result)
    {
        auto const& [guid, unbindTime] = row.FetchTuple<uint64, uint64>();
        _storeUnbind.emplace(guid, unbindTime);
    }

    LOG_INFO("module.vip", ">> Loaded {} vip unbinds in {}", _storeUnbind.size(), sw);
    LOG_INFO("module.vip", "");
}

void Vip::LoadVipVendors()
{
    StopWatch sw;
    _storeVendors.clear(); // for reload case

    LOG_INFO("module.vip", "Load vip vendors...");

    QueryResult result = WorldDatabase.Query("SELECT CreatureEntry, VipLevel FROM vip_vendors");
    if (!result)
    {
        LOG_WARN("module.vip", ">> Loaded 0 vip vendors. DB table `vip_unbind` is empty.");
        LOG_INFO("module.vip", "");
        return;
    }

    for (auto& row : *result)
    {
        auto const& [creatureEntry, vipLevel] = row.FetchTuple<uint32, uint8>();

        CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(creatureEntry);
        if (!creatureTemplate)
        {
            LOG_ERROR("module.vip", "> Vip: Non existing creature entry {}. Skip", creatureEntry);
            continue;
        }

        if (!(creatureTemplate->npcflag & UNIT_NPC_FLAG_VENDOR))
        {
            LOG_ERROR("module.vip", "> Vip: Creature entry {} is not vendor. Skip", creatureEntry);
            continue;
        }

        if (!vipLevel || vipLevel > _maxLevel)
        {
            LOG_ERROR("module.vip", "> Vip: Creature entry {} have {} vip level. Skip", creatureEntry, vipLevel);
            continue;
        }

        _storeVendors.emplace(creatureEntry, vipLevel);
    }

    LOG_INFO("module.vip", ">> Loaded {} vip vendors in {}", _storeVendors.size(), sw);
    LOG_INFO("module.vip", "");
}

void Vip::LoadVipLevels()
{
    StopWatch sw;
    _vipLevelsInfo.clear(); // for reload case

    LOG_INFO("module.vip", "Load vip levels info...");

    QueryResult result = WorldDatabase.Query("SELECT `Level`, `MountSpell`, `LearnSpells`, `CanUseUnbindCommand` FROM `wh_vip_levels_config` ORDER BY `Level` ASC");
    if (!result)
    {
        LOG_WARN("module.vip", ">> Loaded 0 vip levels info. DB table `vip_unbind` is empty.");
        LOG_INFO("module.vip", "");
        return;
    }

    for (auto const& fields : *result)
    {
        VipLevelInfo levelInfo{};
        levelInfo.Level = fields[0].Get<uint32>();

        if (!levelInfo.Level)
        {
            LOG_ERROR("module.vip", "> Vip: Level cannot be 0. Skip");
            continue;
        }

        if (levelInfo.Level > _maxLevel)
            _maxLevel = levelInfo.Level;

        levelInfo.MountSpell = fields[1].Get<uint32>();

        if (levelInfo.MountSpell && !sSpellMgr->GetSpellInfo(levelInfo.MountSpell))
        {
            LOG_ERROR("module.vip", "> Vip: Can't find mount spell: {}. Skip", levelInfo.MountSpell);
            continue;
        }

        if (!fields[2].IsNull())
        {
            auto spellList = fields[2].Get<std::string_view>();
            for (auto const& spellString : Warhead::Tokenize(spellList, ',', false))
            {
                auto spellID = Warhead::StringTo<uint32>(spellString);
                if (!spellID)
                {
                    LOG_ERROR("module.vip", "> Vip: Incorrect spell '{}' in vip spell list ({}) for level {}. Skip", spellString, spellList, levelInfo.Level);
                    continue;
                }

                if (!sSpellMgr->GetSpellInfo(*spellID))
                {
                    LOG_ERROR("module.vip", "> Vip: Can't find spell {} in spell store. Skip", *spellID);
                    continue;
                }

                levelInfo.LearnSpells.emplace_back(*spellID);
            }
        }

        levelInfo.CanUseUnbindCommands = fields[3].Get<bool>();
        _vipLevelsInfo.emplace(levelInfo.Level, std::move(levelInfo));
    }

    LOG_INFO("module.vip", ">> Loaded {} vip levels info in {}", _vipLevelsInfo.size(), sw);
    LOG_INFO("module.vip", "");
}

void Vip::LearnSpells(Player* player, uint32 vipLevel)
{
    if (!_isEnable)
        return;

    if (!player)
    {
        LOG_ERROR("module.vip", "> Vip::LearnSpells: Not found player");
        return;
    }

    if (!IsVip(player))
    {
        LOG_ERROR("module.vip", "> Vip::LearnSpells: Player {} is no vip", player->GetGUID().ToString());
        return;
    }

    IterateVipSpellsForPlayer(player, true);
}

void Vip::UnLearnSpells(Player* player, bool unlearnMount /*= true*/)
{
    if (!_isEnable)
        return;

    if (!player)
    {
        LOG_ERROR("module.vip", "> Vip::UnLearnSpells: Not found player");
        return;
    }

    IterateVipSpellsForPlayer(player, false);
}

void Vip::SendVipInfo(ChatHandler* handler, ObjectGuid targetGuid)
{
    auto data = sCharacterCache->GetCharacterCacheByGuid(targetGuid);
    if (!data)
    {
        handler->PSendSysMessage("# Игрок не найден");
        return;
    }

    auto const& vipInfo = GetVipInfo(data->AccountId);

    if (!vipInfo)
        handler->PSendSysMessage("# Игрок: {} не является випом", data->Name);
    else
    {
        handler->PSendSysMessage("# Игрок: {}", data->Name);
        handler->PSendSysMessage("# Уровень премиум аккаунта: {}", std::get<2>(*vipInfo));
        handler->PSendSysMessage("# Оставшееся время: {}", GetDuration(vipInfo));

        if (auto const& vipRateInfo = GetVipRateInfo(std::get<2>(*vipInfo)))
        {
            handler->PSendSysMessage("# Премиум рейты:");
            handler->PSendSysMessage("# Рейтинг получения опыта: {}", std::get<0>(*vipRateInfo));
            handler->PSendSysMessage("# Рейтинг получения чести: {}", std::get<1>(*vipRateInfo));
            handler->PSendSysMessage("# Рейтинг получения арены: {}", std::get<2>(*vipRateInfo));
            handler->PSendSysMessage("# Рейтинг получения репутации: {}", std::get<3>(*vipRateInfo));
        }
    }
}

void Vip::SendVipListRates(ChatHandler* handler)
{
    handler->PSendSysMessage("# Премиум рейты для разных уровней вип:");

    if (_storeRates.empty())
    {
        handler->PSendSysMessage("# Премиум рейты не обнаружены");
        return;
    }

    for (auto const& [vipLevel, vipRates] : _storeRates)
    {
        handler->PSendSysMessage("# Рейты для премиум уровня: {}", vipLevel);
        handler->PSendSysMessage("# Рейтинг получения опыта: {}", std::get<0>(vipRates));
        handler->PSendSysMessage("# Рейтинг получения чести: {}", std::get<1>(vipRates));
        handler->PSendSysMessage("# Рейтинг получения арены: {}", std::get<2>(vipRates));
        handler->PSendSysMessage("# Рейтинг получения репутации: {}", std::get<3>(vipRates));
        handler->PSendSysMessage("# --");
    }
}

bool Vip::CanUsingVendor(Player* player, Creature* creature)
{
    if (!_isEnable)
        return true;

    auto const& creatureEntry = creature->GetEntry();
    bool isVipVendor = IsVipVendor(creatureEntry);

    if (!isVipVendor)
        return true;

    if (!IsVip(player))
        return false;

    return GetLevel(player) >= GetVendorVipLevel(creatureEntry);
}

bool Vip::IsVipVendor(uint32 entry)
{
    return _storeVendors.contains(entry);
}

uint32 Vip::GetVendorVipLevel(uint32 entry)
{
    auto const& itr = _storeVendors.find(entry);
    return itr != _storeVendors.end() ? itr->second : 0;
}

void Vip::AddVendorVipLevel(uint32 entry, uint32 vendorVipLevel)
{
    auto const& itr = _storeVendors.find(entry);
    if (itr != _storeVendors.end())
    {
        LOG_WARN("modules.vip", "> Vip::AddVendorVipLevel: Creature {} is vip vendor, level {}. Erase", entry, itr->second);
        _storeVendors.erase(entry);

        // Del from DB
        WorldDatabase.Execute("DELETE FROM `vip_vendors` WHERE `CreatureEntry` = {}", entry);
    }

    _storeVendors.emplace(entry, vendorVipLevel);

    // Add to DB
    WorldDatabase.Execute("INSERT INTO `vip_vendors` (`CreatureEntry`, `VipLevel`) VALUES({}, {})", entry, vendorVipLevel);
}

void Vip::DeleteVendorVipLevel(uint32 entry)
{
    if (!_isEnable || !IsVipVendor(entry))
        return;

    _storeVendors.erase(entry);

    // Del from DB
    WorldDatabase.Execute("DELETE FROM `vip_vendors` WHERE `CreatureEntry` = {}", entry);
}

Vip::WarheadVip* Vip::GetVipInfo(uint32 accountID)
{
    return Warhead::Containers::MapGetValuePtr(_store, accountID);
}

Vip::WarheadVipRates* Vip::GetVipRateInfo(uint32 vipLevel)
{
    return Warhead::Containers::MapGetValuePtr(_storeRates, vipLevel);
}

Seconds* Vip::GetUndindTime(uint64 guid)
{
    return Warhead::Containers::MapGetValuePtr(_storeUnbind, guid);
}

Player* Vip::GetPlayerFromAccount(uint32 accountID)
{
    auto session = sWorld->FindSession(accountID);
    if (!session)
        return nullptr;

    return session->GetPlayer();
}

std::string Vip::GetDuration(WarheadVip* vipInfo)
{
    if (!vipInfo)
        return "<неизвестно>";

    auto const& [startTime, endTime, level] = *vipInfo;
    return Warhead::Time::ToTimeString(endTime - GameTime::GetGameTime(), 3, TimeFormat::FullText);
}

VipLevelInfo* Vip::GetVipLevelInfo(uint32 level)
{
    return Warhead::Containers::MapGetValuePtr(_vipLevelsInfo, level);
}

void Vip::IterateVipSpellsForPlayer(Player* player, bool isLearn)
{
    if (!_isEnable)
        return;

    auto level{ isLearn ? GetLevel(player) : _maxLevel };
    if (!level)
        return;

    for (std::size_t i{ 1 }; i <= level; i++)
    {
        auto levelInfo{ GetVipLevelInfo(i) };
        if (!levelInfo)
            continue;

        // Vip spells
        if (!levelInfo->LearnSpells.empty())
        {
            for (auto const& spellID : levelInfo->LearnSpells)
            {
                if (isLearn && !player->HasSpell(spellID))
                    player->learnSpell(spellID);
                else if (!isLearn && player->HasSpell(spellID))
                    player->removeSpell(spellID, SPEC_MASK_ALL, false);
            }
        }

        // Vip mount
        if (levelInfo->MountSpell)
        {
            if (isLearn && !player->HasSpell(levelInfo->MountSpell))
                player->learnSpell(levelInfo->MountSpell);
            else if (!isLearn && player->HasSpell(levelInfo->MountSpell))
                player->removeSpell(levelInfo->MountSpell, SPEC_MASK_ALL, false);
        }
    }
}

namespace fmt
{
    template<>
    struct formatter<Vip::WarheadVip> : formatter<string_view>
    {
        // parse is inherited from formatter<string_view>.
        template <typename FormatContext>
        auto format(Optional<Vip::WarheadVip> vipInfo, FormatContext& ctx)
        {
            string_view info = "<unknown>";

            if (vipInfo)
            {
                auto const& [startTime, endTime, level] = *vipInfo;

                info = Warhead::StringFormat("Start time: {}. End time: {}. Level: {}",
                    Warhead::Time::TimeToHumanReadable(startTime), Warhead::Time::TimeToHumanReadable(endTime), level);
            }

            return formatter<string_view>::format(info, ctx);
        }
    };
}
