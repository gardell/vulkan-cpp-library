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
const float pi = 3.141592653589793;
const float min_roughness = 0.04;

struct light_type {
    vec3 direction;
    vec3 color;
};

layout(constant_id = 0) const int max_lights = 1;
layout(constant_id = 1) const bool enable_tangent = true;
layout(constant_id = 2) const bool enable_texturing = true;
layout(constant_id = 3) const bool enable_base_color_sampler = true;
layout(constant_id = 4) const bool enable_emissive_sampler = true;
layout(constant_id = 5) const bool enable_metallic_roughness_sampler = true;
layout(constant_id = 6) const bool enable_occlusion_sampler = true;
layout(constant_id = 7) const int base_color_sampler_texcoord_index = 0;
layout(constant_id = 8) const int normal_sampler_texcoord_index = 0;
layout(constant_id = 9) const int emissive_sampler_texcoord_index = 0;
layout(constant_id = 10) const int metallic_roughness_sampler_texcoord_index = 0;
layout(constant_id = 11) const int occlusion_sampler_texcoord_index = 0;

layout(std140, binding = 1) uniform material {
    vec4 base_color_factor;
    float metallic_factor;
    float roughness_factor;
    float normal_scale;
    float occlusion_strength;
    vec3 emissive_factor;
} umaterial;

layout(std140, binding = 2) uniform light {
    light_type lights[max_lights];
} ulight;

layout (binding = 3) uniform sampler2D base_color_sampler;
layout (binding = 4) uniform sampler2D normal_sampler;
layout (binding = 5) uniform sampler2D emissive_sampler;
layout (binding = 6) uniform sampler2D metallic_roughness_sampler;
layout (binding = 7) uniform sampler2D occlusion_sampler;

layout (location = 0) in vec2 texcoord0;
layout (location = 1) in vec2 texcoord1;
layout (location = 2) in mat3 normal;
layout (location = 5) in vec3 eye_position;

layout (location = 0) out vec4 uFragColor;

vec4 SRGBtoLINEAR(vec4 srgbIn) {
    #ifdef MANUAL_SRGB
    #ifdef SRGB_FAST_APPROXIMATION
    vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
    #else //SRGB_FAST_APPROXIMATION
    vec3 bLess = step(vec3(0.04045),srgbIn.xyz);
    vec3 linOut = mix( srgbIn.xyz/vec3(12.92), pow((srgbIn.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), bLess );
    #endif //SRGB_FAST_APPROXIMATION
    return vec4(linOut,srgbIn.w);;
    #else //MANUAL_SRGB
    return srgbIn;
    #endif //MANUAL_SRGB
}

vec3 diffuse(vec3 diffuse_color) {
    return diffuse_color / pi;
}

vec3 specularReflection(vec3 reflectance0, vec3 reflectance90, float VdotH) {
    return reflectance0
      + (reflectance90 - reflectance0)
      * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

float geometricOcclusion(float NdotL, float NdotV, float alphaRoughness) {
    float attenuationL =
      2.0 * NdotL / (NdotL + sqrt(alphaRoughness * alphaRoughness + (1.0 - alphaRoughness * alphaRoughness)
      * (NdotL * NdotL)));
    float attenuationV =
      2.0 * NdotV / (NdotV + sqrt(alphaRoughness * alphaRoughness + (1.0 - alphaRoughness * alphaRoughness)
      * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

float microfacetDistribution(float NdotH, float alphaRoughness) {
    float roughnessSq = alphaRoughness * alphaRoughness;
    float f = (NdotH * roughnessSq - NdotH) * NdotH + 1.0;
    return roughnessSq / (pi * f * f);
}

void main() {
    vec2 roughness_metallic;
    if (enable_texturing && enable_metallic_roughness_sampler) {
        roughness_metallic = texture(metallic_roughness_sampler, metallic_roughness_sampler_texcoord_index == 0 ? texcoord0 : texcoord1).gb * vec2(umaterial.roughness_factor, umaterial.metallic_factor);
    } else {
        roughness_metallic = vec2(umaterial.roughness_factor, umaterial.metallic_factor);
    }
    float roughness_factor = clamp(roughness_metallic.r, min_roughness, 1.0);
    float alpha_roughness = roughness_factor * roughness_factor;
    float metallic_factor = clamp(roughness_metallic.g, 0.0, 1.0);
    vec4 base_color;
    if (enable_texturing && enable_base_color_sampler) {
        base_color = SRGBtoLINEAR(texture(base_color_sampler, base_color_sampler_texcoord_index == 0 ? texcoord0 : texcoord1)) * umaterial.base_color_factor;
    } else {
        base_color = umaterial.base_color_factor;
    }
    
    vec3 f0 = vec3(0.04);
    vec3 diffuse_color = vec3(base_color) * (vec3(1.0) - f0) * (1.0 - metallic_factor);
    vec3 specular_color = mix(f0, vec3(base_color), metallic_factor);

    float reflectance = max(max(specular_color.r, specular_color.g), specular_color.b);
    
    vec3 reflectance0 = specular_color;
    vec3 reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0) * vec3(1.0);

    vec3 n;
    if (enable_tangent && enable_texturing) {
        n = normalize(normal * ((2.0 * vec3(texture(normal_sampler, normal_sampler_texcoord_index == 0 ? texcoord0 : texcoord1)) - 1.0))) * umaterial.normal_scale;
    } else {
        n = transpose(normal)[2] * umaterial.normal_scale;
    }
    vec3 v = -normalize(eye_position);
    // TODO(gardell): Why normalize?
    vec3 reflection = -normalize(reflect(v, n));

    float NdotV = abs(dot(n, v)) + 0.001;
    
    vec3 color;

    for (int i = 0; i < max_lights; ++i) {
        vec3 l = ulight.lights[i].direction;
        vec3 h = normalize(l+v);

        float NdotL = clamp(dot(n, l), 0.001, 1.0);
        float NdotH = clamp(dot(n, h), 0.0, 1.0);
        float LdotH = clamp(dot(l, h), 0.0, 1.0);
        float VdotH = clamp(dot(v, h), 0.0, 1.0);

        // Calculate the shading terms for the microfacet specular shading model
        vec3 F = specularReflection(reflectance0, reflectance90, VdotH);
        float D = microfacetDistribution(NdotH, alpha_roughness);
        float G = geometricOcclusion(NdotL, NdotV, alpha_roughness);

        // Calculation of analytical lighting contribution
        vec3 diffuseContrib = (1.0 - F) * diffuse(diffuse_color);
        vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
        color = NdotL * ulight.lights[i].color * (diffuseContrib + specContrib);
    }

    if (enable_texturing && enable_occlusion_sampler) {
        color *= umaterial.occlusion_strength * texture(occlusion_sampler, occlusion_sampler_texcoord_index == 0 ? texcoord0 : texcoord1).r;
    }

    if (enable_texturing && enable_emissive_sampler) {
        color += SRGBtoLINEAR(texture(emissive_sampler, emissive_sampler_texcoord_index == 0 ? texcoord0 : texcoord1)).rgb * umaterial.emissive_factor;
    } else {
        color += umaterial.emissive_factor;
    }
    
    uFragColor = vec4(pow(color, vec3(1.0/2.2)), base_color.a);
}
