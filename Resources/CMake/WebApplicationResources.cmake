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


set(BASE_URL "https://orthanc.uclouvain.be/downloads/third-party-downloads")

DownloadPackage(
  "da0189f7c33bf9f652ea65401e0a3dc9"
  "${BASE_URL}/dicom-web/bootstrap-4.3.1.zip"
  "${CMAKE_CURRENT_BINARY_DIR}/bootstrap-4.3.1")

DownloadPackage(
  "8242afdc5bd44105d9dc9e6535315484"
  "${BASE_URL}/dicom-web/vuejs-2.6.10.tar.gz"
  "${CMAKE_CURRENT_BINARY_DIR}/vue-2.6.10")

DownloadPackage(
  "3e2b4e1522661f7fcf8ad49cb933296c"
  "${BASE_URL}/dicom-web/axios-0.19.0.tar.gz"
  "${CMAKE_CURRENT_BINARY_DIR}/axios-0.19.0")

set(WEB_APPLICATION_RESOURCES
  AXIOS_MIN_JS           ${CMAKE_CURRENT_BINARY_DIR}/axios-0.19.0/dist/axios.min.js
  AXIOS_MIN_MAP          ${CMAKE_CURRENT_BINARY_DIR}/axios-0.19.0/dist/axios.min.map
  BOOTSTRAP_MIN_CSS      ${CMAKE_CURRENT_BINARY_DIR}/bootstrap-4.3.1/dist/css/bootstrap.min.css
  BOOTSTRAP_MIN_CSS_MAP  ${CMAKE_CURRENT_BINARY_DIR}/bootstrap-4.3.1/dist/css/bootstrap.min.css.map
  VUE_MIN_JS             ${CMAKE_CURRENT_BINARY_DIR}/vue-2.6.10/dist/vue.min.js
  )
