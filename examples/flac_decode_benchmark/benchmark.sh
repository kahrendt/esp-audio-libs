#!/bin/bash
#
# FLAC Decoder Benchmark Script
#
# Usage:
#   ./benchmark.sh                    # Full validation + benchmark
#   ./benchmark.sh --quick            # Skip validation, just benchmark
#   ./benchmark.sh --no-clean         # Incremental build (faster)
#   ./benchmark.sh --validate-only    # Just run validation
#   ./benchmark.sh --save-baseline    # Save current result as baseline
#
# Combines these options as needed:
#   ./benchmark.sh --quick --no-clean # Fastest iteration (no validation, incremental build)
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Check if venv exists
if [ ! -d ".venv" ]; then
    echo "Creating virtual environment..."
    uv venv
    echo "Installing dependencies..."
    uv pip install pyserial
fi

# Run the benchmark script with all arguments passed through
exec .venv/bin/python validate_and_benchmark.py "$@"
