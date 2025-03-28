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
#include "Spell.h"
#include "zulgurub.h"

/*
 * @todo
 * - Fix timers (research some more)
 */

enum Says
{
    SAY_VENOXIS_TRANSFORM           = 1,        // Let the coils of hate unfurl!
    SAY_VENOXIS_DEATH               = 2         // Ssserenity.. at lassst!
};

enum Spells
{
    // troll form
    SPELL_THRASH                    = 3391,
    SPELL_DISPEL_MAGIC              = 23859,
    SPELL_RENEW                     = 23895,
    SPELL_HOLY_NOVA                 = 23858,
    SPELL_HOLY_FIRE                 = 23860,
    SPELL_HOLY_WRATH                = 23979,
    // snake form
    SPELL_POISON_CLOUD              = 23861,
    SPELL_VENOM_SPIT                = 23862,

    SPELL_PARASITIC_SERPENT         = 23865,
    SPELL_SUMMON_PARASITIC_SERPENT  = 23866,
    SPELL_PARASITIC_SERPENT_TRIGGER = 23867,
    // used when swapping event-stages
    SPELL_VENOXIS_TRANSFORM         = 23849,    // 50% health - shapechange to cobra
    SPELL_FRENZY                    = 8269,      // 20% health - frenzy
    SPELL_POISON = 24097
};

enum Events
{
    // troll form
    EVENT_THRASH                    = 1,
    EVENT_DISPEL_MAGIC              = 2,
    EVENT_RENEW                     = 3,
    EVENT_HOLY_NOVA                 = 4,
    EVENT_HOLY_FIRE                 = 5,
    EVENT_HOLY_WRATH                = 6,
    // phase-changing
    EVENT_TRANSFORM                 = 7,
    // snake form events
    EVENT_POISON_CLOUD              = 8,
    EVENT_VENOM_SPIT                = 9,
    EVENT_PARASITIC_SERPENT         = 10,
    EVENT_FRENZY                    = 11,
    EVENT_POISON
};

enum Phases
{
    PHASE_ONE                       = 1,    // troll form
    PHASE_TWO                       = 2     // snake form
};

enum NPCs
{
    NPC_PARASITIC_SERPENT           = 14884,
    NPC_RAZZASHI_COBRA    = 11373,
    BOSS_VENOXIS          = 14507
};

class boss_venoxis : public CreatureScript
{
public:
    boss_venoxis() : CreatureScript("boss_venoxis") { }

    struct boss_venoxisAI : public BossAI
    {
        boss_venoxisAI(Creature* creature) : BossAI(creature, DATA_VENOXIS) { }

        void Reset() override
        {
            _Reset();
            // remove all spells and auras from previous attempts
            me->RemoveAllAuras();
            me->SetReactState(REACT_PASSIVE);
            // set some internally used variables to their defaults
            _inMeleeRange = 0;
            _transformed = false;
            _frenzied = false;
            events.SetPhase(PHASE_ONE);

            SpawnCobras();
        }

        void JustDied(Unit* /*killer*/) override
        {
            _JustDied();
            Talk(SAY_VENOXIS_DEATH);
            me->RemoveAllAuras();
        }

        void SpawnCobras()
        {
            me->SummonCreature(NPC_RAZZASHI_COBRA, -12021.20f, -1719.73f, 39.34f, 0.85f, TEMPSUMMON_CORPSE_DESPAWN);
            me->SummonCreature(NPC_RAZZASHI_COBRA, -12029.40f, -1714.54f, 39.36f, 0.68f, TEMPSUMMON_CORPSE_DESPAWN);
            me->SummonCreature(NPC_RAZZASHI_COBRA, -12036.79f, -1704.27f, 40.06f, 0.45f, TEMPSUMMON_CORPSE_DESPAWN);
            me->SummonCreature(NPC_RAZZASHI_COBRA, -12037.70f, -1694.20f, 39.35f, 0.27f, TEMPSUMMON_CORPSE_DESPAWN);
        }

        void SetCombatCombras()
        {
            std::list<Creature*> cobraList;
            me->GetCreatureListWithEntryInGrid(cobraList, NPC_RAZZASHI_COBRA, 50.0f);

            if (!cobraList.empty())
            {
                for (auto cobras : cobraList)
                {
                    cobras->SetInCombatWithZone();
                }
            }
        }

        void EnterCombat(Unit* /*who*/) override
        {
            _EnterCombat();
            me->SetReactState(REACT_AGGRESSIVE);
            // Always running events
            events.ScheduleEvent(EVENT_THRASH, 5000);
            // Phase one events (regular form)
            events.ScheduleEvent(EVENT_HOLY_NOVA, urand(5000, 15000), 0, PHASE_ONE);
            events.ScheduleEvent(EVENT_DISPEL_MAGIC, 35000, 0, PHASE_ONE);
            events.ScheduleEvent(EVENT_HOLY_FIRE, urand(10000,20000), 0, PHASE_ONE);
            events.ScheduleEvent(EVENT_RENEW, 30000, 0, PHASE_ONE);
            events.ScheduleEvent(EVENT_HOLY_WRATH, urand(15000, 25000), 0, PHASE_ONE);

            events.SetPhase(PHASE_ONE);

            // Set zone in combat
            DoZoneInCombat();
            SetCombatCombras();
        }

        void DamageTaken(Unit*, uint32& /*damage*/, DamageEffectType, SpellSchoolMask) override
        {
            // check if venoxis is ready to transform
            if (!_transformed && !HealthAbovePct(50))
            {
                _transformed = true;
                // schedule the event that changes our phase
                events.ScheduleEvent(EVENT_TRANSFORM, 100);
            }
            // we're losing health, bad, go frenzy
            else if (!_frenzied && !HealthAbovePct(20))
            {
                _frenzied = true;
                events.ScheduleEvent(EVENT_FRENZY, 100);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            // return back to main code if we're still casting
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    // thrash is available in all phases
                    case EVENT_THRASH:
                        DoCast(me, SPELL_THRASH, true);
                        events.ScheduleEvent(EVENT_THRASH, urand(10000, 20000));
                        break;

                    // troll form spells and Actions (first part)
                    case EVENT_DISPEL_MAGIC:
                        DoCast(me, SPELL_DISPEL_MAGIC);
                        events.ScheduleEvent(EVENT_DISPEL_MAGIC, urand(15000, 20000), 0, PHASE_ONE);
                        break;
                    case EVENT_RENEW:
                        DoCast(me, SPELL_RENEW);
                        events.ScheduleEvent(EVENT_RENEW, urand(25000, 30000), 0, PHASE_ONE);
                        break;
                    case EVENT_HOLY_WRATH:
                        if (Unit* target = SelectTarget(SelectTargetMethod::MaxThreat))
                            DoCast(target, SPELL_HOLY_WRATH);
                        events.ScheduleEvent(EVENT_HOLY_WRATH, urand(12000, 22000), 0, PHASE_ONE);
                        break;
                    case EVENT_HOLY_FIRE:
                        if (Unit* target = SelectTarget(SelectTargetMethod::Random))
                            DoCast(target, SPELL_HOLY_FIRE);
                        events.ScheduleEvent(EVENT_HOLY_FIRE, urand(10000, 24000), 0, PHASE_ONE);
                        break;
                    case EVENT_HOLY_NOVA:
                        DoCastSelf(SPELL_HOLY_NOVA);
                        events.ScheduleEvent(EVENT_HOLY_NOVA, urand(10000, 24000), 0, PHASE_ONE);
                        break;

                    //
                    // snake form spells and Actions
                    //

                    case EVENT_VENOM_SPIT:
                        if (Unit* target = SelectTarget(SelectTargetMethod::Random))
                            DoCast(target, SPELL_VENOM_SPIT);
                        events.ScheduleEvent(EVENT_VENOM_SPIT, urand(5000, 15000), 0, PHASE_TWO);
                        break;
                    case EVENT_POISON_CLOUD:
                        if (Unit* target = SelectTarget(SelectTargetMethod::Random))
                            DoCast(target, SPELL_POISON_CLOUD);
                        events.ScheduleEvent(EVENT_POISON_CLOUD, urand(15000, 20000), 0, PHASE_TWO);
                        break;
                    case EVENT_PARASITIC_SERPENT:
                        if (Unit* target = SelectTarget(SelectTargetMethod::Random))
                            DoCast(target, SPELL_SUMMON_PARASITIC_SERPENT);
                        events.ScheduleEvent(EVENT_PARASITIC_SERPENT, 15000, 0, PHASE_TWO);
                        break;
                    case EVENT_FRENZY:
                        // frenzy at 20% health
                        DoCast(me, SPELL_FRENZY, true);
                        break;

                    //
                    // shape and phase-changing
                    //

                    case EVENT_TRANSFORM:
                        // shapeshift at 50% health
                        DoCast(me, SPELL_VENOXIS_TRANSFORM);
                        Talk(SAY_VENOXIS_TRANSFORM);
                        DoResetThreatList();

                        // phase two events (snakeform)
                        events.ScheduleEvent(EVENT_VENOM_SPIT, 5000, 0, PHASE_TWO);
                        events.ScheduleEvent(EVENT_POISON_CLOUD, 10000, 0, PHASE_TWO);
                        events.ScheduleEvent(EVENT_PARASITIC_SERPENT, 30000, 0, PHASE_TWO);

                        // transformed, start phase two
                        events.SetPhase(PHASE_TWO);

                        break;
                    default:
                        break;
                }
            }

            DoMeleeAttackIfReady();
        }

    private:
        uint8 _inMeleeRange;
        bool _transformed;
        bool _frenzied;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetZulGurubAI<boss_venoxisAI>(creature);
    }
};

class npc_razzashi_cobra_venoxis : public CreatureScript
{
public:
    npc_razzashi_cobra_venoxis() : CreatureScript("npc_razzashi_cobra_venoxis") {}

    struct npc_razzashi_cobra_venoxis_AI : public ScriptedAI
    {
        npc_razzashi_cobra_venoxis_AI(Creature* creature) : ScriptedAI(creature) {}

        EventMap events;

        void Reset()
        {
            events.Reset();
        }

        void EnterCombat(Unit*)
        {
            events.ScheduleEvent(EVENT_POISON, 8 * IN_MILLISECONDS);

            if (Creature* Venoxis = GetVenoxis())
            {
                Venoxis->SetInCombatWithZone();
            }
        }

        Creature* GetVenoxis()
        {
            return me->FindNearestCreature(BOSS_VENOXIS, 200.0f, true);
        }

        void UpdateAI(uint32 diff)
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_POISON:
                {
                    me->CastSpell(me->GetVictim(), SPELL_POISON);
                    events.ScheduleEvent(EVENT_POISON, 15 * IN_MILLISECONDS);
                    break;
                }
                }
            }

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_razzashi_cobra_venoxis_AI(creature);
    }
};

void AddSC_boss_venoxis()
{
    new boss_venoxis();
    new npc_razzashi_cobra_venoxis();
}
