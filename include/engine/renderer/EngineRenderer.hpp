#pragma once
#include "engine/renderer/RenderGraphRenderer.hpp"
#include "engine/renderer/ResourceManager.hpp"
#include "engine/scene/Camera.hpp"

namespace egn {

class EngineRenderer : public RenderGraphRenderer {
private:
    struct EngineRendererImpl *mImpl = nullptr;
public:
    EngineRenderer(Registry &reg, vk::Device const &device);
    EngineRenderer(EngineRenderer &&o);
    EngineRenderer &operator=(EngineRenderer &&o);
    ~EngineRenderer();

    EngineRenderer(EngineRenderer const &) = delete;
    EngineRenderer &operator=(EngineRenderer const &) = delete;

    void setCamera(RestrictedEntity<Camera> camera);
    void preFrame(RestrictedEntity<vk::Image> target, uint frameIndex);
    void render(CommandBuffers &commandBuffers);
};

// Data that is available to all passes via userData pointer.
// All this mess is because the user data pointer is void *
// and some data is not meant to be possibly changed.
// I am also too lazy to change the data pointer type.
class PassData {
private:
    Registry *mReg = nullptr;
    vk::RingBuffer mUniformBuffer;
    DescriptorManager mDescManager;

    vk::Device mDevice;
    VkFormat mDepthFormat = VK_FORMAT_UNDEFINED;
    Entity mCamera;
    uint mFrameIndex = 0;

    friend EngineRenderer;
public:
    inline Registry &reg() {
        return *mReg;
    }
    inline vk::RingBuffer &uniformBuffer() {
        return mUniformBuffer;
    }
    inline DescriptorManager &descManager() {
        return mDescManager;
    }
    inline vk::Device const &device() {
        return mDevice;
    }
    inline VkFormat depthFormat() {
        return mDepthFormat;
    }
    inline Entity camera() {
        return mCamera;
    }
    inline uint frameIndex() {
        return mFrameIndex;
    }
};

// Some utils here

VkPipelineColorBlendAttachmentState constexpr ALPHA_BLENDING{
    .blendEnable = true,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = 0xFF,
};
VkPipelineColorBlendAttachmentState constexpr ADDITIVE_BLENDING{
    .blendEnable = true,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = 0xFF,
};
VkPipelineColorBlendAttachmentState constexpr NO_BLENDING{
    .blendEnable = false,
    .colorWriteMask = 0xFF,
};

rg::ResourceTraits constexpr COLOR_ATTACHMENT_TRAITS{
    .imageTraits = {
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
    },
    .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .stages = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
};
rg::ResourceTraits constexpr DEPTH_STENCIL_ATTACHMENT_TRAITS{
    .imageTraits = {
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
        .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
    },
    .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    .stages = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
};
rg::ResourceTraits constexpr SHADER_COLOR_READ_TRAITS{
    .bufferTraits = {
        .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    },
    .imageTraits = {
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    },
    .access = VK_ACCESS_2_SHADER_READ_BIT,
    .stages = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
};
rg::ResourceTraits constexpr SHADER_DEPTH_READ_TRAITS{
    .bufferTraits = {
        .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    },
    .imageTraits = {
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
        .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
        .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    },
    .access = VK_ACCESS_2_SHADER_READ_BIT,
    .stages = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
};
rg::ResourceTraits constexpr TRANSFER_SRC_TRAITS{
    .imageTraits = {
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    },
    .access = VK_ACCESS_2_TRANSFER_READ_BIT,
    .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
};
rg::ResourceTraits constexpr TRANSFER_DST_TRAITS{
    .imageTraits = {
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    },
    .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
};
rg::RenderGraphImageCreateInfo constexpr MAIN_IMAGE_INFO{
    .imageInfo = {
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .view = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT},
    },
    .resizeToTarget = true,
};

template <typename T> inline uint64_t pushUniform(vk::RingBuffer &uniformBuffer, T const &data, uint frame, uint alignment = 16) {
    uint64_t offset = uniformBuffer.request(sizeof(data), frame, alignment);
    std::memcpy(static_cast<char *>(uniformBuffer.getBuffer()->hostAddress) + offset, &data, sizeof(data));
    offset += uniformBuffer.getBuffer()->deviceAddress;
    return offset;
}
template <std::ranges::range T> inline uint64_t pushUniform(vk::RingBuffer &uniformBuffer, T const &data, uint frame, uint alignment = 16) {
    uint32_t size = std::ranges::size(data) * sizeof(*std::ranges::data(data));
    uint64_t offset = uniformBuffer.request(size, frame, alignment);
    std::memcpy(static_cast<char *>(uniformBuffer.getBuffer()->hostAddress) + offset, std::ranges::data(data), size);
    offset += uniformBuffer.getBuffer()->deviceAddress;
    return offset;
}
enum class ShaderType { Glsl, Slang, Auto };
inline vk::Shader makeShader(VkDevice device, std::string_view path, ShaderType type = ShaderType::Auto) {
    if(type == ShaderType::Auto) {
        auto ext = path.substr(path.find_last_of('.'));
        if(ext == ".glsl") {
            type = ShaderType::Glsl;
        } else if(ext == ".slang") {
            type = ShaderType::Slang;
        } else {
            LOG_ERROR("Could not deduce shader type for extension \"{}\" in \"{}\"", ext, path);
            return { };
        }
    }

    auto shader = vk::makeShader({
        .backend = type == ShaderType::Glsl ? vk::ShaderBackend::GLSL : vk::ShaderBackend::SLANG,
        .src = "shaders/" + std::string(path),
        .bin = "shaders-bin/" + std::string(path),
        .device = device,
        .includeDirs = { "shaders" },
        .systemIncludeDirs = { "shaders/lib" },
        .definitions = { },
        .debugInfo = true,
    });
    assert(shader.valid && "Krill issue");

    return shader;
}

};
