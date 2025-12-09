# Custom Git Commands

A collection of custom git commands that extend Git's functionality with AI-powered commit workflows.

## Installation

### Homebrew (Recommended)

```bash
brew tap yp583/custom-git
brew install custom-git
```

### Manual Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/yp583/custom-git.git
   cd custom-git
   ```

2. **Run the setup script:**
   ```bash
   ./scripts/setup.sh
   ```

## Quick Start

```bash
git mcommit           # Simple AI commit message generation
git gcommit           # Smart commit clustering with interactive UI
git qcommit           # Templated quick commit for predefined messages like "merged" or "changed formula"
```

## Requirements

Set your OpenAI API key using **either** method:

**Option 1: Environment variable**
```bash
export OPENAI_API_KEY="sk-..."
```

**Option 2: Git config** (recommended for per-machine setup)
```bash
git config --global custom.openaiApiKey "sk-..."
```

The environment variable takes precedence if both are set.

---

## Available Commands

### `git mcommit` - AI Commit Message Generator

Generates a single commit message for all staged changes using OpenAI's gpt-4o-mini model.

**Usage:**
```bash
git add .              # Stage your changes first
git mcommit            # Generate AI commit message and commit
git mcommit -i         # Edit message in vim before committing
git mcommit -h         # Show help
```

**Options:**
| Flag | Description |
|------|-------------|
| `-i`, `--interactive` | Open vim to edit the generated message before committing |
| `-h`, `--help` | Show help message |

**How it works:**
1. Captures `git diff --cached` (staged changes)
2. Sends diff to OpenAI API with each line prefixed as "Insertion:" or "Deletion:"
3. Returns a concise, single-line commit message
4. Prompts for confirmation (y/n) before committing

**Example:**
```bash
$ git add src/auth.py
$ git mcommit
Generating AI commit message...
Proposed commit message:
add JWT token validation to authentication middleware

Confirm commit? (y/n): y
Created commit with message: add JWT token validation to authentication middleware
Smart commit complete!
```

---

### `git gcommit` - Smart Commit Clustering

Analyzes staged changes semantically, clusters similar changes together, and creates separate commits for each cluster. Features an interactive terminal UI built with Ink (React for CLI) to review and adjust before applying.

**Usage:**
```bash
git gcommit                 # Default threshold (0.5)
git gcommit -v              # Verbose output (shows C++ binary stderr)
git gcommit --dev           # Step through phases with confirmation prompts
git gcommit -h              # Show help
```

**Options:**
| Flag | Description |
|------|-------------|
| `-v`, `--verbose` | Show verbose output from the C++ clustering engine |
| `--dev` | Developer mode: pause between phases for debugging |
| `-h`, `--help` | Show help message |

**How it works:**

The command runs in two phases:

**Phase 1 - Analysis (Merge Mode):**
1. Parses `git diff --cached` into structured chunks using a custom diff parser
2. Detects language per file (Python, C++, Java, JavaScript, Go, or plain text)
3. For code files: parses AST using tree-sitter to chunk at semantic boundaries (functions, classes)
4. For text files: chunks by lines (max 1000 chars per chunk)
5. Generates embeddings for each chunk using OpenAI's `text-embedding-3-small` model
6. Runs hierarchical clustering (single-linkage) on the embedding vectors
7. Applies UMAP dimensionality reduction for 2D scatter plot visualization
8. Outputs dendrogram data for threshold selection

**Phase 2 - Commit Generation (Threshold Mode):**
1. Applies the selected threshold to the dendrogram to form final clusters
2. Creates patch files for each cluster in `/tmp/gcommit/`
3. Generates commit messages for each cluster using `gpt-4o-mini`
4. Applies patches sequentially on a staging branch
5. Presents interactive UI for review

**Interactive UI Controls:**

*Dendrogram View (threshold selection):*
| Key | Action |
|-----|--------|
| `←/→` or `h/l` | Adjust threshold (changes number of clusters) |
| `Enter` | Confirm threshold and proceed |
| `q` | Cancel and quit |

*Diff View:*
| Key | Action |
|-----|--------|
| `TAB` | Switch focus between file tree and diff panel |
| `j/k` | Navigate files (tree) or scroll diff (diff panel) |
| `Shift+H/L` | Navigate between commits |
| `v` | Toggle to scatter plot view |
| `a` | Apply all commits and merge to original branch |
| `q` | Cancel and quit (no changes made) |

*Scatter Plot View:*
| Key | Action |
|-----|--------|
| `Shift+H/L` | Navigate between commits |
| `v` | Toggle to diff view |
| `a` | Apply all commits |
| `q` | Cancel and quit |

**Workflow:**
1. Stage changes with `git add`
2. Run `git gcommit` - processes changes and shows dendrogram
3. Adjust threshold with arrow keys to control number of commits
4. Press Enter to generate commits
5. Review commits in diff viewer, navigate with keyboard
6. Press `a` to apply or `q` to cancel

**Supported Languages:**
- Python (tree-sitter-python)
- C++ (tree-sitter-cpp)
- Java (tree-sitter-java)
- JavaScript (tree-sitter-javascript)
- Go (tree-sitter-go)
- Plain text files (line-based chunking)

**Example:**
```bash
$ git add .
$ git gcommit
# Interactive dendrogram appears - adjust threshold
# Shows: "3 clusters at threshold 0.40"
# Press Enter to proceed
# Review each commit in diff viewer
# Press 'a' to apply
✓ Created 3 commit(s) on main
  - refactor authentication middleware for better error handling
  - add new user profile API endpoints
  - update documentation for API changes
```

---

## Architecture

```
custom-git/
├── commands/
│   ├── mcommit/              # Simple AI commit
│   │   ├── src/main.cpp      # Reads diff, calls OpenAI, outputs message
│   │   └── git-mcommit       # Bash wrapper: handles confirmation, editor
│   └── gcommit/              # Smart commit clustering
│       ├── src/
│       │   ├── main.cpp      # Two-phase: merge mode + threshold mode
│       │   ├── hierarchal.cpp # Single-linkage hierarchical clustering
│       │   └── umap.hpp      # UMAP wrapper for visualization
│       └── terminal-ui/      # Node.js Ink app
│           └── source/
│               ├── cli.tsx   # Entry point with meow CLI parser
│               ├── app.tsx   # Main React app with phase state machine
│               ├── contexts/ # Git operations context (simple-git)
│               └── components/ # FileTree, DiffViewer, ScatterPlot, Dendrogram
├── shared/                   # Static library linked by all commands
│   ├── diffreader.*          # Git diff parser → DiffChunk structs
│   ├── ast.*                 # Tree-sitter integration, language detection
│   ├── async_https_api.*     # Non-blocking HTTPS client (kqueue + OpenSSL)
│   ├── async_openai_api.*    # OpenAI embeddings + chat (gpt-4o-mini)
│   └── utils.*               # Cosine similarity, commit message prompts
├── scripts/
│   ├── setup.sh              # Build + install to ~/bin
│   ├── build_all.sh          # Build only (for development)
│   └── install.sh            # Install built binaries
└── Formula/                  # Homebrew formula
```

## Development

**Build only (no install):**
```bash
./scripts/build_all.sh
```

**Build individual command:**
```bash
cd commands/gcommit && mkdir -p build && cd build && cmake .. && make
```

**Test locally without installing:**
```bash
git diff --cached | ./commands/gcommit/build/git_gcommit.o -m
```

**Dependencies:**
- OpenSSL (ssl, crypto) - for HTTPS connections
- cpp-tree-sitter + language grammars (auto-downloaded via CPM)
- nlohmann/json (auto-downloaded via CPM)
- umappp (auto-downloaded via CPM) - for dimensionality reduction
- Node.js + npm (for gcommit terminal UI)

---

## TODOs

- Add a simple cmd to autocomplete git checkout based on existing branches (including remote ones)
- Better naming of commands?
- Add ability to read from git configs for all cmd line args 
