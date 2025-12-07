#include "ast.hpp"

using namespace std;
size_t calculateDiffLinesSize(const vector<DiffLine> &lines) {
  size_t totalSize = 0;
  for (const DiffLine &line : lines) {
    totalSize += line.content.length() + 1;
  }
  return totalSize;
}

int calculateLineOffset(const vector<DiffLine> &lines, size_t startIdx, size_t endIdx) {
  int offset = 0;
  for (size_t i = startIdx; i < endIdx && i < lines.size(); i++) {
    switch (lines[i].mode) {
    case EQ:
    case DELETION:
      offset++;
      break;
    case INSERTION:
    case NO_NEWLINE:
      break;
    }
  }
  return offset;
}

size_t byteToLineIndex(const vector<DiffLine> &lines, size_t bytePos) {
  size_t currentByte = 0;
  for (size_t i = 0; i < lines.size(); i++) {
    size_t lineEnd = currentByte + lines[i].content.length() + 1;
    if (bytePos < lineEnd) {
      return i;
    }
    currentByte = lineEnd;
  }
  return lines.empty() ? 0 : lines.size() - 1;
}

vector<DiffChunk> chunkByLines(const DiffChunk &inputChunk, size_t maxChars) {
  vector<DiffChunk> chunks;

  if (inputChunk.lines.empty()) {
    return chunks;
  }

  size_t totalSize = calculateDiffLinesSize(inputChunk.lines);
  if (totalSize <= maxChars) {
    chunks.push_back(inputChunk);
    return chunks;
  }

  size_t startLineIdx = 0;
  int cumulative_offset = 0;
  bool is_first = true;

  while (startLineIdx < inputChunk.lines.size()) {
    DiffChunk currentChunk;
    currentChunk.filepath = inputChunk.filepath;
    currentChunk.old_filepath = inputChunk.old_filepath;
    currentChunk.start = inputChunk.start + cumulative_offset;
    currentChunk.is_new = inputChunk.is_new;

    size_t currentSize = 0;
    size_t currentLineIdx = startLineIdx;

    while (currentLineIdx < inputChunk.lines.size()) {
      const DiffLine &line = inputChunk.lines[currentLineIdx];
      size_t lineSize = line.content.length() + 1;

      if (!currentChunk.lines.empty() && currentSize + lineSize > maxChars) {
        break;
      }

      currentChunk.lines.push_back(line);
      currentSize += lineSize;
      currentLineIdx++;
    }

    currentChunk.is_deleted = inputChunk.is_deleted;

    chunks.push_back(currentChunk);

    if (currentLineIdx >= inputChunk.lines.size()) {
      break;
    }

    cumulative_offset += calculateLineOffset(inputChunk.lines, startLineIdx, currentLineIdx);
    startLineIdx = currentLineIdx;
    is_first = false;
  }

  return chunks;
}

extern "C" {
TSLanguage *tree_sitter_python();
TSLanguage *tree_sitter_cpp();
TSLanguage *tree_sitter_java();
TSLanguage *tree_sitter_javascript();
TSLanguage *tree_sitter_go();
}

vector<DiffChunk> chunkDiffInternal(const ts::Node &node, const DiffChunk &diffChunk,
                                     size_t maxChars) {
  vector<DiffChunk> newChunks;

  if (diffChunk.lines.empty()) {
    return newChunks;
  }

  // Collect split points (line indices where AST nodes end)
  vector<size_t> splitPoints;
  splitPoints.push_back(0);

  for (size_t i = 0; i < node.getNumChildren(); i++) {
    ts::Node child = node.getChild(i);
    size_t endLineIdx = byteToLineIndex(diffChunk.lines, child.getByteRange().end);
    size_t splitPoint = endLineIdx + 1;
    if (splitPoint > splitPoints.back() && splitPoint <= diffChunk.lines.size()) {
      splitPoints.push_back(splitPoint);
    }
  }

  if (splitPoints.back() < diffChunk.lines.size()) {
    splitPoints.push_back(diffChunk.lines.size());
  }

  // Create chunks from split ranges, respecting maxChars
  DiffChunk currentChunk;
  currentChunk.filepath = diffChunk.filepath;
  currentChunk.old_filepath = diffChunk.old_filepath;
  size_t currentChunkSize = 0;
  size_t currentChunkStartIdx = 0;

  for (size_t i = 0; i + 1 < splitPoints.size(); i++) {
    size_t startIdx = splitPoints[i];
    size_t endIdx = splitPoints[i + 1];

    vector<DiffLine> segmentLines(
      diffChunk.lines.begin() + startIdx,
      diffChunk.lines.begin() + endIdx
    );
    size_t segmentSize = calculateDiffLinesSize(segmentLines);

    if (!currentChunk.lines.empty() && currentChunkSize + segmentSize > maxChars) {
      currentChunk.start = diffChunk.start + calculateLineOffset(diffChunk.lines, 0, currentChunkStartIdx);
      newChunks.push_back(currentChunk);
      currentChunk = DiffChunk();
      currentChunk.filepath = diffChunk.filepath;
      currentChunk.old_filepath = diffChunk.old_filepath;
      currentChunkSize = 0;
      currentChunkStartIdx = startIdx;
    }

    if (currentChunk.lines.empty()) {
      currentChunkStartIdx = startIdx;
    }

    currentChunk.lines.insert(currentChunk.lines.end(),
                               segmentLines.begin(), segmentLines.end());
    currentChunkSize += segmentSize;
  }

  if (!currentChunk.lines.empty()) {
    currentChunk.start = diffChunk.start + calculateLineOffset(diffChunk.lines, 0, currentChunkStartIdx);
    newChunks.push_back(currentChunk);
  }

  // Mark ALL chunks with is_new/is_deleted so patch ordering works after clustering
  for (auto &chunk : newChunks) {
    chunk.is_new = diffChunk.is_new;
    chunk.is_deleted = diffChunk.is_deleted;
  }

  return newChunks;
}

vector<DiffChunk> chunkDiff(const ts::Node &node, const DiffChunk &diffChunk,
                            size_t maxChars) {
  return chunkDiffInternal(node, diffChunk, maxChars);
}

ts::Tree codeToTree(const string &code, const string &language) {
  TSLanguage *lang;
  if (language == "python") {
    lang = tree_sitter_python();
  } else if (language == "cpp") {
    lang = tree_sitter_cpp();
  } else if (language == "java") {
    lang = tree_sitter_java();
  } else if (language == "javascript") {
    lang = tree_sitter_javascript();
  } else if (language == "typescript") {
    lang = tree_sitter_javascript();
  } else if (language == "go") {
    lang = tree_sitter_go();
  } else {
    lang = tree_sitter_cpp();
  }
  ts::Parser parser{lang};
  return parser.parseString(code);
}

string detectLanguageFromPath(const string &filepath) {
  size_t lastDot = filepath.find_last_of(".");
  if (lastDot == string::npos) {
    return "cpp";
  }

  string extension = filepath.substr(lastDot);

  if (extension == ".py") {
    return "python";
  } else if (extension == ".cpp" || extension == ".c" || extension == ".h" ||
             extension == ".hpp") {
    return "cpp";
  } else if (extension == ".java") {
    return "java";
  } else if (extension == ".js" || extension == ".jsx") {
    return "javascript";
  } else if (extension == ".ts" || extension == ".tsx") {
    return "typescript";
  } else if (extension == ".go") {
    return "go";
  } else if (extension == ".cpp") {
    return "cpp";
  } else {
    return "text";
  }
}
