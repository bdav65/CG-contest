#include "kdify.hpp"

#include <queue>

#define DEBUG

#ifdef DEBUG
#include <iostream>


static std::ostream &operator<<(std::ostream &os, const glm::vec3 &v) {
    return os << '[' << v.x << ", " << v.y << ", " << v.z << ']';
}


static std::ostream &operator<<(std::ostream &os, const Photon &p) {
    return os << "Photon {" << p.color << ", " << p.incident << ", " << p.location << ", " << p.axis << '}';
}

#endif


struct Node {
    int i, l, r;
};


uint nearest_photon(Photon *photons, int n, glm::vec3 location) {
    uint previous = 0;
    uint current = 0;

    uint best_photon = 0;
    float best_distance = 1.0 / 0.0;

    do {
        // std::cout << "at photon " << current << '\n';
        Photon p = photons[current];

        // update best distance
        float d = glm::distance(location, p.location);

        if (d < best_distance) {
            best_photon = current;
            best_distance = d;
        }

        uint good, bad;
        uint parent = current > 0 ? (current - 1) / 2 : 0;

        float axis_distance = location[p.axis] - p.location[p.axis];

        if (axis_distance < 0) {
            good = 2 * current + 1;
            bad  = 2 * current + 2;
        } else {
            good = 2 * current + 2;
            bad  = 2 * current + 1;
        }
        
        if (good < n && previous <= current) { // came from parent (or started from root)
            previous = current;
            current = good;
        } else if (previous == good) { // came from "good" subtree
            if (bad < n && std::abs(axis_distance) < best_distance) { // there might be a better point on the "bad" side
                previous = current;
                current = bad;
            } else {
                previous = current;
                current = parent;
            }
        } else { // came from "bad" subtree
            previous = current;
            current = parent;
        }
    } while (previous != current);

    return best_photon;
}


// https://stackoverflow.com/questions/5289613/generate-random-float-between-two-floats
float randf(float a, float b) {
    float random = ((float) rand()) / (float) RAND_MAX;
    float diff = b - a;
    float r = random * diff;
    return a + r;
}


static void verify_nearest(int samples, Photon *data, int n) {
    glm::vec3 minimum {INFINITY, INFINITY, INFINITY};
    glm::vec3 maximum {-INFINITY, -INFINITY, -INFINITY};

    for (int i = 0; i < n; i++) {
        minimum = glm::min(minimum, data[i].location);
        maximum = glm::max(maximum, data[i].location);
    }

    minimum *= 1.2;
    maximum *= 1.2;

    for (int i = 0; i < samples; i++) {
        glm::vec3 test {
            randf(minimum.x, maximum.x),
            randf(minimum.y, maximum.y),
            randf(minimum.z, maximum.z)
        };

        // std::cout << "Testing with location " << test << '\n';

        // uint naive = 0;
        // float naive_distance = INFINITY;

        // for (int j = 0; j < n; j++) {
        //     float distance = glm::distance(test, data[j].location);

        //     if (distance < naive_distance) {
        //         naive_distance = distance;
        //         naive = j;
        //     }
        // }

        // std::cout << "\tNaive gave photon " << naive << '\n';

        uint kd = nearest_photon(data, n, test);

        // std::cout << "\tK-NN gave photon " << kd << '\n';

        assert(kd + 1 > 0);
    }
}


static std::pair<glm::vec3, glm::vec3> verify_kd_tree_rec(int i, Photon *data, int n) {
    int l = i * 2 + 1;
    int r = i * 2 + 2;

    Photon p = data[i];

    glm::vec3 minimum = p.location;
    glm::vec3 maximum = p.location;

    if (l < n) {
        auto [lmin, lmax] = verify_kd_tree_rec(l, data, n);

        if (lmax[p.axis] > p.location[p.axis] + 0.01) {
            std::cout << p << " has invalid left child " << l << ' ' << data[l] << " with bounds " << lmin << ", " << lmax << std::endl;
            assert(lmax[p.axis] < p.location[p.axis]);
        }

        minimum = glm::min(minimum, lmin);
        maximum = glm::max(maximum, lmax);
    }

    if (r < n) {
        auto [rmin, rmax] = verify_kd_tree_rec(r, data, n);

        if (rmin[p.axis] < p.location[p.axis] - 0.01) {
            std::cout << p << " has invalid right child " << r << ' ' << data[r] << " with bounds " << rmin << ", " << rmax << std::endl;
            assert(rmin[p.axis] > p.location[p.axis]);
        }


        minimum = glm::min(minimum, rmin);
        maximum = glm::max(maximum, rmax);
    }

    return {minimum, maximum};
}


static void verify_kd_tree(Photon *data, int n) {
    verify_kd_tree_rec(0, data, n);
}


int kdifyRecursive(std::vector<Node> &nodes, int l, int r, Photon *data) {
    // std::cout << "Call " << l << ", " << r << '\n';

    if (r <= l) {
        return -1;
    }

    if (r - l <= 1) {
        return l;
    }

    int n = r - l - 1;
    int layer = 1;

    int ln = 0;

    while (n >= 2 * layer) {
        ln += layer;
        layer *= 2;
        n -= layer;
    }

    ln += std::min(n, layer);

    int pivot = l + ln;

    // std::cout << "Left receives " << ln << " items, right " << r - l - ln - 1 << '\n';

    // Find bounding coordinates
    glm::vec3 minimum {INFINITY, INFINITY, INFINITY};
    glm::vec3 maximum {-INFINITY, -INFINITY, -INFINITY};

    for (int i = l; i < r; i++) {
        minimum = glm::min(minimum, data[nodes[i].i].location);
        maximum = glm::max(maximum, data[nodes[i].i].location);
    }

    // std::cout << "Minimum " << minimum << " and maximum " << maximum << '\n';

    glm::vec3 delta = maximum - minimum;
    float dim = glm::max(delta.x, glm::max(delta.y, delta.z));

    // Sort in dimension with largest delta
    for (int d = 0; d < 3; d++) {
        if (dim != delta[d]) {
            continue;
        }

        // std::cout << "Splitting on axis " << d << std::endl;

        std::nth_element(nodes.begin() + l, nodes.begin() + pivot, nodes.begin() + r, [&](Node const &n1, Node const &n2){
            return data[n1.i].location[d] < data[n2.i].location[d];
        });

        float pivot_value = data[nodes[pivot].i].location[d];

        for (int li = l, ri = r - 1; li < pivot; li++) {
            float left  = data[nodes[li].i].location[d];
            float right = data[nodes[ri].i].location[d];

            if (left < pivot_value) {
                continue;
            }

            if (right < pivot_value) {
                std::swap(nodes[li], nodes[ri]);
                ri--;
            }
        }

        // std::sort(nodes.begin() + l, nodes.begin() + r, [&](Node const &n1, Node const &n2){
        //     return data[n1.i].location[d] < data[n2.i].location[d];
        // });

        data[nodes[pivot].i].axis = d;

        break;
    }

    // recurse to find left and right children
    nodes[pivot].l = kdifyRecursive(nodes, l, pivot, data);
    nodes[pivot].r = kdifyRecursive(nodes, pivot + 1, r, data);

    // std::cout << "\tNode " << data[nodes[pivot].i] << " has children " << nodes[pivot].l << ' ' << data[nodes[nodes[pivot].l].i].location << " and " << nodes[pivot].r << ' ' << data[nodes[nodes[pivot].r].i].location << '\n';

    return pivot;
}


void kdify(Photon *data, int n) {
    std::vector<Node> nodes;
    nodes.reserve(n);

    for (int i = 0; i < n; i++) {
        nodes.push_back({i, -1, -1});
    }

    int root = kdifyRecursive(nodes, 0, nodes.size(), data);

    // construct indices of kd heap with bfs
    std::queue<int> waiting;
    std::vector<int> indices;

    indices.resize(n);
    waiting.push(root);

    for (int i = 0; !waiting.empty(); i++) {
        Node current = nodes[waiting.front()];
        waiting.pop();

        // std::cout << "setting " << current.i << " -> " << i << std::endl;

        indices[current.i] = i;

        if (current.l >= 0) {
            waiting.push(current.l);
        }

        if (current.r >= 0) {
            waiting.push(current.r);
        }
    }

    // for (int i = 0; i < n; i++) {
    //     std::cout << indices[i] << ' ';
    // }

    // std::cout << '\n';

    // do the actual shuffling
    for (int i = 0; i < n;) {
        if (i == indices[i]) {
            i++;
        } else {
            std::swap(data[i], data[indices[i]]);
            std::swap(nodes[i], nodes[indices[i]]);
            std::swap(indices[i], indices[indices[i]]);
        }
    }

    verify_kd_tree(data, n);
    // verify_nearest(1000, data, n);
}


// void kdify(Photon *data, int n) {
//     std::vector<Photon> tags, indices;

//     tags.resize(n);
//     indices.resize(n);

//     for (int i = 0; i < n; i++) {
//         indices[i].location.x = i;
//     }

//     for (int l = 0; l <= ceil(log2(n)); l++) {
//         int dimension = l % 3;

//         for (int i = 0; i < n; i++) {
//             indices[i].location.x += i * l - dimension;
//             indices[i].location.y *= i - l - dimension;
//             indices[i].location.z *= i * l - dimension;
//         }

//         std::sort(indices.begin(), indices.end(), [&](Photon const &n1, Photon const &n2){
//             return n1.location[dimension] < n2.location[dimension];
//         });
//     }
// }
