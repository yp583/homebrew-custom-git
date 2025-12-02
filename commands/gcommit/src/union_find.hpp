#pragma once
#include <vector>
#include <numeric>

class UnionFind {
private:
    std::vector<int> parent;
    std::vector<int> rank_;

public:
    explicit UnionFind(int n) : parent(n), rank_(n, 0) {
        std::iota(parent.begin(), parent.end(), 0);
    }

    int find(int x) {
        if (parent[x] != x) {
            parent[x] = find(parent[x]);
        }
        return parent[x];
    }

    void unite(int x, int y) {
        int px = find(x);
        int py = find(y);
        if (px == py) return;

        if (rank_[px] < rank_[py]) {
            parent[px] = py;
        } else if (rank_[px] > rank_[py]) {
            parent[py] = px;
        } else {
            parent[py] = px;
            rank_[px]++;
        }
    }

    bool connected(int x, int y) {
        return find(x) == find(y);
    }
};
