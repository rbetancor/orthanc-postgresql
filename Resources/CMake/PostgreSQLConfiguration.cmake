# Orthanc - A Lightweight, RESTful DICOM Store
# Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
# Department, University Hospital of Liege, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU Affero General Public License
# as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Affero General Public License for more details.
# 
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.


#####################################################################
## PostgreSQL
#####################################################################

if (STATIC_BUILD OR NOT USE_SYSTEM_LIBPQ)
  SET(PQ_SOURCES_DIR ${CMAKE_BINARY_DIR}/postgresql-9.4.0)
  DownloadPackage(
    "349552802c809c4e8b09d8045a437787"
    "http://www.montefiore.ulg.ac.be/~jodogne/Orthanc/ThirdPartyDownloads/postgresql-9.4.0.tar.gz"
    "${PQ_SOURCES_DIR}")

  if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(PQ_CONFIG_H ${PQ_SOURCES_DIR}/src/include/pg_config.h.win32)

    if (${MSVC})
      configure_file(
        ${PQ_SOURCES_DIR}/src/include/pg_config.h.win32
        ${AUTOGENERATED_DIR}/pg_config.h
        COPY_ONLY)

      configure_file(
        ${PQ_SOURCES_DIR}/src/include/pg_config_ext.h.win32
        ${AUTOGENERATED_DIR}/pg_config_ext.h
        COPY_ONLY)

    else()
      if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
        set(PQ_CONFIG_H ${CMAKE_SOURCE_DIR}/Resources/Platforms/pg_config-windows64.h)
      else()
        set(PQ_CONFIG_H ${CMAKE_SOURCE_DIR}/Resources/Platforms/pg_config-windows32.h)
      endif()
    endif()

    add_definitions(
      -DEXEC_BACKEND
      )

    configure_file(
      ${PQ_SOURCES_DIR}/src/include/port/win32.h
      ${AUTOGENERATED_DIR}/pg_config_os.h
      COPY_ONLY)

  else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

    if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
      set(PQ_CONFIG_H ${CMAKE_SOURCE_DIR}/Resources/Platforms/pg_config-linux64.h)
    else()
      set(PQ_CONFIG_H ${CMAKE_SOURCE_DIR}/Resources/Platforms/pg_config-linux32.h)
    endif()

    add_definitions(
      -D_GNU_SOURCE
      -D_THREAD_SAFE
      -D_POSIX_PTHREAD_SEMANTICS
      )

    configure_file(
      ${PQ_SOURCES_DIR}/src/include/port/linux.h
      ${AUTOGENERATED_DIR}/pg_config_os.h
      COPY_ONLY)
  endif()


  configure_file(
    ${PQ_CONFIG_H}
    ${AUTOGENERATED_DIR}/pg_config.h
    COPY_ONLY
    )

  configure_file(
    ${CMAKE_SOURCE_DIR}/Resources/Platforms/pg_config_ext.h
    ${AUTOGENERATED_DIR}/pg_config_ext.h
    COPY_ONLY
    )

  file(WRITE
    ${AUTOGENERATED_DIR}/pg_config_paths.h
    "")

  add_definitions(
    -D_REENTRANT
    -DFRONTEND
    -DUNSAFE_STAT_OK
    -DSYSCONFDIR=""
    )

  include_directories(
    ${PQ_SOURCES_DIR}/src/include
    ${PQ_SOURCES_DIR}/src/include/libpq
    ${PQ_SOURCES_DIR}/src/interfaces/libpq
    )

  set(LIBPQ_SOURCES
    ${PQ_SOURCES_DIR}/src/interfaces/libpq/fe-auth.c 
    ${PQ_SOURCES_DIR}/src/interfaces/libpq/fe-connect.c
    ${PQ_SOURCES_DIR}/src/interfaces/libpq/fe-exec.c
    ${PQ_SOURCES_DIR}/src/interfaces/libpq/fe-lobj.c
    ${PQ_SOURCES_DIR}/src/interfaces/libpq/fe-misc.c
    ${PQ_SOURCES_DIR}/src/interfaces/libpq/fe-print.c
    ${PQ_SOURCES_DIR}/src/interfaces/libpq/fe-protocol2.c
    ${PQ_SOURCES_DIR}/src/interfaces/libpq/fe-protocol3.c
    ${PQ_SOURCES_DIR}/src/interfaces/libpq/fe-secure.c
    ${PQ_SOURCES_DIR}/src/interfaces/libpq/libpq-events.c
    ${PQ_SOURCES_DIR}/src/interfaces/libpq/pqexpbuffer.c

    # libpgport C files we always use
    ${PQ_SOURCES_DIR}/src/port/chklocale.c
    ${PQ_SOURCES_DIR}/src/port/inet_net_ntop.c
    ${PQ_SOURCES_DIR}/src/port/noblock.c
    ${PQ_SOURCES_DIR}/src/port/pgstrcasecmp.c
    ${PQ_SOURCES_DIR}/src/port/pqsignal.c
    ${PQ_SOURCES_DIR}/src/port/thread.c

    ${PQ_SOURCES_DIR}/src/backend/libpq/ip.c
    ${PQ_SOURCES_DIR}/src/backend/libpq/md5.c
    ${PQ_SOURCES_DIR}/src/backend/utils/mb/encnames.c
    ${PQ_SOURCES_DIR}/src/backend/utils/mb/wchar.c
    ${PQ_SOURCES_DIR}/src/port/strlcpy.c
    )

  if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    include_directories(
      ${PQ_SOURCES_DIR}/src/include/port/win32
      ${PQ_SOURCES_DIR}/src/port
      )
    
    LIST(APPEND LIBPQ_SOURCES
      # libpgport C files that are needed if identified by configure
      ${PQ_SOURCES_DIR}/src/interfaces/libpq/pthread-win32.c
      ${PQ_SOURCES_DIR}/src/interfaces/libpq/win32.c
      ${PQ_SOURCES_DIR}/src/port/crypt.c 
      ${PQ_SOURCES_DIR}/src/port/inet_aton.c
      ${PQ_SOURCES_DIR}/src/port/open.c
      ${PQ_SOURCES_DIR}/src/port/pgsleep.c
      ${PQ_SOURCES_DIR}/src/port/snprintf.c
      ${PQ_SOURCES_DIR}/src/port/system.c 
      ${PQ_SOURCES_DIR}/src/port/win32setlocale.c
      ${PQ_SOURCES_DIR}/src/port/getaddrinfo.c
      )
      
    if (${CMAKE_COMPILER_IS_GNUCXX})
      LIST(APPEND LIBPQ_SOURCES ${PQ_SOURCES_DIR}/src/port/win32error.c)
    endif()
  endif()

  if (${CMAKE_COMPILER_IS_GNUCXX})
    LIST(APPEND LIBPQ_SOURCES
      ${PQ_SOURCES_DIR}/src/port/getpeereid.c
      )
  elseif (${MSVC})
    include_directories(
      ${PQ_SOURCES_DIR}/src/include/port/win32_msvc
      )
    
    LIST(APPEND LIBPQ_SOURCES
      ${PQ_SOURCES_DIR}/src/port/dirent.c 
      ${PQ_SOURCES_DIR}/src/port/dirmod.c 
      )
  endif()

  source_group(ThirdParty\\PostgreSQL REGULAR_EXPRESSION ${PQ_SOURCES_DIR}/.*)

else()
  include(${CMAKE_SOURCE_DIR}/Resources/CMake/FindPostgreSQL.cmake)
  include_directories(
    ${PostgreSQL_INCLUDE_DIR}
    ${PostgreSQL_TYPE_INCLUDE_DIR}
    )
  link_libraries(${PostgreSQL_LIBRARY})
endif()
