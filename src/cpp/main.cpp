#include "main.hpp"


int main(int argc, char *argv[]) {
    std::cout << "Computer Graphics - Ray tracer\n\n";

    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " scene-file [--not-headless]\n";
        return EXIT_FAILURE;
    }

    if (argc == 3 && (strcmp(argv[1], "--not-headless") && strcmp(argv[2], "--not-headless"))) {
        std::cerr << "The only supported flag is --not-headless\n";
        return EXIT_FAILURE;
    }

    Scene scene{argv[1]};
    VulkanHelper helper(!(argc == 3));

    // create the photon map for the scene
    StagingBuffer photons = [&]{
        PhotonMap photonMap {scene, helper};
        return photonMap.compute();
    }();

    // pass the scene and args into the raytracer
    RayTracer raytracer{scene, helper, photons};
    raytracer.run();

    return EXIT_SUCCESS;
}
