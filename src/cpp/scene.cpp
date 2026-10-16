#include "scene.hpp"

#include "OBJ-Loader/OBJ_Loader.h"

#include <format>
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


void Scene::loadCameraInformation(Json const &root) {
    Json const &eye = root["Eye"];
    this->cameraLocation = glm::vec3(eye[0], eye[1], eye[2]);
    this->shadows = root["Shadows"];
    this->bounces = root["MaxBounces"];
}


void Scene::loadObjects(Json const &root) {
    Json const &jsonObjects = root["Objects"];

    for (Json const &obj : jsonObjects) {
        uint object_index = objects.size();
        objects.push_back({getMaterial(obj["material"])});

        if (obj["type"] == "sphere") {
            glm::vec3 position(obj["position"][0], obj["position"][1], obj["position"][2]);
            float radius = obj["radius"];

            spheres.push_back(Sphere {
                object_index,
                radius,
                position
            });
        }
        
        else if (obj["type"] == "triangle") {
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
        }
        
        else if (obj["type"] == "aabb") {
            glm::vec3 a {obj["v0"][0], obj["v0"][1], obj["v0"][2]};
            glm::vec3 h {obj["v1"][0], obj["v1"][1], obj["v1"][2]};

            glm::vec3 b {h.x, a.y, a.z};
            glm::vec3 c {a.x, h.y, a.z};
            glm::vec3 d {h.x, h.y, a.z};
            glm::vec3 e {a.x, a.y, h.z};
            glm::vec3 f {h.x, a.y, h.z};
            glm::vec3 g {a.x, h.y, h.z};

            triangles.push_back(Triangle {object_index, a, b, c, {0, 0, -1}}); // -z face
            triangles.push_back(Triangle {object_index, b, c, d, {0, 0, -1}});
            triangles.push_back(Triangle {object_index, a, b, e, {0, -1, 0}}); // -y face
            triangles.push_back(Triangle {object_index, b, e, f, {0, -1, 0}});
            triangles.push_back(Triangle {object_index, a, c, e, {-1, 0, 0}}); // -x face
            triangles.push_back(Triangle {object_index, c, e, g, {-1, 0, 0}});
            triangles.push_back(Triangle {object_index, e, f, g, {0, 0, 1}}); // +z face
            triangles.push_back(Triangle {object_index, f, g, h, {0, 0, 1}});
            triangles.push_back(Triangle {object_index, c, d, g, {0, 1, 0}}); // +y face
            triangles.push_back(Triangle {object_index, d, g, h, {0, 1, 0}});
            triangles.push_back(Triangle {object_index, b, d, f, {1, 0, 0}}); // +x face
            triangles.push_back(Triangle {object_index, d, f, h, {1, 0, 0}});
        }

        else if (obj["type"] == "model") {
            loadModel(object_index, obj["file"]);
        }
        
        else {
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
    return Material {
        glm::vec3(mat["color"][0], mat["color"][1], mat["color"][2]),
        mat["roughness"],
        mat["IOR"],
        mat["transparency"]
    };
}


glm::vec3 toGlmVec(objl::Vector3 v) {
    return glm::vec3 {v.X, v.Y, v.Z};
}


void Scene::loadModel(uint objectIndex, std::string file) {
    objl::Loader loader;

    if (!loader.LoadFile(file) || loader.LoadedMeshes.empty()) {
        throw std::runtime_error(std::format("Failed to load model \"{}\"\n", file));
    }

    objl::Mesh mesh = loader.LoadedMeshes.front();
    
    std::cout << "Found mesh " << mesh.MeshName << '\n';

    for (int t = 0; t < mesh.Indices.size(); t += 3) {
        int v0 = mesh.Indices[t];
        int v1 = mesh.Indices[t + 1];
        int v2 = mesh.Indices[t + 2];

        glm::vec3 normal = glm::normalize(toGlmVec(mesh.Vertices[v0].Normal + mesh.Vertices[v1].Normal + mesh.Vertices[v2].Normal));

        std::cout << "Triangle [" << mesh.Vertices[v0].Position.X << ", " << mesh.Vertices[v0].Position.Y << ", " << mesh.Vertices[v0].Position.Z << "] [" << mesh.Vertices[v1].Position.X << ", " << mesh.Vertices[v1].Position.Y << ", " << mesh.Vertices[v1].Position.Z << "] [" << mesh.Vertices[v2].Position.X << ", " << mesh.Vertices[v2].Position.Y << ", " << mesh.Vertices[v2].Position.Z << "]\n";

        triangles.push_back(Triangle {
            objectIndex,
            toGlmVec(mesh.Vertices[v0].Position),
            toGlmVec(mesh.Vertices[v1].Position),
            toGlmVec(mesh.Vertices[v2].Position),
            normal
        });
    }
}
