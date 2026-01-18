#!/usr/bin/env python3
"""
Autonomous FLAC benchmark runner for ESP32.
Builds, uploads, and captures benchmark output until completion.
"""

import subprocess
import sys
import serial
import serial.tools.list_ports
import time
import argparse


def find_esp32_port():
    """Find the ESP32 serial port."""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        # Common ESP32 identifiers
        if any(x in port.description.lower() for x in ['cp210', 'ch340', 'ftdi', 'usb', 'uart']):
            return port.device
        if any(x in port.device.lower() for x in ['usbserial', 'usbmodem', 'ttyusb', 'ttyacm']):
            return port.device
    # Fallback: return first available port
    if ports:
        return ports[0].device
    return None


def run_benchmark(env='esp32s3', clean=True, upload=True, baud=115200, timeout_sec=120):
    """Run the benchmark and capture output."""

    # Step 1: Clean if requested
    if clean:
        print("Cleaning build artifacts...")
        subprocess.run(['rm', '-rf', '.pio', 'sdkconfig', 'sdkconfig.esp32s3'], check=False)

    # Step 2: Build and upload
    if upload:
        print(f"Building and uploading for {env}...")
        result = subprocess.run(
            ['pio', 'run', '-e', env, '-t', 'upload'],
            capture_output=False
        )
        if result.returncode != 0:
            print("Upload failed!")
            return None
        print("Upload complete. Starting monitor...")
        time.sleep(1)  # Brief pause for device to reset

    # Step 3: Find serial port
    port = find_esp32_port()
    if not port:
        print("Could not find ESP32 serial port!")
        return None
    print(f"Using serial port: {port}")

    # Step 4: Open serial and capture until "Benchmark complete."
    output_lines = []
    try:
        with serial.Serial(port, baud, timeout=1) as ser:
            start_time = time.time()
            benchmark_started = False

            while (time.time() - start_time) < timeout_sec:
                if ser.in_waiting > 0:
                    try:
                        line = ser.readline().decode('utf-8', errors='replace').rstrip()
                        if line:
                            print(line)
                            output_lines.append(line)

                            # Detect benchmark start
                            if 'FLAC Decode Benchmark' in line:
                                benchmark_started = True

                            # Exit when benchmark is complete
                            if 'Benchmark complete.' in line:
                                print("\n--- Benchmark finished ---")
                                return '\n'.join(output_lines)
                    except Exception as e:
                        print(f"Read error: {e}")
                else:
                    time.sleep(0.01)

            print(f"\nTimeout after {timeout_sec} seconds")
            if not benchmark_started:
                print("Benchmark never started - device may need manual reset")
            return '\n'.join(output_lines) if output_lines else None

    except serial.SerialException as e:
        print(f"Serial error: {e}")
        return None


def main():
    parser = argparse.ArgumentParser(description='Run FLAC decode benchmark on ESP32')
    parser.add_argument('-e', '--env', default='esp32s3', help='PlatformIO environment')
    parser.add_argument('--no-clean', action='store_true', help='Skip cleaning build artifacts')
    parser.add_argument('--no-upload', action='store_true', help='Skip build/upload (just monitor)')
    parser.add_argument('-b', '--baud', type=int, default=115200, help='Serial baud rate')
    parser.add_argument('-t', '--timeout', type=int, default=120, help='Timeout in seconds')

    args = parser.parse_args()

    result = run_benchmark(
        env=args.env,
        clean=not args.no_clean,
        upload=not args.no_upload,
        baud=args.baud,
        timeout_sec=args.timeout
    )

    if result:
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == '__main__':
    main()
