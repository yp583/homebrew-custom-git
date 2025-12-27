#!/bin/bash

echo "Note: The PATH entry for ~/bin was not removed from your shell config."
echo "You can manually remove it from ~/.zshrc or ~/.bashrc if desired."

# Custom Git Commands Uninstall Script
# This script removes all custom git commands installed by install.sh

set -e  # Exit on any error

echo "Uninstalling Custom Git Commands..."

# Get the root directory of the repo
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$HOME/bin"

if [ ! -d "$BIN_DIR" ]; then
    echo "No $BIN_DIR directory found. Nothing to uninstall."
    exit 0
fi

# Remove installed commands
for cmd_dir in "$REPO_ROOT/commands"/*; do
    if [ -d "$cmd_dir" ]; then
        cmd_name=$(basename "$cmd_dir")

        # Remove C++ executable
        if [ -f "$BIN_DIR/git_${cmd_name}.o" ]; then
            echo "Removing git_${cmd_name}.o..."
            rm "$BIN_DIR/git_${cmd_name}.o"
        fi

        # Remove git command script
        if [ -f "$BIN_DIR/git-$cmd_name" ]; then
            echo "Removing git-$cmd_name..."
            rm "$BIN_DIR/git-$cmd_name"
        fi
    fi
done

echo ""
echo "Uninstall complete!"
echo ""
echo "Note: The PATH entry for ~/bin was not removed from your shell config."
echo "You can manually remove it from ~/.zshrc or ~/.bashrc if desired."

            echo "Removing git-$cmd_name..."
            rm "$BIN_DIR/git-$cmd_name"
        fi
    fi
done

echo ""
echo "Uninstall complete!"
echo ""
echo "Note: The PATH entry for ~/bin was not removed from your shell config."
echo "You can manually remove it from ~/.zshrc or ~/.bashrc if desired."
#!/bin/bash

echo "Note: The PATH entry for ~/bin was not removed from your shell config."
echo "You can manually remove it from ~/.zshrc or ~/.bashrc if desired."

# Custom Git Commands Uninstall Script
# This script removes all custom git commands installed by install.sh

set -e  # Exit on any error

echo "Uninstalling Custom Git Commands..."

# Get the root directory of the repo
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$HOME/bin"

if [ ! -d "$BIN_DIR" ]; then
    echo "No $BIN_DIR directory found. Nothing to uninstall."
    exit 0
fi

# Remove installed commands
for cmd_dir in "$REPO_ROOT/commands"/*; do
    if [ -d "$cmd_dir" ]; then
        cmd_name=$(basename "$cmd_dir")

        # Remove C++ executable
        if [ -f "$BIN_DIR/git_${cmd_name}.o" ]; then
            echo "Removing git_${cmd_name}.o..."
            rm "$BIN_DIR/git_${cmd_name}.o"
        fi

        # Remove git command script
        if [ -f "$BIN_DIR/git-$cmd_name" ]; then
