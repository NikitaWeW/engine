#pragma once
#include "engine/renderer/IRenderer.hpp"
#include "engine/renderer/RenderGraph.hpp"

namespace egn {

/// Renders the provided render graph
/// Render graph resource "target" is set to the render target and may not be defined.
class RenderGraphRenderer : public IRenderer {
private:
    rg::RenderGraphResult mRenderGraph;
    void *mData = nullptr;
public:
    RenderGraphRenderer() = default;
    void build(rg::RenderGraphBuilder &&builder);
    
    void preFrame(RestrictedEntity<vk::Image> target, uint frameIndex);
    void render(CommandBuffers &commandBuffers);

    inline void setData(void *data) {
        mData = data;
    }
    inline void setResource(std::string const &name, Entity eResource) {
        mRenderGraph.setResource(name, eResource);
    }
    inline rg::RenderGraphResult const &getRenderGraph() const {
        return mRenderGraph;
    }
};

}
