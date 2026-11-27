#include "engine/renderer/ResourceManager.hpp"
#include "engine/renderer/EngineRenderer.hpp"

using namespace egn;

constexpr VkFormat egn::toVkFormat(ImageFormat const &format) noexcept {
    using Type = ImageFormat::Type;
    using Swiz = ImageFormat::ComponentSwizzle;

    constexpr auto packKey = [](unsigned r, unsigned g, unsigned b, unsigned a, Type t) constexpr -> uint64_t {
        return (static_cast<uint64_t>(r & 0xFF) << 32) |
               (static_cast<uint64_t>(g & 0xFF) << 24) |
               (static_cast<uint64_t>(b & 0xFF) << 16) |
               (static_cast<uint64_t>(a & 0xFF) << 8)  |
               static_cast<uint64_t>(t);
    };

    const bool isBgraSwizzle =
        format.swizzle.r == Swiz::B &&
        (format.swizzle.g == Swiz::G || format.swizzle.g == Swiz::IDENTITY) &&
        format.swizzle.b == Swiz::R &&
        (format.swizzle.a == Swiz::A || format.swizzle.a == Swiz::IDENTITY);

    const uint64_t key = packKey(format.packing.r, format.packing.g, format.packing.b, format.packing.a, format.type);

    switch (key) {
        case packKey(8, 0, 0, 0, Type::UNORM): return VK_FORMAT_R8_UNORM;
        case packKey(8, 0, 0, 0, Type::SNORM): return VK_FORMAT_R8_SNORM;
        case packKey(8, 0, 0, 0, Type::UINT):  return VK_FORMAT_R8_UINT;
        case packKey(8, 0, 0, 0, Type::SINT):  return VK_FORMAT_R8_SINT;
        case packKey(8, 0, 0, 0, Type::SRGB):  return VK_FORMAT_R8_SRGB;

        case packKey(8, 8, 0, 0, Type::UNORM): return VK_FORMAT_R8G8_UNORM;
        case packKey(8, 8, 0, 0, Type::SNORM): return VK_FORMAT_R8G8_SNORM;
        case packKey(8, 8, 0, 0, Type::UINT):  return VK_FORMAT_R8G8_UINT;
        case packKey(8, 8, 0, 0, Type::SINT):  return VK_FORMAT_R8G8_SINT;
        case packKey(8, 8, 0, 0, Type::SRGB):  return VK_FORMAT_R8G8_SRGB;

        case packKey(8, 8, 8, 0, Type::UNORM): return isBgraSwizzle ? VK_FORMAT_B8G8R8_UNORM : VK_FORMAT_R8G8B8_UNORM;
        case packKey(8, 8, 8, 0, Type::SNORM): return isBgraSwizzle ? VK_FORMAT_B8G8R8_SNORM : VK_FORMAT_R8G8B8_SNORM;
        case packKey(8, 8, 8, 0, Type::UINT):  return isBgraSwizzle ? VK_FORMAT_B8G8R8_UINT  : VK_FORMAT_R8G8B8_UINT;
        case packKey(8, 8, 8, 0, Type::SINT):  return isBgraSwizzle ? VK_FORMAT_B8G8R8_SINT  : VK_FORMAT_R8G8B8_SINT;
        case packKey(8, 8, 8, 0, Type::SRGB):  return isBgraSwizzle ? VK_FORMAT_B8G8R8_SRGB  : VK_FORMAT_R8G8B8_SRGB;

        case packKey(8, 8, 8, 8, Type::UNORM): return isBgraSwizzle ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
        case packKey(8, 8, 8, 8, Type::SNORM): return isBgraSwizzle ? VK_FORMAT_B8G8R8A8_SNORM : VK_FORMAT_R8G8B8A8_SNORM;
        case packKey(8, 8, 8, 8, Type::UINT):  return isBgraSwizzle ? VK_FORMAT_B8G8R8A8_UINT  : VK_FORMAT_R8G8B8A8_UINT;
        case packKey(8, 8, 8, 8, Type::SINT):  return isBgraSwizzle ? VK_FORMAT_B8G8R8A8_SINT  : VK_FORMAT_R8G8B8A8_SINT;
        case packKey(8, 8, 8, 8, Type::SRGB):  return isBgraSwizzle ? VK_FORMAT_B8G8R8A8_SRGB  : VK_FORMAT_R8G8B8A8_SRGB;

        case packKey(16, 0, 0, 0, Type::UNORM):  return VK_FORMAT_R16_UNORM;
        case packKey(16, 0, 0, 0, Type::SNORM):  return VK_FORMAT_R16_SNORM;
        case packKey(16, 0, 0, 0, Type::UINT):   return VK_FORMAT_R16_UINT;
        case packKey(16, 0, 0, 0, Type::SINT):   return VK_FORMAT_R16_SINT;
        case packKey(16, 0, 0, 0, Type::SFLOAT): return VK_FORMAT_R16_SFLOAT;

        case packKey(16, 16, 0, 0, Type::UNORM):  return VK_FORMAT_R16G16_UNORM;
        case packKey(16, 16, 0, 0, Type::SNORM):  return VK_FORMAT_R16G16_SNORM;
        case packKey(16, 16, 0, 0, Type::UINT):   return VK_FORMAT_R16G16_UINT;
        case packKey(16, 16, 0, 0, Type::SINT):   return VK_FORMAT_R16G16_SINT;
        case packKey(16, 16, 0, 0, Type::SFLOAT): return VK_FORMAT_R16G16_SFLOAT;

        case packKey(16, 16, 16, 0, Type::UNORM):  return VK_FORMAT_R16G16B16_UNORM;
        case packKey(16, 16, 16, 0, Type::SNORM):  return VK_FORMAT_R16G16B16_SNORM;
        case packKey(16, 16, 16, 0, Type::UINT):   return VK_FORMAT_R16G16B16_UINT;
        case packKey(16, 16, 16, 0, Type::SINT):   return VK_FORMAT_R16G16B16_SINT;
        case packKey(16, 16, 16, 0, Type::SFLOAT): return VK_FORMAT_R16G16B16_SFLOAT;

        case packKey(16, 16, 16, 16, Type::UNORM):  return VK_FORMAT_R16G16B16A16_UNORM;
        case packKey(16, 16, 16, 16, Type::SNORM):  return VK_FORMAT_R16G16B16A16_SNORM;
        case packKey(16, 16, 16, 16, Type::UINT):   return VK_FORMAT_R16G16B16A16_UINT;
        case packKey(16, 16, 16, 16, Type::SINT):   return VK_FORMAT_R16G16B16A16_SINT;
        case packKey(16, 16, 16, 16, Type::SFLOAT): return VK_FORMAT_R16G16B16A16_SFLOAT;

        case packKey(32, 0, 0, 0, Type::UINT):   return VK_FORMAT_R32_UINT;
        case packKey(32, 0, 0, 0, Type::SINT):   return VK_FORMAT_R32_SINT;
        case packKey(32, 0, 0, 0, Type::SFLOAT): return VK_FORMAT_R32_SFLOAT;

        case packKey(32, 32, 0, 0, Type::UINT):   return VK_FORMAT_R32G32_UINT;
        case packKey(32, 32, 0, 0, Type::SINT):   return VK_FORMAT_R32G32_SINT;
        case packKey(32, 32, 0, 0, Type::SFLOAT): return VK_FORMAT_R32G32_SFLOAT;

        case packKey(32, 32, 32, 0, Type::UINT):   return VK_FORMAT_R32G32B32_UINT;
        case packKey(32, 32, 32, 0, Type::SINT):   return VK_FORMAT_R32G32B32_SINT;
        case packKey(32, 32, 32, 0, Type::SFLOAT): return VK_FORMAT_R32G32B32_SFLOAT;

        case packKey(32, 32, 32, 32, Type::UINT):   return VK_FORMAT_R32G32B32A32_UINT;
        case packKey(32, 32, 32, 32, Type::SINT):   return VK_FORMAT_R32G32B32A32_SINT;
        case packKey(32, 32, 32, 32, Type::SFLOAT): return VK_FORMAT_R32G32B32A32_SFLOAT;

        case packKey(5, 6, 5, 0, Type::UNORM):     return isBgraSwizzle ? VK_FORMAT_B5G6R5_UNORM_PACK16 : VK_FORMAT_R5G6B5_UNORM_PACK16;
        case packKey(4, 4, 4, 4, Type::UNORM):     return isBgraSwizzle ? VK_FORMAT_B4G4R4A4_UNORM_PACK16 : VK_FORMAT_R4G4B4A4_UNORM_PACK16;
        case packKey(10, 10, 10, 2, Type::UNORM):  return isBgraSwizzle ? VK_FORMAT_A2B10G10R10_UNORM_PACK32 : VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        case packKey(10, 10, 10, 2, Type::UINT):   return isBgraSwizzle ? VK_FORMAT_A2B10G10R10_UINT_PACK32  : VK_FORMAT_A2R10G10B10_UINT_PACK32;
        case packKey(5, 5, 5, 1, Type::UNORM):     return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
        case packKey(11, 11, 10, 0, Type::UFLOAT): return VK_FORMAT_B10G11R11_UFLOAT_PACK32;

        default:
            return VK_FORMAT_UNDEFINED;
    }
}
static VkFence createFence(VkDevice dev) {
    VkFence fence = nullptr;
    VkFenceCreateInfo fenceCI{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    CHECK_VK_RES(vkCreateFence(dev, &fenceCI, nullptr, &fence));
    return fence;
}

ResourceAllocator::ResourceAllocator(vk::AllocationCreateInfo const &allocInfo, VkCommandPool commandPool, VkQueue queue) {
    mAllocInfo = allocInfo;

    VkCommandBufferAllocateInfo commandBufferAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .commandBufferCount = 1,
    };
    CHECK_VK_RES(vkAllocateCommandBuffers(mAllocInfo.device, &commandBufferAllocInfo, &mCommandBuffer));
    mFence = createFence(mAllocInfo.device);
    mQueue = queue;
}
ResourceAllocator::ResourceAllocator(ResourceAllocator &&o) {
    operator=(std::move(o));
}
ResourceAllocator &ResourceAllocator::operator=(ResourceAllocator &&o) {
    std::swap(mAllocInfo, o.mAllocInfo);
    std::swap(mCommandBuffer, o.mCommandBuffer);
    std::swap(mFence, o.mFence);
    std::swap(mQueue, o.mQueue);
    std::swap(mProcessedImages, o.mProcessedImages);
    std::swap(mProcessedCount, o.mProcessedCount);
    o.mFence = nullptr;
    return *this;
}
ResourceAllocator::~ResourceAllocator() {
    if(mFence && mAllocInfo.device) {
        vkDestroyFence(mAllocInfo.device, mFence, nullptr);
    }
}
uint32_t ResourceAllocator::processImage(Entity eImage) {
    assert(eImage.valid() && (eImage.contains<Texture>()) && "Invalid model!");
    assert(mCommandBuffer && "ResourceAllocator uninitialized! (Make sure to not use the default constructor)");
    if(!eImage.contains<vk::Image>() && eImage.contains<Texture>()) {
        auto &image = eImage.get<Texture>();

        // if(image.bitmap.numComponents == 3)
        //     LOG_WARN("Making R32G32B32 texture2D \"{}\". Maybe change it to 32 bits or something...", image.path);

        VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        switch(image.addressMode) {
            case Texture::AddressMode::Repeat: addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT; break;
            case Texture::AddressMode::MirroredRepeat: addressMode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT; break;
            case Texture::AddressMode::ClampToEdge: addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; break;
            case Texture::AddressMode::ClampToBorder: addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; break;
            case Texture::AddressMode::MirrorClampToEdge: addressMode = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE; break;
            default: LOG_WARN("Unknown address mode in {}!", eImage); break;
        }
        vk::ImageCreateInfo ci{
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
            .allocInfo = mAllocInfo,
            .commandBuffer = mCommandBuffer,
            .image = {
                .imageType = VK_IMAGE_TYPE_2D,
                .format = toVkFormat(image.image.format),
                .dimensions = {
                    .width = image.image.dimensions.width,
                    .height = image.image.dimensions.height,
                    .depth = image.image.dimensions.depth,
                    .mipLevels = image.numMipLevels,
                    .arrayLayers = image.image.dimensions.arrayLayers,
                },
                .sampler = {
                    .magFilter = image.linearSampling ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
                    .minFilter = image.linearSampling ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
                    .mipmapMode = image.linearSampling ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR,
                    .addressModeU = addressMode,
                    .addressModeV = addressMode,
                    .addressModeW = addressMode,
                    .anisotropyEnable = image.linearSampling,
                },
                .view = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D
                },
            },
            .data = image.image.pixels.data(),
            .name = image.path
        };
        eImage.emplace<vk::Image>(vk::makeImage(ci));
        eImage.emplace<ImageIndex>(static_cast<uint32_t>(mProcessedImages.size()));
        eImage.name(ci.name);
        mProcessedImages.emplace_back(eImage);

        vk::insertImageMemoryBarrier(mCommandBuffer, eImage.get<vk::Image>().image, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, { VK_IMAGE_ASPECT_COLOR_BIT, 0, ci.image.dimensions.mipLevels, 0, 1 });

        ++mProcessedCount;

        // LOG_TRACE("Allocated image e{} \"{}\" {}x{}, {} {} mips of type {} format {} usage {} filter {} address mode {}",
        //     eImage.id(),
        //     image.path,
        //     image.bitmap.size.x, image.bitmap.size.y,
        //     image.srgb ? "srgb" : "linear",
        //     image.numMipLevels,
        //     string_VkImageViewType(eImage.get<vk::Image>().createInfo.image.view.viewType),
        //     string_VkFormat(eImage.get<vk::Image>().createInfo.image.format),
        //     string_VkImageUsageFlags(eImage.get<vk::Image>().createInfo.usage),
        //     string_VkFilter(ci.image.sampler.minFilter),
        //     string_VkSamplerAddressMode(ci.image.sampler.addressModeU)
        // );
    }
    // TODO: cubemaps

    return eImage.get<ImageIndex>().index;
}
void ResourceAllocator::processModel(Entity eModel) {
    assert(eModel.valid() && eModel.contains<Model>() && "Invalid model!");
    if(eModel.contains<VulkanModel>())
        return;

    auto &model = eModel.get<Model>();
    eModel.emplace<VulkanModel>();
    auto &vulkanModel = eModel.get<VulkanModel>();
    for(uint i = 0; i < model.meshes.size(); ++i) {
        auto const &mesh = model.meshes[i];
        auto &vulkanMesh = vulkanModel.meshes.emplace_back();

        vulkanMesh.meshIndex = i;
        vulkanMesh.indexCount = mesh.geometry.indices.size();

        vulkanMesh.buffers = {
            .pos = vk::makeBuffer(mAllocInfo, mesh.geometry.positions, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
            .attr = vk::makeBuffer(mAllocInfo, mesh.geometry.attributes, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
            .idx = vk::makeBuffer(mAllocInfo, mesh.geometry.indices, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
        };

        vulkanMesh.material.textures = {
            .albedo = processImage(Entity{ &eModel.reg(), mesh.material.textures.albedo }),
            .metallic = processImage(Entity{ &eModel.reg(), mesh.material.textures.metallic }),
            .roughness = processImage(Entity{ &eModel.reg(), mesh.material.textures.roughness }),
            .ambient = processImage(Entity{ &eModel.reg(), mesh.material.textures.ambient }),
            .normal = processImage(Entity{ &eModel.reg(), mesh.material.textures.normal }),
            .displacement = processImage(Entity{ &eModel.reg(), mesh.material.textures.displacement }),
        };

        vulkanMesh.material.properties = {
            .ambient = mesh.material.properties.ambient,
            .albedo = mesh.material.properties.albedo,
            .specular = mesh.material.properties.specular,
            .emission = mesh.material.properties.emission,
            .shininess = mesh.material.properties.shininess,
            .metallic = mesh.material.properties.metallic,
            .roughness = mesh.material.properties.roughness,
            .ior = mesh.material.properties.ior,
        };
    }
    ++mProcessedCount;
}
void ResourceAllocator::begin() {
    assert(mProcessedCount == 0 && "Call end() before begin()");
    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    CHECK_VK_RES(vkBeginCommandBuffer(mCommandBuffer, &beginInfo));
}
bool ResourceAllocator::end() {
    if(mProcessedCount == 0) {
        return false;
    }
    vkEndCommandBuffer(mCommandBuffer);

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &mCommandBuffer,
    };
    CHECK_VK_RES(vkQueueSubmit(mQueue, 1, &submitInfo, mFence));
    CHECK_VK_RES(vkWaitForFences(mAllocInfo.device, 1, &mFence, true, UINT64_MAX));
    mProcessedCount = 0;
    return true;
}

void DescriptorManager::emplace(DescriptorWrite write) {
    assert(!write.imageInfo.empty() || !write.bufferInfo.empty());
    erase(write.dstPipeline, { write.dstSet, write.dstBinding }, write.dstFrame);
    mWrites.emplace_back(write);
    write.dstPipeline.try_emplace<ResourceDirty>();

    if(!mPipelines.contains(write.dstPipeline)) {
        ++write.dstPipeline.try_emplace<DescriptorResourceUsage>().usageCount;
    }
    ++mPipelines[write.dstPipeline];

    for(auto &info : write.imageInfo) {
        ++info.resource.try_emplace<DescriptorResourceUsage>().usageCount;
    }
    for(auto &info : write.bufferInfo) {
        ++info.resource.try_emplace<DescriptorResourceUsage>().usageCount;
    }
}
void DescriptorManager::erase(Entity pipeline, vk::DescriptorBinding binding, uint frame) {
    for(uint i = 0; i < mWrites.size(); ++i) {
        auto &write = mWrites[i];
        if(write.dstPipeline == pipeline && write.dstSet == binding.set && write.dstBinding == binding.binding && write.dstFrame == frame) {
            std::swap(write, mWrites.back());
            mWrites.pop_back();
            assert(mPipelines.contains(pipeline));
            auto &numResourcesInPipeline = mPipelines.at(pipeline);
            assert(numResourcesInPipeline != 0);
            --numResourcesInPipeline;
            if(numResourcesInPipeline == 0) {
                mPipelines.erase(pipeline);
                auto &usageCount = write.dstPipeline.get<DescriptorResourceUsage>().usageCount;
                assert(usageCount != 0);
                --usageCount;
                if(usageCount == 0) {
                    write.dstPipeline.erase<DescriptorResourceUsage>();
                }
            }
        }
    }
}
static void updateCounter(Entity e) {
    assert(e.valid());
    assert(e.contains<ResourceDirty>());
    assert(e.contains<DescriptorResourceUsage>());
    auto &updateCount = e.get<ResourceDirty>().updateCount;
    auto const &usageCount = e.get<DescriptorResourceUsage>().usageCount;
    ++updateCount;
    if(updateCount == usageCount) {
        e.erase<ResourceDirty>();
    }
}
void DescriptorManager::update(uint frame) {
    std::map<Entity, std::vector<vk::DescriptorWrite>> writes;
    for(auto const &write : mWrites) {
        if(write.dstFrame != frame) {
            continue;
        }

        Entity ePipeline = write.dstPipeline;
        bool dirty = ePipeline.contains<ResourceDirty>();
        if(!dirty) {
            for(auto const &info : write.imageInfo) {
                if(info.resource.contains<ResourceDirty>()) {
                    dirty = true;
                    // LOG_TRACE("Resource {} is dirty!", Entity(info.resource));
                    break;
                }
            }
            for(auto const &info : write.bufferInfo) {
                if(info.resource.contains<ResourceDirty>()) {
                    // LOG_TRACE("Resource {} is dirty!", Entity(info.resource));
                    dirty = true;
                    break;
                }
            }
        }

        // if(std::ranges::distance(ePipeline.reg().view<ResourceDirty>()))
        //     LOG_TRACE("Pipeline {} dirty {}", ePipeline, dirty);

        if(!dirty) {
            continue;
        }

        vk::DescriptorWrite descWrite;
        descWrite.dstSet = write.dstSet;
        descWrite.dstBinding = write.dstBinding;
        descWrite.dstArrayElement = write.dstElement;
        descWrite.imageInfo.reserve(write.imageInfo.size());
        descWrite.bufferInfo.reserve(write.bufferInfo.size());

        for(auto const &info : write.imageInfo) {
            assert(info.resource.contains<vk::Image>() && "Invalid write!");
            auto &res = info.resource.get<vk::Image>();
            descWrite.imageInfo.emplace_back(VkDescriptorImageInfo{ .sampler = res.sampler, .imageView = res.view, .imageLayout = info.layout });
        }
        for(auto const &info : write.bufferInfo) {
            assert(info.resource.contains<vk::Buffer>() && "Invalid write!");
            auto &res = info.resource.get<vk::Buffer>();
            descWrite.bufferInfo.emplace_back(VkDescriptorBufferInfo{ .buffer = res.buffer, .offset = info.offset, .range = info.size });
        }

        writes[ePipeline].emplace_back(std::move(descWrite));

        // LOG_TRACE("Updating pipeline {} set {} binding {} frame {} element {} images {} buffers {}", write.dstPipeline, write.dstSet,
        //     write.dstBinding, write.dstFrame, write.dstElement, write.imageInfo.size(), write.bufferInfo.size());
        for(auto const &info : write.imageInfo) {
            if(info.resource.contains<ResourceDirty>()) {
                updateCounter(info.resource);
            }
            // LOG_TRACE("  image {} {}", Entity(info.resource), string_VkImageLayout(info.layout));
        }
        for(auto const &info : write.bufferInfo) {
            if(info.resource.contains<ResourceDirty>()) {
                updateCounter(info.resource);
            }
            // LOG_TRACE("  buffer {} offset {} size {}", Entity(info.resource), info.offset, info.size);
        }
    }

    for(auto &[ePipeline, descWrites] : writes) {
        if(ePipeline.contains<ResourceDirty>()) {
            updateCounter(ePipeline);
        }
        // Automatically checks if the required num of unsized descriptors is bigger.
        // clang-format off
        vk::allocateDescriptors(Entity(ePipeline).get<vk::Pipeline>(), {
            .numFrames = frame,
            .unsizedDescSize = std::max_element(descWrites.begin(), descWrites.end(), 
                [](auto const &lhs, auto const &rhs) { return lhs.size() < rhs.size(); })->size(),
        });
        // clang-format on
        vk::writeDescriptors(ePipeline.get<vk::Pipeline>(), descWrites, frame);
    }

    for(auto const &write : mWrites) {
        if(write.dstFrame != frame) {
            continue;
        }

        if(write.dstPipeline.contains<ResourceDirty>()) {
            Entity(write.dstPipeline).erase<ResourceDirty>();
        }

        for(auto const &info : write.imageInfo) {
            if(info.resource.contains<ResourceDirty>()) {
                Entity(info.resource).erase<ResourceDirty>();
            }
        }
        for(auto const &info : write.bufferInfo) {
            if(info.resource.contains<ResourceDirty>()) {
                Entity(info.resource).erase<ResourceDirty>();
            }
        }
    }
}
