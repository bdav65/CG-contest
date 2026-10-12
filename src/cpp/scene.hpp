#pragma once

#include <filesystem>
#include <numeric>
#include <fstream>
#include <cmath>
#include <iostream>
#include <glm/glm.hpp>

#include "json/json.hpp"

using Json = nlohmann::json;

struct Material {
    glm::vec3 color;
    float ka;
    float kd;
    float ks;
    uint n;
    int hasTexture;
    int isGlass;
    float nt;
};

struct Object {
    Material material;
    alignas(16) uint shapeType; // true = sphere; false = triangle

    // these are used if it's a sphere
    alignas(16) glm::vec3 center;
    float radius; // this one is also the cylinder's radius
    alignas(16) glm::vec3 rotationAxis;
    float rotationAngle;

    // these are used if it's a triangle
    alignas(16) glm::vec3 v0;
    alignas(16) glm::vec3 v1;
    alignas(16) glm::vec3 v2;

    // these are used if it's a cylinder
    alignas(16) glm::vec3 base;
    alignas(16) glm::vec3 direction;
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
    std::vector<Object> objects;

   public:
    explicit Scene(const std::filesystem::path &path);
    glm::vec3 cameraOrigin() { return cameraLocation; }
    bool renderShadows() const { return shadows; }
    int maxBounces() const { return bounces; }
    std::filesystem::path texturePath() const { return texture; }
    std::tuple<std::vector<Object>, std::vector<Light>> sceneData();

   private:
    void loadCameraInformation(Json const &root);
    void loadObjects(Json const &root);
    void loadLights(Json const &root);
};

// Helper function
Material getMaterial(Json const &materialNode);
