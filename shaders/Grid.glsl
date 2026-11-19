#version 460
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

layout(push_constant) uniform PushConstants {
    mat4 projMat;
    mat4 viewMat;
    // vec4 color;
} pushConstants;

/////////////
#pragma stage vertex
/////////////

layout(location=0) out VS_OUT {
    vec3 fragmentPosition;
    vec3 cameraPosition;
    vec2 texCoord;
} vs_out;

const vec3 vertices[4] = vec3[4](
    vec3(-1.0f, 0.0f, -1.0f),
    vec3(-1.0f, 0.0f,  1.0f),
    vec3( 1.0f, 0.0f, -1.0f),
    vec3( 1.0f, 0.0f,  1.0f)
);
const vec2 texCoords[4] = vec2[4](
    vec2(0.0f, 0.0f),
    vec2(0.0f, 1.0f),
    vec2(1.0f, 0.0f),
    vec2(1.0f, 1.0f)
);

const float gridSize = 50;
const float gridHeight = 0;
const float gridTiling = 0.1;

void main() {
    vec3 cameraPosition = inverse(pushConstants.viewMat)[3].xyz;
    vec3 vertexPosition = vertices[gl_VertexIndex] * gridSize;
    vertexPosition += floor(cameraPosition * gridTiling) / gridTiling;
    vertexPosition.y = gridHeight;
    vs_out.fragmentPosition = vertexPosition;
    vs_out.cameraPosition = cameraPosition;
    vs_out.texCoord = texCoords[gl_VertexIndex];
    gl_Position = pushConstants.projMat * pushConstants.viewMat * vec4(vs_out.fragmentPosition, 1);
}

///////////////
#pragma stage fragment
///////////////
layout(location=0) out vec4 oColor;


layout(location=0) in VS_OUT {
    vec3 fragmentPosition;
    vec3 cameraPosition;
    vec2 texCoord;
} fs_in;

const float maxFadeDistance = 50;
const float gridTiling = 100;
const float uvTiling = 1;

#include <grid.glsl>
#include <hex.glsl>

void main() {
    oColor = hex4(0xFFFFFFFF);

    float falloff = smoothstep(1.0, 0.0, length(fs_in.fragmentPosition - fs_in.cameraPosition) / maxFadeDistance); // fade the grid
    oColor.a = (grid(0.01, uvTiling * 10 * fs_in.texCoord, gridTiling) + grid(0.003, uvTiling * 1 * fs_in.texCoord, gridTiling)) * falloff;
}