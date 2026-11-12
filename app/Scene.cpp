#include "Scene.hpp"
#include "Controller.hpp"
#include "ModelUtils.hpp"

#include "engine/renderer/EngineRenderer.hpp"
#include "engine/renderer/ResourceManager.hpp"
#include "engine/scene/Animation.hpp"
#include "engine/scene/Resources.hpp"

#include <random>

struct CycleAnimations { };

static egn::Transform lookat(glm::vec3 pos, glm::vec3 center) {
    auto dir = glm::normalize(center - pos);
    auto up = glm::abs(glm::dot(dir, { 0, 1, 0 })) > 0.999 ? glm::vec3{ 1, 0, 0 } : glm::vec3{ 0, 1, 0 };
    return { .position = pos, .orientation = glm::normalize(glm::quatLookAt(dir, up)) };
}
static egn::Transform lookdir(glm::vec3 pos, glm::vec3 dir) {
    auto up = glm::abs(glm::dot(dir, { 0, 1, 0 })) > 0.999 ? glm::vec3{ 1, 0, 0 } : glm::vec3{ 0, 1, 0 };
    return { .position = pos, .orientation = glm::normalize(glm::quatLookAt(dir, up)) };
}
static void assetGrid(egn::Registry &reg, std::vector<egn::Entity> const &props, glm::uvec3 dimensions, glm::vec3 offset, float distance) {
    assert(!props.empty());
    static std::random_device dev;
    static std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> distP(0, props.size() - 1);
    std::uniform_int_distribution<std::mt19937::result_type> distX(0, dimensions.x - 1);
    std::uniform_int_distribution<std::mt19937::result_type> distY(0, dimensions.y - 1);
    std::uniform_int_distribution<std::mt19937::result_type> distZ(0, dimensions.z - 1);
    auto position = [&](float x, float y, float z) -> glm::vec3 { return (glm::vec3(x, y, z) - glm::vec3(dimensions) * 0.5f) * distance + offset; };

    for(uint x = 0; x < dimensions.x; ++x)
        for(uint y = 0; y < dimensions.y; ++y)
            for(uint z = 0; z < dimensions.z; ++z) {
                // x+y*dimensions.x+z*dimensions.x*dimensions.y;
                reg.create(egn::ModelInstance{ props[distP(rng)] }, lookat(position(x, y, z), position(distX(rng), distY(rng), distZ(rng))));
            }
}

void scene::TestScene::setupRenderer(egn::Registry &reg, egn::IRenderer **renderer, egn::vk::Device const &device) {
    mRenderer = std::unique_ptr<egn::IRenderer>(new egn::EngineRenderer(reg, device));
    static_cast<egn::EngineRenderer *>(mRenderer.get())->setCamera(mCamera);
    *renderer = mRenderer.get();
    mDevice = device;
}
void scene::TestScene::setup(egn::Registry &reg) {
    const auto prototypeTextures = egn::Material::Textures{ .albedo = loadTexture(reg, "assets/textures/prototype/texture_03.png", false) };
    std::vector<egn::Entity> props;
    loadModelTo(reg, "assets/models/suzanne.glb", props, prototypeTextures);
    loadModelTo(reg, "assets/models/cube.glb", props, prototypeTextures);
    loadModelTo(reg, "assets/models/sphere.glb", props, prototypeTextures);
    loadModelTo(reg, "assets/models/teapot.glb", props, prototypeTextures);

    const glm::uvec3 numProps = { 20, 3, 10 };
    const float distance = 2;
    const glm::vec3 offset = { 0, -1, -10 };
    assetGrid(reg, props, numProps, offset, distance);

    const glm::vec3 startPos(0, -4, 0);
    const glm::vec3 step(2, 0, 0);
    for(uint i = 0; i < props.size(); ++i) {
        reg.create(egn::ModelInstance{ props[i] }, egn::Transform{ .position = startPos + static_cast<float>(i) * step });
    }

    const auto cubes = loadModel(reg, "assets/models/deccer_cubes/SM_Deccer_Cubes_Textured_Complex.gltf", { });
    if(cubes) {
        reg.create(egn::ModelInstance{ cubes }, egn::Transform{ .position = { -10, 0, 10 } });
    }

    auto fox = loadModel(reg, "assets/models/fox.glb");
    fox.emplace<egn::Transform>() = {
        .orientation = glm::quatLookAt(glm::vec3(0, 0, 1), glm::vec3(0, 1, 0)),
        .scale = glm::vec3(0.014f),
    };

    reg.create(egn::ModelInstance(fox), CycleAnimations(), lookdir({ -9.8f, -2.4f, 12.9f }, { 0.8, 0.0f, 0.6 }));

    reg.create(egn::ModelInstance(loadModel(reg, "assets/models/doohickey.glb")), CycleAnimations(), lookdir({ 8, -2, 4 }, { 0, 0, -1 }));

    dumpModels(reg, "tmp/models");

    assert(reg.view<MainWindow>().smallSize() == 1);
    auto window = *reg.view<MainWindow>().begin();
    mCamera = Controller::createCamera(window, { 0, 2, 4 }, { 0, 0, 0 });
    mEventListener = reg.create<egn::WindowEventListener>();
    mEventListener.get<egn::WindowEventListener>().window = window;
}
void scene::TestScene::updateHotkeys(egn::Registry &reg) {
    auto &listener = mEventListener.get<egn::WindowEventListener>();
    while(!listener.keyEvents.empty()) {
        auto event = listener.keyEvents.front();
        listener.keyEvents.pop();

        if(event.key == GLFW_KEY_R && event.action == GLFW_PRESS) {
            CHECK_VK_RES(vkDeviceWaitIdle(mDevice.device));
            for(auto e : reg.view<egn::vk::Pipeline, egn::vk::Shader>()) {
                auto &pipeline = e.get<egn::vk::Pipeline>();
                auto &shader = e.get<egn::vk::Shader>();

                auto newShader = egn::vk::makeShader(shader.createInfo);
                if(!newShader.valid)
                    continue;

                egn::vk::destroy(shader);
                shader = newShader;
                egn::vk::destroy(pipeline);
                switch(pipeline.type) {
                    case egn::vk::Pipeline::Type::Graphics:
                        pipeline = egn::vk::makePipeline(shader, pipeline.createInfo.layout, pipeline.createInfo.graphics);
                        break;

                    case egn::vk::Pipeline::Type::Compute:
                        pipeline = egn::vk::makePipeline(shader, pipeline.createInfo.layout, pipeline.createInfo.compute);
                        break;

                    case egn::vk::Pipeline::Type::RayTracing:
                        pipeline = egn::vk::makePipeline(shader, pipeline.createInfo.layout, pipeline.createInfo.raytracing);
                        break;

                    default: assert(false && "unknown pipeline");
                }
                egn::vk::allocateDescriptors(pipeline);

                e.emplace<egn::ResourceDirty>();
            }
        }
        if(event.key == GLFW_KEY_Z && event.action == GLFW_PRESS) {
            for(auto e : reg.view<PositionedCamera>()) {
                auto &pos = e.get<PositionedCamera>();
                LOG_INFO("Camera {} - position = {}, orientation = {}, forward = {}",
                    e,
                    fmt::streamed(pos.position),
                    fmt::streamed(pos.orientation),
                    fmt::streamed(glm::vec3(glm::mat4_cast(pos.orientation) * glm::vec4(0, 0, -1, 0))));
            }
        }
    }
}
void scene::TestScene::cycleAnimations(egn::Registry &reg) {
    static std::random_device dev;
    static std::mt19937 rng(dev());

    for(auto eInstance : reg.view<CycleAnimations, egn::ModelInstance>()) {
        auto const &model = eInstance.get<egn::ModelInstance>().eModel.getc();
        if(!model.animations.empty()) {
            eInstance.try_emplace<egn::CurrentAnimation>(model.animations.front().name);
        }

        if(glm::mod<float>(mRuntime, 5) < 0.01 && !eInstance.contains<egn::AnimationTransition>()) {
            if(model.animations.size() <= 1)
                continue;

            std::uniform_int_distribution<> distrib(0, model.animations.size() - 1);

            std::string newAnimation = model.animations.at(distrib(rng)).name;
            auto const &newAnim = *std::find_if(model.animations.begin(), model.animations.end(), [&](egn::Animation const &animation) {
                return animation.name == newAnimation;
            });
            float duration = 0.5 * newAnim.durationTicks / newAnim.ticksPerSecond;

            LOG_INFO("Transition from {} to {} in {:.2f}s",
                eInstance.contains<egn::CurrentAnimation>() ? eInstance.get<egn::CurrentAnimation>().name : "unknown",
                newAnimation,
                duration);

            eInstance.emplace<egn::AnimationTransition>(egn::AnimationTransition{
                .to = newAnimation,
                .factorPerSecond = 1 / duration,
                .easeFunction = egn::ease::inOutCubic,
            });
        }
    }
}
void scene::TestScene::update(egn::Registry &reg, float deltatime) {
    mRuntime += deltatime;

    updateHotkeys(reg);
    cycleAnimations(reg);
    mController.update(reg, deltatime);
    PositionedCamera::update(reg);

    for(auto e : reg.view<egn::ModelInstance, egn::CurrentAnimation>()) {
        animate(e, deltatime);
    }
}
