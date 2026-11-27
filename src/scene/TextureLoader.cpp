#include "engine/scene/Loaders.hpp"
#include "engine/io/Logging.hpp"
#include "stb_image.h"
#include "glm/gtc/integer.hpp"

using namespace egn;

TextureLoader::TextureLoader(ecs::registry &reg) {
    mReg = &reg;
}
static ecs::entity makeImage(unsigned char *buff, unsigned width, unsigned height, unsigned numChannels, ecs::registry &reg, std::string_view path) {
    if(!buff) {
        LOG_ERROR("failed to load texture: \"{}\"!: {}", path, stbi_failure_reason());
        return INVALID_ENTITY;
    }
    assert(width > 0 && height > 0 && numChannels > 0 && "failed to load a texture");
    ImageFormat format;
    if(numChannels >= 1)
        format.packing.r = 8;
    if(numChannels >= 2)
        format.packing.g = 8;
    if(numChannels >= 3)
        format.packing.b = 8;
    if(numChannels >= 4)
        format.packing.a = 8;
    format.type = ImageFormat::Type::UNORM;

    Texture texture;
    texture.image = { .pixels = std::vector<unsigned char>(buff, buff + width * height * numChannels),
        .format = format,
        .dimensions = { static_cast<unsigned int>(width), static_cast<unsigned int>(height) } };
    texture.path = path;
    texture.numMipLevels = static_cast<uint32_t>(glm::ceil(glm::log2(std::max(texture.image.dimensions.width, texture.image.dimensions.height))));
    texture.linearSampling = texture.image.dimensions.width * texture.image.dimensions.height > 100 * 100 && texture.numMipLevels > 1;
    stbi_image_free(buff);

    return reg.create(std::move(texture));
}
ecs::entity TextureLoader::loadFromFile(std::string_view path, TextureLoaderOptions options) {
    assert(mReg && "Uninitialized TextureLoader!");

    for(auto e_texture : mReg->view<Texture>())
        if(mReg->get<Texture>(e_texture).path == path)
            return e_texture;

    int width = 0, height = 0, numChannels = 0;
    stbi_set_flip_vertically_on_load(options.flip);
    unsigned char *buff = stbi_load(path.data(), &width, &height, &numChannels, options.desiredChannels);
    if(options.desiredChannels != 0)
        numChannels = options.desiredChannels;

    return makeImage(buff, width, height, numChannels, *mReg, path);
}
ecs::entity TextureLoader::loadFromMemory(void const *data, size_t size, TextureLoaderOptions options) {
    assert(mReg && "Uninitialized TextureLoader!");

    int width = 0, height = 0, numChannels = 0;
    stbi_set_flip_vertically_on_load(options.flip);
    unsigned char *buff = stbi_load_from_memory(static_cast<unsigned char const *>(data), size, &width, &height, &numChannels,
        options.desiredChannels);
    if(options.desiredChannels != 0)
        numChannels = options.desiredChannels;

    return makeImage(buff, width, height, numChannels, *mReg, "");
}
