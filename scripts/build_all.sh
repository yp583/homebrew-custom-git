#!/bin/bash

# Build All Custom Git Commands
# This script builds all commands for development/testing without installing

set -e  # Exit on any error

echo "Building all custom git commands..."

# Get the root directory of the repo
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Function to build a command
build_command() {
    local cmd_name=$1
    local cmd_dir="$REPO_ROOT/commands/$cmd_name"
    
    echo "Building $cmd_name..."

    if [ ! -d "$cmd_dir" ]; then
        echo "ERROR: Command directory $cmd_dir not found"
        return 1
    fi
    
    cd "$cmd_dir"

    # Skip if no CMakeLists.txt (pure bash script commands)
    if [ ! -f "CMakeLists.txt" ]; then
        echo "SKIPPED: $cmd_name (no CMakeLists.txt - bash script only)"
        return 0
    fi

    # Create build directory and build
    mkdir -p build
    cd build
    cmake .. || { echo "ERROR: CMake configuration failed for $cmd_name"; return 1; }
    make || { echo "ERROR: Build failed for $cmd_name"; return 1; }

    if [ -f "git_${cmd_name}.o" ]; then
        echo "SUCCESS: $cmd_name built successfully"
    else
        echo "ERROR: Executable git_${cmd_name}.o not found"
        return 1
    fi
}

# Build all commands in the commands directory
echo "Discovering commands..."
built_commands=()
for cmd_dir in "$REPO_ROOT/commands"/*; do
    if [ -d "$cmd_dir" ]; then
        cmd_name=$(basename "$cmd_dir")
        if build_command "$cmd_name"; then
            built_commands+=("$cmd_name")
        fi
    fi
done

echo ""
echo "Build complete!"
echo ""
echo "Built commands:"
for cmd in "${built_commands[@]}"; do
    echo "  $cmd (executable: commands/$cmd/build/git_${cmd}.o)"
done
echo ""
echo "To test a command locally:"
echo "  cd commands/gcommit && git diff HEAD^^^..HEAD | ./build/git_gcommit.o"
echo ""
echo "To install all commands system-wide:"
echo "  ./scripts/setup.sh"