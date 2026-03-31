"""
Benchmark orthogonalization performance.
Tests the main bottleneck with realistic ductwork path data.
"""

import json
import subprocess
import sys
import os
import time
import random
import math


def generate_ductwork_paths(num_paths=262, grid_size=800):
    """Generate realistic ductwork paths - mostly horizontal/vertical with slight angles."""
    paths = []

    for i in range(num_paths):
        # Random starting point
        x1 = random.uniform(50, grid_size - 50)
        y1 = random.uniform(50, grid_size - 50)

        # Random length
        length = random.uniform(50, 200)

        # Mostly orthogonal with small deviation (simulates hand-drawn lines)
        base_angle = random.choice([0, 90, 180, 270])  # Mostly H/V
        deviation = random.uniform(-8, 8)  # Small angle deviation
        angle = math.radians(base_angle + deviation)

        x2 = x1 + length * math.cos(angle)
        y2 = y1 + length * math.sin(angle)

        # Some paths have 3-4 points (branches with bends)
        if random.random() > 0.7:
            # Add intermediate point
            mid_x = (x1 + x2) / 2 + random.uniform(-20, 20)
            mid_y = (y1 + y2) / 2 + random.uniform(-20, 20)
            paths.append({
                'id': i,
                'points': [
                    {'x': x1, 'y': y1},
                    {'x': mid_x, 'y': mid_y},
                    {'x': x2, 'y': y2}
                ],
                'layerName': 'Blue Ductwork'
            })
        else:
            paths.append({
                'id': i,
                'points': [
                    {'x': x1, 'y': y1},
                    {'x': x2, 'y': y2}
                ],
                'layerName': 'Blue Ductwork'
            })

    return paths


def run_ortho_benchmark(num_paths=262):
    """Benchmark orthogonalization with given number of paths."""

    print(f"\n{'='*60}")
    print(f"ORTHOGONALIZATION BENCHMARK: {num_paths} paths")
    print('='*60)

    # Generate test data
    print("\nGenerating test paths...")
    paths = generate_ductwork_paths(num_paths)

    total_points = sum(len(p['points']) for p in paths)
    total_segments = sum(len(p['points']) - 1 for p in paths)
    print(f"Generated {len(paths)} paths with {total_points} points, {total_segments} segments")

    # Estimate ExtendScript time
    # snapAnchors is O(paths * points * segments) per iteration, up to 8 iterations
    es_operations = total_points * total_segments * 4  # ~4 iterations average
    es_time_estimate = es_operations * 0.015  # ~15us per operation in ExtendScript (conservative)
    print(f"\nESTIMATED ExtendScript time: {es_time_estimate/1000:.1f} seconds")
    print(f"  (based on {es_operations:,} operations at ~15us each)")

    # Run Python orthogonalization
    print("\nRunning Python orthogonalization...")
    input_data = json.dumps({
        'paths': paths,
        'params': {'snap_threshold': 5, 'steep_min': 17, 'steep_max': 70}
    })

    start = time.time()
    result = subprocess.run(
        [sys.executable, 'geometry_engine.py', 'orthogonalize'],
        input=input_data,
        capture_output=True,
        text=True,
        cwd=os.path.dirname(os.path.abspath(__file__))
    )
    total_time = (time.time() - start) * 1000

    if result.returncode != 0:
        print(f"ERROR: {result.stderr}")
        return

    output = json.loads(result.stdout)

    print(f"\nRESULTS:")
    print(f"  - Iterations: {output.get('iterations', '?')}")
    print(f"  - Snaps made: {output.get('total_snaps', '?')}")
    print(f"  - Ortho changes: {output.get('total_ortho_changes', '?')}")
    print(f"  - Python processing: {output.get('time_ms', '?'):.1f}ms")
    print(f"  - Total (incl subprocess): {total_time:.1f}ms")

    python_time = output.get('time_ms', total_time)
    speedup = es_time_estimate / python_time if python_time > 0 else 0

    print(f"\n{'='*60}")
    print(f"ESTIMATED SPEEDUP: ~{speedup:.0f}x faster")
    print(f"  ExtendScript: ~{es_time_estimate/1000:.1f} seconds")
    print(f"  Python: ~{python_time/1000:.2f} seconds")
    print('='*60)


if __name__ == '__main__':
    # Test with actual user's path count
    run_ortho_benchmark(262)

    # Also test scaling
    print("\n\nSCALING TEST:")
    for n in [100, 262, 500, 1000]:
        run_ortho_benchmark(n)
