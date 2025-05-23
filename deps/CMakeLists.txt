#
# This file is part of the WarheadCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# User has manually chosen to ignore the git-tests, so throw them a warning.
# This is done EACH compile so they can be alerted about the consequences.
#

add_subdirectory(SFMT)
add_subdirectory(WarheadRegex)
add_subdirectory(argon2)
add_subdirectory(boost)
add_subdirectory(fmt)
add_subdirectory(indicators)
add_subdirectory(jemalloc)
add_subdirectory(openssl)
add_subdirectory(ranges-v3)
add_subdirectory(stdfs)
add_subdirectory(threads)
add_subdirectory(utf8cpp)
add_subdirectory(valgrind)

if ((APPS_BUILD AND (NOT APPS_BUILD STREQUAL "none")) OR BUILD_TOOLS_DB_IMPORT)
  add_subdirectory(mysql)
endif()

if (BUILD_APPLICATION_WORLDSERVER OR BUILD_TOOLS_MAPS)
  add_subdirectory(zlib)
  add_subdirectory(g3dlite)
  add_subdirectory(recastnavigation)
endif()

if (BUILD_APPLICATION_WORLDSERVER)
  add_subdirectory(readline)
  add_subdirectory(gsoap)
  add_subdirectory(efsw)
  add_subdirectory(gperftools)
  add_subdirectory(dpp)
endif()

if (BUILD_TOOLS_MAPS)
  add_subdirectory(bzip2)
  add_subdirectory(libmpq)
endif()
