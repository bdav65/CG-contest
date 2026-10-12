#include "scene.hpp"

#include <fstream>


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
        Material material = getMaterial(obj["material"]);
        uint object_index = objects.size();
        objects.push_back(Object {material});

        if (obj["type"] == "sphere") {
            glm::vec3 position(obj["position"][0], obj["position"][1], obj["position"][2]);
            float radius = obj["radius"];

            spheres.push_back(Sphere {
                object_index,
                radius,
                position
            });
        } else if (obj["type"] == "triangle") {
            glm::vec3 v0 {obj["v0"][0], obj["v0"][1], obj["v0"][2]};
            glm::vec3 v1 {obj["v1"][0], obj["v1"][1], obj["v1"][2]};
            glm::vec3 v2 {obj["v2"][0], obj["v2"][1], obj["v2"][2]};
            glm::vec3 normal = obj.contains("normal") ? 
                glm::vec3(obj["normal"][0], obj["normal"][1], obj["normal"][2]) :
                glm::normalize(glm::cross(v2 - v0, v1 - v0));

            triangles.push_back(Triangle {
                object_index,
                v0,
                v1,
                v2,
                normal
            });
        } else {
            throw std::runtime_error("Encountered incompatible shape! Try running another scene");
        }
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
    int isGlass = mat.contains("isTransparent") && mat["isTransparent"];
    float nt = isGlass ? static_cast<float>(mat["nt"]) : 0.0;

    return Material {
        glm::vec3(mat["color"][0], mat["color"][1], mat["color"][2]),
        mat["ka"],
        mat["kd"],
        mat["ks"],
        mat["n"],
        isGlass,
        nt
    };
}
