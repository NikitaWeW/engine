#version 460
#include <hex.glsl>
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

layout(buffer_reference, scalar, buffer_reference_align = 16) readonly buffer BoneData {
    mat4 data[];
};
layout(buffer_reference, scalar, buffer_reference_align = 16) readonly buffer UniformBuffer {
    mat4 proj;
    mat4 view;
};

layout(push_constant) uniform PushConstants {
    BoneData bones;
    UniformBuffer uBuffer;
}
pushConstants;

#pragma stage vertex

layout(location = 0) out VsOut {
    vec3 viewPos;
    vec3 normal;
}
vs_out;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

void main() {
    mat4 bone = pushConstants.bones.data[gl_InstanceIndex];
    vs_out.viewPos = (pushConstants.uBuffer.view * bone * vec4(aPosition, 1)).xyz;
    gl_Position = pushConstants.uBuffer.proj * vec4(vs_out.viewPos, 1);
    vs_out.normal = normalize(pushConstants.uBuffer.view * inverse(transpose(bone)) * vec4(aNormal, 0)).xyz;
}

#pragma stage fragment

layout(location = 0) in VsOut {
    vec3 viewPos;
    vec3 normal;
}
fs_in;


layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oPosition;
layout(location = 2) out vec4 oNormal;
layout(location = 3) out vec4 oPbr;

void main() {
    oAlbedo = hex4(0x2c5b34ff);
    oNormal = vec4(fs_in.normal, 1);
    oPosition = vec4(fs_in.viewPos, 1);
    oPbr = vec4(0, 1, 0, 0);
}
