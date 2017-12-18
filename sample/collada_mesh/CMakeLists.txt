#
# Copyright 2016 Google Inc. All Rights Reserved.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

include_directories(include)
include_directories(../../types/include)
include_directories(../../vcc/include)
include_directories(${VULKAN_CPP_LIBRARY_BINARY_DIR}/vcc/include)
include_directories(../../vcc-image/include)
include_directories(${GLM_SRC_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../../opencollada-src/COLLADABaseUtils/include/)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../../opencollada-src/COLLADAFramework/include/)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../../opencollada-src/COLLADASaxFrameworkLoader/include/)
if(NOT VULKAN_SDK_DIR STREQUAL "")
  include_directories(${VULKAN_SDK_DIR}/include)
  link_directories(${VULKAN_SDK_DIR}/lib)
endif()

set(SAMPLE_COLLADA_INCLUDES
  "include/collada_parser.h"
  "include/transform_iterator.h"
  "include/triangulate.h"
)

set(SAMPLE_COLLADA_SRCS
  "src/collada.cpp"
  "src/collada_parser.cpp"
  "src/triangulate.cpp"
)

set(SAMPLE_COLLADA_SHADER_SRCS
  "src/collada-frag.frag"
  "src/collada-geom.geom"
  "src/collada-vert.vert"
  "src/collada-edge-frag.frag"
  "src/collada-edge-vert.vert"
  "src/collada-point-frag.frag"
  "src/collada-point-vert.vert"
)

if(DEFINED ANDROID_NDK)
  include_directories(${ANDROID_NDK}/sources/android/native_app_glue/)

  add_library(sample_collada SHARED ${SAMPLE_COLLADA_SRCS} ${SAMPLE_COLLADA_INCLUDES}
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

  find_library(log-lib log)
  find_library(android-lib android)
  find_library(vulkan-lib vulkan)
  find_library(jnigraphics-lib jnigraphics)
  target_link_libraries(sample_collada ${log-lib} ${android-lib} ${vulkan-lib} ${jnigraphics-lib})
else()
  add_executable(sample_collada ${SAMPLE_COLLADA_SRCS} ${SAMPLE_COLLADA_INCLUDES})
endif()

target_link_libraries(sample_collada vcc vcc-image OpenCOLLADAFramework_static OpenCOLLADASaxFrameworkLoader_static)

if(WIN32)
  target_link_libraries(sample_collada wsock32 ws2_32)
endif()

if(NOT DEFINED ANDROID_NDK)
  foreach(FILE ${SAMPLE_COLLADA_SHADER_SRCS})
    get_filename_component(FILEWE ${FILE} NAME_WE)
    set(FILE_OUTPUT ${FILEWE}.spv)
    add_custom_command(TARGET sample_collada POST_BUILD DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${FILE}
      COMMAND glslangValidator
      ARGS -V -o ${CMAKE_CURRENT_BINARY_DIR}/${FILE_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}/${FILE}
      WORKING_DIRECTORY .)
  endforeach()

  add_custom_command(TARGET sample_collada POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/textures ${CMAKE_CURRENT_BINARY_DIR}/textures)
  add_custom_command(TARGET sample_collada POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/models ${CMAKE_CURRENT_BINARY_DIR}/models)
  add_custom_command(TARGET sample_collada POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:png> $<TARGET_FILE_DIR:sample_collada>/)
  add_custom_command(TARGET sample_collada POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:zlib> $<TARGET_FILE_DIR:sample_collada>/)
endif()
