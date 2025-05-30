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

#ifndef WARHEAD_DEFINE_H
#define WARHEAD_DEFINE_H

#include "CompilerDefs.h"
#include <cinttypes>

#if WARHEAD_COMPILER == WARHEAD_COMPILER_GNU
#  if !defined(__STDC_FORMAT_MACROS)
#    define __STDC_FORMAT_MACROS
#  endif
#  if !defined(__STDC_CONSTANT_MACROS)
#    define __STDC_CONSTANT_MACROS
#  endif
#  if !defined(_GLIBCXX_USE_NANOSLEEP)
#    define _GLIBCXX_USE_NANOSLEEP
#  endif
#  if defined(HELGRIND)
#    include <valgrind/helgrind.h>
#    undef _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE
#    undef _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER
#    define _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE(A) ANNOTATE_HAPPENS_BEFORE(A)
#    define _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER(A)  ANNOTATE_HAPPENS_AFTER(A)
#  endif
#endif

#define WARHEAD_LITTLEENDIAN 0
#define WARHEAD_BIGENDIAN    1

#if !defined(WARHEAD_ENDIAN)
#  if defined (BOOST_BIG_ENDIAN)
#    define WARHEAD_ENDIAN WARHEAD_BIGENDIAN
#  else
#    define WARHEAD_ENDIAN WARHEAD_LITTLEENDIAN
#  endif
#endif

#if WARHEAD_PLATFORM == WARHEAD_PLATFORM_WINDOWS
#define _USE_MATH_DEFINES
#endif // WARHEAD_PLATFORM

#ifdef WARHEAD_API_USE_DYNAMIC_LINKING
#  if WARHEAD_COMPILER == WARHEAD_COMPILER_MICROSOFT
#    define WH_API_EXPORT __declspec(dllexport)
#    define WH_API_IMPORT __declspec(dllimport)
#  elif WARHEAD_COMPILER == WARHEAD_COMPILER_GNU
#    define WH_API_EXPORT __attribute__((visibility("default")))
#    define WH_API_IMPORT
#  else
#    error compiler not supported!
#  endif
#else
#  define WH_API_EXPORT
#  define WH_API_IMPORT
#endif

#ifdef WARHEAD_API_EXPORT_COMMON
#  define WH_COMMON_API WH_API_EXPORT
#else
#  define WH_COMMON_API WH_API_IMPORT
#endif

#ifdef WARHEAD_API_EXPORT_DATABASE
#  define WH_DATABASE_API WH_API_EXPORT
#else
#  define WH_DATABASE_API WH_API_IMPORT
#endif

#ifdef WARHEAD_API_EXPORT_SHARED
#  define WH_SHARED_API WH_API_EXPORT
#else
#  define WH_SHARED_API WH_API_IMPORT
#endif

#ifdef WARHEAD_API_EXPORT_GAME
#  define WH_GAME_API WH_API_EXPORT
#else
#  define WH_GAME_API WH_API_IMPORT
#endif

#define UI64LIT(N) UINT64_C(N)

using int64 = std::int64_t;
using int32 = std::int32_t;
using int16 = std::int16_t;
using int8 = std::int8_t;
using uint64 = std::uint64_t;
using uint32 = std::uint32_t;
using uint16 = std::uint16_t;
using uint8 = std::uint8_t;

#endif //WARHEAD_DEFINE_H
