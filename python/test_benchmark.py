"""
Benchmark test for geometry_engine.py
Simulates processing 261+ ductwork paths like your real scenario.
"""

import json
import random
import time
import subprocess
import sys
import os

def generate_ductwork_paths(num_paths=261, grid_size=800):
    """
    Generate realistic ductwork-like paths:
    - Mostly horizontal and vertical lines
    - Some branching/connecting
    - Mix of trunk lines and branches
    """
    paths = []

    # Create some trunk lines (longer horizontal/vertical lines)
    num_trunks = num_paths // 10
    for i in range(num_trunks):
        if random.random() > 0.5:
            # Horizontal trunk
            y = random.uniform(50, grid_size - 50)
            x1 = random.uniform(0, grid_size / 3)
            x2 = random.uniform(grid_size * 2/3, grid_size)
            paths.append({
                'id': len(paths),
                'points': [{'x': x1, 'y': y}, {'x': x2, 'y': y}]
            })
        else:
            # Vertical trunk
            x = random.uniform(50, grid_size - 50)
            y1 = random.uniform(0, grid_size / 3)
            y2 = random.uniform(grid_size * 2/3, grid_size)
            paths.append({
                'id': len(paths),
                'points': [{'x': x, 'y': y1}, {'x': x, 'y': y2}]
            })

    # Create branch lines (shorter, connect to trunks)
    while len(paths) < num_paths:
        # Pick a random existing path to branch from
        parent_idx = random.randint(0, len(paths) - 1)
        parent = paths[parent_idx]

        # Pick a point along the parent
        if len(parent['points']) >= 2:
            t = random.uniform(0.2, 0.8)
            p1 = parent['points'][0]
            p2 = parent['points'][-1]
            branch_x = p1['x'] + t * (p2['x'] - p1['x'])
            branch_y = p1['y'] + t * (p2['y'] - p1['y'])

            # Create a perpendicular branch
            dx = p2['x'] - p1['x']
            dy = p2['y'] - p1['y']

            # Perpendicular direction
            if abs(dx) > abs(dy):
                # Parent is mostly horizontal, branch goes vertical
                length = random.uniform(30, 150)
                direction = random.choice([-1, 1])
                end_x = branch_x
                end_y = branch_y + direction * length
            else:
                # Parent is mostly vertical, branch goes horizontal
                length = random.uniform(30, 150)
                direction = random.choice([-1, 1])
                end_x = branch_x + direction * length
                end_y = branch_y

            # Maybe add intermediate points for multi-segment paths
            if random.random() > 0.7:
                mid_x = (branch_x + end_x) / 2 + random.uniform(-10, 10)
                mid_y = (branch_y + end_y) / 2 + random.uniform(-10, 10)
                paths.append({
                    'id': len(paths),
                    'points': [
                        {'x': branch_x, 'y': branch_y},
                        {'x': mid_x, 'y': mid_y},
                        {'x': end_x, 'y': end_y}
                    ]
                })
            else:
                paths.append({
                    'id': len(paths),
                    'points': [
                        {'x': branch_x, 'y': branch_y},
                        {'x': end_x, 'y': end_y}
                    ]
                })

    return paths


def run_benchmark(num_paths=261):
    """Run the geometry engine and measure performance."""

    print(f"\n{'='*60}")
    print(f"BENCHMARK: {num_paths} paths")
    print('='*60)

    # Generate test data
    print("\nGenerating test paths...")
    start = time.time()
    paths = generate_ductwork_paths(num_paths)
    gen_time = (time.time() - start) * 1000
    print(f"Generated {len(paths)} paths in {gen_time:.1f}ms")

    # Prepare input
    input_data = json.dumps({'paths': paths, 'params': {'max_dist': 10.0}})

    # Run find_connections
    print("\nRunning find_connections...")
    start = time.time()

    result = subprocess.run(
        [sys.executable, 'geometry_engine.py', 'find_connections'],
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
    print(f"  - Paths processed: {output.get('path_count', '?')}")
    print(f"  - Valid paths: {output.get('valid_path_count', '?')}")
    print(f"  - Connections found: {len(output.get('connections', []))}")
    print(f"  - Ignored anchors: {len(output.get('ignored_anchors', []))}")
    print(f"  - Python processing time: {output.get('time_ms', '?'):.1f}ms")
    print(f"  - Total time (incl. subprocess): {total_time:.1f}ms")

    # Run build_groups too
    print("\nRunning build_groups...")
    start = time.time()

    result = subprocess.run(
        [sys.executable, 'geometry_engine.py', 'build_groups'],
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

    print(f"\nGROUPING RESULTS:")
    print(f"  - Groups formed: {len(output.get('groups', []))}")
    print(f"  - Python processing time: {output.get('time_ms', '?'):.1f}ms")
    print(f"  - Total time (incl. subprocess): {total_time:.1f}ms")

    # Show some group sizes
    groups = output.get('groups', [])
    if groups:
        sizes = sorted([len(g) for g in groups], reverse=True)[:5]
        print(f"  - Largest groups: {sizes}")

    print("\n" + "="*60)

    # Compare to estimated ExtendScript time
    # ExtendScript O(n^2) with n=261 would be ~68,000 comparisons
    # At ~1ms per comparison (conservative), that's ~68 seconds
    n = num_paths
    estimated_es_time = (n * n * 0.5) / 1000  # seconds, very conservative
    print(f"ESTIMATED ExtendScript time: {estimated_es_time:.0f}+ seconds")
    print(f"Actual Python time: {output.get('time_ms', 0)/1000:.2f} seconds")
    print(f"SPEEDUP: ~{estimated_es_time / (output.get('time_ms', 1)/1000):.0f}x faster")
    print("="*60)


if __name__ == '__main__':
    # Test with different sizes
    for size in [100, 261, 500]:
        run_benchmark(size)
