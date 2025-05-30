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

#include "DBCacheMgr.h"

void DBCacheMgr::InitGossipStrings()
{
    _queryStrings.emplace(DBCacheTable::NpcText, "SELECT ID, "
                                                 "text0_0, text0_1, BroadcastTextID0, lang0, Probability0, EmoteDelay0_0, Emote0_0, EmoteDelay0_1, Emote0_1, EmoteDelay0_2, Emote0_2, "
                                                 "text1_0, text1_1, BroadcastTextID1, lang1, Probability1, EmoteDelay1_0, Emote1_0, EmoteDelay1_1, Emote1_1, EmoteDelay1_2, Emote1_2, "
                                                 "text2_0, text2_1, BroadcastTextID2, lang2, Probability2, EmoteDelay2_0, Emote2_0, EmoteDelay2_1, Emote2_1, EmoteDelay2_2, Emote2_2, "
                                                 "text3_0, text3_1, BroadcastTextID3, lang3, Probability3, EmoteDelay3_0, Emote3_0, EmoteDelay3_1, Emote3_1, EmoteDelay3_2, Emote3_2, "
                                                 "text4_0, text4_1, BroadcastTextID4, lang4, Probability4, EmoteDelay4_0, Emote4_0, EmoteDelay4_1, Emote4_1, EmoteDelay4_2, Emote4_2, "
                                                 "text5_0, text5_1, BroadcastTextID5, lang5, Probability5, EmoteDelay5_0, Emote5_0, EmoteDelay5_1, Emote5_1, EmoteDelay5_2, Emote5_2, "
                                                 "text6_0, text6_1, BroadcastTextID6, lang6, Probability6, EmoteDelay6_0, Emote6_0, EmoteDelay6_1, Emote6_1, EmoteDelay6_2, Emote6_2, "
                                                 "text7_0, text7_1, BroadcastTextID7, lang7, Probability7, EmoteDelay7_0, Emote7_0, EmoteDelay7_1, Emote7_1, EmoteDelay7_2, Emote7_2 "
                                                 "FROM npc_text");
    _queryStrings.emplace(DBCacheTable::PageText, "SELECT ID, Text, NextPageID FROM page_text");
    _queryStrings.emplace(DBCacheTable::GossipMenu, "SELECT MenuID, TextID FROM gossip_menu");
    _queryStrings.emplace(DBCacheTable::GossipMenuOption, "SELECT MenuID, OptionID, OptionIcon, OptionText, OptionBroadcastTextID, OptionType, OptionNpcFlag, ActionMenuID, ActionPoiID, BoxCoded, BoxMoney, BoxText, BoxBroadcastTextID "
                                                    "FROM gossip_menu_option ORDER BY MenuID, OptionID");
    _queryStrings.emplace(DBCacheTable::NpcVendor, "SELECT entry, item, maxcount, incrtime, ExtendedCost FROM npc_vendor ORDER BY entry, slot ASC, item, ExtendedCost");
    _queryStrings.emplace(DBCacheTable::NpcTrainer, "SELECT b.ID, a.SpellID, a.MoneyCost, a.ReqSkillLine, a.ReqSkillRank, a.ReqLevel, a.ReqSpell FROM npc_trainer AS a "
                                                      "INNER JOIN npc_trainer AS b ON a.ID = -(b.SpellID) "
                                                      "UNION SELECT * FROM npc_trainer WHERE SpellID > 0");
}
