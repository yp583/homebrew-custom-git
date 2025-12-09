#include "ast.hpp"
#include "async_openai_api.hpp"
#include "utils.hpp"
#include "hierarchal.hpp"
#include "diffreader.hpp"
#include "umap.hpp"
#include <vector>
#include <fstream>
#include <filesystem>
#include <sstream>

using namespace std;
using json = nlohmann::json;

struct ClusteredCommit {
  int cluster_id;
  vector<string> patch_files;
  string message;

  json to_json() const {
    return json{
      {"cluster_id", cluster_id},
      {"patch_files", patch_files},
      {"message", message}
    };
  }
};

string get_api_key() {
  const char* api_key_env = getenv("OPENAI_API_KEY");
  string api_key = api_key_env ? api_key_env : "";

  if (api_key.empty()) {
    FILE* pipe = popen("git config --get custom.openaiApiKey 2>/dev/null", "r");
    if (pipe) {
      char c;
      while ((c = fgetc(pipe)) != EOF && c != '\n') {
        api_key += c;
      }
      pclose(pipe);
    }
  }
  return api_key;
}

int run_merge_mode(int verbose);
int run_threshold_mode(float threshold, const string& json_path, int verbose);

int main(int argc, char *argv[]) {
  float dist_thresh = -1;
  int verbose = 0;
  bool merge_mode = false;
  string json_path;

  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    if (arg == "-vv") {
      verbose = 2;
    } else if (arg == "-v") {
      verbose = 1;
    } else if (arg == "-m") {
      merge_mode = true;
    } else if (arg == "-t") {
      if (i + 2 < argc) {
        try {
          dist_thresh = stof(argv[++i]);
          json_path = argv[++i];
        } catch (...) {
          cerr << "Error: -t requires threshold and json file path" << endl;
          return 1;
        }
      } else {
        cerr << "Error: -t requires threshold and json file path" << endl;
        return 1;
      }
    } else {
      cerr << "Usage: " << argv[0] << " -m [-v|-vv]  (merge mode)" << endl;
      cerr << "       " << argv[0] << " -t <threshold> <json_file> [-v|-vv]  (threshold mode)" << endl;
      return 1;
    }
  }

  if (!merge_mode && dist_thresh < 0) {
    cerr << "Error: Must specify either -m or -t <threshold> <json_file>" << endl;
    return 1;
  }

  if (merge_mode) {
    return run_merge_mode(verbose);
  } else {
    return run_threshold_mode(dist_thresh, json_path, verbose);
  }
}

// Phase 1: Read diff, get embeddings, cluster, output dendrogram + chunks
int run_merge_mode(int verbose) {
  string api_key = get_api_key();
  if (api_key.empty()) {
    cerr << "Error: OPENAI_API_KEY not found" << endl;
    return 1;
  }

  DiffReader dr(cin);
  dr.ingestDiff();
  if (verbose >= 1) cerr << "Parsed " << dr.getChunks().size() << " chunks from git diff" << endl;

  vector<DiffChunk> all_chunks;
  for (const DiffChunk& chunk : dr.getChunks()) {
    if (chunk.is_rename) {
      all_chunks.push_back(chunk);
      continue;
    }

    string language = detectLanguageFromPath(chunk.filepath);
    vector<DiffChunk> file_chunks;

    if (language != "text") {
      string file_content = combineContent(chunk);
      ts::Tree tree = codeToTree(file_content, language);
      file_chunks = chunkDiff(tree.getRootNode(), chunk);
    } else {
      file_chunks = chunkByLines(chunk);
    }
    all_chunks.insert(all_chunks.end(), file_chunks.begin(), file_chunks.end());
  }

  if (all_chunks.empty()) {
    cerr << "Error: No chunks to process" << endl;
    return 1;
  }

  AsyncHTTPSConnection conn(verbose);
  AsyncOpenAIAPI openai_api(conn, api_key);
  vector<future<HTTPSResponse>> embedding_futures;

  if (verbose >= 1) cerr << "Getting embeddings for " << all_chunks.size() << " chunks..." << endl;

  for (const auto& chunk : all_chunks) {
    string content = combineContent(chunk);
    if (chunk.is_rename) {
      content = "renamed file from " + chunk.old_filepath + " to " + chunk.filepath;
    } else if (content.empty()) {
      content = "file: " + chunk.filepath;
    }
    embedding_futures.push_back(openai_api.async_embedding(content));
  }

  openai_api.run_requests();

  vector<vector<float>> embeddings;
  for (auto& fut : embedding_futures) {
    try {
      embeddings.push_back(parse_embedding(fut.get().body));
    } catch (...) {
      embeddings.push_back({});
    }
    if (verbose >= 1) cerr << "." << flush;
  }
  if (verbose >= 1) cerr << " done" << endl;

  HierachicalClustering hc;
  if (verbose >= 1) cerr << "Running hierarchical clustering..." << endl;
  vector<MergeEvent> merges = hc.cluster(embeddings);
  if (verbose >= 1) cerr << "Clustering complete. " << merges.size() << " merge events" << endl;

  // Run UMAP for visualization
  vector<UmapPoint> umap_points;
  if (embeddings.size() >= 3) {
    if (verbose >= 1) cerr << "Running UMAP dimensionality reduction..." << endl;
    try {
      umap_points = compute_umap(embeddings);
      if (verbose >= 1) cerr << "UMAP complete." << endl;
    } catch (const exception& e) {
      if (verbose >= 1) cerr << "UMAP failed: " << e.what() << endl;
      umap_points = {};
    }
  } else {
    if (verbose >= 1) cerr << "Skipping UMAP (need >= 3 chunks)" << endl;
  }

  // Build output JSON
  json output;

  // Dendrogram
  json dendrogram;
  json labels = json::array();
  for (const auto& chunk : all_chunks) {
    labels.push_back(chunk.filepath);
  }
  dendrogram["labels"] = labels;

  json merges_json = json::array();
  float max_distance = 0;
  for (const auto& merge : merges) {
    merges_json.push_back({
      {"left", merge.cluster_a_id},
      {"right", merge.cluster_b_id},
      {"distance", merge.distance}
    });
    if (merge.distance > max_distance) max_distance = merge.distance;
  }
  dendrogram["merges"] = merges_json;
  dendrogram["max_distance"] = max_distance;
  output["dendrogram"] = dendrogram;

  // Chunks with UMAP coordinates
  json chunks_json = json::array();
  for (size_t i = 0; i < all_chunks.size(); i++) {
    json chunk_j = chunk_to_json(all_chunks[i]);
    chunk_j["index"] = i;

    // Add UMAP coordinates for visualization
    if (i < umap_points.size()) {
      chunk_j["umap_x"] = umap_points[i].x;
      chunk_j["umap_y"] = umap_points[i].y;
    } else {
      chunk_j["umap_x"] = 0.0;
      chunk_j["umap_y"] = 0.0;
    }

    // Add preview for scatter plot tooltip
    string preview = combineContent(all_chunks[i]);
    if (preview.size() > 100) preview = preview.substr(0, 100) + "...";
    chunk_j["preview"] = preview;

    chunks_json.push_back(chunk_j);
  }
  output["chunks"] = chunks_json;

  cout << output.dump() << endl;
  return 0;
}

// Phase 2: Read JSON, apply threshold, create patches, generate commits
int run_threshold_mode(float threshold, const string& json_path, int verbose) {
  string api_key = get_api_key();
  if (api_key.empty()) {
    cerr << "Error: OPENAI_API_KEY not found" << endl;
    return 1;
  }

  ifstream json_file(json_path);
  if (!json_file.is_open()) {
    cerr << "Error: Cannot open " << json_path << endl;
    return 1;
  }

  json input;
  try {
    json_file >> input;
  } catch (const exception& e) {
    cerr << "Error parsing JSON: " << e.what() << endl;
    return 1;
  }

  // Parse merge events
  vector<MergeEvent> merges;
  for (const auto& m : input["dendrogram"]["merges"]) {
    merges.push_back({
      m["left"].get<size_t>(),
      m["right"].get<size_t>(),
      m["distance"].get<float>()
    });
  }

  // Parse chunks
  vector<DiffChunk> all_chunks;
  for (const auto& c : input["chunks"]) {
    all_chunks.push_back(chunk_from_json(c));
  }

  if (verbose >= 1) cerr << "Loaded " << all_chunks.size() << " chunks, " << merges.size() << " merges" << endl;
  if (verbose >= 1) cerr << "Applying threshold " << threshold << endl;

  // Get clusters at threshold
  vector<vector<int>> clusters = get_clusters_at_threshold(merges, threshold);
  if (verbose >= 1) cerr << "Found " << clusters.size() << " clusters" << endl;

  // Group chunks by cluster and create patches
  filesystem::remove_all("/tmp/gcommit");
  filesystem::create_directories("/tmp/gcommit");

  vector<vector<string>> clusters_patch_paths;

  for (size_t i = 0; i < clusters.size(); i++) {
    const vector<int>& cluster = clusters[i];
    if (verbose >= 1) cerr << "Cluster " << i << ": " << cluster.size() << " chunks" << endl;

    // Gather chunks for this cluster
    vector<DiffChunk> cluster_chunks;
    for (int idx : cluster) {
      cluster_chunks.push_back(all_chunks[idx]);
    }

    // Create patches for this cluster
    vector<string> patches = createPatches(cluster_chunks);

    string cluster_dir = "/tmp/gcommit/cluster_" + to_string(i);
    filesystem::create_directories(cluster_dir);

    vector<string> patch_paths;
    int patch_num = 0;
    for (size_t j = 0; j < patches.size(); j++) {
      if (patches[j].empty()) {
        if (verbose >= 1) cerr << "Skipping empty patch" << endl;
        continue;
      }
      string patch_path = cluster_dir + "/patch_" + to_string(patch_num++) + ".patch";
      ofstream patch_file(patch_path);
      patch_file << patches[j];
      patch_file.close();
      patch_paths.push_back(patch_path);
      if (verbose >= 1) cerr << "Wrote " << patch_path << endl;
    }
    clusters_patch_paths.push_back(patch_paths);
  }

  // Generate commit messages
  AsyncHTTPSConnection conn(verbose);
  AsyncOpenAIAPI openai_api(conn, api_key);
  vector<future<string>> message_futures;
  vector<ClusteredCommit> commits;

  for (size_t i = 0; i < clusters_patch_paths.size(); i++) {
    vector<string>& patch_paths = clusters_patch_paths[i];
    if (patch_paths.empty()) {
      if (verbose >= 1) cerr << "Skipping cluster with no valid patches" << endl;
      continue;
    }

    string diff_context = "";
    ClusteredCommit commit{static_cast<int>(i), vector<string>(), "empty commit"};

    for (const string& path : patch_paths) {
      ifstream patch_file(path);
      if (!patch_file.is_open()) {
        cerr << "Error opening file: " << path << endl;
        return 1;
      }

      string line;
      while (getline(patch_file, line)) {
        if (!line.empty() && line[0] == '+') {
          diff_context += "Insertion: ";
        } else if (!line.empty() && line[0] == '-') {
          diff_context += "Deletion: ";
        }
        diff_context += line + "\n";
      }
      patch_file.close();
      diff_context += "\n\n\n";
      commit.patch_files.push_back(path);
    }

    message_futures.push_back(async_generate_commit_message(openai_api, diff_context));
    commits.push_back(commit);
  }

  openai_api.run_requests();

  for (size_t i = 0; i < commits.size(); i++) {
    commits[i].message = message_futures[i].get();
  }

  // Build chunk-to-cluster mapping
  vector<int> chunk_to_cluster(all_chunks.size(), -1);
  for (size_t i = 0; i < clusters.size(); i++) {
    for (int idx : clusters[i]) {
      chunk_to_cluster[idx] = static_cast<int>(i);
    }
  }

  // Output JSON with commits and visualization
  json output;

  // Commits
  json commits_json = json::array();
  for (const ClusteredCommit& commit : commits) {
    commits_json.push_back(commit.to_json());
  }
  output["commits"] = commits_json;

  // Visualization data
  json viz_output;

  // Points with cluster assignments
  json points_json = json::array();
  for (const auto& c : input["chunks"]) {
    size_t idx = c["index"].get<size_t>();
    points_json.push_back({
      {"id", idx},
      {"x", c.value("umap_x", 0.0)},
      {"y", c.value("umap_y", 0.0)},
      {"cluster_id", chunk_to_cluster[idx]},
      {"filepath", c["filepath"].get<string>()},
      {"preview", c.value("preview", "")}
    });
  }
  viz_output["points"] = points_json;

  // Clusters with messages
  json clusters_meta = json::array();
  for (size_t i = 0; i < commits.size(); i++) {
    clusters_meta.push_back({
      {"id", commits[i].cluster_id},
      {"message", commits[i].message}
    });
  }
  viz_output["clusters"] = clusters_meta;

  output["visualization"] = viz_output;

cout << output.dump() << endl;

  if (verbose >= 1) cerr << "Output complete." << endl;
  return 0;
}