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

#include "MovementPacketBuilder.h"
#include "ByteBuffer.h"
#include "MoveSpline.h"

namespace Movement
{
    inline void operator << (ByteBuffer& b, const Vector3& v)
    {
        b << v.x << v.y << v.z;
    }

    inline void operator >> (ByteBuffer& b, Vector3& v)
    {
        b >> v.x >> v.y >> v.z;
    }

    enum MonsterMoveType
    {
        MonsterMoveNormal       = 0,
        MonsterMoveStop         = 1,
        MonsterMoveFacingSpot   = 2,
        MonsterMoveFacingTarget = 3,
        MonsterMoveFacingAngle  = 4
    };

    void PacketBuilder::WriteCommonMonsterMovePart(const MoveSpline& move_spline, ByteBuffer& data)
    {
        MoveSplineFlag splineflags = move_spline.splineflags;

        data << uint8(0);                                       // sets/unsets MOVEMENTFLAG2_UNK7 (0x40)
        data << move_spline.spline.getPoint(move_spline.spline.first());
        data << move_spline.GetId();

        switch (splineflags & MoveSplineFlag::Mask_Final_Facing)
        {
            case MoveSplineFlag::Final_Target:
                data << uint8(MonsterMoveFacingTarget);
                data << move_spline.facing.target;
                break;
            case MoveSplineFlag::Final_Angle:
                data << uint8(MonsterMoveFacingAngle);
                data << move_spline.facing.angle;
                break;
            case MoveSplineFlag::Final_Point:
                data << uint8(MonsterMoveFacingSpot);
                data << move_spline.facing.f.x << move_spline.facing.f.y << move_spline.facing.f.z;
                break;
            default:
                data << uint8(MonsterMoveNormal);
                break;
        }

        // add fake Enter_Cycle flag - needed for client-side cyclic movement (client will erase first spline vertex after first cycle done)
        // Xinef: this flag breaks cycle for ground movement, client teleports npc between last and first point instead of using smooth movement
        if (splineflags & MoveSplineFlag::Flying)
            splineflags.enter_cycle = move_spline.isCyclic();
        data << uint32(splineflags & uint32(~MoveSplineFlag::Mask_No_Monster_Move));

        if (splineflags.animation)
        {
            data << splineflags.getAnimationId();
            data << move_spline.effect_start_time;
        }

        data << move_spline.Duration();

        if (splineflags.parabolic)
        {
            data << move_spline.vertical_acceleration;
            data << move_spline.effect_start_time;
        }
    }

    void PacketBuilder::WriteStopMovement(Vector3 const& pos, uint32 splineId, ByteBuffer& data)
    {
        data << uint8(0);                                       // sets/unsets MOVEMENTFLAG2_UNK7 (0x40)
        data << pos;
        data << splineId;
        data << uint8(MonsterMoveStop);
    }

    void WriteLinearPath(const Spline<int32>& spline, ByteBuffer& data)
    {
        uint32 last_idx = spline.getPointCount() - 3;
        const Vector3* real_path = &spline.getPoint(1);

        data << last_idx;
        data << real_path[last_idx];   // destination
        if (last_idx > 1)
        {
            Vector3 middle = (real_path[0] + real_path[last_idx]) / 2.f;
            Vector3 offset;
            // first and last points already appended
            for (uint32 i = 1; i < last_idx; ++i)
            {
                offset = middle - real_path[i];
                data.appendPackXYZ(offset.x, offset.y, offset.z);
            }
        }
    }

    void WriteCatmullRomPath(const Spline<int32>& spline, ByteBuffer& data)
    {
        uint32 count = spline.getPointCount() - 3;
        data << count;
        data.append<Vector3>(&spline.getPoint(2), count);
    }

    void WriteCatmullRomCyclicPath(const Spline<int32>& spline, ByteBuffer& data, bool flying)
    {
        uint32 count = spline.getPointCount() - 3;
        data << uint32(count + 1);
        if (flying)
        {
            data << spline.getPoint(1); // fake point, client will erase it from the spline after first cycle done
            data.append<Vector3>(&spline.getPoint(2), count);
        }
        else
        {
            data.append<Vector3>(&spline.getPoint(2), count);
            data << Vector3::zero(); //Xinef: fake point
        }
    }

    void PacketBuilder::WriteMonsterMove(const MoveSpline& move_spline, ByteBuffer& data)
    {
        WriteCommonMonsterMovePart(move_spline, data);

        const Spline<int32>& spline = move_spline.spline;
        MoveSplineFlag splineflags = move_spline.splineflags;
        if (splineflags & MoveSplineFlag::Mask_CatmullRom)
        {
            if (splineflags.cyclic)
                WriteCatmullRomCyclicPath(spline, data, splineflags & MoveSplineFlag::Flying);
            else
                WriteCatmullRomPath(spline, data);
        }
        else
            WriteLinearPath(spline, data);
    }

    void PacketBuilder::WriteCreate(const MoveSpline& move_spline, ByteBuffer& data)
    {
        //WriteClientStatus(mov, data);
        //data.append<float>(&mov.m_float_values[SpeedWalk], SpeedMaxCount);
        //if (mov.SplineEnabled())
        {
            MoveSplineFlag const& splineFlags = move_spline.splineflags;

            data << splineFlags.raw();

            if (splineFlags.final_angle)
            {
                data << move_spline.facing.angle;
            }
            else if (splineFlags.final_target)
            {
                data << move_spline.facing.target;
            }
            else if (splineFlags.final_point)
            {
                data << move_spline.facing.f.x << move_spline.facing.f.y << move_spline.facing.f.z;
            }

            data << move_spline.timePassed();
            data << move_spline.Duration();
            data << move_spline.GetId();

            data << float(1.f);                             // splineInfo.duration_mod; added in 3.1
            data << float(1.f);                             // splineInfo.duration_mod_next; added in 3.1

            data << move_spline.vertical_acceleration;      // added in 3.1
            data << move_spline.effect_start_time;          // added in 3.1

            uint32 nodes = move_spline.getPath().size();
            data << nodes;
            data.append<Vector3>(&move_spline.getPath()[0], nodes);
            data << uint8(move_spline.spline.mode());       // added in 3.1
            data << (move_spline.isCyclic() ? Vector3::zero() : move_spline.FinalDestination());
        }
    }
}
