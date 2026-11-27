#pragma once
#include "engine/dsa/ECS.hpp"
#include "engine/dsa/EaseFunctions.hpp"
#include "glm/gtc/quaternion.hpp"
#include <string>
#include <vector>

namespace egn {

/// Wh.. thats IT?
struct Camera3D {
    glm::mat4 projMat;
    glm::mat4 viewMat;
};

struct ImageFormat {
    enum class Type { SFLOAT, UFLOAT, SINT, UINT, SNORM, UNORM, SRGB };
    enum class ComponentSwizzle { IDENTITY, ZERO, ONE, R, G, B, A };
    struct Packing {
        // In bits
        unsigned r = 0;
        unsigned g = 0;
        unsigned b = 0;
        unsigned a = 0;

        // In bytes
        inline constexpr unsigned size() const noexcept {
            return (r + g + b + a) / 8;
        }
    };
    struct Swizzle {
        ComponentSwizzle r = ComponentSwizzle::IDENTITY;
        ComponentSwizzle g = ComponentSwizzle::IDENTITY;
        ComponentSwizzle b = ComponentSwizzle::IDENTITY;
        ComponentSwizzle a = ComponentSwizzle::IDENTITY;
    };
    Packing packing;
    Type type;
    Swizzle swizzle;
};
constexpr ImageFormat IMAGE_FORMAT_R8G8B8A8_UNORM = {
    .packing = { 8, 8, 8, 8 },
    .type = ImageFormat::Type::UNORM,
};
constexpr ImageFormat IMAGE_FORMAT_R8G8B8_UNORM = {
    .packing = { 8, 8, 8, 0 },
    .type = ImageFormat::Type::UNORM,
};
constexpr ImageFormat IMAGE_FORMAT_R16G16B16A16_SFLOAT = {
    .packing = { 16, 16, 16, 16 },
    .type = ImageFormat::Type::SFLOAT,
};
struct Image {
    struct Dimensions {
        unsigned width = 1;
        unsigned height = 1;
        unsigned depth = 1;
        unsigned arrayLayers = 1;
    };
    std::vector<unsigned char> pixels;
    ImageFormat format = IMAGE_FORMAT_R8G8B8A8_UNORM;
    Dimensions dimensions;

    inline std::size_t offsetOf(Dimensions pos) const {
        return format.packing.size() *
               (pos.arrayLayers * dimensions.depth + pos.depth * dimensions.height + pos.height * dimensions.width + pos.width);
    }
};

struct Texture {
    Image image;
    std::string path;
    unsigned mipLevels = 1;
    bool linearSampling = false;
    unsigned numMipLevels = 1;
    enum class AddressMode {
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder,
        MirrorClampToEdge,
    } addressMode = AddressMode::Repeat;
};

struct Material {
    /// @brief Contains entities with the Texture component, invalid if not present.
    struct Textures {
        ecs::entity albedo = 0;
        ecs::entity metallic = 0;
        ecs::entity roughness = 0;
        ecs::entity ambient = 0;
        ecs::entity normal = 0;
        ecs::entity displacement = 0;
    } textures;
    struct Properties {
        glm::vec3 ambient;
        glm::vec4 albedo{ 1.0f };
        glm::vec3 specular;
        glm::vec3 emission;

        float shininess;
        float metallic = 1.0f;
        float roughness = 1.0f;
        float ior;
    } properties;
};
struct Animation {
    struct PositionKey {
        glm::vec3 value;
        float timeTicks;
    };
    struct OrientationKey {
        glm::quat value;
        float timeTicks;
    };
    struct ScaleKey {
        glm::vec3 value;
        float timeTicks;
    };
    struct Keyframes {
        std::vector<PositionKey> positions;
        std::vector<OrientationKey> orientations;
        std::vector<ScaleKey> scales;
    };

    std::vector<Keyframes> bones;
    std::string name = "";
    float durationTicks = 0;
    float ticksPerSecond = 0;
};
constexpr std::uint32_t MAX_WEIGHTS_PER_VERTEX = 4;
struct Mesh {
    struct Geometry {
        struct Attributes {
            glm::vec2 uv;
            glm::vec3 normal;
            glm::vec3 tangent;

            // optional
            int32_t boneIDs[MAX_WEIGHTS_PER_VERTEX]; // -1 for no id.
            float weights[MAX_WEIGHTS_PER_VERTEX];
        };
        std::vector<glm::vec3> positions;
        std::vector<Attributes> attributes;
        std::vector<uint32_t> indices;
    } geometry;

    Material material;
};
struct Model {
    std::vector<Mesh> meshes;
    std::vector<Animation> animations;
    std::string path;

    std::vector<ecs::entity> lights;

    struct Skeleton {
        glm::mat4 globalInverseTransform;
        std::vector<glm::mat4> bindTransform;
        std::vector<glm::mat4> nodeTransform;
        std::vector<int> parents; // -1 if root
        std::unordered_map<std::string, unsigned> boneMap; // bone name to bone id
    } skeleton;
};

struct ModelInstance {
    RestrictedEntity<Model> eModel;
};

struct Transform3D {
    glm::vec3 position{ 0 };
    glm::quat orientation{ 1, 0, 0, 0 };
    glm::vec3 scale{ 1 };
    inline glm::mat4 getMat() const {
        return glm::translate(glm::mat4{ 1.0f }, position) * glm::mat4_cast(orientation) * glm::scale(glm::mat4{ 1.0f }, scale);
    };
};

struct CurrentAnimation {
    std::string name;
    enum class RepeatMode { EXIT, LOOP, MIRROR } repeat = RepeatMode::LOOP;
    float time = 0; // normalized
    float speed = 1;
    std::vector<glm::mat4> boneMatrices;
    // used to avoid unnecessary allocations to construct hierarchy-free boneMatrices, but can be used too.
    std::vector<glm::mat4> localBoneMatrices;
};
struct AnimationTransition {
    std::string to;
    float factor = 0;
    float factorPerSecond = 1; // how much does transition factor change per second
    ease::FuncPtr easeFunction = ease::linear;
};

void animate(RestrictedEntityAll<ModelInstance, CurrentAnimation> entity, float deltatime);
void calculateBoneMatrices(std::vector<glm::mat4> &boneMatrices, Model::Skeleton const &skeleton, float normalizedTime, Animation const &animation,
    Animation const *secondAnimation = nullptr, AnimationTransition transition = { });

// TODO: skeleton retargeting

Animation const *findAnimation(Model const &model, std::string_view name);

} // namespace egn
