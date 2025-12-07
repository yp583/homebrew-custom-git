#include "diffreader.hpp"
#include <vector>
#include <fstream>
#include <map>

DiffReader::DiffReader(istream& in, bool verbose)
    : in(in),
      verbose(verbose),
      diff_header_regex(regex("^diff --git a/(.*) b/(.*)")),
      in_file(false),
      in_chunk(false),
      curr_line_num(0),
      current_is_deleted(false),
      current_is_new(false)
{}
vector<DiffChunk> DiffReader::getChunks() const {
    return this->chunks;
}

void DiffReader::flushPendingRename() {
    if (this->in_file && !this->in_chunk &&
        this->current_old_filepath != this->current_filepath) {
        DiffChunk rename_chunk = DiffChunk{};
        rename_chunk.filepath = this->current_filepath;
        rename_chunk.old_filepath = this->current_old_filepath;
        rename_chunk.is_deleted = false;
        rename_chunk.is_new = false;
        rename_chunk.is_rename = true;
        rename_chunk.start = 0;
        this->chunks.push_back(rename_chunk);
        if (this->verbose) {
            cout << "PURE RENAME DETECTED: " << this->current_old_filepath
                 << " -> " << this->current_filepath << endl;
        }
    }
}

void DiffReader::ingestDiffLine(string line) {
    smatch match;

    if (regex_match(line, match, this->diff_header_regex)) {
        this->flushPendingRename();

        this->current_old_filepath = match[1].str();
        this->current_filepath = match[2].str();
        this->curr_line_num = 0;
        this->current_is_deleted = false;
        this->current_is_new = false;
        this->in_file = true;
        this->in_chunk = false;
        if (this->verbose){
            cout << "LINE WAS NEW FILE: " << line << endl;
        }
        return;
    }

    regex deleted_regex("^deleted file mode");
    if (this->in_file && regex_search(line, deleted_regex)) {
        this->current_is_deleted = true;
        if (this->verbose){
            cout << "FILE MARKED AS DELETED: " << line << endl;
        }
        return;
    }

    regex new_file_regex("^new file mode");
    if (this->in_file && regex_search(line, new_file_regex)) {
        this->current_is_new = true;
        if (this->verbose){
            cout << "FILE MARKED AS NEW: " << line << endl;
        }
        return;
    }

    if (this->in_file && line.substr(0, 2) == "@@") {
        this->in_chunk = true;

        DiffChunk current_chunk = DiffChunk{};
        current_chunk.filepath = this->current_filepath;
        current_chunk.old_filepath = this->current_old_filepath;
        current_chunk.is_deleted = this->current_is_deleted;
        current_chunk.is_new = this->current_is_new;

        regex hunk_regex("^@@ -(\\d+),?(\\d*) \\+(\\d+),?(\\d*) @@");
        smatch m;
        if (regex_search(line, m, hunk_regex)) {
            current_chunk.start = stoi(m[1].str());
        }

        this->chunks.push_back(current_chunk);

        if (this->verbose){
            cout << "LINE WAS NEW CHUNK: " << line << endl;
        }
        return;
    }

    if (this->in_file && this->in_chunk && !this->chunks.empty()) {
        DiffLine dline;
        dline.content = line.substr(1);
        dline.line_num = this->curr_line_num;

        if (this->verbose){
            cout << "LINE BEING ADDED: " << line << endl;
        }

        if (line[0] == '+') {
            dline.mode = INSERTION;
        } else if (line[0] == '-') {
            dline.mode = DELETION;
        } else if (line[0] == ' ') {
            dline.mode = EQ;
        } else if (line[0] == '\\') {
            dline.mode = NO_NEWLINE;
            dline.content = line;
        }

        this->chunks.back().lines.push_back(dline);
        this->curr_line_num += 1;
    }
}

void DiffReader::ingestDiff() {
    string line;
    while (getline(this->in, line)) {
        this->ingestDiffLine(line);
    }
    this->flushPendingRename();
}

DiffReader::~DiffReader() {}

string combineContent(DiffChunk chunk) {
    string result = "";
    for (const DiffLine& line : chunk.lines) {
        result += line.content + "\n";
    }
    return result;
};

int getNumLines(string filepath) {
    ifstream rFile(filepath);

    if (!rFile.is_open()) {
        cerr << "Error opening file!" << endl;
        return -1;
    }

    int count = 0;
    string line;
    while (getline(rFile, line)) {
        count++;
    }

    return count;
}



string createPatch(DiffChunk chunk, bool include_file_header) {
    string patch;
    bool is_rename = (chunk.old_filepath != chunk.filepath) && !chunk.is_new && !chunk.is_deleted;
    bool is_pure_rename = is_rename && chunk.lines.empty();

    if (is_pure_rename) {
        patch += "diff --git a/" + chunk.old_filepath + " b/" + chunk.filepath + "\n";
        patch += "similarity index 100%\n";
        patch += "rename from " + chunk.old_filepath + "\n";
        patch += "rename to " + chunk.filepath + "\n";
        return patch;
    }

    if (include_file_header) {
        if (is_rename) {
            patch += "diff --git a/" + chunk.old_filepath + " b/" + chunk.filepath + "\n";
            patch += "rename from " + chunk.old_filepath + "\n";
            patch += "rename to " + chunk.filepath + "\n";
        }

        if (chunk.is_new) {
            patch += "--- /dev/null\n";
        } else {
            patch += "--- a/" + chunk.old_filepath + "\n";
        }
        if (chunk.is_deleted) {
            patch += "+++ /dev/null\n";
        } else {
            patch += "+++ b/" + chunk.filepath + "\n";
        }
    }

    int old_count = 0, new_count = 0;
    bool has_changes = false;
    for (const DiffLine& line : chunk.lines) {
        if (line.mode == EQ)             { old_count++; new_count++; }
        else if (line.mode == DELETION)  { old_count++; has_changes = true; }
        else if (line.mode == INSERTION) { new_count++; has_changes = true; }
    }

    if (!has_changes) {
        return "";
    }

    patch += "@@ -" + to_string(chunk.start) + "," + to_string(old_count) +
             " +" + to_string(chunk.start) + "," + to_string(new_count) + " @@\n";

    for (const DiffLine& line : chunk.lines) {
        switch (line.mode) {
            case EQ:        patch += " " + line.content + "\n"; break;
            case INSERTION: patch += "+" + line.content + "\n"; break;
            case DELETION:  patch += "-" + line.content + "\n"; break;
            case NO_NEWLINE: patch += line.content + "\n"; break;
        }
    }

    return patch;
}

string createDeletePatch(string filepath) {
    string delete_patch = "diff --git a/" + filepath + " b/" + filepath + "\n";
    delete_patch += "deleted file mode 100644\n";
    delete_patch += "--- a/" + filepath + "\n";
    delete_patch += "+++ /dev/null\n";
    return delete_patch;
}

vector<string> createPatches(vector<DiffChunk> chunks) {
    vector<string> patches;
    unordered_map<string, string> renamed_files;
    unordered_map<string, map<int, int>> file_cumulative_deltas;

    unordered_map<string, size_t> deleted_file_last_idx;
    unordered_map<string, size_t> new_file_first_idx;
    
    for (size_t i = 0; i < chunks.size(); i++) {
        if (chunks[i].is_deleted) {
            deleted_file_last_idx[chunks[i].filepath] = i;
        }
        if (chunks[i].is_new && new_file_first_idx.find(chunks[i].filepath) == new_file_first_idx.end()) {
            new_file_first_idx[chunks[i].filepath] = i;
        }
    }

    for (size_t i = 0; i < chunks.size(); i++) {
        DiffChunk chunk = chunks[i];
        auto it = renamed_files.find(chunk.old_filepath);
        if (it != renamed_files.end()) {
            chunk.old_filepath = it->second;
            chunk.filepath = it->second;
        }

        if (chunk.old_filepath != chunk.filepath && !chunk.is_new && !chunk.is_deleted) {
            renamed_files[chunk.old_filepath] = chunk.filepath;
        }

        bool is_deleted_file = chunk.is_deleted;
        string filepath = chunk.filepath;

        // Only first chunk of a new file gets is_new for patch generation
        auto new_it = new_file_first_idx.find(filepath);
        if (chunk.is_new && (new_it == new_file_first_idx.end() || new_it->second != i)) {
            chunk.is_new = false;
        }

        chunk.is_deleted = false;

        int original_start = chunk.start;

        int adjustment = 0;
        auto& cumulative_deltas = file_cumulative_deltas[filepath];
        auto it_delta = cumulative_deltas.lower_bound(original_start);
        if (it_delta != cumulative_deltas.begin()) {
            --it_delta;
            adjustment = it_delta->second;
        }
        chunk.start += adjustment;

        patches.push_back(createPatch(chunk, true));

        int old_count = 0, new_count = 0;
        for (const DiffLine& line : chunk.lines) {
            if (line.mode == EQ) { old_count++; new_count++; }
            else if (line.mode == DELETION) { old_count++; }
            else if (line.mode == INSERTION) { new_count++; }
        }
        
        int delta = new_count - old_count;
        if (delta != 0) {
            auto update_it = cumulative_deltas.lower_bound(original_start);
            while (update_it != cumulative_deltas.end()) {
                update_it->second += delta;
                ++update_it;
            }
            cumulative_deltas[original_start] = adjustment + delta;
        }

        auto del_it = deleted_file_last_idx.find(filepath);
        if (is_deleted_file && del_it != deleted_file_last_idx.end() && del_it->second == i) {
            string delete_patch = createDeletePatch(filepath);
            patches.push_back(delete_patch);
        }
    }

    return patches;
}
