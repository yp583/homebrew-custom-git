#include "hierarchal.hpp"
#include <unordered_map>

UnionFind::UnionFind(size_t size) {
  this->parents = vector<size_t>(size);
  this->rank = vector<size_t>(size, 0);
  for (size_t i = 0; i < size; i++) {
    this->parents[i] = i;
  }
}

size_t UnionFind::find(size_t i) {
  if (this->parents[i] != i) {
    this->parents[i] = this->find(this->parents[i]);
  }
  return this->parents[i];
}

void UnionFind::unite(size_t i, size_t j) {
  size_t irep = this->find(i);
  size_t jrep = this->find(j);

  if (irep == jrep) return;

  if (this->rank[irep] < this->rank[jrep]) {
    this->parents[irep] = jrep;
  } else if (this->rank[irep] > this->rank[jrep]) {
    this->parents[jrep] = irep;
  } else {
    this->parents[jrep] = irep;
    this->rank[irep]++;
  }
}

vector<vector<size_t>> UnionFind::get_sets() {
  unordered_map<size_t, vector<size_t>> groups;

  for (size_t i = 0; i < parents.size(); i++) {
    size_t root = find(i);
    groups[root].push_back(i);
  }

  vector<vector<size_t>> result;
  for (auto& pair : groups) {
    result.push_back(pair.second);
  }

  return result;
}

HierachicalClustering::HierachicalClustering() {}

vector<MergeEvent> HierachicalClustering::cluster(vector<vector<float>> data) {
  vector<vector<float>> dist_mat(data.size(), vector<float>(data.size(), -1));

  for (size_t i = 0; i < data.size(); i++) {
    for (size_t j = i + 1; j < data.size(); j++) {
      dist_mat[i][j] = 1 - cos_sim(data[i], data[j]);
    }
  }

  UnionFind uf(data.size());
  vector<MergeEvent> merges;

  for (size_t merge_count = 0; merge_count < data.size() - 1; merge_count++) {
    float min_dist = numeric_limits<float>::infinity();
    size_t min_a = 0, min_b = 0;

    for (size_t i = 0; i < data.size(); i++) {
      for (size_t j = i + 1; j < data.size(); j++) {
        if (uf.find(i) != uf.find(j) && dist_mat[i][j] < min_dist) {
          min_dist = dist_mat[i][j];
          min_a = i;
          min_b = j;
        }
      }
    }

    merges.push_back({uf.find(min_a), uf.find(min_b), min_dist});
    uf.unite(min_a, min_b);
  }

  return merges;
}

vector<vector<int>> get_clusters_at_threshold(
  const vector<MergeEvent>& merges,
  float threshold
) {
  size_t num_leaves = merges.size() + 1;
  UnionFind uf(num_leaves);

  for (const auto& merge : merges) {
    if (merge.distance > threshold) break;
    uf.unite(merge.cluster_a_id, merge.cluster_b_id);
  }

  vector<vector<size_t>> sets = uf.get_sets();
  vector<vector<int>> result;
  for (const auto& s : sets) {
    vector<int> cluster;
    for (size_t idx : s) cluster.push_back(static_cast<int>(idx));
    result.push_back(cluster);
  }
  return result;
}

HierachicalClustering::~HierachicalClustering() {}
