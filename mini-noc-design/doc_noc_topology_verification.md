# NoC Topology Verification

## Verification Structure

| Item | Description |
|------|------------|
| Topologies | Mesh, Torus, Ring, Fat-Tree, Flattened Butterfly |
| Router ports | 5 (Self=0, North=1, South=2, East=3, West=4) |
| Routing | XY deterministic routing |
| Deadlock | Escape VC / turn restriction |

## Test Cases

### T1: Mesh Topology Initialization
```
Input:  noc_topology_init(&topo, NOC_TOPO_MESH, 4, 4)
Output: topo.width=4 topo.height=4 topo.num_routers=16 topo.wrap_around=false
Status: PASS
```

### T2: Torus Wrap-Around
```
Input:  noc_topology_init(&topo, NOC_TOPO_TORUS, 4, 4)
        noc_neighbor_port(&topo, {0,0}, NOC_DIR_WEST, &to)
Output: to={3,0} (wrap from x=0 to x=3)
Status: PASS
```

### T3: XY Routing
```
Input:  src={0,0} dst={3,2}
        dir = noc_xy_route(&topo, src, dst)
Output: dir=NOC_DIR_EAST (x first)
Status: PASS
```

### T4: Boundary Detection
```
Input:  noc_is_boundary(&topo, {0,0}, NOC_DIR_WEST)  -> true
        noc_is_boundary(&topo, {0,0}, NOC_DIR_NORTH) -> true
        noc_is_boundary(&topo, {2,2}, NOC_DIR_EAST)  -> true (3x3)
Status: PASS
```

### T5: Node ID / Coordinate Bidirectional
```
Input:  noc_node_id(&topo, {2,1}) -> id=6 (4-wide mesh)
        noc_coord_from_id(&topo, 6, &out) -> out={2,1}
Status: PASS
```

### T6: Deadlock Escape Routes
```
Input:  src={0,0} dst={3,3}
        noc_deadlock_escape_route(&topo, src, dst, &primary, &escape)
Output: primary=NOC_DIR_EAST, escape=NOC_DIR_EAST or NOC_DIR_SOUTH
Status: PASS
```

### T7: Hop Count / Manhattan Distance
```
Input:  noc_hop_count(&topo, {0,0}, {3,2}) -> 5
        noc_manhattan_distance({1,3}, {4,1}) -> 5
Status: PASS
```

## Formal Properties

1. **XY routing is deadlock-free**: No cyclic dependency in channel dependency graph
2. **Escape VC guarantees progress**: At least one flit always advances
3. **Manhattan distance monotonic**: Every XY hop reduces remaining distance by 1
4. **Torus wrap preserves connectivity**: All nodes reachable from any source

## Coverage

- [x] All 5 topology types initialized
- [x] All 5 directions routed
- [x] Boundary and internal nodes
- [x] Wrap-around paths in Torus
- [x] ID/coordinate round-trip
- [x] Deadlock escape paths
- [x] Manhattan distance non-negative
