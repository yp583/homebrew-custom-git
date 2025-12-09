#include <vector>
#include <iostream>
#include <limits>
#include "utils.hpp"

using namespace std;

struct MergeEvent {
  size_t cluster_a_id;
  size_t cluster_b_id;
  float distance;
};

class UnionFind {
  private:
    vector<size_t> parents;
    vector<size_t> rank;
  public:
    UnionFind(size_t size);
    size_t find(size_t i);
    void unite(size_t i, size_t j);
    vector<vector<size_t>> get_sets();
};

class HierachicalClustering {
public:
  HierachicalClustering();
  vector<MergeEvent> cluster(vector<vector<float>> data);
  ~HierachicalClustering();
};

vector<vector<int>> get_clusters_at_threshold(
  const vector<MergeEvent>& merges,
  float threshold
);

