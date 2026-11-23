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

RenderGraphResourceCreateInfo::RenderGraphResourceCreateInfo() {
}
RenderGraphResourceCreateInfo::RenderGraphResourceCreateInfo(RenderGraphResourceCreateInfo const &o) {
    *this = o;
}
RenderGraphResourceCreateInfo::RenderGraphResourceCreateInfo(RenderGraphResourceCreateInfo &&o) {
    *this = std::move(o);
}
RenderGraphResourceCreateInfo &RenderGraphResourceCreateInfo::operator=(RenderGraphResourceCreateInfo const &o) {
    type = o.type;
    switch(type) {
        case External: external = o.external; break;
        case Image: image = o.image; break;
        case Buffer: buffer = o.buffer; break;
        case None: break;
    }
    return *this;
}
RenderGraphResourceCreateInfo &RenderGraphResourceCreateInfo::operator=(RenderGraphResourceCreateInfo &&o) {
    type = std::move(o.type);
    switch(type) {
        case External: external = std::move(o.external); break;
        case Image: image = std::move(o.image); break;
        case Buffer: buffer = std::move(o.buffer); break;
        case None: break;
    }
    return *this;
}
RenderGraphResourceCreateInfo::~RenderGraphResourceCreateInfo() {
}
RenderGraphResourceCreateInfo::RenderGraphResourceCreateInfo(RenderGraphExternalResourceInfo const &info) {
    type = External;
    external = info;
}
RenderGraphResourceCreateInfo::RenderGraphResourceCreateInfo(RenderGraphImageCreateInfo const &info) {
    type = Image;
    image = info;
}
RenderGraphResourceCreateInfo::RenderGraphResourceCreateInfo(RenderGraphBufferCreateInfo const &info) {
    type = Buffer;
    buffer = info;
}

struct ResourceAllocationInfo {
    RenderGraphResourceCreateInfo ci;
    VkImageUsageFlags imageUsage = 0;
    VkBufferUsageFlags bufferUsage = 0;
    VkDeviceSize bufferSize = 0;
};
struct PlzAllocate { };
struct egn::rg::_impl::RenderGraphImpl {
    vk::AllocationCreateInfo allocInfo;
    Registry *reg = nullptr;
    vk::QueueFamilies queueFamilies;

    std::unordered_set<Entity> resourcePool;
    std::unordered_map<std::string, NodeState> nodeState;
    std::unordered_map<std::string, RenderPass> passes;
    std::unordered_map<std::string, ResourceInfo> resources;
    std::unordered_map<std::string, AliasResourceInfo> aliases;
    std::vector<std::string> passStack;

    inline bool isAlias(std::string const &name) const {
        return aliases.contains(name);
    }
    inline ResourceInfo &getRes(std::string const &name) {
        return resources.at(name);
    }
    inline AliasResourceInfo &getAlias(std::string const &name) {
        return aliases.at(name);
    }
};

struct egn::rg::RenderGraphImpl {
    std::unordered_map<std::string, RenderPass> passes;
    std::unordered_map<std::string, ResourceInfo> resources;
    std::unordered_map<std::string, AliasResourceInfo> aliases;
    std::vector<std::string> passStack;
};

bool RenderGraphResourceBuilder::addResource(std::string const &name, RenderGraphResourceCreateInfo info) {
    if(info.type == RenderGraphResourceCreateInfo::None) {
        LOG_ERROR("Invalid resource type ResourceCreateInfo::None!");
        return false;
    }
    if(mResources.contains(std::string(name))) {
        LOG_ERROR("External resource \"{}\" already exists!", name);
        return false;
    }
    mResources[std::string(name)] = info;
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
    return mResources.contains(name) || mAliasResources.contains(name);
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
    if(mImpl->aliases.contains(name)) {
        realName = mImpl->aliases.at(name).realTarget;
    }
    if(!mImpl->resources.contains(realName)) {
        LOG_ERROR("Render graph does not contain the resource \"{}\"", realName);
        return false;
    }

    auto &res = mImpl->resources.at(realName);
    if(res.ci.type != RenderGraphResourceCreateInfo::External) {
        LOG_ERROR("Cannot set resource \"{}\", because it is not external!");
        return false;
    }

    res.eResource = eResource;

    return true;
}
Entity RenderGraph::getResource(std::string const &name) const {
    assert(valid());

    std::string realName = name;
    if(mImpl->aliases.contains(name)) {
        realName = mImpl->aliases.at(name).realTarget;
    }
    if(!mImpl->resources.contains(realName)) {
        LOG_ERROR("Render graph does not contain the resource \"{}\"", realName);
        return Entity();
    }

    return mImpl->resources.at(realName).eResource;
}
RenderGraph::ResourceType RenderGraph::getResourceType(std::string const &name) const {
    assert(valid());

    if(mImpl->aliases.contains(name)) {
        return ResourceType::Alias;
    }

    if(!mImpl->resources.contains(std::string(name))) {
        LOG_ERROR("Render graph does not contain the resource \"{}\"", name);
        throw EngineException("Render graph does not contain the resource!");
    }

    switch(mImpl->resources.at(std::string(name)).ci.type) {
        case RenderGraphResourceCreateInfo::External: return ResourceType::External; break;
        case RenderGraphResourceCreateInfo::Image: return ResourceType::Image; break;
        case RenderGraphResourceCreateInfo::Buffer: return ResourceType::Buffer; break;
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
ResourceUsage const &RenderGraph::getResourceWrite(std::string const &name) const {
    assert(valid());
    if(!mImpl->resources.contains(std::string(name))) {
        LOG_ERROR("Render graph does not contain the resource \"{}\"", name);
        throw EngineException("Render graph does not contain the resource!");
    }

    return mImpl->resources.at(std::string(name)).written;
}
std::unordered_map<std::string, ResourceInfo> const &RenderGraph::getResources() const {
    return mImpl->resources;
}
std::unordered_map<std::string, AliasResourceInfo> const &RenderGraph::getAliasResources() const {
    return mImpl->aliases;
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

        for(auto info : pass->reads) {
            info.name.erase(std::remove(info.name.begin(), info.name.end(), '\"'), info.name.end());
            ss << newline(indent)
               << fmt::format("\"{}\" -> \"{}\" [label=\"{}\"]{}", graph.getResourceWrite(info.name).name, passName, info.name, edgeAttributes);
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

        RG_LOG_TRACE("Adding pass {} queue {}", renderGraphPass.name, string_VkQueueFlagBits(renderGraphPass.queue));

        RenderPassBuilder passBuilder(&resourceBuilder);
        renderGraphPass.storage->setup(passBuilder, data);
        for(auto const &[name, traits] : passBuilder.getWrites()) {
            RG_LOG_TRACE("  Writes {} {}", name, string_VkAccessFlags2(traits.access));
            auto &resourceInfo = renderGraph.resources[name];

            if(!resourceInfo.written.name.empty()) {
                LOG_ERROR("Cannot write to resource \"{}\" more than once!(error in pass \"{}\")", name, renderGraphPass.name);
                return false;
            }
            resourceInfo.name = name;
            resourceInfo.written = { renderGraphPass.name, traits };

            renderGraphPass.writes.emplace_back(name, traits);
        }
        for(auto const &info : passBuilder.getReads()) {
            RG_LOG_TRACE("  Reads {} {}", info.name, string_VkAccessFlags2(info.traits.access));
            auto &resourceInfo = renderGraph.resources[info.name];
            if(resourceInfo.written.name == renderGraphPass.name) {
                LOG_ERROR("Cannot read and write to the same resource \"{}\". Use different names!(error in pass \"{}\")",
                    info.name,
                    renderGraphPass.name);
                return false;
            }

            resourceInfo.name = info.name;
            resourceInfo.read.emplace_back(renderGraphPass.name, info.traits);

            renderGraphPass.reads.emplace_back(info.name, info.traits);
        }
        for(auto const &dependencyName : passBuilder.getExplicitDependencies()) {
            RG_LOG_TRACE("  Explicitly depends on \"{}\"", dependencyName);
            renderGraphPass.explicitPassDependencies.emplace_back(dependencyName);
            renderGraph.passes[dependencyName].explicitPassDependents.emplace_back(renderGraphPass.name);
            dependencies.emplace_back(passName, dependencyName);
        }
    }

    RG_LOG_TRACE("Resources:");

    for(auto const &[name, info] : resourceBuilder.getResources()) {
#if RENDER_GRAPH_LOG
        if(info.type == RenderGraphResourceCreateInfo::External) {
            RG_LOG_TRACE("  External resource {} {}", name, info.external.eResource);
        } else if(info.type == RenderGraphResourceCreateInfo::Image) {
            RG_LOG_TRACE("  Image resource {} {} {} {}x{}x{} resize to target {} -> {}x{}",
                name,
                string_VkFormat(info.image.imageInfo.format),
                string_VkImageType(info.image.imageInfo.imageType),
                info.image.imageInfo.dimensions.width,
                info.image.imageInfo.dimensions.height,
                info.image.imageInfo.dimensions.depth,
                info.image.resizeToTarget,
                info.image.resizeRatio.x,
                info.image.resizeRatio.y);
        } else if(info.type == RenderGraphResourceCreateInfo::Buffer) {
            RG_LOG_TRACE("  Buffer resource {} {} bytes", name, info.buffer.size);
        }
#endif

        if(renderGraph.resources.contains(name) && renderGraph.resources.at(name).ci.type != RenderGraphResourceCreateInfo::None) {
            LOG_ERROR("Resoure \"{}\" of different type already exists", name);
            return false;
        }

        auto &resourceInfo = renderGraph.resources[name];
        resourceInfo.name = name;
        resourceInfo.ci = RenderGraphResourceCreateInfo(info);
        if(info.type == RenderGraphResourceCreateInfo::External) {
            resourceInfo.eResource = info.external.eResource;
        }
    }

    // Convert alias resources to their targets
    for(auto const &[name, target] : resourceBuilder.getAliasResources()) {
        RG_LOG_TRACE("  Alias resource \"{}\" -> \"{}\"", name, target);
        if(renderGraph.resources.contains(name) && renderGraph.resources.at(name).ci.type != RenderGraphResourceCreateInfo::None) {
            LOG_ERROR("Resoure \"{}\" of different type already exists", name);
            return false;
        }

        // The alias is unused.
        if(!renderGraph.resources.contains(name)) {
            continue;
        }

        std::string realTarget = target;
        while(resourceBuilder.getAliasResources().contains(realTarget)) {
            realTarget = resourceBuilder.getAliasResources().at(realTarget);
        }

        if(!renderGraph.resources.contains(realTarget)) {
            LOG_ERROR("Resource \"{}\" aliases non-existing resource \"{}\"!", name, realTarget);
            return false;
        }

        auto const &srcResource = renderGraph.resources.at(name);

        auto &targetResource = renderGraph.resources.at(realTarget);
        targetResource.aliased.emplace_back(name);

        auto &aliasInfo = renderGraph.aliases[name];
        aliasInfo.name = name;
        aliasInfo.target = target;
        aliasInfo.realTarget = realTarget;
        aliasInfo.read = srcResource.read;
        aliasInfo.written = srcResource.written;

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
            if(!(renderGraph.resources.contains(res) && renderGraph.resources.at(res).ci.type != RenderGraphResourceCreateInfo::None) &&
                !renderGraph.aliases.contains(res)) {
                LOG_ERROR("Pass \"{}\" writes to unknown resource \"{}\"", pass.name, res);
                return false;
            }
        }
        for(auto const &info : pass.reads) {
            if(!(renderGraph.resources.contains(info.name) && renderGraph.resources.at(info.name).ci.type != RenderGraphResourceCreateInfo::None) &&
                !renderGraph.aliases.contains(info.name)) {
                LOG_ERROR("Pass \"{}\" reads from unknown resource \"{}\"", pass.name, info.name);
                return false;
            }
        }
    }

    return true;
}

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
    for(auto const &resource : pass.writes) {
        if(renderGraph.isAlias(resource.name)) {
            auto const &info = renderGraph.getAlias(resource.name);
            if(!info.read.empty() || renderGraph.getRes(info.realTarget).ci.type == RenderGraphResourceCreateInfo::External) {
                used = true;
                break;
            }
        } else {
            auto const &info = renderGraph.getRes(resource.name);
            if(!info.read.empty() || info.ci.type == RenderGraphResourceCreateInfo::External) {
                used = true;
                break;
            }
            for(auto const &alias : info.aliased) {
                auto const &info = renderGraph.getAlias(alias);
                if(!info.read.empty() || renderGraph.getRes(info.realTarget).ci.type == RenderGraphResourceCreateInfo::External) {
                    used = true;
                    break;
                }
            }
        }
    }
    if(!used) {
        RG_LOG_TRACE("Pass \"{}\" is unused!", pass.name);
        return true;
    }

    RG_LOG_TRACE("Processing pass \"{}\"", pass.name);

    // Process dependencies
    for(auto const &resource : pass.reads) {
        RG_LOG_TRACE("  \"{}\" reads \"{}\" {} history {}", pass.name, resource.name, string_VkAccessFlags2(resource.traits.access), resource.history);
        if(renderGraph.isAlias(resource.name)) {
            auto const &info = renderGraph.getAlias(resource.name);
            if(!processPass(info.written.name, renderGraph, depth - 1, true)) {
                return false;
            }
        } else {
            auto const &info = renderGraph.getRes(resource.name);
            if(!processPass(info.written.name, renderGraph, depth - 1, true)) {
                return false;
            }
        }
    }
    for(auto const &dependency : pass.explicitPassDependencies) {
        RG_LOG_TRACE("  \"{}\" depends on \"{}\"", pass.name, dependency);
        if(!processPass(dependency, renderGraph, depth - 1, true)) {
            return false;
        }
    }

    if(nodeState != NodeState::Added) {
        nodeState = NodeState::Added;
        pass.index = renderGraph.passStack.size();
        renderGraph.passStack.emplace_back(pass.name);
        RG_LOG_TRACE("  Adding pass \"{}\"", passName);
        if(backtrack) {
            RG_LOG_TRACE("  Backtracking.");
            return true;
        }
    }

    // Process dependents
    for(auto const &resource : pass.writes) {
        RG_LOG_TRACE("  \"{}\" writes \"{}\" {}", pass.name, resource.name, string_VkAccessFlags2(resource.traits.access));

        if(renderGraph.isAlias(resource.name)) {
            auto const &info = renderGraph.getAlias(resource.name);
            for(auto const &dependency : info.read)
                if(!processPass(dependency.name, renderGraph, depth - 1, backtrack))
                    return false;

            // Process passes that read from the actual resource
            // If there is no pass that writes directly to it
            auto const &target = renderGraph.getRes(info.realTarget);
            if(target.written.name.empty()) {
                for(auto const &dependency : target.read)
                    if(!processPass(dependency.name, renderGraph, depth - 1, backtrack))
                        return false;
            }
        } else {
            auto const &info = renderGraph.getRes(resource.name);
            for(auto const &dependent : info.read) {
                if(!processPass(dependent.name, renderGraph, depth - 1, backtrack))
                    return false;
            }
            for(auto const &alias : info.aliased) {
                auto const &aliasInfo = renderGraph.getAlias(alias);
                if(aliasInfo.target != resource.name) {
                    continue;
                }

                for(auto const &dependent : info.read) {
                    if(!processPass(dependent.name, renderGraph, depth - 1, backtrack))
                        return false;
                }
            }
        }
    }
    for(auto const &passName : pass.explicitPassDependents) {
        if(!processPass(passName, renderGraph, depth - 1, backtrack)) {
            return false;
        }
    }

    return true;
}
static void calculateLifetimes(_impl::RenderGraphImpl &renderGraph) {
    ZoneScoped;

    for(auto &[_, resource] : renderGraph.resources) {
        for(auto const &dependency : resource.read) {
            auto const &pass = renderGraph.passes.at(dependency.name);
            resource.lifetimeEnd = std::max(resource.lifetimeEnd, pass.index);
        }
        if(!resource.written.name.empty()) {
            auto const &pass = renderGraph.passes.at(resource.written.name);
            resource.lifetimeBegin = std::min(resource.lifetimeBegin, pass.index);
        }
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
        (first.imageInfo.sampler.flags & second.imageInfo.sampler.flags) && first.imageInfo.sampler.magFilter == second.imageInfo.sampler.magFilter &&
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
static RenderGraphResourceCreateInfo::Type getResourceType(RestrictedEntity<ResourceAllocationInfo> e) {
    ZoneScoped;

    auto const &info = *e;
    if(info.ci.type == RenderGraphResourceCreateInfo::External) {
        assert(e.contains<vk::Image>() || e.contains<vk::Buffer>());
        return e.contains<vk::Image>() ? RenderGraphResourceCreateInfo::Image : RenderGraphResourceCreateInfo::Buffer;
    } else {
        return info.ci.type;
    }
}
static bool isResourceCompatible(Entity e, RenderGraphResourceCreateInfo const &ci, VkImageUsageFlags usage) {
    assert(e);
    assert(e.contains<RenderGraphResourceCreateInfo>());
    auto type = getResourceType(e);

    if(type != ci.type)
        return false;

    // External resources must mach the usage
    if(e.contains<vk::Image>() && (e.get<vk::Image>().createInfo.usage & usage) != usage) {
        return false;
    }

    auto const &poolCi = e.get<RenderGraphResourceCreateInfo>();

    if(ci.type == RenderGraphResourceCreateInfo::Image && !isCompatible(ci.image, poolCi.image)) {
        return false;
    }

    return true;
}
static Entity findPoolResource(RenderGraphResourceCreateInfo const &ci, _impl::RenderGraphImpl &renderGraph, VkImageUsageFlags usage = 0) {
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
            if(resource.ci.type == RenderGraphResourceCreateInfo::External)
                continue;

            assert(!e.valid());
            e = findPoolResource(resource.ci, renderGraph);

            if(!e.valid()) {
                assert(renderGraph.reg && "Should have called RenderGraphBuilder::setAllocInfo!");
                e = renderGraph.reg->create(resource.ci);
                e.name(resourceName);
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
                if(ci.type == RenderGraphResourceCreateInfo::Image) {
                    allocInfo.imageUsage |= info.traits.imageTraits.usage;
                } else {
                    allocInfo.bufferUsage |= info.traits.bufferTraits.usage;
                }
            }

            // May not use resources if the contents are used in the next frame
            if(info.history)
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

        assert(allocInfo.ci.type == RenderGraphResourceCreateInfo::Image || allocInfo.ci.type == RenderGraphResourceCreateInfo::Buffer);

        if(allocInfo.ci.type == RenderGraphResourceCreateInfo::Image) {
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
// TODO: cull duplicate barriers
static bool placeBarriers(_impl::RenderGraphImpl &renderGraph) {
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
                RG_LOG_TRACE("  Ignoring \"{}\" barrier -> {}.", info.name, string_VkAccessFlags2(info.traits.access));
                continue;
            }
            auto const &resource = renderGraph.resources.at(info.name);
            auto &state = resourceState[resource.eResource];
            auto prevState = info.history ? lastScope.at(resource.eResource) : state;

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

    calculateLifetimes(renderGraph);
    aliasResources(renderGraph);
    allocateResources(renderGraph);

    if(!placeBarriers(renderGraph)) {
        LOG_ERROR("Failed to build barriers!");
        return RenderGraph(nullptr);
    }

    RenderGraphImpl *res = new RenderGraphImpl{
        .passes = std::move(renderGraph.passes),
        .resources = std::move(renderGraph.resources),
        .aliases = std::move(renderGraph.aliases),
        .passStack = std::move(renderGraph.passStack),
    };

    return RenderGraph(res);
}
