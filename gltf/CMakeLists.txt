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

include_directories(${GLM_SRC_DIR})
include_directories(include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../json-src/src)

set(GLTF_INCLUDES
    "include/gltf/base64.h"
    "include/gltf/conversions.h"
    "include/gltf/format.h"
    "include/gltf/optional.h"
    "include/gltf/parser.h"
    "include/gltf/types.h"
)

set(GLTF_SRCS
    "src/base64.cpp"
    "src/conversions.cpp"
    "src/parser.cpp"
    "src/types.cpp"
)

add_library(gltf ${GLTF_INCLUDES} ${GLTF_SRCS})
