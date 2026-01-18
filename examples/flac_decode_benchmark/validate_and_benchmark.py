#!/usr/bin/env python3
"""
Combined FLAC decoder validation and benchmark script.

1. Builds and runs host validation tests (expects 73 pass, 9 fail)
2. If validation passes, runs ESP32 benchmark
3. Compares results against stored baseline

Usage:
    ./benchmark.sh                    # Full validation + benchmark
    ./benchmark.sh --quick            # Skip validation, just benchmark
    ./benchmark.sh --no-clean         # Incremental build (faster)
    ./benchmark.sh --validate-only    # Just run validation
    ./benchmark.sh --save-baseline    # Save current result as baseline
"""

import subprocess
import sys
import os
import re
import json
import argparse
from datetime import datetime

# Paths relative to this script's location (examples/flac_decode_benchmark)
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..', '..'))
HOST_EXAMPLE_DIR = os.path.join(REPO_ROOT, 'host_examples', 'flac_to_wav')
BENCHMARK_DIR = SCRIPT_DIR
BASELINE_FILE = os.path.join(SCRIPT_DIR, 'baseline.json')

# Expected test results for validation
EXPECTED_PASSED = 73
EXPECTED_FAILED = 9


def load_baseline():
    """Load baseline results from file."""
    if os.path.exists(BASELINE_FILE):
        try:
            with open(BASELINE_FILE, 'r') as f:
                return json.load(f)
        except (json.JSONDecodeError, IOError):
            return None
    return None


def save_baseline(decode_time_ms, rtf, speed_x):
    """Save current results as baseline."""
    baseline = {
        'decode_time_ms': decode_time_ms,
        'rtf': rtf,
        'speed_x': speed_x,
        'saved_at': datetime.now().isoformat(),
    }
    with open(BASELINE_FILE, 'w') as f:
        json.dump(baseline, f, indent=2)
    print(f"Baseline saved: {decode_time_ms} ms ({speed_x}x real-time)")


def run_validation(clean=True):
    """Build and run host validation tests. Returns True if results match expected."""
    print("=" * 60)
    print("STEP 1: Host Validation Tests")
    print("=" * 60)
    print(f"Working directory: {HOST_EXAMPLE_DIR}")
    print()

    # Check directory exists
    if not os.path.exists(HOST_EXAMPLE_DIR):
        print(f"ERROR: Host example directory not found: {HOST_EXAMPLE_DIR}")
        return False

    os.chdir(HOST_EXAMPLE_DIR)

    # Step 1a: Clean build directory (if requested)
    if clean:
        print("Cleaning build directory...")
        subprocess.run(['rm', '-rf', 'build'], check=False)

    # Step 1b: Configure with CMake
    print("Configuring with CMake...")
    result = subprocess.run(['cmake', '-B', 'build'], capture_output=True, text=True)
    if result.returncode != 0:
        print("ERROR: CMake configuration failed!")
        print(result.stderr)
        return False

    # Step 1c: Build
    print("Building...")
    result = subprocess.run(['cmake', '--build', 'build'], capture_output=True, text=True)
    if result.returncode != 0:
        print("ERROR: Build failed!")
        print(result.stderr)
        return False
    print("Build successful!")
    print()

    # Step 1d: Run tests
    print("Running validation tests...")
    result = subprocess.run(
        ['python3', 'test_flac_decoder.py'],
        capture_output=True,
        text=True,
        cwd=HOST_EXAMPLE_DIR
    )

    # Print test output
    print(result.stdout)
    if result.stderr:
        print(result.stderr)

    # Parse results from output
    passed_match = re.search(r'Passed:\s*(\d+)', result.stdout)
    failed_match = re.search(r'Failed:\s*(\d+)', result.stdout)

    if not passed_match or not failed_match:
        print("ERROR: Could not parse test results!")
        return False

    passed = int(passed_match.group(1))
    failed = int(failed_match.group(1))

    print()
    print("-" * 40)
    print(f"Expected: {EXPECTED_PASSED} passed, {EXPECTED_FAILED} failed")
    print(f"Actual:   {passed} passed, {failed} failed")
    print("-" * 40)

    if passed == EXPECTED_PASSED and failed == EXPECTED_FAILED:
        print("VALIDATION PASSED - Output is correct!")
        return True
    else:
        print("VALIDATION FAILED - Output does not match expected results!")
        if passed != EXPECTED_PASSED:
            print(f"  - Expected {EXPECTED_PASSED} passed, got {passed}")
        if failed != EXPECTED_FAILED:
            print(f"  - Expected {EXPECTED_FAILED} failed, got {failed}")
        return False


def run_benchmark(clean=True):
    """Run the ESP32 benchmark. Returns (output, metrics) or (None, None) on failure."""
    print()
    print("=" * 60)
    print("STEP 2: ESP32 Benchmark")
    print("=" * 60)
    print(f"Working directory: {BENCHMARK_DIR}")
    if not clean:
        print("(incremental build - no clean)")
    print()

    os.chdir(BENCHMARK_DIR)

    # Import and run the benchmark script
    try:
        import serial
        import serial.tools.list_ports
    except ImportError:
        print("ERROR: pyserial not installed. Run: uv pip install pyserial")
        return None, None

    # Import the benchmark module
    sys.path.insert(0, BENCHMARK_DIR)
    from run_benchmark import run_benchmark as do_benchmark

    print("Starting benchmark...")
    print()

    result = do_benchmark(env='esp32s3', clean=clean, upload=True, baud=115200, timeout_sec=180)

    if not result:
        return None, None

    # Extract key metrics
    time_match = re.search(r'Total decode time:\s*([\d.]+)\s*ms', result)
    rtf_match = re.search(r'RTF:\s*([\d.]+)', result)
    speed_match = re.search(r'([\d.]+)x\)', result)

    if time_match and rtf_match and speed_match:
        metrics = {
            'decode_time_ms': float(time_match.group(1)),
            'rtf': float(rtf_match.group(1)),
            'speed_x': float(speed_match.group(1)),
        }
        return result, metrics

    return result, None


def print_summary(metrics, baseline, validation_passed, save_as_baseline=False):
    """Print a clear, parseable summary line."""
    print()
    print("=" * 60)

    if metrics is None:
        print("RESULT: FAIL | Benchmark did not complete")
        print("=" * 60)
        return

    decode_time = metrics['decode_time_ms']
    speed = metrics['speed_x']
    rtf = metrics['rtf']

    # Calculate delta vs baseline
    delta_str = ""
    if baseline:
        delta = decode_time - baseline['decode_time_ms']
        if delta < 0:
            delta_str = f" | {abs(delta):.2f} ms FASTER than baseline"
        elif delta > 0:
            delta_str = f" | {delta:.2f} ms SLOWER than baseline (REGRESSION)"
        else:
            delta_str = " | same as baseline"

    status = "PASS" if validation_passed else "PASS (validation skipped)"

    # The key summary line - easy to parse
    print(f"RESULT: {status} | {decode_time:.2f} ms | {speed:.1f}x real-time{delta_str}")
    print("=" * 60)

    # Save as baseline if requested
    if save_as_baseline:
        save_baseline(decode_time, rtf, speed)


def main():
    parser = argparse.ArgumentParser(
        description='Validate FLAC decoder correctness and run ESP32 benchmark'
    )
    parser.add_argument(
        '--quick', '-q',
        action='store_true',
        help='Quick mode: skip validation, just run benchmark'
    )
    parser.add_argument(
        '--skip-validation',
        action='store_true',
        help='Skip validation tests (same as --quick)'
    )
    parser.add_argument(
        '--validate-only',
        action='store_true',
        help='Only run validation tests, skip benchmark'
    )
    parser.add_argument(
        '--no-clean',
        action='store_true',
        help='Incremental build (skip cleaning build directories)'
    )
    parser.add_argument(
        '--save-baseline',
        action='store_true',
        help='Save current result as the baseline for future comparisons'
    )
    args = parser.parse_args()

    # --quick is alias for --skip-validation
    skip_validation = args.quick or args.skip_validation
    clean = not args.no_clean

    original_dir = os.getcwd()
    baseline = load_baseline()

    if baseline:
        print(f"Baseline: {baseline['decode_time_ms']:.2f} ms ({baseline['speed_x']:.1f}x real-time)")
        print()

    validation_passed = True

    try:
        # Step 1: Validation (unless skipped)
        if not skip_validation:
            validation_passed = run_validation(clean=clean)
            if not validation_passed:
                print()
                print("=" * 60)
                print("RESULT: FAIL | Validation failed - decoder output is incorrect")
                print("=" * 60)
                return 1

            if args.validate_only:
                print()
                print("=" * 60)
                print("RESULT: PASS | Validation complete (--validate-only)")
                print("=" * 60)
                return 0

        # Step 2: Benchmark
        result, metrics = run_benchmark(clean=clean)

        print_summary(
            metrics,
            baseline,
            validation_passed=not skip_validation,
            save_as_baseline=args.save_baseline
        )

        if result and metrics:
            return 0
        else:
            return 1

    finally:
        os.chdir(original_dir)


if __name__ == '__main__':
    sys.exit(main())
