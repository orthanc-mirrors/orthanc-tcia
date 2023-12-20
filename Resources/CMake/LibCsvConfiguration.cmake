# TCIA plugin for Orthanc
# Copyright (C) 2021 Sebastien Jodogne, UCLouvain, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.


if (STATIC_BUILD OR NOT USE_SYSTEM_LIBCSV)
  set(LIBCSV_SOURCES_DIR ${CMAKE_BINARY_DIR}/libcsv-3.0.3)
  DownloadPackage(
    "d3307a7bd41d417da798cd80c80aa42a"
    "https://orthanc.uclouvain.be/downloads/third-party-downloads/libcsv-3.0.3.tar.gz"
    "${LIBCSV_SOURCES_DIR}")

  include_directories(
    ${LIBCSV_SOURCES_DIR}
    )

  set(LIBCSV_SOURCES
    ${LIBCSV_SOURCES_DIR}/libcsv.c
    )

else()
  check_include_file(csv.h HAVE_LIBCSV_H)
  if (NOT HAVE_LIBCSV_H)
    message(FATAL_ERROR "Please install the libcsv-dev package")
  endif()

  link_libraries(csv)
endif()
