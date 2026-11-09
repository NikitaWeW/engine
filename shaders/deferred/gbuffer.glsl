#version 460
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

struct MaterialProperties {
    vec3 ambient;
    vec4 albedo;
    vec3 specular;
    vec3 emission;

    float shininess;
    float metallic;
    float roughness;
    float ior;
};
struct MaterialTextures {
    uint albedo;
    uint metallic;
    uint roughness;
    uint ambient;
    uint normal;
    uint displacement;
};
struct Material {
    MaterialProperties properties;
    MaterialTextures textures;
};

layout(buffer_reference, scalar, buffer_reference_align = 16) readonly buffer BoneData {
    mat4 data[];
};
struct ModelData {
    mat4 modelMat;
    mat4 normMat;

    Material material;
    BoneData boneData;
    bool animated;
};
layout(buffer_reference, scalar, buffer_reference_align = 16) readonly buffer InstanceData {
    ModelData data[];
};
layout(buffer_reference, scalar, buffer_reference_align = 16) readonly buffer UniformBuffer {
    mat4 projMat;
    mat4 viewMat;
};

layout(push_constant) uniform PushConstants {
    UniformBuffer uBuffer;
    InstanceData instanceData;
}
pushConstants;

#pragma stage vertex

layout(location = 0) out VsOut {
    vec3 viewPos;
    vec2 uv;
    mat3 tbn;
    flat uint instanceID;
}
vs_out;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in ivec4 aBoneID;
layout(location = 5) in vec4 aBoneWeights;

void main() {
    ModelData modelData = pushConstants.instanceData.data[gl_InstanceIndex];
    vs_out.instanceID = gl_InstanceIndex;

    vec3 position = vec3(0);
    vec3 normal = vec3(0);
    vec3 tangent = vec3(0);

    if(modelData.animated) {
        for(int i = 0; i < 4; ++i) {
            if(aBoneID[i] == -1)
                continue;
            mat4 matrix = modelData.boneData.data[int(aBoneID[i])];
            float weight = aBoneWeights[i];
            position += (matrix * vec4(aPosition, 1)).xyz * weight;
            normal += (matrix * vec4(aNormal, 0)).xyz * weight;
            tangent += (matrix * vec4(aTangent, 0)).xyz * weight;
        }
    } else {
        position = aPosition;
        normal = aNormal;
        tangent = aTangent;
    }

    vs_out.viewPos = (pushConstants.uBuffer.viewMat * modelData.modelMat * vec4(position, 1)).xyz;
    gl_Position = (pushConstants.uBuffer.projMat * vec4(vs_out.viewPos, 1));
    vs_out.uv = aUV;

    normal = normalize(pushConstants.uBuffer.viewMat * modelData.normMat * vec4(normal, 0)).xyz;
    tangent = normalize(pushConstants.uBuffer.viewMat * modelData.normMat * vec4(tangent, 0)).xyz;
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    vec3 bitangent = cross(tangent, normal);
    vs_out.tbn = mat3(tangent, bitangent, normal);
}

#pragma stage fragment

layout(location = 0) in VsOut {
    vec3 viewPos;
    vec2 uv;
    mat3 tbn;
    flat uint instanceID;
}
fs_in;

layout(set = 0, binding = 0) uniform sampler2D uTextures[];

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oPosition;
layout(location = 2) out vec4 oNormal;
layout(location = 3) out vec4 oPbr;

#extension GL_EXT_nonuniform_qualifier : require

vec4 texture(uint index, vec2 uv) {
    return texture(uTextures[index], uv);
}

void main() {
    ModelData modelData = pushConstants.instanceData.data[fs_in.instanceID];

    vec2 uv = fs_in.uv;
    oAlbedo = modelData.material.properties.albedo * texture(modelData.material.textures.albedo, uv);
    oPosition = vec4(fs_in.viewPos, 1);

    oNormal.a = 1;
    oNormal.xyz = texture(modelData.material.textures.normal, uv).rgb * 2 - 1;
    oNormal.xyz = fs_in.tbn * oNormal.xyz;
    oNormal.xyz = normalize(oNormal.xyz) * 0.5 + 0.5;

    oPbr.r = clamp(modelData.material.properties.metallic * texture(modelData.material.textures.metallic, uv).r, 0, 1);
    oPbr.g = clamp(modelData.material.properties.roughness * texture(modelData.material.textures.roughness, uv).r, 0, 1);
    oPbr.b = 0; // unused lol
    oPbr.a = 0; // unused lol

    oAlbedo.a = 1;
}
