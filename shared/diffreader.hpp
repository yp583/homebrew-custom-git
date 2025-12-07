#ifndef DIFFREADER_HPP
#define DIFFREADER_HPP

#include <iostream>
#include <string>
#include <regex>
#include <vector>
#include <unordered_map>
#include <algorithm>
using namespace std;
enum DiffMode {
    EQ = 0,
    INSERTION = 1,
    DELETION = 2,
    NO_NEWLINE = 3
};
struct DiffLine {
    DiffMode mode;
    string content;
    int line_num;
};
struct DiffChunk {
    string filepath;      // New path (or same as old if not renamed)
    string old_filepath;  // Old path (for renames, same as filepath if not renamed)
    vector<DiffLine> lines;
    int start = 1;
    bool is_deleted = false;  // File is being deleted (whole file removal)
    bool is_new = false;      // File is being created (new file)
    bool is_rename = false;   // Pure rename (no content changes)
};


class DiffReader {
private:
    istream& in;
    bool verbose;

    regex diff_header_regex;

    bool in_file;
    bool in_chunk;
    int curr_line_num;
    string current_filepath;
    string current_old_filepath;  // Old path from "a/" in diff header
    bool current_is_deleted;   // Track if current file is being deleted
    bool current_is_new;       // Track if current file is being created

    vector<DiffChunk> chunks;

    void ingestDiffLine(string line);
    void flushPendingRename();

public:
    DiffReader(istream& in, bool verbose = false);
    vector<DiffChunk> getChunks() const;
    void ingestDiff();
    ~DiffReader();
};

string combineContent(DiffChunk chunk);
string createPatch(DiffChunk chunk, bool include_file_header = true);
vector<string> createPatches(vector<DiffChunk> chunks);

// JSON serialization
#include <nlohmann/json.hpp>
nlohmann::json chunk_to_json(const DiffChunk& chunk);
DiffChunk chunk_from_json(const nlohmann::json& j);

#endif // DIFFREADER_HPP
