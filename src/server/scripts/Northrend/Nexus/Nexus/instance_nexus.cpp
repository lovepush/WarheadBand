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

#include "ScriptObject.h"
#include "ScriptedCreature.h"
#include "nexus.h"
#include <sstream>

DoorData const doorData[] =
{
    { GO_TELESTRA_SPHERE,   DATA_TELESTRA_ORB,  DOOR_TYPE_PASSAGE },
    { GO_ANOMALUS_SPHERE,   DATA_ANOMALUS_ORB,  DOOR_TYPE_PASSAGE },
    { GO_ORMOROK_SPHERE,    DATA_ORMOROK_ORB,   DOOR_TYPE_PASSAGE },
    { 0,                    0,                  DOOR_TYPE_ROOM    }
};

class instance_nexus : public InstanceMapScript
{
public:
    instance_nexus() : InstanceMapScript("instance_nexus", 576) { }

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_nexus_InstanceMapScript(map);
    }

    struct instance_nexus_InstanceMapScript : public InstanceScript
    {
        instance_nexus_InstanceMapScript(Map* map) : InstanceScript(map) {}

        void Initialize() override
        {
            SetBossNumber(MAX_ENCOUNTERS);
            LoadDoorData(doorData);
        }

        void OnCreatureCreate(Creature* creature) override
        {
            Map::PlayerList const& players = instance->GetPlayers();
            TeamId TeamIdInInstance = TEAM_NEUTRAL;
            if (!players.IsEmpty())
                if (Player* pPlayer = players.begin()->GetSource())
                    TeamIdInInstance = pPlayer->GetTeamId();

            switch (creature->GetEntry())
            {
                case NPC_ALLIANCE_RANGER:
                    creature->SetFaction(FACTION_MONSTER_2);
                    if (TeamIdInInstance == TEAM_ALLIANCE)
                        creature->UpdateEntry(NPC_HORDE_RANGER);
                    break;
                case NPC_ALLIANCE_BERSERKER:
                    creature->SetFaction(FACTION_MONSTER_2);
                    if (TeamIdInInstance == TEAM_ALLIANCE)
                        creature->UpdateEntry(NPC_HORDE_BERSERKER);
                    break;
                case NPC_ALLIANCE_COMMANDER:
                    creature->SetFaction(FACTION_MONSTER_2);
                    if (TeamIdInInstance == TEAM_ALLIANCE)
                        creature->UpdateEntry(NPC_HORDE_COMMANDER);
                    break;
                case NPC_ALLIANCE_CLERIC:
                    creature->SetFaction(FACTION_MONSTER_2);
                    if (TeamIdInInstance == TEAM_ALLIANCE)
                        creature->UpdateEntry(NPC_HORDE_CLERIC);
                    break;
                case NPC_COMMANDER_STOUTBEARD:
                    creature->SetFaction(FACTION_MONSTER_2);
                    if (TeamIdInInstance == TEAM_ALLIANCE)
                        creature->UpdateEntry(NPC_COMMANDER_KOLURG);
                    break;
            }
        }

        void OnGameObjectCreate(GameObject* gameObject) override
        {
            switch (gameObject->GetEntry())
            {
                case GO_TELESTRA_SPHERE:
                    if (GetBossState(DATA_TELESTRA_ORB) != DONE && GetBossState(DATA_MAGUS_TELESTRA_EVENT) == DONE)
                        gameObject->RemoveGameObjectFlag(GO_FLAG_NOT_SELECTABLE);
                    AddDoor(gameObject, true);
                    break;
                case GO_ANOMALUS_SPHERE:
                    if (GetBossState(DATA_ANOMALUS_ORB) != DONE && GetBossState(DATA_ANOMALUS_EVENT) == DONE)
                        gameObject->RemoveGameObjectFlag(GO_FLAG_NOT_SELECTABLE);
                    AddDoor(gameObject, true);
                    break;
                case GO_ORMOROK_SPHERE:
                    if (GetBossState(DATA_ORMOROK_ORB) != DONE && GetBossState(DATA_ORMOROK_EVENT) == DONE)
                        gameObject->RemoveGameObjectFlag(GO_FLAG_NOT_SELECTABLE);
                    AddDoor(gameObject, true);
                    break;
            }
        }

        void OnGameObjectRemove(GameObject* gameObject) override
        {
            switch (gameObject->GetEntry())
            {
                case GO_TELESTRA_SPHERE:
                case GO_ANOMALUS_SPHERE:
                case GO_ORMOROK_SPHERE:
                    AddDoor(gameObject, false);
                    break;
            }
        }

        void SetData(uint32 type, uint32) override
        {
            switch (type)
            {
                case GO_TELESTRA_SPHERE:
                    SetBossState(DATA_TELESTRA_ORB, NOT_STARTED);
                    SetBossState(DATA_TELESTRA_ORB, DONE);
                    break;
                case GO_ANOMALUS_SPHERE:
                    SetBossState(DATA_ANOMALUS_ORB, NOT_STARTED);
                    SetBossState(DATA_ANOMALUS_ORB, DONE);
                    break;
                case GO_ORMOROK_SPHERE:
                    SetBossState(DATA_ORMOROK_ORB, NOT_STARTED);
                    SetBossState(DATA_ORMOROK_ORB, DONE);
                    break;
            }
        }

        bool SetBossState(uint32 id, EncounterState state) override
        {
            if (!InstanceScript::SetBossState(id, state))
                return false;

            if (state != DONE || id > DATA_ORMOROK_EVENT)
                return true;

            BossInfo const* bossInfo = GetBossInfo(id + DATA_TELESTRA_ORB);
            for (DoorSet::const_iterator i = bossInfo->door[DOOR_TYPE_PASSAGE].begin(); i != bossInfo->door[DOOR_TYPE_PASSAGE].end(); ++i)
                (*i)->RemoveGameObjectFlag(GO_FLAG_NOT_SELECTABLE);
            return true;
        }

        std::string GetSaveData() override
        {
            std::ostringstream saveStream;
            saveStream << "N E X " << GetBossSaveData();
            return saveStream.str();
        }

        void Load(const char* in) override
        {
            if( !in )
                return;

            char dataHead1, dataHead2, dataHead3;
            std::istringstream loadStream(in);
            loadStream >> dataHead1 >> dataHead2 >> dataHead3;
            if (dataHead1 == 'N' && dataHead2 == 'E' && dataHead3 == 'X')
            {
                for (uint8 i = 0; i < MAX_ENCOUNTERS; ++i)
                {
                    uint32 tmpState;
                    loadStream >> tmpState;
                    if (tmpState == IN_PROGRESS || tmpState > SPECIAL)
                        tmpState = NOT_STARTED;
                    SetBossState(i, EncounterState(tmpState));
                }
            }
        }
    };
};

enum eFrayer
{
    SPELL_SUMMON_SEED_POD               = 52796,
    SPELL_SEED_POD                      = 48082,
    SPELL_AURA_OF_REGENERATION          = 52067,
    SPELL_CRYSTAL_BLOOM                 = 48058,
    SPELL_ENSNARE                       = 48053
};

class npc_crystalline_frayer : public CreatureScript
{
public:
    npc_crystalline_frayer() : CreatureScript("npc_crystalline_frayer") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetNexusAI<npc_crystalline_frayerAI>(creature);
    }

    struct npc_crystalline_frayerAI : public ScriptedAI
    {
        npc_crystalline_frayerAI(Creature* creature) : ScriptedAI(creature)
        {
        }

        bool _allowDeath;
        uint32 restoreTimer;
        uint32 abilityTimer1;
        uint32 abilityTimer2;

        void Reset() override
        {
            restoreTimer = 0;
            abilityTimer1 = 0;
            abilityTimer2 = 30000;
            me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        }

        void EnterCombat(Unit*) override
        {
            _allowDeath = me->GetInstanceScript()->GetBossState(DATA_ORMOROK_EVENT) == DONE;
        }

        void EnterEvadeMode(EvadeReason why) override
        {
            if (me->isRegeneratingHealth())
                ScriptedAI::EnterEvadeMode(why);
        }

        void DamageTaken(Unit*, uint32& damage, DamageEffectType, SpellSchoolMask) override
        {
            if (damage >= me->GetHealth())
            {
                if (!_allowDeath)
                {
                    me->RemoveAllAuras();
                    me->GetThreatMgr().ClearAllThreat();
                    me->CombatStop(true);
                    damage = 0;

                    me->SetReactState(REACT_PASSIVE);
                    me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                    me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                    me->SetRegeneratingHealth(false);
                    me->CastSpell(me, SPELL_SUMMON_SEED_POD, true);
                    me->CastSpell(me, SPELL_SEED_POD, true);
                    me->CastSpell(me, SPELL_AURA_OF_REGENERATION, false);
                    restoreTimer = 1;
                }
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (restoreTimer)
            {
                restoreTimer += diff;
                if (restoreTimer >= 90 * IN_MILLISECONDS)
                {
                    Talk(0);
                    me->SetRegeneratingHealth(true);
                    restoreTimer = 0;
                    me->SetReactState(REACT_AGGRESSIVE);
                    me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                    me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                }
                return;
            }

            if (!UpdateVictim())
                return;

            abilityTimer1 += diff;
            abilityTimer2 += diff;

            if (abilityTimer1 >= 5000)
            {
                me->CastSpell(me->GetVictim(), SPELL_ENSNARE, false);
                abilityTimer1 = 0;
            }

            if (abilityTimer2 >= 30000)
            {
                me->CastSpell(me->GetVictim(), SPELL_CRYSTAL_BLOOM, false);
                abilityTimer2 = 0;
            }
        }
    };
};

void AddSC_instance_nexus()
{
    new instance_nexus();
    new npc_crystalline_frayer();
}
