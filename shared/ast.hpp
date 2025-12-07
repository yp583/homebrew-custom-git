#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <cpp-tree-sitter.h>
#include <vector>
#include "diffreader.hpp"

using namespace std;
// Function declarations
vector<DiffChunk> chunkDiff(const ts::Node& node, const DiffChunk& diffChunk, size_t maxChars = 1500);
ts::Tree codeToTree(const string& code, const string& language);
string detectLanguageFromPath(const string& filepath);
vector<DiffChunk> chunkByLines(const DiffChunk& inputChunk, size_t maxChars = 1000);
bool isTextFile(const string& filepath);

#endif // AST_HPP 