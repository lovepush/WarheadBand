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
#include "black_temple.h"

enum Supremus
{
    EMOTE_NEW_TARGET                = 0,
    EMOTE_PUNCH_GROUND              = 1,
    EMOTE_GROUND_CRACK              = 2,

    SPELL_SNARE_SELF                = 41922,
    SPELL_MOLTEN_PUNCH              = 40126,
    SPELL_HATEFUL_STRIKE            = 41926,
    SPELL_VOLCANIC_ERUPTION         = 40276,
    SPELL_VOLCANIC_ERUPTION_TRIGGER = 40117,
    SPELL_BERSERK                   = 45078,
    SPELL_CHARGE                    = 41581,

    NPC_SUPREMUS_PUNCH_STALKER      = 23095,

    EVENT_SPELL_BERSERK             = 1,
    EVENT_SPELL_HATEFUL_STRIKE      = 2,
    EVENT_SPELL_MOLTEN_FLAMES       = 3,
    EVENT_SWITCH_PHASE              = 4,
    EVENT_SPELL_VOLCANIC_ERUPTION   = 5,
    EVENT_SWITCH_TARGET             = 6,
    EVENT_CHECK_DIST                = 7,

    EVENT_GROUP_ABILITIES           = 1
};

class boss_supremus : public CreatureScript
{
public:
    boss_supremus() : CreatureScript("boss_supremus") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetBlackTempleAI<boss_supremusAI>(creature);
    }

    struct boss_supremusAI : public BossAI
    {
        boss_supremusAI(Creature* creature) : BossAI(creature, DATA_SUPREMUS)
        {
        }

        void Reset() override
        {
            BossAI::Reset();
        }

        void EnterCombat(Unit* who) override
        {
            BossAI::EnterCombat(who);

            SchedulePhase(false);
            events.ScheduleEvent(EVENT_SPELL_BERSERK, 900000);
            events.ScheduleEvent(EVENT_SPELL_MOLTEN_FLAMES, 4000);
        }

        void SchedulePhase(bool run)
        {
            events.CancelEventGroup(EVENT_GROUP_ABILITIES);
            events.ScheduleEvent(EVENT_SWITCH_PHASE, 60000);
            DoResetThreatList();

            if (!run)
            {
                events.ScheduleEvent(EVENT_SPELL_HATEFUL_STRIKE, 5000, EVENT_GROUP_ABILITIES);
                me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_TAUNT, false);
                me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_ATTACK_ME, false);
                me->RemoveAurasDueToSpell(SPELL_SNARE_SELF);
            }
            else
            {
                events.ScheduleEvent(EVENT_SPELL_VOLCANIC_ERUPTION, 5000, EVENT_GROUP_ABILITIES);
                events.ScheduleEvent(EVENT_SWITCH_TARGET, 0, EVENT_GROUP_ABILITIES);
                events.ScheduleEvent(EVENT_CHECK_DIST, 0, EVENT_GROUP_ABILITIES);
                me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_TAUNT, true);
                me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_ATTACK_ME, true);
                me->CastSpell(me, SPELL_SNARE_SELF, true);
            }
        }

        void JustDied(Unit* killer) override
        {
            BossAI::JustDied(killer);
        }

        void JustSummoned(Creature* summon) override
        {
            summons.Summon(summon);
            if (summon->GetEntry() == NPC_SUPREMUS_PUNCH_STALKER)
            {
                summon->ToTempSummon()->InitStats(20000);
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true))
                    summon->GetMotionMaster()->MoveFollow(target, 0.0f, 0.0f, MOTION_SLOT_CONTROLLED);
            }
            else
                summon->CastSpell(summon, SPELL_VOLCANIC_ERUPTION_TRIGGER, true);
        }

        void SummonedCreatureDespawn(Creature* summon) override
        {
            summons.Despawn(summon);
        }

        Unit* FindHatefulStrikeTarget()
        {
            Unit* target = nullptr;
            ThreatContainer::StorageType const& threatlist = me->GetThreatMgr().GetThreatList();
            for (ThreatContainer::StorageType::const_iterator i = threatlist.begin(); i != threatlist.end(); ++i)
            {
                Unit* unit = ObjectAccessor::GetUnit(*me, (*i)->getUnitGuid());
                if (unit && me->IsWithinMeleeRange(unit))
                    if (!target || unit->GetHealth() > target->GetHealth())
                        target = unit;
            }

            return target;
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            switch (events.ExecuteEvent())
            {
                case EVENT_SPELL_BERSERK:
                    me->CastSpell(me, SPELL_BERSERK, true);
                    break;
                case EVENT_SPELL_HATEFUL_STRIKE:
                    if (Unit* target = FindHatefulStrikeTarget())
                        me->CastSpell(target, SPELL_HATEFUL_STRIKE, false);
                    events.ScheduleEvent(EVENT_SPELL_HATEFUL_STRIKE, urand(1500, 3000), EVENT_GROUP_ABILITIES);
                    break;
                case EVENT_SPELL_MOLTEN_FLAMES:
                    me->CastSpell(me, SPELL_MOLTEN_PUNCH, false);
                    events.ScheduleEvent(EVENT_SPELL_MOLTEN_FLAMES, 20000, EVENT_GROUP_ABILITIES);
                    break;
                case EVENT_SWITCH_PHASE:
                    SchedulePhase(!me->HasAura(SPELL_SNARE_SELF));
                    break;
                case EVENT_SWITCH_TARGET:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100, true))
                    {
                        DoResetThreatList();
                        me->AddThreat(target, 5000000.0f);
                        Talk(EMOTE_NEW_TARGET);
                    }
                    events.ScheduleEvent(EVENT_SWITCH_TARGET, 10000, EVENT_GROUP_ABILITIES);
                    break;
                case EVENT_CHECK_DIST:
                    if (me->GetDistance(me->GetVictim()) > 40.0f)
                    {
                        Talk(EMOTE_PUNCH_GROUND);
                        me->CastSpell(me->GetVictim(), SPELL_CHARGE, true);
                        events.ScheduleEvent(EVENT_CHECK_DIST, 5000, EVENT_GROUP_ABILITIES);
                        break;
                    }
                    events.ScheduleEvent(EVENT_CHECK_DIST, 1, EVENT_GROUP_ABILITIES);
                    break;
                case EVENT_SPELL_VOLCANIC_ERUPTION:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100, true))
                    {
                        me->CastSpell(target, SPELL_VOLCANIC_ERUPTION, true);
                        Talk(EMOTE_GROUND_CRACK);
                    }
                    events.ScheduleEvent(EVENT_SPELL_VOLCANIC_ERUPTION, urand(10000, 18000), EVENT_GROUP_ABILITIES);
                    break;
            }

            DoMeleeAttackIfReady();
        }

        bool CheckEvadeIfOutOfCombatArea() const override
        {
            return me->GetPositionX() < 565 || me->GetPositionX() > 865 || me->GetPositionY() < 545 || me->GetPositionY() > 1000;
        }
    };
};

void AddSC_boss_supremus()
{
    new boss_supremus();
}
