#include "kdify.hpp"

#include <queue>


struct Node {
    int i, l, r;
};


static int kdifyRecursive(std::vector<Node> &nodes, int l, int r, Photon *data) {
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

    // Find bounding coordinates
    glm::vec3 minimum {INFINITY, INFINITY, INFINITY};
    glm::vec3 maximum {-INFINITY, -INFINITY, -INFINITY};

    for (int i = l; i < r; i++) {
        minimum = glm::min(minimum, data[nodes[i].i].location);
        maximum = glm::max(maximum, data[nodes[i].i].location);
    }

    glm::vec3 delta = maximum - minimum;
    float dim = glm::max(delta.x, glm::max(delta.y, delta.z));

    // Sort in dimension with largest delta
    for (int d = 0; d < 3; d++) {
        if (dim != delta[d]) {
            continue;
        }

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

        data[nodes[pivot].i].axis = d;

        break;
    }

    // recurse to find left and right children
    nodes[pivot].l = kdifyRecursive(nodes, l, pivot, data);
    nodes[pivot].r = kdifyRecursive(nodes, pivot + 1, r, data);

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

        indices[current.i] = i;

        if (current.l >= 0) {
            waiting.push(current.l);
        }

        if (current.r >= 0) {
            waiting.push(current.r);
        }
    }

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
}
