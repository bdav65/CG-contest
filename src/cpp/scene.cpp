#include "scene.hpp"

Scene::Scene(const std::filesystem::path& path) {
    std::ifstream inputFile(path);
    if (!inputFile)
        throw std::runtime_error("Could not open input file for reading.");

    Json jsonScene;
    inputFile >> jsonScene;

    this->texture = path.parent_path() / jsonScene["Texture"];
    loadCameraInformation(jsonScene);
    loadObjects(jsonScene);
    loadLights(jsonScene);
}

std::tuple<std::vector<Object>, std::vector<Light>> Scene::sceneData() {
    return std::make_tuple(objects, lights);
}

void Scene::loadCameraInformation(Json const &root) {
    Json const &eye = root["Eye"];
    this->cameraLocation = glm::vec3(eye[0], eye[1], eye[2]);
    this->shadows = root["Shadows"];
    this->bounces = root["MaxBounces"];
}

void Scene::loadObjects(Json const &root) {
    Json const &jsonObjects = root["Objects"];

    for (Json const &obj : jsonObjects) {
        if (obj["type"] == "sphere") {
            Material material = getMaterial(obj["material"]);
            glm::vec3 position(obj["position"][0], obj["position"][1], obj["position"][2]);
            float radius = obj["radius"];

            // Rotation is optional
            glm::vec3 rotationAxis = obj.contains("rotation")
                ? glm::vec3(obj["rotation"][0], obj["rotation"][1], obj["rotation"][2])
                : glm::vec3(0, 0, 1);

            float rotationAngle = obj.contains("angle") ? static_cast<float>(obj["angle"]) : 0.0;

            constexpr glm::vec3 zero {};
            objects.push_back(Object { material, 0, position, radius, rotationAxis, rotationAngle, zero, zero, zero });

        } else if (obj["type"] == "triangle") {
            Material material = getMaterial(obj["material"]);
            glm::vec3 vertices[] {
                glm::vec3(obj["v0"][0], obj["v0"][1], obj["v0"][2]),
                glm::vec3(obj["v1"][0], obj["v1"][1], obj["v1"][2]),
                glm::vec3(obj["v2"][0], obj["v2"][1], obj["v2"][2])
            };

            constexpr glm::vec3 zero {};
            objects.push_back(Object { material, 1, zero, 0, zero, 0.0, vertices[0],  vertices[1], vertices[2] });

        } else if (obj["type"] == "cylinder") {
            Material material = getMaterial(obj["material"]);
            glm::vec3 position(obj["position"][0], obj["position"][1], obj["position"][2]);
            glm::vec3 direction(obj["direction"][0], obj["direction"][1], obj["direction"][2]);
            float radius = obj["radius"];

            constexpr glm::vec3 zero {};
            objects.push_back(Object { material, 2, zero, radius, zero, 0.0, zero, zero, zero, position, direction });

        } else throw std::runtime_error("Encountered incompatible shape! Try running another scene");
    }
}

void Scene::loadLights(Json const &root) {
    Json const &jsonLights = root["Lights"];

    for (Json const &light : jsonLights) {
        glm::vec3 position(light["position"][0], light["position"][1], light["position"][2]);
        glm::vec3 color(light["color"][0], light["color"][1], light["color"][2]);

        lights.push_back(Light { position, color });
    }
}

/* Returns the Material object from the current material node */
Material getMaterial(Json const &mat) {
    int hasTexture = mat.contains("hasTexture") && mat["hasTexture"];
    int isGlass = mat.contains("isTransparent") && mat["isTransparent"];
    float nt = isGlass ? static_cast<float>(mat["nt"]) : 0.0;

    return Material {
        glm::vec3(mat["color"][0], mat["color"][1], mat["color"][2]),
        mat["ka"],
        mat["kd"],
        mat["ks"],
        mat["n"],
        hasTexture,
        isGlass,
        nt
    };
}
