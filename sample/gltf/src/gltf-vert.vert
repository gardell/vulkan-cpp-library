/*
 * Copyright 2016 Google Inc. All Rights Reserved.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(constant_id = 1) const bool enable_tangent = true;
layout(constant_id = 2) const bool enable_texturing = true;

layout(std140, binding = 0) uniform buf {
    mat4 modelview_projection_matrix;
    mat4 modelview_matrix;
    mat3 normal_matrix;
} ubuf;

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_tangent;
// TODO(gardell): in_texcoord[2] and map texcoord sets. Then use specialization constants to target texcoords for each feature
layout (location = 3) in vec2 in_texcoord0;
layout (location = 4) in vec2 in_texcoord1;

layout (location = 0) out vec2 texcoord0;
layout (location = 1) out vec2 texcoord1;
layout (location = 2) out mat3 normal;
layout (location = 5) out vec3 eye_position;

out gl_PerVertex {
        vec4 gl_Position;
};

void main() {
    if (enable_texturing) {
        // TODO(gardell): Could optimize away either of these when only one exist
        texcoord0 = vec2(in_texcoord0);
        texcoord1 = vec2(in_texcoord1);
    }
    eye_position = vec3(ubuf.modelview_matrix * vec4(vertex, 1.0));

    if (enable_tangent) {
        normal = transpose(ubuf.normal_matrix
            * mat3(in_tangent, cross(in_normal, in_tangent), in_normal));
    } else {
        normal = transpose(mat3(vec3(0.0), vec3(0.0), ubuf.normal_matrix * in_normal));
    }

    gl_Position = ubuf.modelview_projection_matrix * vec4(vertex, 1.0);

    // GL->VK conventions
    gl_Position.y = -gl_Position.y;
    gl_Position.z = gl_Position.z / 2.0;
}
