#pragma once
#include <vector>
#include "mst.hpp"

class HDBSCANClustering {
private:
    std::vector<std::vector<int>> clusters;
    std::vector<int> labels;
    std::vector<MSTEdge> mst;
    int min_cluster_size;
    int min_pts;
    int num_points;
    double default_epsilon;

    std::vector<std::vector<int>> extract_clusters_at_epsilon(double epsilon) const;

public:
    HDBSCANClustering(int min_cluster_size = 2, int min_pts = 2);
    void fit(const std::vector<std::vector<float>>& data);

    std::vector<std::vector<int>> get_clusters();
    std::vector<int> get_labels();

    std::vector<std::vector<int>> get_clusters_at_epsilon(double epsilon) const;
    const std::vector<MSTEdge>& get_mst() const;
    std::vector<double> get_merge_distances() const;

    ~HDBSCANClustering();
};
