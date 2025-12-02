#include "hdbscan.hpp"
#include "union_find.hpp"
#include "utils.hpp"
#include <unordered_map>
#include <algorithm>
#include <cmath>

HDBSCANClustering::HDBSCANClustering(int min_cluster_size, int min_pts)
    : min_cluster_size(min_cluster_size), min_pts(min_pts), num_points(0), default_epsilon(0.0) {}

HDBSCANClustering::~HDBSCANClustering() {}

void HDBSCANClustering::fit(const std::vector<std::vector<float>>& data) {
    clusters.clear();
    labels.clear();
    mst.clear();
    num_points = data.size();

    if (data.empty()) return;

    std::vector<std::vector<double>> distances(num_points, std::vector<double>(num_points, 0.0));
    for (int i = 0; i < num_points; i++) {
        for (int j = i + 1; j < num_points; j++) {
            double dist = 1.0 - cos_sim(
                const_cast<std::vector<float>&>(data[i]),
                const_cast<std::vector<float>&>(data[j])
            );
            distances[i][j] = dist;
            distances[j][i] = dist;
        }
    }

    std::vector<double> core_distances = compute_core_distances(distances, min_pts);
    mst = build_mst_prim(distances, core_distances);

    if (!mst.empty()) {
        double max_dist = mst.back().distance;
        double min_dist = mst.front().distance;
        default_epsilon = min_dist + (max_dist - min_dist) * 0.5;
    }

    clusters = extract_clusters_at_epsilon(default_epsilon);

    labels.resize(num_points, -1);
    for (size_t i = 0; i < clusters.size(); i++) {
        for (int idx : clusters[i]) {
            labels[idx] = static_cast<int>(i);
        }
    }
}

std::vector<std::vector<int>> HDBSCANClustering::extract_clusters_at_epsilon(double epsilon) const {
    if (num_points == 0) return {};

    UnionFind uf(num_points);
    for (const auto& edge : mst) {
        if (edge.distance > epsilon) break;
        uf.unite(edge.a, edge.b);
    }

    std::unordered_map<int, std::vector<int>> cluster_map;
    for (int i = 0; i < num_points; i++) {
        cluster_map[uf.find(i)].push_back(i);
    }

    std::vector<std::vector<int>> result;
    std::vector<int> noise;

    for (auto& kv : cluster_map) {
        if (static_cast<int>(kv.second.size()) >= min_cluster_size) {
            result.push_back(std::move(kv.second));
        } else {
            for (int idx : kv.second) {
                noise.push_back(idx);
            }
        }
    }

    for (int idx : noise) {
        result.push_back({idx});
    }

    return result;
}

std::vector<std::vector<int>> HDBSCANClustering::get_clusters() {
    return clusters;
}

std::vector<int> HDBSCANClustering::get_labels() {
    return labels;
}

std::vector<std::vector<int>> HDBSCANClustering::get_clusters_at_epsilon(double epsilon) const {
    return extract_clusters_at_epsilon(epsilon);
}

const std::vector<MSTEdge>& HDBSCANClustering::get_mst() const {
    return mst;
}

std::vector<double> HDBSCANClustering::get_merge_distances() const {
    std::vector<double> distances;
    distances.reserve(mst.size());
    for (const auto& edge : mst) {
        distances.push_back(edge.distance);
    }
    return distances;
}
