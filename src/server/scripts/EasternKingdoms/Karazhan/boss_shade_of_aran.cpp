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

#include "GameObject.h"
#include "ScriptObject.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "karazhan.h"

enum ShadeOfAran
{
    SAY_AGGRO = 0,
    SAY_FLAMEWREATH = 1,
    SAY_BLIZZARD = 2,
    SAY_EXPLOSION = 3,
    SAY_DRINK = 4,
    SAY_ELEMENTALS = 5,
    SAY_KILL = 6,
    SAY_TIMEOVER = 7,
    SAY_DEATH = 8,

    //Spells
    SPELL_FROSTBOLT = 29954,
    SPELL_FIREBALL = 29953,
    SPELL_ARCMISSLE = 29955,
    SPELL_CHAINSOFICE = 29991,
    SPELL_DRAGONSBREATH = 29964,
    SPELL_MASSSLOW = 30035,
    SPELL_FLAME_WREATH = 29946,
    SPELL_AOE_CS = 29961,
    SPELL_PLAYERPULL = 32265,
    SPELL_AEXPLOSION = 29973,
    SPELL_MASS_POLY = 29963,
    SPELL_BLINK_CENTER = 29967,
    SPELL_ELEMENTALS = 29962,
    SPELL_CONJURE = 29975,
    SPELL_DRINK = 30024,
    SPELL_POTION = 32453,
    SPELL_AOE_PYROBLAST = 29978,

    //Creature Spells
    SPELL_CIRCULAR_BLIZZARD = 29951,
    SPELL_WATERBOLT = 31012,
    SPELL_SHADOW_PYRO = 29978,

    //Creatures
    CREATURE_WATER_ELEMENTAL = 17167,
    CREATURE_SHADOW_OF_ARAN = 18254,
    CREATURE_ARAN_BLIZZARD = 17161,
};

enum SuperSpell
{
    SUPER_FLAME = 0,
    SUPER_BLIZZARD,
    SUPER_AE,
};

class boss_shade_of_aran : public CreatureScript
{
public:
    boss_shade_of_aran() : CreatureScript("boss_shade_of_aran") { }

    struct boss_aranAI : public BossAI
    {
        boss_aranAI(Creature* creature) : BossAI(creature, DATA_ARAN)
        {
        }

        uint32 SecondarySpellTimer;
        uint32 NormalCastTimer;
        uint32 SuperCastTimer;
        uint32 BerserkTimer;
        uint32 CloseDoorTimer;                                  // Don't close the door right on aggro in case some people are still entering.

        uint8 LastSuperSpell;

        uint32 FlameWreathTimer;
        uint32 FlameWreathCheckTime;
        ObjectGuid FlameWreathTarget[3];
        float FWTargPosX[3];
        float FWTargPosY[3];

        uint32 CurrentNormalSpell;
        uint32 ArcaneCooldown;
        uint32 FireCooldown;
        uint32 FrostCooldown;

        uint32 DrinkInterruptTimer;

        bool ElementalsSpawned;
        bool Drinking;
        bool DrinkInturrupted;
        void Reset() override
        {
            SecondarySpellTimer = 5000;
            NormalCastTimer = 0;
            SuperCastTimer = 35000;
            BerserkTimer = 720000;
            CloseDoorTimer = 15000;

            LastSuperSpell = rand() % 3;

            FlameWreathTimer = 0;
            FlameWreathCheckTime = 0;

            for (uint8 i = 0; i < 3; ++i)
                FlameWreathTarget[i].Clear();

            CurrentNormalSpell = 0;
            ArcaneCooldown = 0;
            FireCooldown = 0;
            FrostCooldown = 0;

            DrinkInterruptTimer = 10000;

            ElementalsSpawned = false;
            Drinking = false;
            DrinkInturrupted = false;

            // Not in progress
            instance->SetData(DATA_ARAN, NOT_STARTED);
            instance->HandleGameObject(instance->GetGuidData(DATA_GO_LIBRARY_DOOR), true);
        }

        void KilledUnit(Unit* /*victim*/) override
        {
            Talk(SAY_KILL);
        }

        void JustDied(Unit* /*killer*/) override
        {
            Talk(SAY_DEATH);

            instance->SetData(DATA_ARAN, DONE);
            instance->HandleGameObject(instance->GetGuidData(DATA_GO_LIBRARY_DOOR), true);
        }

        void EnterCombat(Unit* /*who*/) override
        {
            Talk(SAY_AGGRO);

            instance->SetData(DATA_ARAN, IN_PROGRESS);
            instance->HandleGameObject(instance->GetGuidData(DATA_GO_LIBRARY_DOOR), false);
            DoZoneInCombat();
        }

        void FlameWreathEffect()
        {
            std::vector<Unit*> targets;
            ThreatContainer::StorageType const& t_list = me->GetThreatMgr().GetThreatList();

            if (t_list.empty())
                return;

            //store the threat list in a different container
            for (ThreatContainer::StorageType::const_iterator itr = t_list.begin(); itr != t_list.end(); ++itr)
            {
                Unit* target = ObjectAccessor::GetUnit(*me, (*itr)->getUnitGuid());
                //only on alive players
                if (target && target->IsAlive() && target->GetTypeId() == TYPEID_PLAYER)
                    targets.push_back(target);
            }

            //cut down to size if we have more than 3 targets
            while (targets.size() > 3)
                targets.erase(targets.begin() + rand() % targets.size());

            uint32 i = 0;
            for (std::vector<Unit*>::const_iterator itr = targets.begin(); itr != targets.end(); ++itr)
            {
                if (*itr)
                {
                    FlameWreathTarget[i] = (*itr)->GetGUID();
                    FWTargPosX[i] = (*itr)->GetPositionX();
                    FWTargPosY[i] = (*itr)->GetPositionY();
                    DoCast((*itr), SPELL_FLAME_WREATH, true);
                    ++i;
                }
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            if (CloseDoorTimer)
            {
                if (CloseDoorTimer <= diff)
                {
                    instance->HandleGameObject(instance->GetGuidData(DATA_GO_LIBRARY_DOOR), false);
                    CloseDoorTimer = 0;
                }
                else
                    CloseDoorTimer -= diff;
            }

            //Cooldowns for casts
            if (ArcaneCooldown)
            {
                if (ArcaneCooldown >= diff)
                    ArcaneCooldown -= diff;
                else
                    ArcaneCooldown = 0;
            }

            if (FireCooldown)
            {
                if (FireCooldown >= diff)
                    FireCooldown -= diff;
                else
                    FireCooldown = 0;
            }

            if (FrostCooldown)
            {
                if (FrostCooldown >= diff)
                    FrostCooldown -= diff;
                else
                    FrostCooldown = 0;
            }

            if (!Drinking && me->GetMaxPower(POWER_MANA) && (me->GetPower(POWER_MANA) * 100 / me->GetMaxPower(POWER_MANA)) < 20)
            {
                Drinking = true;
                me->InterruptNonMeleeSpells(false);

                Talk(SAY_DRINK);

                if (!DrinkInturrupted)
                {
                    DoCast(me, SPELL_MASS_POLY, true);
                    DoCast(me, SPELL_CONJURE, false);
                    DoCast(me, SPELL_DRINK, false);
                    me->SetStandState(UNIT_STAND_STATE_SIT);
                    DrinkInterruptTimer = 10000;
                }
            }

            //Drink Interrupt
            if (Drinking && DrinkInturrupted)
            {
                Drinking = false;
                me->RemoveAurasDueToSpell(SPELL_DRINK);
                me->SetStandState(UNIT_STAND_STATE_STAND);
                me->SetPower(POWER_MANA, me->GetMaxPower(POWER_MANA) - 32000);
                DoCast(me, SPELL_POTION, false);
            }

            //Drink Interrupt Timer
            if (Drinking && !DrinkInturrupted)
            {
                if (DrinkInterruptTimer >= diff)
                    DrinkInterruptTimer -= diff;
                else
                {
                    me->SetStandState(UNIT_STAND_STATE_STAND);
                    DoCast(me, SPELL_POTION, true);
                    DoCast(me, SPELL_AOE_PYROBLAST, false);
                    DrinkInturrupted = true;
                    Drinking = false;
                }
            }

            //Don't execute any more code if we are drinking
            if (Drinking)
                return;

            //Normal casts
            if (NormalCastTimer <= diff)
            {
                if (!me->IsNonMeleeSpellCast(false))
                {
                    Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100, true);
                    if (!target)
                        return;

                    uint32 Spells[3];
                    uint8 AvailableSpells = 0;

                    //Check for what spells are not on cooldown
                    if (!ArcaneCooldown)
                    {
                        Spells[AvailableSpells] = SPELL_ARCMISSLE;
                        ++AvailableSpells;
                    }
                    if (!FireCooldown)
                    {
                        Spells[AvailableSpells] = SPELL_FIREBALL;
                        ++AvailableSpells;
                    }
                    if (!FrostCooldown)
                    {
                        Spells[AvailableSpells] = SPELL_FROSTBOLT;
                        ++AvailableSpells;
                    }

                    //If no available spells wait 1 second and try again
                    if (AvailableSpells)
                    {
                        CurrentNormalSpell = Spells[rand() % AvailableSpells];
                        DoCast(target, CurrentNormalSpell);
                    }
                }
                NormalCastTimer = 1000;
            }
            else
                NormalCastTimer -= diff;

            if (SecondarySpellTimer <= diff)
            {
                switch (urand(0, 1))
                {
                    case 0:
                        DoCast(me, SPELL_AOE_CS);
                        break;
                    case 1:
                        if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100, true))
                            DoCast(target, SPELL_CHAINSOFICE);
                        break;
                }
                SecondarySpellTimer = urand(5000, 20000);
            }
            else
                SecondarySpellTimer -= diff;

            if (SuperCastTimer <= diff)
            {
                uint8 Available[2];

                switch (LastSuperSpell)
                {
                    case SUPER_AE:
                        Available[0] = SUPER_FLAME;
                        Available[1] = SUPER_BLIZZARD;
                        break;
                    case SUPER_FLAME:
                        Available[0] = SUPER_AE;
                        Available[1] = SUPER_BLIZZARD;
                        break;
                    case SUPER_BLIZZARD:
                        Available[0] = SUPER_FLAME;
                        Available[1] = SUPER_AE;
                        break;
                }

                LastSuperSpell = Available[urand(0, 1)];

                switch (LastSuperSpell)
                {
                    case SUPER_AE:
                        Talk(SAY_EXPLOSION);

                        DoCast(me, SPELL_BLINK_CENTER, true);
                        DoCast(me, SPELL_PLAYERPULL, true);
                        DoCast(me, SPELL_MASSSLOW, true);
                        DoCast(me, SPELL_AEXPLOSION, false);
                        break;

                    case SUPER_FLAME:
                        Talk(SAY_FLAMEWREATH);

                        FlameWreathTimer = 20000;
                        FlameWreathCheckTime = 500;

                        FlameWreathTarget[0].Clear();
                        FlameWreathTarget[1].Clear();
                        FlameWreathTarget[2].Clear();

                        FlameWreathEffect();
                        break;

                    case SUPER_BLIZZARD:
                        Talk(SAY_BLIZZARD);

                        if (Creature* pSpawn = me->SummonCreature(CREATURE_ARAN_BLIZZARD, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 25000))
                        {
                            pSpawn->SetFaction(me->GetFaction());
                            pSpawn->CastSpell(pSpawn, SPELL_CIRCULAR_BLIZZARD, false);
                        }
                        break;
                }

                SuperCastTimer = urand(35000, 40000);
            }
            else
                SuperCastTimer -= diff;

            if (!ElementalsSpawned && HealthBelowPct(40))
            {
                ElementalsSpawned = true;

                Creature* ElementalOne = me->SummonCreature(CREATURE_WATER_ELEMENTAL, -11168.1f, -1939.29f, 232.092f, 1.46f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 90000);
                Creature* ElementalTwo = me->SummonCreature(CREATURE_WATER_ELEMENTAL, -11138.2f, -1915.38f, 232.092f, 3.00f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 90000);
                Creature* ElementalThree = me->SummonCreature(CREATURE_WATER_ELEMENTAL, -11161.7f, -1885.36f, 232.092f, 4.59f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 90000);
                Creature* ElementalFour = me->SummonCreature(CREATURE_WATER_ELEMENTAL, -11192.4f, -1909.36f, 232.092f, 6.19f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 90000);

                if (ElementalOne)
                {
                    Unit* target = SelectTarget(SelectTargetMethod::Random, 1, 100, true);
                    if (!target)
                        return;

                    DoStartNoMovement(target);
                    ElementalOne->SetInCombatWithZone();
                    ElementalOne->CombatStart(target);
                    ElementalOne->SetFaction(me->GetFaction());
                    ElementalOne->SetUnitMovementFlags(MOVEMENTFLAG_ROOT);
                    ElementalOne->SetModifierValue(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, 0);
                }

                if (ElementalTwo)
                {
                    Unit* target = SelectTarget(SelectTargetMethod::Random, 1, 100, true);
                    if (!target)
                        return;

                    DoStartNoMovement(target);
                    ElementalTwo->SetInCombatWithZone();
                    ElementalTwo->CombatStart(target);
                    ElementalTwo->SetFaction(me->GetFaction());
                    ElementalTwo->SetUnitMovementFlags(MOVEMENTFLAG_ROOT);
                    ElementalTwo->SetModifierValue(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, 0);
                }

                if (ElementalThree)
                {
                    Unit* target = SelectTarget(SelectTargetMethod::Random, 1, 100, true);
                    if (!target)
                        return;

                    DoStartNoMovement(target);
                    ElementalThree->SetInCombatWithZone();
                    ElementalThree->CombatStart(target);
                    ElementalThree->SetFaction(me->GetFaction());
                    ElementalThree->SetUnitMovementFlags(MOVEMENTFLAG_ROOT);
                    ElementalThree->SetModifierValue(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, 0);
                }

                if (ElementalFour)
                {
                    Unit* target = SelectTarget(SelectTargetMethod::Random, 1, 100, true);
                    if (!target)
                        return;

                    DoStartNoMovement(target);
                    ElementalFour->SetInCombatWithZone();
                    ElementalFour->CombatStart(target);
                    ElementalFour->SetFaction(me->GetFaction());
                    ElementalFour->SetUnitMovementFlags(MOVEMENTFLAG_ROOT);
                    ElementalFour->SetModifierValue(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, 0);
                }

                Talk(SAY_ELEMENTALS);
            }

            if (BerserkTimer <= diff)
            {
                for (uint32 i = 0; i < 5; ++i)
                {
                    if (Creature* unit = me->SummonCreature(CREATURE_SHADOW_OF_ARAN, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 5000))
                    {
                        unit->Attack(me->GetVictim(), true);
                        unit->SetFaction(me->GetFaction());
                    }
                }

                Talk(SAY_TIMEOVER);

                BerserkTimer = 60000;
            }
            else
                BerserkTimer -= diff;

            //Flame Wreath check
            if (FlameWreathTimer)
            {
                if (FlameWreathTimer >= diff)
                    FlameWreathTimer -= diff;
                else
                    FlameWreathTimer = 0;

                if (FlameWreathCheckTime <= diff)
                {
                    for (uint8 i = 0; i < 3; ++i)
                    {
                        if (!FlameWreathTarget[i])
                            continue;

                        Unit* unit = ObjectAccessor::GetUnit(*me, FlameWreathTarget[i]);
                        if (unit && !unit->IsWithinDist2d(FWTargPosX[i], FWTargPosY[i], 3))
                        {
                            unit->CastSpell(unit, 20476, true, 0, 0, me->GetGUID());
                            unit->CastSpell(unit, 11027, true);
                            FlameWreathTarget[i].Clear();
                        }
                    }
                    FlameWreathCheckTime = 500;
                }
                else
                    FlameWreathCheckTime -= diff;
            }

            if (ArcaneCooldown && FireCooldown && FrostCooldown)
                DoMeleeAttackIfReady();
        }

        void DamageTaken(Unit*, uint32& damage, DamageEffectType, SpellSchoolMask) override
        {
            if (!DrinkInturrupted && Drinking && damage)
                DrinkInturrupted = true;
        }

        void SpellHit(Unit* /*pAttacker*/, SpellInfo const* Spell) override
        {
            //We only care about interrupt effects and only if they are durring a spell currently being cast
            if ((Spell->Effects[0].Effect != SPELL_EFFECT_INTERRUPT_CAST &&
                    Spell->Effects[1].Effect != SPELL_EFFECT_INTERRUPT_CAST &&
                    Spell->Effects[2].Effect != SPELL_EFFECT_INTERRUPT_CAST) || !me->IsNonMeleeSpellCast(false))
                return;

            //Interrupt effect
            me->InterruptNonMeleeSpells(false);

            //Normally we would set the cooldown equal to the spell duration
            //but we do not have access to the DurationStore

            switch (CurrentNormalSpell)
            {
                case SPELL_ARCMISSLE:
                    ArcaneCooldown = 5000;
                    break;
                case SPELL_FIREBALL:
                    FireCooldown = 5000;
                    break;
                case SPELL_FROSTBOLT:
                    FrostCooldown = 5000;
                    break;
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return GetKarazhanAI<boss_aranAI>(creature);
    }
};

class npc_aran_elemental : public CreatureScript
{
public:
    npc_aran_elemental() : CreatureScript("npc_aran_elemental") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new water_elementalAI(creature);
    }

    struct water_elementalAI : public ScriptedAI
    {
        water_elementalAI(Creature* creature) : ScriptedAI(creature)
        {
        }

        uint32 CastTimer;

        void Reset() override
        {
            CastTimer = 2000 + (rand() % 3000);
        }

        void EnterCombat(Unit* /*who*/) override { }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            if (CastTimer <= diff)
            {
                DoCastVictim(SPELL_WATERBOLT);
                CastTimer = urand(2000, 5000);
            }
            else
                CastTimer -= diff;
        }
    };
};

void AddSC_boss_shade_of_aran()
{
    new boss_shade_of_aran();
    new npc_aran_elemental();
}
