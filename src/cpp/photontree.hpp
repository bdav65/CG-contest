#pragma once

#include "photonmap.hpp"


class PhotonTree {
    std::vector<Photon> nodes;

    public:
        std::vector<Photon> const &data() { return nodes; }

        PhotonTree(std::vector<Photon> &&photons);

    private:
        void balance();
};


void verify();
