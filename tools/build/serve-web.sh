#!/usr/bin/env bash
# Serve the web build on localhost for quick testing.
# Usage: ./serve-web.sh [port]
set -euo pipefail
PORT=${1:-8080}

# Determine script directory
script_dir=$(cd "$(dirname "$0")" && pwd)

# If the script is already inside the web output, serve that directory directly
if [ -f "$script_dir/CowEngine.js" ] || [ -f "$script_dir/index.html" ]; then
    ROOT_DIR="$script_dir"
else
    # If the script sits in tools/build inside the repo, compute repo root
    if [[ "$script_dir" == *"/tools/build"* ]]; then
        repo_root=$(cd "$script_dir/../.." && pwd)
        ROOT_DIR="$repo_root/dist/game-web"
    else
        # Otherwise search upward for a CMakeLists.txt to locate the repo root
        cur="$script_dir"
        found=""
        while [ "$cur" != "/" ]; do
            if [ -f "$cur/CMakeLists.txt" ]; then
                found="$cur"
                break
            fi
            cur=$(dirname "$cur")
        done
        if [ -n "$found" ]; then
            ROOT_DIR="$found/dist/game-web"
        else
            # Fallback to repository-standard location under HOME if nothing found
            ROOT_DIR="$HOME/repos/CowEngine/dist/game-web"
        fi
    fi
fi

if [ ! -d "$ROOT_DIR" ]; then
    echo "Web build not found at $ROOT_DIR" >&2
    echo "Run: ./tools/build/build-all-games.sh" >&2
    exit 1
fi

echo "Serving $ROOT_DIR on http://localhost:$PORT/"
python3 -m http.server --directory "$ROOT_DIR" "$PORT"
