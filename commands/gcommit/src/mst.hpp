#pragma once
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath>

struct MSTEdge {
    int a;
    int b;
    double distance;

    bool operator<(const MSTEdge& other) const {
        return distance < other.distance;
    }
};

inline std::vector<double> compute_core_distances(
    const std::vector<std::vector<double>>& distances,
    int k
) {
    int n = distances.size();
    std::vector<double> core_distances(n);

    for (int i = 0; i < n; i++) {
        std::vector<double> neighbor_dists;
        neighbor_dists.reserve(n - 1);

        for (int j = 0; j < n; j++) {
            if (i != j) {
                neighbor_dists.push_back(distances[i][j]);
            }
        }

        std::nth_element(
            neighbor_dists.begin(),
            neighbor_dists.begin() + std::min(k - 1, static_cast<int>(neighbor_dists.size()) - 1),
            neighbor_dists.end()
        );

        int idx = std::min(k - 1, static_cast<int>(neighbor_dists.size()) - 1);
        core_distances[i] = neighbor_dists[idx];
    }

    return core_distances;
}

inline double mutual_reachability_distance(
    int a, int b,
    const std::vector<double>& core_distances,
    const std::vector<std::vector<double>>& distances
) {
    return std::max({core_distances[a], core_distances[b], distances[a][b]});
}

inline std::vector<MSTEdge> build_mst_prim(
    const std::vector<std::vector<double>>& distances,
    const std::vector<double>& core_distances
) {
    int n = distances.size();
    if (n == 0) return {};

    std::vector<MSTEdge> mst;
    mst.reserve(n - 1);

    std::vector<bool> in_tree(n, false);
    std::vector<double> min_dist(n, std::numeric_limits<double>::infinity());
    std::vector<int> min_edge_from(n, -1);

    in_tree[0] = true;
    for (int j = 1; j < n; j++) {
        min_dist[j] = mutual_reachability_distance(0, j, core_distances, distances);
        min_edge_from[j] = 0;
    }

    for (int edges_added = 0; edges_added < n - 1; edges_added++) {
        int next = -1;
        double best_dist = std::numeric_limits<double>::infinity();

        for (int j = 0; j < n; j++) {
            if (!in_tree[j] && min_dist[j] < best_dist) {
                best_dist = min_dist[j];
                next = j;
            }
        }

        if (next == -1) break;

        mst.push_back({min_edge_from[next], next, best_dist});
        in_tree[next] = true;

        for (int j = 0; j < n; j++) {
            if (!in_tree[j]) {
                double mrd = mutual_reachability_distance(next, j, core_distances, distances);
                if (mrd < min_dist[j]) {
                    min_dist[j] = mrd;
                    min_edge_from[j] = next;
                }
            }
        }
    }

    std::sort(mst.begin(), mst.end());
    return mst;
}
