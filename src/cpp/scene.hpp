#pragma once

#include <filesystem>
#include <glm/glm.hpp>

#include "json/json.hpp"

using Json = nlohmann::json;


struct Material {
    alignas(16) glm::vec3 color;
    float roughness;
    float IOR;
    float transparency;
};


struct Object {
    Material material;
};


struct Sphere {
    uint obj;
    float radius;
    alignas(16) glm::vec3 center;
};


struct Triangle {
    uint obj;
    alignas(16) glm::vec3 v0;
    alignas(16) glm::vec3 v1;
    alignas(16) glm::vec3 v2;
    alignas(16) glm::vec3 normal;
};


struct Light {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 color;
};


struct CausticsConfig {
    uint photonCount;
    uint maxBounces;
    uint maxTries;
    uint photonsPerLight;
};


struct RaytracerConfig {
    glm::vec3 cameraOrigin;
    float focalLength;
    glm::uvec2 resolution;
    int subpixelsPerAxis;
    int _;
    int maxBounces;
};


/**
 * Class that represents and prepares the Scene from a file.
 */
class Scene {
    CausticsConfig causticsConfig;
    RaytracerConfig raytracerConfig;

    std::filesystem::path texture;
    std::vector<Light> lights;
    std::vector<Sphere> spheres;
    std::vector<Triangle> triangles;
    std::vector<Object> objects;

    public:
        explicit Scene(const std::filesystem::path &path);

        RaytracerConfig getRaytracerConfig() const { return raytracerConfig; }
        CausticsConfig getCausticsConfig();

        std::vector<Object> const &getObjects() const { return objects; }
        std::vector<Sphere> const &getSpheres() const { return spheres; }
        std::vector<Triangle> const &getTriangles() const { return triangles; }
        std::vector<Light> const &getLights() const { return lights; }

    private:
        void loadCausticsConfig(Json const &root);
        void loadRaytracerConfig(Json const &root);
        void loadObjects(Json const &root);
        void loadLights(Json const &root);
        void loadModel(uint objectIndex, Json const &root);
};
