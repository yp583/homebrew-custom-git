#include "ast.hpp"
#include "async_openai_api.hpp"
#include "utils.hpp"
#include "hierarchal.hpp"
#include "diffreader.hpp"
#include "umap.hpp"
#include <vector>
#include <fstream>
#include <filesystem>

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

int main(int argc, char *argv[]) {
  float dist_thresh = 0.5;
  int verbose = 0;
  bool interactive = false;

  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    if (arg == "-vv") {
      verbose = 2;
    } else if (arg == "-v") {
      verbose = 1;
    } else if (arg == "-i") {
      interactive = true;
    } else if (arg == "-d") {
      if (i + 1 < argc) {
        try {
          dist_thresh = stof(argv[++i]);
        } catch (...) {
          cerr << "Error: -d requires a numeric threshold value" << endl;
          return 1;
        }
      } else {
        cerr << "Error: -d requires a threshold value" << endl;
        return 1;
      }
    } else {
      try {
        dist_thresh = stof(arg);
      } catch (...) {
        cerr << "Usage: " << argv[0] << " [-d threshold] [-i] [-v|-vv]" << endl;
        return 1;
      }
    }
  }

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

  if (api_key.empty()) {
    cerr << "Error: OPENAI_API_KEY not found in environment or git config (custom.openaiApiKey)" << endl;
    return 1;
  }

  DiffReader dr(cin);
  dr.ingestDiff();
  if (verbose >= 1) cerr << "Parsed " << dr.getChunks().size() << " chunks from git diff" << endl;

  // AST-chunk the diff
  vector<DiffChunk> all_chunks;
  for (const DiffChunk& chunk : dr.getChunks()) {
    // Pure renames have no lines - pass through directly
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

  // Get embeddings
  AsyncHTTPSConnection conn(verbose);
  AsyncOpenAIAPI openai_api(conn, api_key);
  vector<future<HTTPSResponse>> embedding_futures;

  if (verbose >= 1) cerr << "Getting embeddings for " << all_chunks.size() << " chunks..." << endl;

  const size_t MAX_EMBEDDING_CHARS = 16000;
  for (const auto& chunk : all_chunks) {
    string content = combineContent(chunk);
    // For pure renames/empty chunks, use descriptive text for embedding
    if (chunk.is_rename) {
      content = "renamed file from " + chunk.old_filepath + " to " + chunk.filepath;
    } else if (content.empty()) {
      content = "file: " + chunk.filepath;
    }
    if (content.size() > MAX_EMBEDDING_CHARS) {
      content = content.substr(0, MAX_EMBEDDING_CHARS);
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

  if (verbose >= 1) cerr << "Starting hierarchical clustering (threshold=" << dist_thresh << ")..." << endl;

  hc.cluster(embeddings, dist_thresh);
  vector<vector<int>> clusters = hc.get_clusters();
  if (verbose >= 1) cerr << "Clustering complete. Found " << clusters.size() << " clusters" << endl;

  vector<UmapPoint> umap_points;
  if (interactive) {
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
      if (verbose >= 1) cerr << "Skipping UMAP (need >= 3 chunks, got " << embeddings.size() << ")" << endl;
    }
  }

  vector<int> chunk_to_cluster(all_chunks.size(), -1);
  for (size_t i = 0; i < clusters.size(); i++) {
    for (int idx : clusters[i]) {
      chunk_to_cluster[idx] = static_cast<int>(i);
    }
  }

  vector<DiffChunk> all_cluster_chunks;
  vector<size_t> cluster_end_idx;

  for (size_t i = 0; i < clusters.size(); i++) {
    const vector<int>& cluster = clusters[i];

    if (verbose >= 1) cerr << "Cluster " << (i + 1) << ":" << endl;

    for (int idx: cluster) {
      all_cluster_chunks.push_back(all_chunks[idx]);
    }
    size_t prev_end = cluster_end_idx.empty() ? 0 : cluster_end_idx.back();
    cluster_end_idx.push_back(prev_end + cluster.size());
  }

  vector<string> patches = createPatches(all_cluster_chunks);
  vector<vector<string>> clusters_patch_paths;

  for (size_t i = 0; i < cluster_end_idx.size(); i++) {
    clusters_patch_paths.push_back(vector<string>());
    string cluster_dir = "/tmp/patches/cluster_" + to_string(i);
    filesystem::create_directories(cluster_dir);

    size_t start_idx = (i == 0) ? 0 : cluster_end_idx[i - 1];
    size_t end_idx = cluster_end_idx[i];

    int patch_num = 0;
    for (size_t j = start_idx; j < end_idx && j < patches.size(); j++) {
      if (patches[j].empty()) {
        if (verbose >= 1) cerr << "Skipping empty patch at index " << j << endl;
        continue;
      }
      string patch_path = cluster_dir + "/patch_" + to_string(patch_num++) + ".patch";
      ofstream patch_file(patch_path);
      patch_file << patches[j];
      patch_file.close();
      clusters_patch_paths.back().push_back(patch_path);
      if (verbose >= 1) cerr << "Wrote " << patch_path << endl;
    }
  }

  vector<future<string>> message_futures;
  vector<ClusteredCommit> commits;
  int cluster_idx = 0;
  for (vector<string>& patch_paths: clusters_patch_paths) {
    if (patch_paths.empty()) {
      if (verbose >= 1) cerr << "Skipping cluster with no valid patches" << endl;
      cluster_idx++;
      continue;
    }
    string diff_context = "";
    ClusteredCommit commit{cluster_idx, vector<string>(), "empty commit"};
    for (string path: patch_paths) {
      ifstream patch_file;
      patch_file.open(path);

      if (!patch_file.is_open()) {
        cerr << "Error opening file!" << endl;
        return 1;
      }

      string line;
      while (getline(patch_file, line)) {
        if (line[0] == '+') {
          diff_context += "Insertion: ";
        }
        else if (line[0] == '-') {
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
    cluster_idx++;
  }

  openai_api.run_requests();

  for (size_t i = 0; i < commits.size(); i++) {
    commits[i].message = message_futures[i].get();
  }

  json output;

  json commits_json = json::array();
  for (const ClusteredCommit& commit : commits) {
    commits_json.push_back(commit.to_json());
  }
  output["commits"] = commits_json;

  if (interactive) {
    json viz_output;

    json points_json = json::array();
    for (size_t i = 0; i < all_chunks.size(); i++) {
      string preview = combineContent(all_chunks[i]);
      if (preview.size() > 100) preview = preview.substr(0, 100) + "...";

      double x = (i < umap_points.size()) ? umap_points[i].x : 0.0;
      double y = (i < umap_points.size()) ? umap_points[i].y : 0.0;

      points_json.push_back({
        {"id", i},
        {"x", x},
        {"y", y},
        {"cluster_id", chunk_to_cluster[i]},
        {"filepath", all_chunks[i].filepath},
        {"preview", preview}
      });
    }
    viz_output["points"] = points_json;

    json clusters_json = json::array();
    for (size_t i = 0; i < commits.size(); i++) {
      clusters_json.push_back({
        {"id", commits[i].cluster_id},
        {"message", commits[i].message}
      });
    }
    viz_output["clusters"] = clusters_json;

    output["visualization"] = viz_output;
  }

  cout << output.dump() << endl;

  if (verbose >= 1) cerr << "Output complete." << endl;

  return 0;
}