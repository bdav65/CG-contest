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


struct Camera {
    glm::vec3 origin;
    float focalLength;
    glm::uvec2 resolution;
    int subpixelsPerAxis;
    int renderShadows;
    int maxBounces;
};

/**
 * Class that represents and prepares the Scene from a file.
 */
class Scene {
    glm::vec3 cameraLocation;
    bool shadows;
    int bounces;
    std::filesystem::path texture;
    std::vector<Light> lights;
    std::vector<Sphere> spheres;
    std::vector<Triangle> triangles;
    std::vector<Object> objects;

   public:
    explicit Scene(const std::filesystem::path &path);
    glm::vec3 cameraOrigin() { return cameraLocation; }
    bool renderShadows() const { return shadows; }
    int maxBounces() const { return bounces; }
    std::filesystem::path texturePath() const { return texture; }
    std::tuple<std::vector<Object>, std::vector<Light>> sceneData();

    std::vector<Object> const &getObjects() const { return objects; }
    std::vector<Sphere> const &getSpheres() const { return spheres; }
    std::vector<Triangle> const &getTriangles() const { return triangles; }
    std::vector<Light> const &getLights() const { return lights; }

   private:
    void loadCameraInformation(Json const &root);
    void loadObjects(Json const &root);
    void loadLights(Json const &root);
};

// Helper function
Material getMaterial(Json const &materialNode);
