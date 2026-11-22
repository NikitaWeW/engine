#include "Scene.hpp"

namespace rg = egn::rg;
namespace vk = egn::vk;

void scene::TestScene::addCustomGraphics() {
    return;
    auto &builder = mRenderer->builder();

    struct GridPassData {
        egn::Entity pipeline;
        egn::Entity depth;
        egn::Entity out;
    };
    builder.addPass(rg::makePass<GridPassData>(
        "grid", VK_QUEUE_GRAPHICS_BIT,
        [&](rg::RenderPassBuilder &builder, void *) {
            egn::injectPass(builder, "main_image", "grid_main_image", egn::COLOR_ATTACHMENT_TRAITS);
            egn::injectPass(builder, "gbuffer_depth", "grid_gbuffer_depth", egn::DEPTH_STENCIL_ATTACHMENT_TRAITS);
        },
        [&](GridPassData &passData, rg::RenderGraph const &result, void *pRenderer) {
            egn::PassData &renderer = *static_cast<egn::PassData *>(pRenderer);
            if(!passData.pipeline.valid()) {
                passData.pipeline = renderer.reg().create();
                passData.pipeline.name("grid_pipeline");
                passData.pipeline.emplace<vk::Shader>(egn::makeShader(mDevice.device, "Grid.glsl"));
                vk::GraphicsPipelineCreateInfo pipelineCi{
                    .dynamicState = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR },
                    .attachments = {
                        .color = { egn::MAIN_IMAGE_INFO.imageInfo.format },
                        .depth = renderer.depthFormat(),
                    },
                    .depthStencil = {
                        .depthTestEnable = true,
                        .depthWriteEnable = false,
                    },
                    .blending = {
                        .attachments = {egn::ALPHA_BLENDING},
                    },
                };
                passData.pipeline.emplace<vk::Pipeline>(vk::makePipeline(passData.pipeline.get<vk::Shader>(), { }, pipelineCi));
                assert(passData.pipeline.get<vk::Pipeline>().valid);
            }
            passData.depth = result.getResource("grid_main_image");
            passData.out = result.getResource("grid_gbuffer_depth");
            assert(passData.depth.valid());
            assert(passData.out.valid());
        },
        [&](GridPassData &passData, VkCommandBuffer cb, void *pRenderer) {
            egn::PassData &renderer = *static_cast<egn::PassData *>(pRenderer);
            auto const &pipeline = passData.pipeline.get<vk::Pipeline>();

            VkExtent2D extent = {
                passData.out.get<vk::Image>().createInfo.image.dimensions.width,
                passData.out.get<vk::Image>().createInfo.image.dimensions.height,
            };

            std::array<VkRenderingAttachmentInfo, 1> colorAttachmentInfos = {
                VkRenderingAttachmentInfo{
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = passData.out.get<vk::Image>().view,
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue{ .color{
                        { 0.0f, 0.0f, 0.0f, 0.0f },
                    } },
                },
            };

            VkRenderingAttachmentInfo depthAttachmentInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = passData.depth.get<vk::Image>().view,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .clearValue = { .depthStencil = { 1.0f, 0 } },
            };

            VkRenderingInfo renderingInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {
                    .offset = {0, 0},
                    .extent = extent,
                },
                .layerCount = 1,
                .colorAttachmentCount = colorAttachmentInfos.size(),
                .pColorAttachments = colorAttachmentInfos.data(),
                .pDepthAttachment = &depthAttachmentInfo,
            };

            vkCmdBeginRendering(cb, &renderingInfo);

            VkViewport vp{
                .x = 0,
                .y = static_cast<float>(extent.height),
                .width = static_cast<float>(extent.width),
                .height = -static_cast<float>(extent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            vkCmdSetViewport(cb, 0, 1, &vp);
            VkRect2D scissor{
                .offset = { 0, 0 },
                .extent{ .width = extent.width, .height = extent.height },
            };
            vkCmdSetScissor(cb, 0, 1, &scissor);

            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

            struct PushConstants {
                // Inverted
                glm::mat4 projMat;
                glm::mat4 viewMat;
            } pushConstants{
                .projMat = glm::inverse(renderer.camera().get<egn::Camera>().projMat),
                .viewMat = glm::inverse(renderer.camera().get<egn::Camera>().viewMat),
            };

            vkCmdPushConstants(cb,
                passData.pipeline.get<vk::Pipeline>().layout.layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(pushConstants),
                &pushConstants);

            vkCmdDraw(cb, 4, 1, 0, 0);

            vkCmdEndRendering(cb);
        }
    ));
}
