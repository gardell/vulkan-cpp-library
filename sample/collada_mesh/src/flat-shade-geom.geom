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

layout (triangles) in;
layout (triangle_strip, max_vertices=3) out;

layout (location = 0) in vec2 in_texcoord0[];
layout (location = 1) in vec2 in_texcoord1[];
layout (location = 2) in mat3 in_normal[];
layout (location = 5) in vec3 in_eye_position[];

layout (location = 0) out vec2 out_texcoord0;
layout (location = 1) out vec2 out_texcoord1;
layout (location = 2) out mat3 out_normal;
layout (location = 5) out vec3 out_eye_position;

void main() {
    vec3 n = normalize(cross(in_eye_position[1].xyz - in_eye_position[0].xyz, in_eye_position[2].xyz - in_eye_position[0].xyz));
        for(int i = 0; i < gl_in.length(); i++) {
            gl_Position = gl_in[i].gl_Position;
            out_texcoord0 = in_texcoord0[i];
            out_texcoord1 = in_texcoord1[i];
            out_normal = transpose(mat3(vec3(0.0), vec3(0.0), n));
            out_eye_position = in_eye_position[i];
            
            EmitVertex();
        }
}
