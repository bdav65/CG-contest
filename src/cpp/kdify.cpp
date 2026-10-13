#include "kdify.hpp"

#include <queue>

#define DEBUG


struct Node {
    int i, l, r;
};


// something to think about
// https://www.sci.utah.edu/~wald/Publications/2022/kdtree/kd-construction-v2.pdf ??


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

    if (n >= layer) {
        ln += layer;
    } else {
        ln += n;
    }

    int pivot = l + ln;

    // std::cout << "Left receives " << ln << " items, right " << rn << '\n';

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
    for (int i = 0; i < 3; i++) {
        if (dim != delta[i]) {
            continue;
        }

        // std::cout << "Splitting on axis " << i << '\n';

        std::sort(nodes.begin() + l, nodes.begin() + r, [&](Node const &n1, Node const &n2){
            return data[n1.i].location[i] < data[n2.i].location[i];
        });

        data[nodes[pivot].i].axis = i;
        break;
    }

    // recurse to find left and right children
    nodes[pivot].l = kdifyRecursive(nodes, l, pivot, data);
    nodes[pivot].r = kdifyRecursive(nodes, pivot + 1, r, data);

    // std::cout << "\n\tNode " << pivot << " has children " << nodes[pivot].l << " and " << nodes[pivot].r << '\n';

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
            std::swap(indices[i], indices[indices[i]]);
        }
    }
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
