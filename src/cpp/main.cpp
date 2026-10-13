#include "main.hpp"


int main(int argc, char *argv[]) {
    std::cout << "Computer Graphics - Ray tracer\n\n";

    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " scene-file [--headless]\n";
        return EXIT_FAILURE;
    }

    if (argc == 3 && !(strcmp(argv[1], "--headless") || strcmp(argv[2], "--headless"))) {
        std::cerr << "The only supported flag is --headless\n";
        return EXIT_FAILURE;  
    }

    Scene scene{argv[1]};
    bool headless = argc == 3;

    VulkanHelper vulkanHelper {headless};

    // create the photon map for the scene
    PhotonMap photonMap {scene, vulkanHelper};
    auto x = photonMap.compute();

    // pass the scene and args into the raytracer
    // RayTracer raytracer{scene, headless};

    // raytracer.run();

    return EXIT_SUCCESS;
}
