#include "engine/renderer/RenderGraph.hpp"
#include "tracy/Tracy.hpp"

#ifndef RENDER_GRAPH_LOG
#define RENDER_GRAPH_LOG 1
#endif

#if RENDER_GRAPH_LOG
#define RG_LOG_TRACE LOG_TRACE
#else
#define RG_LOG_TRACE(...)
#endif

using namespace egn;
using namespace egn::rg;

enum class NodeState { None = 0, Visited, Added };

struct ResourceCreateInfo {
    enum Type { Unknown, External, Image, Buffer } type = Unknown;
    union {
        RenderGraphExternalResourceInfo external;
        RenderGraphImageCreateInfo image;
        RenderGraphBufferCreateInfo buffer;
    };

    ResourceCreateInfo() {
    }
    ResourceCreateInfo(ResourceCreateInfo const &o) {
        *this = o;
    }
    ResourceCreateInfo(ResourceCreateInfo &&o) {
        *this = std::move(o);
    }
    ResourceCreateInfo &operator=(ResourceCreateInfo const &o) {
        type = o.type;
        switch(type) {
            case External: external = o.external; break;
            case Image: image = o.image; break;
            case Buffer: buffer = o.buffer; break;
            case Unknown: break;
        }
        return *this;
    }
    ResourceCreateInfo &operator=(ResourceCreateInfo &&o) {
        type = std::move(o.type);
        switch(type) {
            case External: external = std::move(o.external); break;
            case Image: image = std::move(o.image); break;
            case Buffer: buffer = std::move(o.buffer); break;
            case Unknown: break;
        }
        return *this;
    }
    ~ResourceCreateInfo() {
    }
    ResourceCreateInfo(RenderGraphExternalResourceInfo const &info) {
        type = External;
        external = info;
    }
    ResourceCreateInfo(RenderGraphImageCreateInfo const &info) {
        type = Image;
        image = info;
    }
    ResourceCreateInfo(RenderGraphBufferCreateInfo const &info) {
        type = Buffer;
        buffer = info;
    }
};
struct ResourceAllocationInfo {
    ResourceCreateInfo ci;
    VkImageUsageFlags imageUsage = 0;
    VkBufferUsageFlags bufferUsage = 0;
    VkDeviceSize bufferSize = 0;
};
struct PlzAllocate { };
struct ResourceInfo {
    std::string name;

    Entity eResource;
    bool keepImage = false;
    ResourceCreateInfo ci;

    // Indices in the pass stack
    uint lifetimeBegin = std::numeric_limits<uint>::max();
    uint lifetimeEnd = 0;

    std::vector<ResourceReadUsage> read;
    std::vector<ResourceUsage> written; // In chronological order.

    std::string lastPassWrite;
};
struct egn::rg::_impl::RenderGraphImpl {
    vk::AllocationCreateInfo allocInfo;
    Registry *reg = nullptr;
    vk::QueueFamilies queueFamilies;

    std::unordered_set<Entity> resourcePool;
    std::unordered_map<std::string, NodeState> nodeState;
    std::unordered_map<std::string, RenderPass> passes;
    std::unordered_map<std::string, ResourceInfo> resources;
    std::unordered_map<std::string, std::string> aliasResources;
    std::vector<std::string> passStack;
};

struct egn::rg::RenderGraphImpl {
    std::unordered_map<std::string, RenderPass> passes;
    std::unordered_map<std::string, ResourceInfo> resources;
    std::vector<std::string> passStack;
    std::unordered_map<std::string, std::string> aliasResources;
    std::unordered_set<Entity> allocatedResources; // For easy access
    std::unordered_map<std::string, Entity> externalResources;
};
struct PleaseKeepTheImageContents { };

bool RenderGraphResourceBuilder::addExternalResource(std::string const &name, RenderGraphExternalResourceInfo info) {
    if(mExternalResources.contains(std::string(name))) {
        LOG_ERROR("External resource \"{}\" already exists!", name);
        return false;
    }
    mExternalResources[std::string(name)] = info;
    return true;
}
bool RenderGraphResourceBuilder::addImageResource(std::string const &name, RenderGraphImageCreateInfo info) {
    if(mImageResources.contains(std::string(name))) {
        LOG_ERROR("Image resource \"{}\" already exists!", name);
        return false;
    }
    mImageResources[std::string(name)] = info;
    return true;
}
bool RenderGraphResourceBuilder::addBufferResource(std::string const &name, RenderGraphBufferCreateInfo info) {
    if(mBufferResources.contains(std::string(name))) {
        LOG_ERROR("Buffer resource \"{}\" already exists!", name);
        return false;
    }
    mBufferResources[std::string(name)] = info;
    return true;
}
void RenderGraphResourceBuilder::addAliasResource(std::string const &name, std::string const &src) {
    if(mAliasResources.contains(src)) {
        addAliasResource(name, mAliasResources.at(src));
    } else {
        mAliasResources[name] = src;
    }
}
bool RenderGraphResourceBuilder::contains(std::string const &name) const {
    return mImageResources.contains(name) || mAliasResources.contains(name) || mExternalResources.contains(name) || mBufferResources.contains(name);
}

std::unordered_map<std::string, RenderGraphExternalResourceInfo> const &RenderGraphResourceBuilder::getExternalResources() const {
    return mExternalResources;
}
std::unordered_map<std::string, RenderGraphImageCreateInfo> const &RenderGraphResourceBuilder::getImageResources() const {
    return mImageResources;
}
std::unordered_map<std::string, RenderGraphBufferCreateInfo> const &RenderGraphResourceBuilder::getBufferResources() const {
    return mBufferResources;
}
std::unordered_map<std::string, std::string> const &RenderGraphResourceBuilder::getAliasResources() const {
    return mAliasResources;
}

RenderPassBuilder::RenderPassBuilder(RenderGraphResourceBuilder *resourceBuilder) {
    mResourceBuilder = resourceBuilder;
}
RenderGraphResourceBuilder *RenderPassBuilder::resource() {
    return mResourceBuilder;
}
void RenderPassBuilder::attachResourceRead(ResourceReadUsage const &resource) {
    mReads.emplace_back(resource);
}
void RenderPassBuilder::attachResourceWrite(ResourceUsage const &resource) {
    mWrites.emplace_back(resource);
}
void RenderPassBuilder::addExplicitDependency(std::string_view passName) {
    mExplicitDependencies.emplace_back(passName);
}

std::vector<ResourceReadUsage> const &RenderPassBuilder::getReads() const {
    return mReads;
}
std::vector<ResourceUsage> const &RenderPassBuilder::getWrites() const {
    return mWrites;
}
std::vector<std::string> const &RenderPassBuilder::getExplicitDependencies() const {
    return mExplicitDependencies;
}

void RenderGraphBuilder::setAllocInfo(vk::AllocationCreateInfo const &allocInfo, Registry &reg) {
    this->allocInfo = allocInfo;
    this->reg = &reg;
}
void RenderGraphBuilder::setQueueFamilies(vk::QueueFamilies const &queueFamilies) {
    this->queueFamilies = queueFamilies;
}
bool RenderGraphBuilder::addPass(Pass &&pass) {
    if(passes.contains(pass.name)) {
        LOG_ERROR("Pass \"{}\" already exists!", pass.name);
        return false;
    }

    passOrder.emplace_back(pass.name);
    passes[pass.name] = std::move(pass);
    return true;
}
bool RenderGraphBuilder::removePass(std::string const &name) {
    if(!passes.contains(std::string(name))) {
        LOG_ERROR("Cannot remove pass \"{}\": does not exist!");
        return false;
    }

    passOrder.erase(std::find(passOrder.begin(), passOrder.end(), name));
    passes.erase(std::string(name));
    return true;
}

RenderGraph::RenderGraph(RenderGraphImpl *data) {
    mImpl = data;
}
RenderGraph::~RenderGraph() {
    if(mImpl)
        delete mImpl;
}
RenderGraph::RenderGraph(RenderGraph &&rhs) {
    *this = std::move(rhs);
}
RenderGraph &RenderGraph::operator=(RenderGraph &&rhs) {
    std::swap(mImpl, rhs.mImpl);
    return *this;
}
bool RenderGraph::valid() const {
    return mImpl;
}
bool RenderGraph::setResource(std::string const &name, Entity eResource) {
    if(!valid()) {
        return false;
    }

    std::string realName = name;
    if(mImpl->aliasResources.contains(name)) {
        realName = mImpl->aliasResources.at(name);
    }
    if(!mImpl->resources.contains(realName)) {
        LOG_ERROR("Render graph does not contain the resource \"{}\"", realName);
        return false;
    }

    auto &res = mImpl->resources.at(realName);
    if(res.ci.type != ResourceCreateInfo::External) {
        LOG_ERROR("Cannot set resource \"{}\", because it is not external!");
        return false;
    }

    res.eResource = eResource;
    mImpl->externalResources[realName] = eResource;

    return true;
}
Entity RenderGraph::getResource(std::string const &name) const {
    assert(valid());

    std::string realName = name;
    if(mImpl->aliasResources.contains(name)) {
        realName = mImpl->aliasResources.at(name);
    }
    if(!mImpl->resources.contains(realName)) {
        LOG_ERROR("Render graph does not contain the resource \"{}\"", realName);
        return Entity();
    }

    return mImpl->resources.at(realName).eResource;
}
ResourceType RenderGraph::getResourceType(std::string const &name) const {
    assert(valid());

    if(mImpl->aliasResources.contains(name)) {
        return ResourceType::Alias;
    }

    if(!mImpl->resources.contains(std::string(name))) {
        LOG_ERROR("Render graph does not contain the resource \"{}\"", name);
        throw EngineException("Render graph does not contain the resource!");
    }

    switch(mImpl->resources.at(std::string(name)).ci.type) {
        case ResourceCreateInfo::External: return ResourceType::External; break;
        case ResourceCreateInfo::Image: return ResourceType::Image; break;
        case ResourceCreateInfo::Buffer: return ResourceType::Buffer; break;
        default: assert(false && "what"); break;
    }
}
std::vector<ResourceReadUsage> const &RenderGraph::getResourceRead(std::string const &name) const {
    assert(valid());
    if(!mImpl->resources.contains(std::string(name))) {
        LOG_ERROR("Render graph does not contain the resource \"{}\"", name);
        throw EngineException("");
    }

    return mImpl->resources.at(std::string(name)).read;
}
std::unordered_set<Entity> const &RenderGraph::getAllocatedResources() const {
    assert(valid());
    return mImpl->allocatedResources;
}
std::unordered_map<std::string, Entity> const &RenderGraph::getExternalResources() const {
    assert(valid());
    return mImpl->externalResources;
}
std::vector<ResourceUsage> const &RenderGraph::getResourceWrite(std::string const &name) const {
    assert(valid());
    if(!mImpl->resources.contains(std::string(name))) {
        LOG_ERROR("Render graph does not contain the resource \"{}\"", name);
        throw EngineException("Render graph does not contain the resource!");
    }

    return mImpl->resources.at(std::string(name)).written;
}
RenderPass const *RenderGraph::getPass(std::string const &name) const {
    assert(valid());

    if(!mImpl->passes.contains(std::string(name))) {
        LOG_ERROR("Render graph does not contain pass \"{}\"", name);
        return nullptr;
    }

    return &mImpl->passes.at(std::string(name));
}
std::vector<std::string> const &RenderGraph::getPassStack() const {
    assert(valid());

    return mImpl->passStack;
}

static std::string collapseAttributes(std::vector<std::string> const &attributes) {
    std::string res;
    for(auto const &attrib : attributes)
        res.append("[").append(attrib).append("]");
    return res;
}
std::string egn::rg::dumpGraphviz(RenderGraph const &graph, int indent, GraphvizSettings settings) {
    ZoneScoped;

    assert(graph.valid());

    std::stringstream ss;
    auto newline = [indent](int i) { return (indent >= 0) ? ("\n" + std::string(i, ' ')) : " "; };

    ss << "digraph RenderGraph {";
    for(auto const &attrib : settings.graphAttributes)
        ss << newline(indent) << attrib << ";";

    std::string nodeAttributes = collapseAttributes(settings.nodeAttributes);
    std::string edgeAttributes = collapseAttributes(settings.edgeAttributes);

    for(auto passName : graph.getPassStack()) {
        auto pass = graph.getPass(passName);
        passName.erase(std::remove(passName.begin(), passName.end(), '\"'), passName.end());

        ss << newline(indent) << "\"" << passName << "\" " << nodeAttributes << ";";

        for(auto dependent : pass->reads) {
            dependent.name.erase(std::remove(dependent.name.begin(), dependent.name.end(), '\"'), dependent.name.end());
            for(auto const &dependency : graph.getResourceWrite(dependent.name)) {
                ss << newline(indent) << fmt::format("\"{}\" -> \"{}\" [label=\"{}\"]{}", dependency.name, passName, dependent.name, edgeAttributes);
            }
        }
    }

    ss << newline(0) << "}";

    return ss.str();
}

bool egn::rg::_impl::setupRenderGraph(RenderGraphBuilder const &builder, void *data, RenderGraphImpl &renderGraph) {
    ZoneScoped;

    if(builder.allocInfo.has_value()) {
        renderGraph.reg = builder.reg;
        renderGraph.allocInfo = builder.allocInfo.value();
    }
    assert(builder.queueFamilies.has_value() && "Forgot to call RenderGraphBuilder::setQueueFamilies?");
    renderGraph.queueFamilies = builder.queueFamilies.value();

    RenderGraphResourceBuilder resourceBuilder = builder.mResources;
    struct ExplicitDependency {
        std::string dependent;
        std::string dependency;
    };
    std::vector<ExplicitDependency> dependencies;

    for(auto const &passName : builder.passOrder) {
        auto &pass = builder.passes.at(passName);
        if(renderGraph.passes.contains(passName)) {
            LOG_ERROR("Render graph already contains pass \"{}\"", passName);
            return false;
        }

        auto &renderGraphPass = renderGraph.passes[passName] = RenderPass{
            .name = pass.name,
            .queue = pass.queue,
            .storage = std::unique_ptr<IRenderPassStorage>(pass.storage->clone()),
        };

        RG_LOG_TRACE("Adding pass \"{}\" queue {}", renderGraphPass.name, string_VkQueueFlagBits(renderGraphPass.queue));

        RenderPassBuilder passBuilder(&resourceBuilder);
        renderGraphPass.storage->setup(passBuilder, data);
        for(auto const &[name, traits] : passBuilder.getWrites()) {
            RG_LOG_TRACE("  Writes {} {}", name, string_VkAccessFlags2(traits.access));
            auto &resourceInfo = renderGraph.resources[name];

            resourceInfo.name = name;
            resourceInfo.written.emplace_back(renderGraphPass.name, traits);

            renderGraphPass.writes.emplace_back(name, traits);
        }
        for(auto const &info : passBuilder.getReads()) {
            RG_LOG_TRACE("  Reads {} {}", info.name, string_VkAccessFlags2(info.traits.access));
            auto &resourceInfo = renderGraph.resources[info.name];
            auto it = std::find_if(resourceInfo.written.begin(), resourceInfo.written.end(), [&](auto const &e) {
                return e.name == renderGraphPass.name;
            });
            if(it != resourceInfo.written.end() && it->traits.valid() && info.traits.valid()) {
                LOG_ERROR("Cannot read and write to the same resource \"{}\". Use different names!(error in pass \"{}\")",
                    info.name,
                    renderGraphPass.name);
                return false;
            }

            resourceInfo.name = info.name;
            resourceInfo.read.emplace_back(info.name, info.traits, renderGraphPass.name);
            renderGraphPass.reads.emplace_back(info);
        }
        for(auto const &dependencyName : passBuilder.getExplicitDependencies()) {
            RG_LOG_TRACE("  Explicitly depends on \"{}\"", dependencyName);
            renderGraphPass.explicitPassDependencies.emplace_back(dependencyName);
            dependencies.emplace_back(passName, dependencyName);
        }
    }

    RG_LOG_TRACE("Resources:");

    for(auto const &[name, info] : resourceBuilder.getExternalResources()) {
        RG_LOG_TRACE("  External resource {} {}", name, info.eResource);
        if(renderGraph.resources.contains(name) && renderGraph.resources.at(name).ci.type != ResourceCreateInfo::Unknown) {
            LOG_ERROR("Resoure \"{}\" of different type already exists", name);
            return false;
        }

        auto &resourceInfo = renderGraph.resources[name];
        resourceInfo.name = name;
        resourceInfo.eResource = info.eResource;
        resourceInfo.ci = ResourceCreateInfo(info);
    }
    for(auto const &[name, info] : resourceBuilder.getImageResources()) {
        RG_LOG_TRACE("  Image resource {} {} {} {}x{}x{} resize to target {} -> {}x{} allow reuse {}",
            name,
            string_VkFormat(info.imageInfo.format),
            string_VkImageType(info.imageInfo.imageType),
            info.imageInfo.dimensions.width,
            info.imageInfo.dimensions.height,
            info.imageInfo.dimensions.depth,
            info.resizeToTarget,
            info.resizeRatio.x,
            info.resizeRatio.y,
            !info.keep);

        if(renderGraph.resources.contains(name) && renderGraph.resources.at(name).ci.type != ResourceCreateInfo::Unknown) {
            LOG_ERROR("Resoure \"{}\" of different type already exists", name);
            return false;
        }

        auto &resourceInfo = renderGraph.resources[name];
        resourceInfo.name = name;
        resourceInfo.ci = info;
    }
    for(auto const &[name, info] : resourceBuilder.getBufferResources()) {
        RG_LOG_TRACE("  Buffer resource {} {} bytes", name, info.size);
        if(renderGraph.resources.contains(name) && renderGraph.resources.at(name).ci.type != ResourceCreateInfo::Unknown) {
            LOG_ERROR("Resoure \"{}\" of different type already exists", name);
            return false;
        }

        auto &resourceInfo = renderGraph.resources[name];
        resourceInfo.name = name;
        resourceInfo.ci = info;
    }

    renderGraph.aliasResources = resourceBuilder.getAliasResources();

    // Convert alias resources to their targets
    for(auto const &[name, target] : renderGraph.aliasResources) {
        RG_LOG_TRACE("  Alias resource \"{}\" -> \"{}\"", name, target);
        if(renderGraph.resources.contains(name) && renderGraph.resources.at(name).ci.type != ResourceCreateInfo::Unknown) {
            LOG_ERROR("Resoure \"{}\" of different type already exists", name);
            return false;
        }
        if(!renderGraph.resources.contains(target)) {
            LOG_ERROR("Resource \"{}\" aliases non-existing resource \"{}\"!", name, target);
            return false;
        }

        if(renderGraph.aliasResources.contains(target)) {
            LOG_ERROR("Resource \"{}\" aliases resource \"{}\", which is an alias itself!", name, target);
            return false;
        }

        // The alias is unused.
        if(!renderGraph.resources.contains(name)) {
            continue;
        }

        auto const &srcResource = renderGraph.resources.at(name);
        auto &targetResource = renderGraph.resources.at(target);
        targetResource.read.append_range(srcResource.read);
        targetResource.written.append_range(srcResource.written);

        for(auto const &info : srcResource.read) {
            auto &pass = renderGraph.passes.at(info.name);
            auto it = std::find_if(pass.reads.begin(), pass.reads.end(), [&](auto const &e) { return e.name == name; });
            assert(it != pass.reads.end());
            it->name = target;
        }
        for(auto const &[passName, _] : srcResource.written) {
            auto &pass = renderGraph.passes.at(passName);
            auto it = std::find_if(pass.writes.begin(), pass.writes.end(), [&](auto const &e) { return e.name == name; });
            assert(it != pass.writes.end());
            it->name = target;
        }
    }
    for(auto const &[name, _] : renderGraph.aliasResources) {
        renderGraph.resources.erase(name);
    }

    for(auto const &[dependent, dependency] : dependencies) {
        if(!renderGraph.passes.contains(dependency)) {
            LOG_ERROR("Pass \"{}\" explicitly depends on non-existing pass \"{}\"", dependent, dependency);
            return false;
        }
    }
    for(auto const &[_, pass] : renderGraph.passes) {
        for(auto const &[res, _] : pass.writes) {
            if(!(renderGraph.resources.contains(res) && renderGraph.resources.at(res).ci.type != ResourceCreateInfo::Unknown)) {
                LOG_ERROR("Pass \"{}\" writes to unknown resource \"{}\"", pass.name, res);
                return false;
            }
        }
        for(auto const &info : pass.reads) {
            if(!(renderGraph.resources.contains(info.name) && renderGraph.resources.at(info.name).ci.type != ResourceCreateInfo::Unknown)) {
                LOG_ERROR("Pass \"{}\" reads from unknown resource \"{}\"", pass.name, info.name);
                return false;
            }
        }
        for(auto const &dependency : pass.explicitPassDependencies) {
            if(!renderGraph.passes.contains(dependency)) {
                LOG_ERROR("Pass \"{}\" explicitly depends on unknown pass \"{}\"", pass.name, dependency);
                return false;
            }
            renderGraph.passes.at(dependency).explicitPassDependents.emplace_back(pass.name);
        }
    }

    return true;
}

// Simplified version of vk::RenderGraph's processPass
static bool processPass(std::string const &passName, _impl::RenderGraphImpl &renderGraph, uint depth, bool backtrack = false) {
    ZoneScoped;

    assert(!passName.empty());

    if(depth == 0) {
        LOG_ERROR("Max depth exceeded!");
        return false;
    }

    auto &nodeState = renderGraph.nodeState[passName];
    if(nodeState != NodeState::None)
        return true;

    auto &pass = renderGraph.passes.at(passName);

    // Cull passes results of which are not used anywhere,
    // Unless the resource is external
    bool used = false;
    used = !pass.explicitPassDependents.empty();
    if(!used) {
        for(auto const &resource : pass.writes) {
            auto const &info = renderGraph.resources.at(resource.name);

            if(info.ci.type == ResourceCreateInfo::External ||
                true){//std::find_if(info.read.begin(), info.read.end(), [&](auto const &e) { return e.pass.empty() || e.pass == pass.name; }) != info.read.end()) {
                used = true;
                break;
            }
        }
    }
    if(!used) {
        RG_LOG_TRACE("  Pass \"{}\" is unused!", pass.name);
        return true;
    }

    RG_LOG_TRACE("Processing pass \"{}\"", pass.name);

    // Process passes that still read from the old versions of resources this pass writes to.
    for(auto const &resource : pass.writes) {
        auto &info = renderGraph.resources.at(resource.name);
        for(auto const &dependency : info.read) {
            if(dependency.history || dependency.pass == info.lastPassWrite) {
                if(!processPass(dependency.name, renderGraph, depth - 1, true)) {
                    return false;
                }
            }
        }
    }

    // Process dependencies.
    for(auto const &resource : pass.reads) {
        if(!resource.pass.empty()) {
            if(!processPass(resource.pass, renderGraph, depth - 1, true)) {
                return false;
            }
        }
    }

    // Process explicit dependencies.
    for(auto const &passName : pass.explicitPassDependencies) {
        if(!processPass(passName, renderGraph, depth - 1, true)) {
            return false;
        }
    }

    if(nodeState != NodeState::Added) {
        nodeState = NodeState::Added;
        pass.index = renderGraph.passStack.size();
        renderGraph.passStack.emplace_back(pass.name);
        RG_LOG_TRACE("  Adding pass.");
        if(backtrack) {
            RG_LOG_TRACE("  Backtracking.");
            return true;
        }
    }

    for(auto const &pass : pass.explicitPassDependents) {
        if(!processPass(pass, renderGraph, depth - 1, backtrack)) {
            return false;
        }
    }
    for(auto const &resource : pass.writes) {
        auto &resourceInfo = renderGraph.resources.at(resource.name);
        for(auto const &info : resourceInfo.read) {
            if(info.pass.empty()) {
                for(auto const &dependency : resourceInfo.written) {
                    if(dependency.name != pass.name) {
                        if(!processPass(dependency.name, renderGraph, depth - 1, backtrack)) {
                            return false;
                        }
                    }
                }
            }
            if(info.pass == pass.name || info.pass.empty()) {
                if(!processPass(info.name, renderGraph, depth - 1, backtrack)) {
                    return false;
                }
            }
        }
        resourceInfo.lastPassWrite = pass.name;
    }

    return true;
}
static void calculateLifetimes(_impl::RenderGraphImpl &renderGraph) {
    ZoneScoped;

    for(auto &[_, resource] : renderGraph.resources) {
        for(auto const &info : resource.read) {
            auto const &pass = renderGraph.passes.at(info.name);
            resource.lifetimeEnd = std::max(resource.lifetimeEnd, pass.index);
        }
        for(auto const &info : resource.written) {
            auto const &pass = renderGraph.passes.at(info.name);
            resource.lifetimeBegin = std::min(resource.lifetimeBegin, pass.index);
        }
        if(resource.written.empty()) {
            resource.lifetimeBegin = 0;
        }

        RG_LOG_TRACE("Resource \"{}\" \n  begin {} -> \"{}\" \n  end {} -> \"{}\"",
            resource.name,
            resource.lifetimeBegin,
            renderGraph.passStack[resource.lifetimeBegin],
            resource.lifetimeEnd,
            renderGraph.passStack[resource.lifetimeEnd]);
    }
}
static bool isCompatible(RenderGraphImageCreateInfo const &first, RenderGraphImageCreateInfo const &second) {
    return // oh god here we go again
        first.resizeToTarget == second.resizeToTarget && vecEquals(first.resizeRatio, second.resizeRatio) &&
        first.imageInfo.imageType == second.imageInfo.imageType && first.imageInfo.format == second.imageInfo.format &&
        (first.resizeToTarget || (first.imageInfo.dimensions.width == second.imageInfo.dimensions.width &&
                                     first.imageInfo.dimensions.height == second.imageInfo.dimensions.height &&
                                     first.imageInfo.dimensions.depth == second.imageInfo.dimensions.depth &&
                                     first.imageInfo.dimensions.mipLevels == second.imageInfo.dimensions.mipLevels &&
                                     first.imageInfo.dimensions.arrayLayers == second.imageInfo.dimensions.arrayLayers &&
                                     first.imageInfo.dimensions.samples == second.imageInfo.dimensions.samples)) &&
        (first.imageInfo.sampler.flags == second.imageInfo.sampler.flags) &&
        first.imageInfo.sampler.magFilter == second.imageInfo.sampler.magFilter &&
        first.imageInfo.sampler.minFilter == second.imageInfo.sampler.minFilter &&
        first.imageInfo.sampler.mipmapMode == second.imageInfo.sampler.mipmapMode && // still... going...
        first.imageInfo.sampler.addressModeU == second.imageInfo.sampler.addressModeU &&
        first.imageInfo.sampler.addressModeV == second.imageInfo.sampler.addressModeV &&
        first.imageInfo.sampler.addressModeW == second.imageInfo.sampler.addressModeW &&
        std::abs(first.imageInfo.sampler.mipLodBias - second.imageInfo.sampler.mipLodBias) <= 1e-6 &&
        first.imageInfo.sampler.anisotropyEnable == second.imageInfo.sampler.anisotropyEnable &&
        std::abs(first.imageInfo.sampler.maxAnisotropy - second.imageInfo.sampler.maxAnisotropy) <= 1e-6 &&
        first.imageInfo.sampler.compareEnable == second.imageInfo.sampler.compareEnable &&
        first.imageInfo.sampler.compareOp == second.imageInfo.sampler.compareOp &&
        std::abs(first.imageInfo.sampler.minLod - second.imageInfo.sampler.minLod) <= 1e-6 &&
        first.imageInfo.sampler.borderColor == second.imageInfo.sampler.borderColor &&
        std::abs(first.imageInfo.sampler.customBorderColor.r - second.imageInfo.sampler.customBorderColor.r) <= 1e-6 &&
        std::abs(first.imageInfo.sampler.customBorderColor.g - second.imageInfo.sampler.customBorderColor.g) <= 1e-6 &&
        std::abs(first.imageInfo.sampler.customBorderColor.b - second.imageInfo.sampler.customBorderColor.b) <= 1e-6 &&
        std::abs(first.imageInfo.sampler.customBorderColor.a - second.imageInfo.sampler.customBorderColor.a) <= 1e-6 &&
        first.imageInfo.sampler.unnormalizedCoordinates == second.imageInfo.sampler.unnormalizedCoordinates;
} // let's never do THAT again...
// Additionally checks the type of an external resource
static ResourceCreateInfo::Type getResourceType(RestrictedEntity<ResourceAllocationInfo> e) {
    ZoneScoped;

    auto const &info = *e;
    if(info.ci.type == ResourceCreateInfo::External) {
        assert(e.contains<vk::Image>() || e.contains<vk::Buffer>());
        return e.contains<vk::Image>() ? ResourceCreateInfo::Image : ResourceCreateInfo::Buffer;
    } else {
        return info.ci.type;
    }
}
static bool isResourceCompatible(Entity e, ResourceCreateInfo const &ci, VkImageUsageFlags usage) {
    assert(e);
    assert(e.contains<ResourceCreateInfo>());
    auto type = getResourceType(e);

    if(type != ci.type)
        return false;

    // External resources must mach the usage
    if(e.contains<vk::Image>() && (e.get<vk::Image>().createInfo.usage & usage) != usage) {
        return false;
    }

    auto const &poolCi = e.get<ResourceCreateInfo>();

    if(ci.type == ResourceCreateInfo::Image && !isCompatible(ci.image, poolCi.image)) {
        return false;
    }

    return true;
}
static Entity findPoolResource(ResourceCreateInfo const &ci, _impl::RenderGraphImpl &renderGraph, VkImageUsageFlags usage = 0) {
    ZoneScoped;

    for(auto poolRes : renderGraph.resourcePool) {
        if(isResourceCompatible(poolRes, ci, usage)) {
            renderGraph.resourcePool.erase(poolRes);
            return poolRes;
        }
    }
    if(renderGraph.reg) {
        for(auto freeRes : renderGraph.reg->view<RenderGraphUnusedResource>()) {
            if(isResourceCompatible(freeRes, ci, usage)) {
                return freeRes;
            }
        }
    }

    return Entity();
}
static void aliasResources(_impl::RenderGraphImpl &renderGraph) {
    ZoneScoped;

    RG_LOG_TRACE("Aliasing resources");
    for(uint i = 0; i < renderGraph.passStack.size(); ++i) {
        auto const &pass = renderGraph.passes.at(renderGraph.passStack.at(i));

        RG_LOG_TRACE("  Processing pass {}", pass.name);

        // Allocate
        for(auto const &[resourceName, traits] : pass.writes) {
            auto &resource = renderGraph.resources.at(resourceName);
            auto &e = resource.eResource;

            // TODO: external resources are not used before their lifetimes
            if(resource.ci.type == ResourceCreateInfo::External)
                continue;

            assert(!e.valid());
            e = findPoolResource(resource.ci, renderGraph);

            if(!e.valid()) {
                assert(renderGraph.reg && "Should have called RenderGraphBuilder::setAllocInfo!");
                e = renderGraph.reg->create(resource.ci);
                e.name(resourceName);
                if(resource.ci.type == ResourceCreateInfo::Image && resource.ci.image.keep) {
                    e.emplace<PleaseKeepTheImageContents>();
                }
                RG_LOG_TRACE("    Adding new pool resource {} for {} {}", e, resourceName, string_VkAccessFlagBits2(traits.access));
            } else {
                RG_LOG_TRACE("    Found pool resource {} for resource {} {}", e, resourceName, string_VkAccessFlagBits2(traits.access));
                e.name(std::string(e.name()) + "," + resourceName);
            }

            e.try_emplace<ResourceAllocationInfo>(ResourceAllocationInfo{ .ci = resource.ci });
            e.try_emplace<PlzAllocate>();

            auto &allocInfo = e.get<ResourceAllocationInfo>();
            allocInfo.imageUsage |= traits.imageTraits.usage;
            allocInfo.bufferUsage |= traits.bufferTraits.usage;
            allocInfo.bufferSize = std::max<VkDeviceSize>(allocInfo.bufferSize, traits.bufferTraits.size + traits.bufferTraits.offset);
        }

        // Free
        for(auto const &info : pass.reads) {
            auto &resource = renderGraph.resources.at(info.name);
            // Dummy resources may not be recycled
            if(!resource.eResource)
                continue;
            auto const &ci = resource.ci;

            if(resource.eResource.contains<ResourceAllocationInfo>()) {
                auto &allocInfo = resource.eResource.get<ResourceAllocationInfo>();
                if(ci.type == ResourceCreateInfo::Image) {
                    allocInfo.imageUsage |= info.traits.imageTraits.usage;
                } else {
                    allocInfo.bufferUsage |= info.traits.bufferTraits.usage;
                }
            }

            // May not use resources if the contents are used in the next frame
            if(ci.type == ResourceCreateInfo::Image && ci.image.keep)
                continue;
            if(ci.type == ResourceCreateInfo::External && ci.external.keep)
                continue;

            if(resource.lifetimeEnd <= i) {
                RG_LOG_TRACE("    Freeing resource {} {}", resource.name, resource.eResource);
                renderGraph.resourcePool.emplace(resource.eResource);
            }
        }
    }
}
static void allocateResources(_impl::RenderGraphImpl &renderGraph) {
    ZoneScoped;

    RG_LOG_TRACE("Allocating resources");
    if(!renderGraph.reg)
        return;
    for(auto e : renderGraph.reg->view<ResourceAllocationInfo, PlzAllocate>()) {
        auto const &allocInfo = e.get<ResourceAllocationInfo>();
        e.emplace<RenderGraphResource>();
        e.try_erase<RenderGraphUnusedResource>();
        e.erase<PlzAllocate>();

        assert(allocInfo.ci.type == ResourceCreateInfo::Image || allocInfo.ci.type == ResourceCreateInfo::Buffer);

        if(allocInfo.ci.type == ResourceCreateInfo::Image) {
            RG_LOG_TRACE("  Allocating image resource {} for {}", static_cast<Entity &>(e), string_VkImageUsageFlags(allocInfo.imageUsage));
            assert(allocInfo.imageUsage != 0);
            vk::ImageCreateInfo imageCreateInfo{
                .usage = allocInfo.imageUsage,
                .allocInfo = renderGraph.allocInfo,
                .image = allocInfo.ci.image.imageInfo,
            };
            imageCreateInfo.name = e.name();

            if(allocInfo.ci.image.resizeToTarget) {
                e.emplace<ResizeToTarget>(allocInfo.ci.image.resizeRatio);
                e.emplace<vk::ImageCreateInfo>(imageCreateInfo);
            } else { // Deffer the image creation.
                e.emplace<vk::Image>(vk::makeImage(imageCreateInfo));
                assert(e.get<vk::Image>().image);
                assert(e.get<vk::Image>().view);
            }
        } else {
            RG_LOG_TRACE("  Allocating buffer resource {} for {} {} bytes",
                static_cast<Entity &>(e),
                string_VkBufferUsageFlags(allocInfo.bufferUsage),
                allocInfo.bufferSize);
            assert(allocInfo.bufferUsage != 0);
            assert(allocInfo.bufferSize != 0);
            vk::BufferCreateInfo bufferCreateInfo{
                .usage = allocInfo.bufferUsage,
                .allocInfo = renderGraph.allocInfo,
                .size = allocInfo.bufferSize,
                .map = false,
            };
            bufferCreateInfo.name = e.name();

            e.emplace<vk::Buffer>(vk::makeBuffer(bufferCreateInfo));
        }
    }
}
static bool buildBarriers(_impl::RenderGraphImpl &renderGraph) {
    ZoneScoped;

    std::unordered_map<Entity, Barrier::Scope> lastScope;
    for(auto const &passName : renderGraph.passStack) {
        auto const &pass = renderGraph.passes.at(passName);
        uint32_t queue = VK_QUEUE_FAMILY_IGNORED;
        if(!renderGraph.queueFamilies.indices.contains(pass.queue)) {
            LOG_ERROR("Pass \"{}\" needs queue {} which is not in queue families provided to the frame graph!",
                pass.name,
                string_VkQueueFlagBits(pass.queue));
            return false;
        } else {
            queue = renderGraph.queueFamilies.indices.at(pass.queue);
        }
        for(auto const &info : pass.reads) {
            if(!info.traits.valid()) {
                continue;
            }

            auto const &resource = renderGraph.resources.at(info.name);

            lastScope[resource.eResource] = {
                .layout = info.traits.imageTraits.layout,
                .bufferUsage = info.traits.bufferTraits.usage,
                .imageUsage = info.traits.imageTraits.usage,
                .queueIndex = queue,
                .access = info.traits.access,
                .stages = info.traits.stages,
            };
        }
    }

    std::unordered_map<Entity, Barrier::Scope> resourceState;
    for(auto const &passName : renderGraph.passStack) {
        auto &pass = renderGraph.passes.at(passName);
        pass.barriers.reserve(pass.reads.size() + pass.writes.size());
        uint32_t queue = renderGraph.queueFamilies.indices.at(pass.queue);

        for(auto const &info : pass.reads) {
            if(!info.traits.valid()) {
                RG_LOG_TRACE("  Ignoring \"{}\"->\"{}\" barrier -> {}.", info.name, info.pass, string_VkAccessFlags2(info.traits.access));
                continue;
            }
            auto const &resource = renderGraph.resources.at(info.name);
            bool history = resource.eResource.contains<PleaseKeepTheImageContents>() && !resourceState.contains(resource.eResource);
            auto &state = resourceState[resource.eResource];
            auto prevState = history ? lastScope.at(resource.eResource) : state;

            state = {
                .layout = info.traits.imageTraits.layout,
                .bufferUsage = info.traits.bufferTraits.usage,
                .imageUsage = info.traits.imageTraits.usage,
                .queueIndex = queue,
                .access = info.traits.access,
                .stages = info.traits.stages,
            };
            pass.barriers.emplace_back(Barrier{
                .resource = resource.name,
                .src = prevState,
                .dst = state,
                .subresourceRange = info.traits.imageTraits.subresourceRange,
                .offset = info.traits.bufferTraits.offset,
                .size = info.traits.bufferTraits.size,
            });
        }
        for(auto const &[resourceName, traits] : pass.writes) {
            if(!traits.valid()) {
                RG_LOG_TRACE("  Ignoring \"{}\" barrier -> {}.", resourceName, string_VkAccessFlags2(traits.access));
                continue;
            }
            auto const &resource = renderGraph.resources.at(resourceName);
            auto &state = resourceState[resource.eResource];
            auto prevState = state;

            state = {
                .layout = traits.imageTraits.layout,
                .bufferUsage = traits.bufferTraits.usage,
                .imageUsage = traits.imageTraits.usage,
                .queueIndex = queue,
                .access = traits.access,
                .stages = traits.stages,
            };
            pass.barriers.emplace_back(Barrier{
                .resource = resource.name,
                .src = prevState,
                .dst = state,
                .subresourceRange = traits.imageTraits.subresourceRange,
                .offset = traits.bufferTraits.offset,
                .size = traits.bufferTraits.size,
            });
        }
    }

    return true;
}

RenderGraph egn::rg::buildRenderGraph(RenderGraphBuilder const &builder, void *data) {
    ZoneScoped;

    _impl::RenderGraphImpl renderGraph;

    if(!setupRenderGraph(std::move(builder), data, renderGraph)) {
        LOG_ERROR("Failed to set up render graph!");
        return RenderGraph(nullptr);
    }

    for(auto const &[_, pass] : renderGraph.passes) {
        if(pass.reads.empty()) {
            if(!processPass(pass.name, renderGraph, renderGraph.passes.size() * 4)) {
                LOG_ERROR("Failed to order render graph!");
                return RenderGraph(nullptr);
            }
        }
    }

#if RENDER_GRAPH_LOG
    RG_LOG_TRACE("Pass stack:");
    for(uint32_t i = 0; i < renderGraph.passStack.size(); ++i) {
        auto passName = renderGraph.passStack[i];
        auto const &pass = renderGraph.passes.at(passName);
        RG_LOG_TRACE("{:>2}) {}:", i, passName);
        std::string reads;
        for(auto const &info : pass.reads)
            reads.append(info.name).append("; ");
        if(reads.empty())
            reads = "none";
        RG_LOG_TRACE("  Reads: {}", reads);

        std::string writes;
        for(auto const &[name, traits] : pass.writes)
            writes.append(name).append("; ");
        if(writes.empty())
            writes = "none";
        RG_LOG_TRACE("  Writes: {}", writes);
    }
#endif

    if(renderGraph.passStack.empty()) {
        LOG_ERROR("Pass stack empty!");
    }

    for(auto &[_, resource] : renderGraph.resources) {
        std::sort(resource.written.begin(), resource.written.end(), [&](auto const &lhs, auto const &rhs) {
            return renderGraph.passes.at(lhs.name).index < renderGraph.passes.at(rhs.name).index;
        });
    }

    calculateLifetimes(renderGraph);
    aliasResources(renderGraph);
    allocateResources(renderGraph);

    if(!buildBarriers(renderGraph)) {
        LOG_ERROR("Failed to build barriers!");
        return RenderGraph(nullptr);
    }

    RenderGraphImpl *res = new RenderGraphImpl{
        .resources = std::move(renderGraph.resources),
        .passStack = std::move(renderGraph.passStack),
        .aliasResources = std::move(renderGraph.aliasResources),
    };

    for(auto &&[name, pass] : renderGraph.passes) {
        res->passes[name] = std::move(pass);
    }
    for(auto &&[name, resource] : res->resources) {
        if(!resource.eResource.valid()) {
            continue;
        }
        if(resource.ci.type == ResourceCreateInfo::External) {
            res->externalResources[name] = resource.eResource;
        } else {
            res->allocatedResources.emplace(resource.eResource);
        }
    }

    return RenderGraph(res);
}
