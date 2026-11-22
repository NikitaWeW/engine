#pragma once

#include "engine/vk/vk.hpp"
#include "engine/dsa/ECS.hpp"

namespace egn::rg {

class RenderPassBuilder;
class RenderGraph;
using RenderPassSetupCallback_t = void(RenderPassBuilder &builder, void *userData);
template <typename T> using RenderPassPostCompileCallback_t = void(T &passData, RenderGraph const &result, void *userData);
template <typename T> using RenderPassRenderCallback_t = void(T &passData, VkCommandBuffer commandBuffer, void *userData);

struct ResourceTraits {
    struct {
        VkBufferUsageFlags2 usage = 0;
        VkDeviceSize offset = 0;
        VkDeviceSize size = VK_WHOLE_SIZE;

        bool valid() const {
            return usage != 0 && size != 0;
        }
    } bufferTraits;
    struct {
        VkImageUsageFlags usage = 0;
        VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_NONE, 0, 1, 0, 1 };
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

        bool valid() const {
            return usage != 0 && layout != VK_IMAGE_LAYOUT_UNDEFINED;
        }
    } imageTraits;

    VkAccessFlags2 access = VK_ACCESS_NONE;
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_NONE;

    /// @brief If the traits are invalid, no barrier is placed.
    bool valid() const {
        return access != VK_ACCESS_NONE && stages != VK_PIPELINE_STAGE_NONE && (bufferTraits.valid() || imageTraits.valid());
    }
};
struct ResourceUsage {
    std::string name;
    ResourceTraits traits;
};
struct ResourceReadUsage : public ResourceUsage {
    /// Whether to use the resource from the previous frame.
    bool history = false;

    ResourceReadUsage() = default;
    inline ResourceReadUsage(std::string_view name, ResourceTraits const &traits, bool history = false):
        ResourceUsage(std::string(name), traits), history(history) {
    }
};

// Component (for external resources)
struct ResizeToTarget {
    glm::vec2 ratio;
};
struct RenderGraphImageCreateInfo {
    vk::ImageCreateInfo::ImageInfo imageInfo;
    /// Whether to resize the image resource to the target image.
    /// When set to true the dimensions in imageInfo may be omitted.
    bool resizeToTarget = false;
    glm::vec2 resizeRatio{ 1.0f };
    bool keep = false;
};
struct RenderGraphBufferCreateInfo {
    VkDeviceSize size = 0;
};
struct RenderGraphExternalResourceInfo {
    Entity eResource;
    bool keep = false;
};
// Component
struct RenderGraphResource { };
// Component, added to entities that were used by destroyed render graphs but themselves are not destroyed yet.
// Adding this component to the resource allows new render graphs to use it instead of allocating new ones.
struct RenderGraphUnusedResource { };

class RenderGraphResourceBuilder {
private:
    std::unordered_map<std::string, RenderGraphExternalResourceInfo> mExternalResources;
    std::unordered_map<std::string, RenderGraphImageCreateInfo> mImageResources;
    std::unordered_map<std::string, RenderGraphBufferCreateInfo> mBufferResources;
    std::unordered_map<std::string, std::string> mAliasResources;
public:
    /// @brief Yup it adds an external resource.
    /// If a resource already exists it gets overridden.
    /// @param eResource Either valid entity containing vk::Image or vk::Buffer or invalid entity.
    /// @param keep Keeps the contents of the resource from a previous frame (whether to set the barrier layout to undefined). Image only.
    /// @returns True if the resource was added successfully.
    bool addExternalResource(std::string const &name, RenderGraphExternalResourceInfo info);
    bool addImageResource(std::string const &name, RenderGraphImageCreateInfo info);
    bool addBufferResource(std::string const &name, RenderGraphBufferCreateInfo info);
    /// @brief Tells the builder that the @p name resource is actually @p target.
    /// Can be called multiple times. The alias source will be overriden.
    void addAliasResource(std::string const &name, std::string const &target);

    bool contains(std::string const &name) const;

    std::unordered_map<std::string, RenderGraphExternalResourceInfo> const &getExternalResources() const;
    std::unordered_map<std::string, RenderGraphImageCreateInfo> const &getImageResources() const;
    std::unordered_map<std::string, RenderGraphBufferCreateInfo> const &getBufferResources() const;
    std::unordered_map<std::string, std::string> const &getAliasResources() const;
};

class RenderGraphBuilder;
namespace _impl {
    struct RenderGraphImpl;
    bool setupRenderGraph(::egn::rg::RenderGraphBuilder const &, void *, RenderGraphImpl &);
}

class RenderPassBuilder {
private:
    std::vector<ResourceReadUsage> mReads;
    std::vector<ResourceUsage> mWrites;
    std::vector<std::string> mExplicitDependencies;

    RenderGraphResourceBuilder *mResourceBuilder = nullptr;
public:
    RenderPassBuilder(RenderGraphResourceBuilder *resourceBuilder);
    RenderGraphResourceBuilder *resource();
    inline RenderGraphResourceBuilder *operator->() {
        return resource();
    }

    void attachResourceRead(ResourceReadUsage const &resource);
    void attachResourceWrite(ResourceUsage const &resource);
    /// @brief Tells the builder that the pass must be executed only after passName.
    void addExplicitDependency(std::string_view passName);

    std::vector<ResourceReadUsage> const &getReads() const;
    std::vector<ResourceUsage> const &getWrites() const;
    std::vector<std::string> const &getExplicitDependencies() const;
};

/// @brief Type erased RenderPassStorage for storing the pass data struct and the callbacks.
class IRenderPassStorage {
public:
    virtual ~IRenderPassStorage() = default;
    virtual void setup(RenderPassBuilder &builder, void *data = nullptr) = 0;
    virtual void postCompile(RenderGraph const &res, void *data = nullptr) = 0;
    virtual void render(VkCommandBuffer cb, void *data = nullptr) = 0;

    virtual void const *data() const = 0;
    virtual void *data() = 0;

    virtual IRenderPassStorage *clone() = 0;
};
template <typename T> class RenderPassStorage : public IRenderPassStorage {
private:
    T mData;
    std::function<RenderPassSetupCallback_t> mSetupCallback;
    std::function<RenderPassPostCompileCallback_t<T>> mPostCompileCallback;
    std::function<RenderPassRenderCallback_t<T>> mRenderCallback;
public:
    RenderPassStorage() = default;
    // clang-format off
    template<typename... Args>
    inline explicit RenderPassStorage(
        std::function<RenderPassSetupCallback_t>          setupCallback,
        std::function<RenderPassPostCompileCallback_t<T>> postCompileCallback,
        std::function<RenderPassRenderCallback_t<T>>      renderCallback,
        Args...                                           args
    ):  mData(std::forward<Args>(args)...),
        mSetupCallback(setupCallback),
        mPostCompileCallback(postCompileCallback),
        mRenderCallback(renderCallback) {}
    // clang-format on

    inline void setup(RenderPassBuilder &builder, void *data) override {
        mSetupCallback(builder, data);
    }
    inline void postCompile(RenderGraph const &res, void *data) override {
        mPostCompileCallback(mData, res, data);
    }
    inline void render(VkCommandBuffer cb, void *data) override {
        mRenderCallback(mData, cb, data);
    }
    inline IRenderPassStorage *clone() override {
        return new RenderPassStorage<T>(*this);
    }
    inline void const *data() const override {
        return &mData;
    }
    inline void *data() override {
        return &mData;
    }
};

struct Barrier {
    struct Scope {
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkBufferUsageFlags2 bufferUsage = 0;
        VkImageUsageFlags imageUsage = 0;

        uint32_t queueIndex = VK_QUEUE_FAMILY_IGNORED;
        VkAccessFlags2 access = VK_ACCESS_2_NONE;
        VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
    };

    std::string resource;
    Scope src;
    Scope dst;

    VkImageSubresourceRange subresourceRange;
    VkDeviceSize offset;
    VkDeviceSize size;

    inline VkImageMemoryBarrier2 getImageBarrier(RestrictedEntity<vk::Image> eResource) const {
        auto const &image = eResource.get<vk::Image>();
        return { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = src.stages,
            .srcAccessMask = src.access,
            .dstStageMask = dst.stages,
            .dstAccessMask = dst.access,
            .oldLayout = src.layout,
            .newLayout = dst.layout,
            .srcQueueFamilyIndex = src.queueIndex,
            .dstQueueFamilyIndex = dst.queueIndex,
            .image = image.image,
            .subresourceRange = subresourceRange };
    }
    inline VkBufferMemoryBarrier2 getBufferBarrier(RestrictedEntity<vk::Buffer> eResource) const {
        auto const &buffer = eResource.get<vk::Buffer>();
        return { .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = src.stages,
            .srcAccessMask = src.access,
            .dstStageMask = dst.stages,
            .dstAccessMask = dst.access,
            .srcQueueFamilyIndex = src.queueIndex,
            .dstQueueFamilyIndex = dst.queueIndex,
            .buffer = buffer.buffer,
            .offset = offset,
            .size = size };
    }
};
struct RenderPass {
    std::string name;
    uint index = 0; // In the pass stack
    std::vector<ResourceReadUsage> reads;
    std::vector<ResourceUsage> writes;
    std::vector<std::string> explicitPassDependencies;
    std::vector<Barrier> barriers;
    VkQueueFlagBits queue;
    std::unique_ptr<IRenderPassStorage> storage;
};

class RenderGraphBuilder {
public:
    struct Pass {
        std::string name;
        VkQueueFlagBits queue;
        std::unique_ptr<IRenderPassStorage> storage;
    };
private:
    std::vector<std::string> passOrder;
    std::unordered_map<std::string, Pass> passes;
    std::optional<vk::AllocationCreateInfo> allocInfo;
    std::optional<vk::QueueFamilies> queueFamilies;
    RenderGraphResourceBuilder mResources;
    Registry *reg = nullptr;
    friend bool ::egn::rg::_impl::setupRenderGraph(RenderGraphBuilder const &, void *, _impl::RenderGraphImpl &);
public:
    RenderGraphBuilder() = default;
    RenderGraphBuilder(RenderGraphBuilder const &) = delete;
    RenderGraphBuilder &operator=(RenderGraphBuilder const &) = delete;
    RenderGraphBuilder(RenderGraphBuilder &&) = default;
    RenderGraphBuilder &operator=(RenderGraphBuilder &&) = default;
    ~RenderGraphBuilder() = default;

    inline RenderGraphResourceBuilder &resource() {
        return mResources;
    }

    /// @brief Required for allocating stuff (optional)
    /// If not called only RenderPassBuilder::addExternalResource is allowed!
    void setAllocInfo(vk::AllocationCreateInfo const &allocInfo, Registry &reg);

    /// @brief Required for barrier placement (not optional)
    void setQueueFamilies(vk::QueueFamilies const &queueFamilies);

    /// makePass can be used for better syntax.
    bool addPass(Pass &&pass);

    bool removePass(std::string const &name);
    void clear();
};

/// @brief Executable render graph.
class RenderGraph {
private:
    struct RenderGraphImpl *mImpl = nullptr;
public:
    RenderGraph(RenderGraphImpl *data = nullptr); // data could be nullptr for invalid render graphs.
    RenderGraph(RenderGraph const &) = delete;
    RenderGraph &operator=(RenderGraph const &) = delete;
    RenderGraph(RenderGraph &&);
    RenderGraph &operator=(RenderGraph &&);
    ~RenderGraph();

    /// @brief Returns true of the render graph is valid.
    bool valid() const;

    /// @brief Set the external resource.
    /// Must be called for external resources that had invalid entities at creation time.
    /// Overrides the previous resource entity if called multiple times.
    bool setResource(std::string const &name, Entity eResource);

    /// @brief Get the entity associated with the resource.
    /// Multiple resources may have the same entity reused.
    /// Returns an invalid entity if the resource is not found.
    Entity getResource(std::string const &name) const;
    
    enum class ResourceType {
        External, Image, Buffer, Alias
    };
    /// @throws If the render graph does not contain the resource.
    ResourceType getResourceType(std::string const &name) const;

    /// @brief Get the names of the passes that read from the resource.
    /// @throws If the render graph does not contain the resource.
    std::vector<ResourceReadUsage> const &getResourceRead(std::string const &name) const;

    /// @brief Get the name of the pass that write to the resource, empty if no write.
    /// @throws If the render graph does not contain the resource.
    ResourceUsage const &getResourceWrite(std::string const &name) const;

    /// @brief Returns all resources used by this render graph.
    std::unordered_set<Entity> const &getAllocatedResources() const;

    /// @brief Returns all resources used by this render graph.
    std::unordered_map<std::string, Entity> const &getExternalResources() const;

    /// @brief Returns all resources used by this render graph.
    std::unordered_map<std::string, std::string> const &getAliasResources() const;

    /// @brief Get the render pass information.
    RenderPass const *getPass(std::string const &name) const;

    /// @brief Get the execution order of the passes
    std::vector<std::string> const &getPassStack() const;
};

/// @brief Duh.
RenderGraph buildRenderGraph(RenderGraphBuilder const &builder, void *userData = nullptr);

struct GraphvizSettings {
    std::vector<std::string> graphAttributes = { "beautify=true", "nodesep=0.5", "ranksep=0.5", "rankdir=TB" };
    std::vector<std::string> nodeAttributes = { };
    std::vector<std::string> edgeAttributes = { "stype=solid", "constraint=true", "arrowhead=normal" };
};

/// @brief Generate a DOT graph.
/// @param indent If indent is nonnegative, then elements will be pretty-printed with that indent level.
/// An indent level of 0 will only insert newlines. -1 (the default) selects the most compact representation.
std::string dumpGraphviz(RenderGraph const &graph, int indent = -1, GraphvizSettings settings = { });

// ===================================================================================

// clang-format off
template <typename Data_t, typename... Args>
inline RenderGraphBuilder::Pass makePass(
    std::string const &name, 
    VkQueueFlagBits queue, 
    std::function<RenderPassSetupCallback_t> setup,
    std::function<RenderPassPostCompileCallback_t<Data_t>> post, 
    std::function<RenderPassRenderCallback_t<Data_t>> render,
    Args... args
) {
    return RenderGraphBuilder::Pass{ 
        .name = std::string(name),
        .queue = queue,
        .storage = std::unique_ptr<IRenderPassStorage>(new RenderPassStorage<Data_t>(setup, post, render, std::forward<Args>(args)...)), 
    };
}
// clang-format on

} // namespace egn
