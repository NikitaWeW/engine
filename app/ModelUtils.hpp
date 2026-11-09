#pragma once
#include <string>
#include <unordered_set>
#include <fstream>
#include <filesystem>
#include "engine/scene/Loaders.hpp"
#include "engine/scene/Resources.hpp"

namespace scene {

inline std::unordered_set<std::string> getSmoothShadedModelPaths() {
    std::ifstream smoothShadeFile("assets/models/smoothShade.txt");
    if(!smoothShadeFile.is_open()) {
        return { };
    }

    std::unordered_set<std::string> res;
    std::string line;
    while(std::getline(smoothShadeFile, line)) {
        if(!line.empty()) {
            res.emplace(std::move(line));
        }
    }

    LOG_TRACE("Smoothly shading {}", res);
    return res;
}
inline std::string printTexture(egn::Entity e) {
    if(!e.valid() || !e.contains<egn::Texture2D>())
        return fmt::format("{}", e);
    auto const &texture2D = e.get<egn::Texture2D>();
    return fmt::format("{}, \"{:<30} {}x{}, {:>3} {} mips",
        e,
        texture2D.path + "\",",
        texture2D.bitmap.size.x,
        texture2D.bitmap.size.y,
        (texture2D.srgb ? "srgb" : "not srgb"),
        texture2D.numMipLevels);
}
[[maybe_unused]] [[nodiscard]] inline std::string printModelData(egn::Entity e) {
    assert(e.valid() && e.contains<egn::Model>());
    egn::Model const &model = e.get<egn::Model>();
    std::stringstream ss;
    ss << "Model: " << e << ": \"" << model.path << "\"\n";
    ss << "Skeleton: \n";
    ss << "  Bone map size / number of bones: " << model.skeleton.boneMap.size() << "\n";
    if(model.skeleton.boneMap.size() <= 30)
        for(auto const &[name, id] : model.skeleton.boneMap)
            ss << "    [\"" << name << "\": " << id << "]\n";

    ss << "Animations: " << model.animations.size() << "\n";
    for(auto const &animation : model.animations) {
        ss << "-----------------\n";
        ss << "Animation: \"" << animation.name << "\"\n";
        ss << "  Duration: " << animation.durationTicks << " ticks, tps: " << animation.ticksPerSecond << "\n";
        ss << "  Number of bones: " << animation.bones.size() << "\n";
    }

    ss << "Meshes: " << model.meshes.size() << "\n";
    for(auto const &mesh : model.meshes) {
        ss << "-----------------\n";
        ss << "Geometry:\n";
        ss << "  Triangles:   " << mesh.geometry.indices.size() / 3 << "\n";
        ss << "  Indices:     " << mesh.geometry.indices.size() << "\n";
        ss << "  Positions:   " << mesh.geometry.positions.size() << "\n";
        ss << "  Attributes:  " << mesh.geometry.attributes.size() << "\n";
        ss << "Material:\n";
        ss << "  Textures:\n";
        ss << "    Albedo:       " << printTexture(egn::Entity{ &e.reg(), mesh.material.textures.albedo }) << "\n";
        ss << "    Metallic:     " << printTexture(egn::Entity{ &e.reg(), mesh.material.textures.metallic }) << "\n";
        ss << "    Roughness:    " << printTexture(egn::Entity{ &e.reg(), mesh.material.textures.roughness }) << "\n";
        ss << "    Ambient:      " << printTexture(egn::Entity{ &e.reg(), mesh.material.textures.ambient }) << "\n";
        ss << "    Normal:       " << printTexture(egn::Entity{ &e.reg(), mesh.material.textures.normal }) << "\n";
        ss << "    Displacement: " << printTexture(egn::Entity{ &e.reg(), mesh.material.textures.displacement }) << "\n";
        ss << "  Properties:\n";
        ss << "    Ambient:      " << mesh.material.properties.ambient << "\n";
        ss << "    Albedo:       " << mesh.material.properties.albedo << "\n";
        ss << "    Specular:     " << mesh.material.properties.specular << "\n";
        ss << "    Emission:     " << mesh.material.properties.emission << "\n";
        ss << "    Shininess:    " << mesh.material.properties.shininess << "\n";
        ss << "    Metallic:     " << mesh.material.properties.metallic << "\n";
        ss << "    IOR:          " << mesh.material.properties.ior << "\n";
    }

    return ss.str();
}
inline egn::Entity loadTexture(egn::Registry &reg, std::string_view path, bool required = true, bool flip = false) {
    static egn::TextureLoader loader(reg.getReg());

    egn::TextureLoaderOptions options{
        .flip = flip,
        .desiredChannels = 4,
    };
    auto eTexture = egn::Entity{ &reg, loader.loadFromFile(path, options) };
    if(!required && !eTexture.valid())
        return eTexture;
    if(required && !eTexture.valid()) {
        LOG_ERROR("Failed to load {}, which is required!", path);
        assert(false);
        return eTexture;
    }

    // ...

    return eTexture;
}
static egn::Entity loadModel(egn::Registry &reg, std::string_view path, std::optional<egn::Material::Textures> textures = { }, bool required = true) {
    static egn::ModelLoader loader(reg.getReg());
    static auto smoothPaths = getSmoothShadedModelPaths();

    egn::ModelLoaderOptions options{
        .flipUVs = false,
        .flipHandiness = true, // ??
        .windingOrder = egn::ModelLoaderOptions::WindingOrder::CCW,
        .smoothNormalAngle = static_cast<float>(smoothPaths.contains(std::string(path)) ? 35 : 0),
    };
    auto eModel = egn::Entity{ &reg, loader.loadFromFile(path, options) };
    if(!required && !eModel.valid())
        return eModel;
    if(required && !eModel.valid()) {
        LOG_ERROR("Failed to load {}, which is required!", path);
        assert(false);
        return eModel;
    }
    auto &model = eModel.get<egn::Model>();

    if(textures.has_value()) {
        auto defaultMaterial = loader.getDefaultMaterial();
        if(textures->albedo == egn::INVALID_ENTITY)
            textures->albedo = defaultMaterial.textures.albedo;
        if(textures->metallic == egn::INVALID_ENTITY)
            textures->metallic = defaultMaterial.textures.metallic;
        if(textures->roughness == egn::INVALID_ENTITY)
            textures->roughness = defaultMaterial.textures.roughness;
        if(textures->ambient == egn::INVALID_ENTITY)
            textures->ambient = defaultMaterial.textures.ambient;
        if(textures->normal == egn::INVALID_ENTITY)
            textures->normal = defaultMaterial.textures.normal;
        if(textures->displacement == egn::INVALID_ENTITY)
            textures->displacement = defaultMaterial.textures.displacement;

        for(auto &mesh : model.meshes)
            mesh.material.textures = textures.value();
    }

    return eModel;
}
inline bool
loadModelTo(egn::Registry &reg, std::string_view path, std::vector<egn::Entity> &to, std::optional<egn::Material::Textures> textures = { }) {
    auto e = loadModel(reg, path, textures, false);
    if(e) {
        to.emplace_back(e);
    }

    return e;
}
inline void dumpModels(egn::Registry &reg, std::string_view dir) {
    std::filesystem::create_directories(dir);
    std::ofstream modelStr(std::string(dir.data()) + "/models.txt");
    for(egn::RestrictedEntity<egn::Model> e : reg.view<egn::Model>()) {
        modelStr << printModelData(e) << "Geometry dumps:\n";
        for(uint i = 0; i < e->meshes.size(); ++i) {
            auto const &mesh = e->meshes[i].geometry;
            auto path = dir.data() + ("/" + std::filesystem::path(e->path).stem().string() + "_") + std::to_string(i) + ".obj";
            modelStr << "  " << path << "\n";
            std::ofstream s(path);
            if(!s) {
                continue;
            }

            for(auto const &position : mesh.positions) {
                s << "v " << position.x << " " << position.y << " " << position.z << "\n";
            }
            s << "\n";
            for(auto const &a : mesh.attributes) {
                s << "n " << a.normal.x << " " << a.normal.y << " " << a.normal.z << "\n";
            }
            s << "\n";
            for(auto const &a : mesh.attributes) {
                s << "t " << a.uv.x << " " << a.uv.y << "\n";
            }
            s << "\n";

            for(uint j = 0; j < mesh.indices.size(); j += 3) {
                s << "f " << mesh.indices[j + 0] + 1 << " " << mesh.indices[j + 1] + 1 << " " << mesh.indices[j + 2] + 1 << "\n";
            }
            s << "\n";
        }
    }
}

}
