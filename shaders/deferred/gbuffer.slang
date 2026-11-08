struct Material {
    struct MaterialProperties {
        float3 ambient;
        float4 albedo;
        float3 specular;
        float3 emission;

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
    
    MaterialProperties properties;
    MaterialTextures textures;
};
struct BoneData {
    column_major float4x4[] data;
};
struct ModelData {
    column_major float4x4 modelMat;
    column_major float4x4 normMat;
    
    Material material;
    BoneData *boneData;
};

static const uint MAX_WEIGHTS = 6;
struct Attributes {
    float2 uv;
    float3 normal;
    float3 tangent;
    int[MAX_WEIGHTS] boneIDs;
    float[MAX_WEIGHTS] boneWeights;
};

struct VertexData {
    Attributes[] data;
};
struct UniformBuffer {
    column_major float4x4 projMat;
    column_major float4x4 viewMat;
};
struct InstanceData {
    ModelData[] data;
};
struct PushConstants {
    UniformBuffer *uBuffer;
    InstanceData *instanceData;
    StructuredBuffer<uint32_t, ScalarDataLayout> *indexData;
    StructuredBuffer<float3, ScalarDataLayout> *posData;
    VertexData *vertexData;
};

struct VsOut {
    float4 clipPos : SV_Position;
    float3 viewPos;
    float2 uv;
    float3 tangent; // idk weird slang shenanigans
    float3 bitangent;
    float3 normal;
    nointerpolation uint instanceID;
};

[[shader("vertex")]]
VsOut vsMain(uint vertexID : SV_VertexID, uint instanceID : SV_INSTANCEID, uniform PushConstants pushConstants) {
    ModelData modelData = pushConstants.instanceData->data[instanceID];
    VsOut out;
    out.instanceID = instanceID;

    float3 position = float3(0);
    out.normal = float3(0);
    out.tangent = float3(0);

    uint index = (*pushConstants.indexData)[vertexID];
    float3 attrPos = (*pushConstants.posData)[index];
    Attributes attr = pushConstants.vertexData.data[index];

    if(modelData.boneData) {
        for(int i = 0; i < 4; ++i) {
            if(attr.boneIDs[i] == -1) continue;
            float4x4 matrix = modelData.boneData.data[int(attr.boneIDs[i])];
            float weight = attr.boneWeights[i];
            position     += mul(float4(attrPos,      1), matrix).xyz * weight;
            out.normal   += mul(float4(attr.normal,  0), matrix).xyz * weight;
            out.tangent  += mul(float4(attr.tangent, 0), matrix).xyz * weight;
        }
    } else {
        position = attrPos;
        out.normal = attr.normal;
        out.tangent = attr.tangent;
    }

    out.viewPos = mul(mul(pushConstants.uBuffer->viewMat, modelData.modelMat), float4(position, 1)).xyz;
    out.clipPos = mul(pushConstants.uBuffer->projMat, float4(out.viewPos, 1));
    out.uv = attr.uv;
    
    out.normal  = normalize(mul(mul(pushConstants.uBuffer->viewMat, modelData.normMat), float4(out.normal, 0))).xyz;
    out.tangent = normalize(mul(mul(pushConstants.uBuffer->viewMat, modelData.normMat), float4(out.tangent, 0))).xyz;
    out.tangent = normalize(out.tangent - dot(out.tangent, out.normal) * out.normal);
    out.bitangent = cross(out.tangent, out.normal);

    return out;
}

[[vk::binding(0, 0)]] 
uniform Sampler2D uTextures[];

struct FsOut {
    [[vk::location(0)]] float4 albedo;
    [[vk::location(1)]] float4 position;
    [[vk::location(2)]] float4 normal;
    [[vk::location(3)]] float4 pbr;
};

float4 texture(uint index, float2 uv) {
    return uTextures[index].Sample(uv);
}

[[shader("fragment")]]
FsOut fsMain(VsOut in, uniform PushConstants pushConstants) {
    ModelData modelData = pushConstants.instanceData->data[in.instanceID];

    float3x3 tbn = transpose(float3x3(in.tangent, in.bitangent, in.normal));
    var uv = in.uv;
    FsOut out;
    out.albedo = modelData.material.properties.albedo * texture(modelData.material.textures.albedo, uv);
    out.position = float4(in.viewPos, 1);

    out.normal.a = 1;
    out.normal.xyz = texture(modelData.material.textures.normal, uv).rgb * 2 - 1;
    out.normal.xyz = mul(tbn, out.normal.xyz);
    out.normal.xyz = normalize(out.normal.xyz) * 0.5 + 0.5;

    out.pbr.r = clamp(modelData.material.properties.metallic * texture(modelData.material.textures.metallic, uv).r, 0, 1);
    out.pbr.g = clamp(modelData.material.properties.roughness * texture(modelData.material.textures.roughness, uv).r, 0, 1);
    out.pbr.b = 0; // unused lol 
    out.pbr.a = 0; // unused lol

    out.albedo.a = 1;

    return out;
}

