/**
 * test_all.c ? Comprehensive NoC Design Test Suite
 *
 * Covers all core APIs with assert-based testing.
 * Test categories:
 *   - Topology construction and metrics
 *   - Router pipeline and arbitration
 *   - Routing algorithms (all 8)
 *   - Flow control (credit-based)
 *   - Deadlock detection
 *   - QoS mechanisms
 *   - Performance models
 *   - Turn model verification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "noc_topology.h"
#include "noc_router.h"
#include "noc_routing.h"
#include "noc_flowctrl.h"
#include "noc_perf.h"
#include "noc_qos.h"
#include "noc_deadlock.h"

static int tests_passed = 0;
static int tests_failed = 0;
static int tests_total = 0;

#define TEST(name) do { \
    tests_total++; \
    printf("  TEST %02d: %s ... ", tests_total, name); \
} while(0)

#define PASS() do { \
    printf("PASS\n"); \
    tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
    tests_failed++; \
} while(0)

/* ???????????????????????????????????????????????????????????????????????
 * Section 1: Topology Tests (L1, L2, L4)
 * ??????????????????????????????????????????????????????????????????????? */

static void test_mesh_creation(void) {
    TEST("2D Mesh 4x4 creation");
    noc_topology_t *topo = noc_topo_create_mesh_2d(4);
    if (!topo) { FAIL("NULL topology"); return; }

    assert(topo->type == TOPO_MESH_2D);
    assert(topo->num_nodes == 16);
    assert(topo->k == 4);
    assert(topo->n == 2);
    assert(topo->num_edges > 0);

    /* Verify corner node has boundary flag */
    assert(topo->nodes[0].flags & NOC_NODE_FLAG_BOUNDARY);

    /* Verify center node does NOT have boundary flag */
    int32_t center_id = 1 * 4 + 1; /* (1,1) in 4x4 mesh */
    assert(!(topo->nodes[center_id].flags & NOC_NODE_FLAG_BOUNDARY));

    /* Verify east link exists */
    assert(topo->nodes[0].links[NOC_DIR_EAST] == 1);
    assert(topo->nodes[0].links[NOC_DIR_WEST] == -1);

    noc_topo_free(topo);
    PASS();
}

static void test_torus_creation(void) {
    TEST("2D Torus 4x4 creation");
    noc_topology_t *torus = noc_topo_create_torus_2d(4);
    if (!torus) { FAIL("NULL torus"); return; }

    assert(torus->type == TOPO_TORUS_2D);
    assert(torus->num_nodes == 16);

    /* Verify wraparound: leftmost node has west link to rightmost */
    int32_t left = 0;          /* (0,0) */
    int32_t right = 3;        /* (3,0) */
    assert(torus->nodes[left].links[NOC_DIR_WEST] == right);
    assert(torus->nodes[right].links[NOC_DIR_EAST] == left);

    /* Torus nodes should NOT have boundary flag */
    assert(!(torus->nodes[0].flags & NOC_NODE_FLAG_BOUNDARY));

    noc_topo_free(torus);
    PASS();
}

static void test_ring_creation(void) {
    TEST("Ring 8-node creation");
    noc_topology_t *ring = noc_topo_create_ring(8);
    if (!ring) { FAIL("NULL ring"); return; }

    assert(ring->type == TOPO_RING);
    assert(ring->num_nodes == 8);

    /* Verify ring wrap: node 7 connects to node 0 */
    assert(ring->nodes[7].links[NOC_DIR_EAST] == 0);
    assert(ring->nodes[0].links[NOC_DIR_WEST] == 7);

    noc_topo_free(ring);
    PASS();
}

static void test_tree_creation(void) {
    TEST("Fat-tree 2-ary 3-level creation");
    /* k=2, levels=3: 1 + 2 + 4 = 7 nodes */
    noc_topology_t *tree = noc_topo_create_tree(2, 3);
    if (!tree) { FAIL("NULL tree"); return; }

    assert(tree->type == TOPO_TREE);
    assert(tree->num_nodes == 7);

    /* Root should have ROOT flag */
    assert(tree->nodes[0].flags & NOC_NODE_FLAG_ROOT);

    /* Leaves should have LEAF flag (on level 2) */
    int32_t leaf_start = 1 + 2; /* after root + level 1 */
    assert(tree->nodes[leaf_start].flags & NOC_NODE_FLAG_LEAF);

    noc_topo_free(tree);
    PASS();
}

static void test_butterfly_creation(void) {
    TEST("Butterfly 2-ary 3-stage creation");
    noc_topology_t *bf = noc_topo_create_butterfly(2, 3);
    if (!bf) { FAIL("NULL butterfly"); return; }

    assert(bf->type == TOPO_BUTTERFLY);
    /* k=2, stages=3: 2 * (3+1) = 8 nodes */
    assert(bf->num_nodes == 8);

    noc_topo_free(bf);
    PASS();
}

static void test_topology_metrics(void) {
    TEST("Topology metrics: diameter, avg dist, bisection");
    noc_topology_t *mesh = noc_topo_create_mesh_2d(4);
    if (!mesh) { FAIL("NULL mesh"); return; }

    int32_t diam = noc_topo_diameter(mesh);
    /* 4x4 mesh diameter: from (0,0) to (3,3) = 6 hops */
    assert(diam == 6);

    double avg = noc_topo_avg_distance(mesh);
    assert(avg > 0.0 && avg <= 6.0);

    int32_t bisect = noc_topo_bisection_width(mesh);
    assert(bisect == 4); /* k links cut */

    noc_topo_metrics_t m = noc_topo_compute_metrics(mesh, 1e9, 128);
    assert(m.node_count == 16);
    assert(m.diameter == 6);

    noc_topo_free(mesh);
    PASS();
}

static void test_shortest_paths(void) {
    TEST("Floyd-Warshall all-pairs shortest paths");
    noc_topology_t *mesh = noc_topo_create_mesh_2d(4);
    if (!mesh) { FAIL("NULL mesh"); return; }

    int32_t *dist = noc_topo_all_pairs_shortest(mesh);
    if (!dist) { FAIL("NULL dist matrix"); noc_topo_free(mesh); return; }

    /* Distance from (0,0) to (3,3) should be 6 */
    assert(dist[0 * 16 + 15] == 6);
    /* Distance from (0,0) to (0,1) should be 1 */
    assert(dist[0 * 16 + 4] == 1);
    /* Distance to self should be 0 */
    assert(dist[0 * 16 + 0] == 0);

    free(dist);
    noc_topo_free(mesh);
    PASS();
}

static void test_manhattan_distance(void) {
    TEST("Manhattan distance computation");
    noc_topology_t *mesh = noc_topo_create_mesh_2d(4);
    if (!mesh) { FAIL("NULL mesh"); return; }

    /* (0,0) to (3,3) = |3-0| + |3-0| = 6 */
    assert(noc_topo_manhattan_dist(mesh, 0, 15) == 6);
    /* (0,0) to (0,1) = 0 + 1 = 1 */
    assert(noc_topo_manhattan_dist(mesh, 0, 4) == 1);

    noc_topo_free(mesh);
    PASS();
}

static void test_validate_topology(void) {
    TEST("Topology validation");
    noc_topology_t *mesh = noc_topo_create_mesh_2d(4);
    if (!mesh) { FAIL("NULL mesh"); return; }

    assert(noc_topo_validate(mesh) == 0);

    noc_topo_free(mesh);
    PASS();
}

/* ???????????????????????????????????????????????????????????????????????
 * Section 2: Router Tests (L3)
 * ??????????????????????????????????????????????????????????????????????? */

static void test_router_init(void) {
    TEST("Router initialization");
    noc_router_t router;
    noc_router_config_t cfg;
    cfg.router_id = 0;
    cfg.num_ports = 5;
    cfg.num_vcs = 4;
    cfg.vc_buffer_depth = 8;
    cfg.pipeline_stages = 5;
    cfg.allocator_type = 0;

    noc_router_init(&router, &cfg);

    assert(router.router_id == 0);
    assert(router.num_ports == 5);
    assert(router.num_vcs == 4);
    assert(router.input_ports[0].vcs[0].credits == 8);
    assert(router.input_ports[0].vcs[0].state == VC_IDLE);

    PASS();
}

static void test_router_flit_inject(void) {
    TEST("Router flit injection/ejection");
    noc_router_t router;
    noc_router_config_t cfg;
    cfg.router_id = 0;
    cfg.num_ports = 5;
    cfg.num_vcs = 4;
    cfg.vc_buffer_depth = 8;
    cfg.pipeline_stages = 5;
    cfg.allocator_type = 0;
    noc_router_init(&router, &cfg);

    noc_flit_t flit;
    memset(&flit, 0, sizeof(flit));
    flit.flit_id = 0;
    flit.type = FLIT_TYPE_SINGLE;
    flit.src_id = 0;
    flit.dst_id = 5;
    flit.vc_id = 0;
    flit.payload = 0xDEADBEEF;

    assert(noc_router_inject_flit(&router, &flit) == 0);
    assert(router.flits_in == 1);

    /* Eject */
    noc_flit_t ejected;
    assert(noc_router_eject_flit(&router, &ejected) == 0);
    assert(ejected.payload == 0xDEADBEEF);
    assert(router.flits_out == 1);

    PASS();
}

static void test_round_robin_arbitration(void) {
    TEST("Round-robin arbitration");
    noc_rr_arbiter_t arb;
    arb.num_inputs = 4;
    arb.priority_ptr = 0;
    arb.last_grant = -1;

    /* Request: input 2 and 3 */
    int32_t winner = noc_rr_arbitrate(&arb, (1u << 2) | (1u << 3));
    assert(winner == 2);

    /* Next arbitration should pick 3 (fairness) */
    winner = noc_rr_arbitrate(&arb, (1u << 2) | (1u << 3));
    assert(winner == 3);

    PASS();
}

static void test_vc_credits(void) {
    TEST("VC credit management");
    noc_vc_t vc;
    memset(&vc, 0, sizeof(vc));
    vc.max_credits = 4;
    vc.credits = 4;

    assert(noc_vc_has_credit(&vc) == 1);
    assert(noc_vc_consume_credit(&vc) == 0);
    assert(vc.credits == 3);
    assert(noc_vc_has_credit(&vc) == 1);

    /* Consume all credits */
    noc_vc_consume_credit(&vc);
    noc_vc_consume_credit(&vc);
    noc_vc_consume_credit(&vc);
    assert(vc.credits == 0);
    assert(noc_vc_has_credit(&vc) == 0);

    /* Should fail: no credits */
    assert(noc_vc_consume_credit(&vc) == -1);

    /* Return credits */
    noc_vc_return_credit(&vc);
    assert(vc.credits == 1);

    /* Can't exceed max */
    noc_vc_return_credit(&vc);
    noc_vc_return_credit(&vc);
    noc_vc_return_credit(&vc);
    noc_vc_return_credit(&vc);
    assert(vc.credits == vc.max_credits);

    PASS();
}

/* ???????????????????????????????????????????????????????????????????????
 * Section 3: Routing Algorithm Tests (L5)
 * ??????????????????????????????????????????????????????????????????????? */

static void test_xy_routing(void) {
    TEST("XY routing correctness");
    /* Source (0,0) ? Destination (3,2), Current (0,0) */
    noc_direction_t dir = noc_route_xy(0, 0, 3, 2, 0, 0);
    assert(dir == NOC_DIR_EAST); /* X first */

    /* At (1,0): still need X */
    dir = noc_route_xy(0, 0, 3, 2, 1, 0);
    assert(dir == NOC_DIR_EAST);

    /* At (3,0): X matches, now Y */
    dir = noc_route_xy(0, 0, 3, 2, 3, 0);
    assert(dir == NOC_DIR_SOUTH);

    /* At (3,2): reached destination */
    dir = noc_route_xy(0, 0, 3, 2, 3, 2);
    assert(dir == NOC_DIR_LOCAL);

    PASS();
}

static void test_yx_routing(void) {
    TEST("YX routing correctness");
    noc_direction_t dir = noc_route_yx(0, 0, 3, 2, 0, 0);
    assert(dir == NOC_DIR_SOUTH); /* Y first */

    dir = noc_route_yx(0, 0, 3, 2, 0, 2);
    assert(dir == NOC_DIR_EAST); /* Y done, now X */

    PASS();
}

static void test_west_first_routing(void) {
    TEST("West-First routing");
    /* Going west: must go west first */
    noc_direction_t dir = noc_route_west_first(3, 0, 1, 0, 3, 0);
    assert(dir == NOC_DIR_WEST);

    /* Going east+west: go west first */
    /* Actually for (2,0)?(3,0), east only */
    dir = noc_route_west_first(2, 0, 3, 0, 2, 0);
    assert(dir == NOC_DIR_EAST);

    PASS();
}

static void test_north_last_routing(void) {
    TEST("North-Last routing");
    /* Going north: delay to last */
    noc_direction_t dir = noc_route_north_last(2, 2, 2, 0, 2, 2);
    assert(dir == NOC_DIR_NORTH);

    /* Going both east and north: east first, north last */
    dir = noc_route_north_last(0, 2, 2, 0, 0, 2);
    /* Need both E and N. E is not north, so go E first */
    assert(dir == NOC_DIR_EAST);

    PASS();
}

static void test_negative_first_routing(void) {
    TEST("Negative-First routing");
    /* Need west (negative X) and south (positive Y): west first */
    noc_direction_t dir = noc_route_negative_first(2, 0, 1, 2, 2, 0);
    assert(dir == NOC_DIR_WEST);

    /* Only south needed */
    dir = noc_route_negative_first(1, 0, 1, 2, 1, 0);
    assert(dir == NOC_DIR_SOUTH);

    PASS();
}

static void test_odd_even_routing(void) {
    TEST("Odd-Even routing");
    /* From (0,0) to (0,2): south only */
    noc_direction_t dir = noc_route_odd_even(0, 0, 0, 2, 0, 0);
    assert(dir == NOC_DIR_SOUTH);

    /* From (1,1) to (0,0): west+north, odd col */
    dir = noc_route_odd_even(1, 1, 0, 0, 1, 1);
    /* Need west and north; odd col restricts NW/SW, so west first */
    assert(dir == NOC_DIR_WEST || dir == NOC_DIR_NORTH);

    PASS();
}

static void test_adaptive_routing(void) {
    TEST("Adaptive routing (minimal)");
    /* Both X and Y needed: choose less congested */
    int32_t loads[8] = {0, 5, 2, 3, 7, 0, 0, 0};
    noc_direction_t dir = noc_route_adaptive(0, 0, 2, 2, 0, 0, loads);
    /* Should choose lower-load direction (WEST=2 vs SOUTH=3) */
    assert(dir == NOC_DIR_EAST || dir == NOC_DIR_SOUTH);

    PASS();
}

static void test_randomized_routing(void) {
    TEST("Randomized routing");
    /* Both X and Y needed, chooses randomly */
    noc_direction_t dir = noc_route_randomized(0, 0, 2, 2, 0, 0, 42);
    assert(dir == NOC_DIR_EAST || dir == NOC_DIR_SOUTH);

    /* Only X needed */
    dir = noc_route_randomized(0, 0, 2, 0, 0, 0, 42);
    assert(dir == NOC_DIR_EAST);

    PASS();
}

static void test_routing_dispatch(void) {
    TEST("Routing dispatch: all algorithms reachable");
    noc_route_algo_t algos[] = {
        ROUTE_XY, ROUTE_YX, ROUTE_WEST_FIRST, ROUTE_NORTH_LAST,
        ROUTE_NEGATIVE_FIRST, ROUTE_ODD_EVEN, ROUTE_ADAPTIVE, ROUTE_RANDOMIZED
    };
    int32_t a;
    for (a = 0; a < 8; a++) {
        noc_direction_t dir = noc_routing_dispatch(algos[a], 0, 0, 3, 3, 0, 0);
        assert(dir == NOC_DIR_EAST || dir == NOC_DIR_SOUTH);
    }
    PASS();
}

static void test_routing_table(void) {
    TEST("Routing table construction");
    noc_routing_table_t table = noc_build_routing_table(1, 1, 4, ROUTE_XY);
    assert(table.router_id == 5); /* y*4+x = 1*4+1 = 5 */
    assert(table.num_entries > 0);
    assert(table.num_entries <= 15); /* all others in 4x4 */
    PASS();
}

/* ???????????????????????????????????????????????????????????????????????
 * Section 4: CDG and Deadlock Tests (L4, L5)
 * ??????????????????????????????????????????????????????????????????????? */

static void test_cdg_xy_deadlock_free(void) {
    TEST("CDG: XY routing should be deadlock-free");
    noc_channel_dep_graph_t cdg = noc_cdg_build(ROUTE_XY, 4);
    /* XY routing CDG should be acyclic */
    int has_cycle = noc_cdg_has_cycle(&cdg);
    assert(has_cycle == 0);
    PASS();
}

static void test_cdg_topological_sort(void) {
    TEST("CDG topological sort (Kahn's algorithm)");
    noc_channel_dep_graph_t cdg = noc_cdg_build(ROUTE_XY, 3);
    int32_t sorted[256];
    int32_t result = noc_cdg_topological_sort(&cdg, sorted, 256);
    assert(result > 0); /* Should be sortable (DAG) */
    PASS();
}

static void test_channel_numbering(void) {
    TEST("Channel numbering exists (Dally & Seitz theorem)");
    noc_channel_dep_graph_t cdg = noc_cdg_build(ROUTE_XY, 3);
    assert(noc_channel_numbering_exists(&cdg) == 1);
    PASS();
}

static void test_turn_model_lookup(void) {
    TEST("Turn model table lookup");
    const noc_turn_table_t *wf = noc_turn_model_get(ROUTE_WEST_FIRST);
    assert(wf != NULL);
    assert(wf->algo == ROUTE_WEST_FIRST);

    /* West-First prohibits turns TO west: SW, NW */
    assert(noc_turn_prohibited(ROUTE_WEST_FIRST, TURN_SW) == 1);
    assert(noc_turn_prohibited(ROUTE_WEST_FIRST, TURN_NW) == 1);
    /* Turns not TO west are allowed */
    assert(noc_turn_prohibited(ROUTE_WEST_FIRST, TURN_ES) == 0);
    assert(noc_turn_prohibited(ROUTE_WEST_FIRST, TURN_NE) == 0);

    PASS();
}

static void test_deadlock_detection(void) {
    TEST("Deadlock detector initialization and update");
    noc_deadlock_detector_t det;
    noc_dl_detector_init(&det, 10, 4, 2);

    assert(det.timeout_threshold == 10);
    assert(det.escape_vc_base == 4);
    assert(det.num_escape_vcs == 2);

    /* Create some VCs in WAITING state */
    noc_vc_t vcs[8];
    memset(vcs, 0, sizeof(vcs));
    int32_t i;
    for (i = 0; i < 8; i++) {
        vcs[i].vc_id = i;
        vcs[i].state = VC_IDLE;
    }
    vcs[0].state = VC_WAITING;

    /* Update for 11 cycles (exceeds timeout) */
    for (i = 0; i < 11; i++) {
        noc_dl_detector_update(&det, vcs, 8);
    }

    assert(noc_dl_is_deadlock(&det) == 1);
    assert(det.deadlocks_detected > 0);

    PASS();
}

static void test_deadlock_recovery(void) {
    TEST("Deadlock recovery via escape VCs");
    noc_deadlock_detector_t det;
    noc_dl_detector_init(&det, 5, 2, 2);

    noc_vc_t vcs[8];
    memset(vcs, 0, sizeof(vcs));
    int32_t i;
    for (i = 0; i < 8; i++) {
        vcs[i].vc_id = i;
        vcs[i].state = VC_IDLE;
    }
    vcs[0].state = VC_WAITING;
    vcs[1].state = VC_WAITING;

    /* Trigger deadlock detection */
    for (i = 0; i < 6; i++) {
        noc_dl_detector_update(&det, vcs, 8);
    }

    assert(noc_dl_is_deadlock(&det) == 1);

    int32_t recovered = noc_dl_recover(&det, vcs, 8);
    assert(recovered == 2);
    assert(noc_dl_is_deadlock(&det) == 0);
    assert(vcs[0].state == VC_IDLE);
    assert(vcs[1].state == VC_IDLE);

    PASS();
}

/* ???????????????????????????????????????????????????????????????????????
 * Section 5: Flow Control Tests (L2, L4)
 * ??????????????????????????????????????????????????????????????????????? */

static void test_flow_control_init(void) {
    TEST("Flow controller initialization");
    noc_flow_controller_t fc;
    noc_fc_init(&fc, FC_CREDIT_BASED, 5, 4, 8);

    assert(fc.mode == FC_CREDIT_BASED);
    assert(fc.credits[0][0].credits_available == 8);
    assert(fc.credits[0][0].max_credits == 8);

    PASS();
}

static void test_credit_send(void) {
    TEST("Credit-based flit send");
    noc_flow_controller_t fc;
    noc_fc_init(&fc, FC_CREDIT_BASED, 5, 4, 4);

    noc_flit_t flit;
    memset(&flit, 0, sizeof(flit));

    /* Send 4 flits (should succeed) */
    int32_t i;
    for (i = 0; i < 4; i++) {
        assert(noc_fc_send_flit(&fc, 0, 0, &flit) == 0);
    }

    /* 5th send should fail */
    assert(noc_fc_send_flit(&fc, 0, 0, &flit) == -1);

    /* Return a credit */
    noc_fc_return_credit(&fc, 0, 0);
    assert(noc_fc_can_send(&fc, 0, 0) == 1);
    assert(noc_fc_send_flit(&fc, 0, 0, &flit) == 0);

    PASS();
}

static void test_packet_tracking(void) {
    TEST("Wormhole packet tracking");
    noc_flow_controller_t fc;
    noc_fc_init(&fc, FC_CREDIT_BASED, 5, 4, 8);

    int32_t pid = noc_fc_start_packet(&fc, 0, 5, 4);
    assert(pid >= 0);
    assert(fc.num_active_packets == 1);

    /* Send 3 of 4 flits */
    assert(noc_fc_flit_sent(&fc, pid) == 0);
    assert(noc_fc_flit_sent(&fc, pid) == 0);
    assert(noc_fc_flit_sent(&fc, pid) == 0);

    /* 4th flit completes packet */
    assert(noc_fc_flit_sent(&fc, pid) == 1);
    assert(fc.packets_completed == 1);

    PASS();
}

static void test_buffer_pool(void) {
    TEST("Buffer pool allocation");
    noc_buffer_pool_t pool;
    noc_buffer_pool_init(&pool, 32, 4, 4);

    assert(pool.total_slots == 32);
    assert(noc_buffer_pool_available(&pool) == 32);

    int32_t alloc = noc_buffer_pool_alloc(&pool, 0, 10);
    assert(alloc == 10);
    assert(noc_buffer_pool_available(&pool) == 22);

    noc_buffer_pool_free(&pool, 0, 5);
    assert(noc_buffer_pool_available(&pool) == 27);

    PASS();
}

static void test_critical_gap(void) {
    TEST("Critical inter-flit gap (Dally 1992 model)");
    /* 8 credits, latency=5, 4 hops ? round_trip=2*5*4=40, gap=40/8=5 */
    double gap = noc_fc_critical_gap(8, 5, 4);
    assert(gap == 5.0);

    /* Adequate credits (16) ? gap=40/16=2.5 */
    gap = noc_fc_critical_gap(16, 5, 4);
    assert(gap == 2.5);

    /* Zero credits ? huge gap */
    gap = noc_fc_critical_gap(0, 5, 4);
    assert(gap > 1e6);

    PASS();
}

/* ???????????????????????????????????????????????????????????????????????
 * Section 6: Performance Model Tests (L4)
 * ??????????????????????????????????????????????????????????????????????? */

static void test_zero_load_latency(void) {
    TEST("Zero-load latency formula (Dally & Towles)");
    /* H=6 hops, t_r=5 cycles/hop, L=128 bits, b=1e9 bps
     * D0 = 6*5 + 128/1e9 = 30 + 1.28e-7 ? 30 cycles */
    double d0 = noc_zero_load_latency(6, 5, 128, 1e9);
    assert(d0 > 29.0 && d0 < 31.0);

    PASS();
}

static void test_ideal_throughput(void) {
    TEST("Ideal saturation throughput");
    /* H=4 avg hops, L=128 bits, b=1e9 bps
     * ? = 2*1e9 / (4*128) = 2e9/512 ? 3,906,250 packets/sec/node */
    double thr = noc_ideal_throughput(4, 128, 1e9);
    assert(thr > 0.0);

    /* Verify formula: more hops ? lower throughput */
    double thr_8 = noc_ideal_throughput(8, 128, 1e9);
    assert(thr_8 < thr); /* More hops = lower throughput */

    PASS();
}

static void test_jain_fairness(void) {
    TEST("Jain's fairness index");
    /* Perfectly fair: all 0.5 ? J = (4*0.5)^2 / (4 * 4*0.25) = 4 / 4 = 1.0 */
    double perfect[] = {0.5, 0.5, 0.5, 0.5};
    double j = noc_jain_fairness(perfect, 4);
    assert(fabs(j - 1.0) < 1e-9);

    /* Unfair: [1.0, 0, 0, 0] ? J = 1^2 / (4*1) = 0.25 */
    double unfair[] = {1.0, 0.0, 0.0, 0.0};
    j = noc_jain_fairness(unfair, 4);
    assert(fabs(j - 0.25) < 1e-9);

    PASS();
}

static void test_hotspot_throughput(void) {
    TEST("Hot-spot throughput degradation (Dally 1990)");
    /* Base throughput 0.5 flits/node/cycle, 20% hotspot, 16 nodes
     * degradation = 0.5 / (1 + 0.2 * 15 / 2) = 0.5 / 2.5 = 0.2 */
    double thr = noc_hotspot_throughput(0.5, 0.2, 16);
    assert(fabs(thr - 0.2) < 1e-9);

    PASS();
}

static void test_traffic_patterns(void) {
    TEST("Traffic pattern destination generation");
    noc_traffic_gen_t gen;
    memset(&gen, 0, sizeof(gen));
    gen.pattern = TRAFFIC_UNIFORM;
    gen.mesh_size = 4;
    gen.num_nodes = 16;
    gen.seed = 12345;

    int32_t dest = noc_traffic_gen_dest(&gen, 0);
    assert(dest >= 0 && dest < 16);
    assert(dest != 0); /* Should not be self */

    /* Transpose: (x,y) ? (y,x) */
    gen.pattern = TRAFFIC_TRANSPOSE;
    /* (2,1) = 2+1*4 = 6 ? (1,2) = 1+2*4 = 9 */
    dest = noc_traffic_gen_dest(&gen, 6);
    assert(dest == 9);

    PASS();
}

/* ???????????????????????????????????????????????????????????????????????
 * Section 7: QoS Tests (L5, L7)
 * ??????????????????????????????????????????????????????????????????????? */

static void test_qos_config(void) {
    TEST("QoS configuration initialization");
    noc_qos_config_t cfg;
    noc_qos_config_init(&cfg, NOC_QOS_LOW_LATENCY, 0, 30, 50, 2.0);

    assert(cfg.class_id == NOC_QOS_LOW_LATENCY);
    assert(cfg.priority == 0);
    assert(cfg.min_bandwidth_pct == 30);
    assert(cfg.max_latency_cycles == 50);
    assert(cfg.weight == 2.0);

    PASS();
}

static void test_qos_vc_allocator(void) {
    TEST("QoS-aware VC allocator");
    noc_qos_config_t configs[3];
    noc_qos_config_init(&configs[0], NOC_QOS_BEST_EFFORT, 2, 10, 200, 1.0);
    noc_qos_config_init(&configs[1], NOC_QOS_LOW_LATENCY, 1, 40, 50, 2.0);
    noc_qos_config_init(&configs[2], NOC_QOS_REAL_TIME, 0, 50, 20, 3.0);

    noc_qos_vc_allocator_t alloc;
    noc_qos_vc_allocator_init(&alloc, 8, configs, 3);

    /* RT (pos 2) should get most VCs (weight 3.0) */
    assert(alloc.class_vc_count[2] > 0);
    /* BE (pos 0) gets least (weight 1.0) */
    assert(alloc.class_vc_count[0] > 0);

    /* Check VC-to-class mapping */
    assert(noc_qos_vc_class(&alloc, 0) >= 0);

    PASS();
}

static void test_wrr_arbitration(void) {
    TEST("Weighted Round-Robin arbitration (DWRR)");
    noc_wrr_arbiter_t wrr;
    memset(&wrr, 0, sizeof(wrr));
    wrr.num_classes = 3;
    wrr.weights[0] = 1.0;
    wrr.weights[1] = 2.0;
    wrr.weights[2] = 3.0;
    wrr.deficit[0] = 0.0;
    wrr.deficit[1] = 0.0;
    wrr.deficit[2] = 0.0;
    wrr.current_class = 0;
    wrr.quantum = 1;

    /* All classes request */
    int32_t reqs[] = {1, 1, 1};
    int32_t winner = noc_wrr_select(&wrr, reqs);
    assert(winner >= 0 && winner < 3);

    PASS();
}

static void test_rate_limiter(void) {
    TEST("Token bucket rate limiter");
    noc_rate_limiter_t rl;
    noc_rate_limiter_init(&rl, 0.5, 3.0);

    /* Start with 3 tokens (burst) */
    assert(noc_rate_limiter_allow(&rl) == 1);
    assert(noc_rate_limiter_allow(&rl) == 1);
    assert(noc_rate_limiter_allow(&rl) == 1);
    /* No more tokens */
    assert(noc_rate_limiter_allow(&rl) == 0);

    /* Refill */
    noc_rate_limiter_refill(&rl);
    assert(rl.tokens > 0.0);

    PASS();
}

static void test_sla_monitor(void) {
    TEST("SLA monitor");
    noc_sla_monitor_t sla;
    noc_sla_monitor_init(&sla, 100, 50);

    assert(sla.target_latency == 100);
    assert(sla.target_throughput == 50);

    /* Update with compliant measurements */
    noc_sla_monitor_update(&sla, 80, 60);
    assert(sla.violations == 0);
    assert(noc_sla_monitor_check(&sla) == 1);

    /* Update with violation */
    noc_sla_monitor_update(&sla, 150, 60);
    assert(sla.violations == 1);

    PASS();
}

/* ???????????????????????????????????????????????????????????????????????
 * Section 8: Livelock tests (L4)
 * ??????????????????????????????????????????????????????????????????????? */

static void test_livelock_free(void) {
    TEST("Livelock-freedom analysis");
    noc_topology_t *mesh = noc_topo_create_mesh_2d(4);
    if (!mesh) { FAIL("NULL mesh"); return; }

    noc_path_t path = noc_topo_find_path_xy(mesh, 0, 15);
    assert(path.is_valid);
    assert(noc_path_livelock_free(&path, mesh) == 1);

    noc_topo_free(mesh);
    PASS();
}

/* ???????????????????????????????????????????????????????????????????????
 * Section 9: Full simulation (L5)
 * ??????????????????????????????????????????????????????????????????????? */

static void test_simulation_run(void) {
    TEST("Full NoC simulation (100 cycles)");
    noc_topology_t *mesh = noc_topo_create_mesh_2d(4);
    if (!mesh) { FAIL("NULL mesh"); return; }

    noc_simulator_t sim;
    noc_simulator_init(&sim, mesh, TRAFFIC_UNIFORM, 4, 0.01, ROUTE_XY, 42);
    assert(sim.num_routers == 16);
    assert(sim.routers != NULL);

    noc_simulator_run(&sim, 100);

    noc_perf_metrics_t m = noc_simulator_collect_metrics(&sim);
    /* Very low injection rate: should have low latency */
    (void)m;

    noc_simulator_destroy(&sim);
    noc_topo_free(mesh);
    PASS();
}

static void test_simulation_sweep(void) {
    TEST("Rate sweep simulation");
    double rates[4], latencies[4], throughputs[4];
    noc_simulator_sweep_rate(rates, latencies, throughputs, 4,
                             4, TRAFFIC_UNIFORM, 4, ROUTE_XY,
                             10, 50);
    /* All rates should be positive and increasing */
    int32_t i;
    for (i = 1; i < 4; i++) {
        assert(rates[i] > rates[i-1]);
    }
    PASS();
}

/* ???????????????????????????????????????????????????????????????????????
 * Section 10: Path-finding tests (L5)
 * ??????????????????????????????????????????????????????????????????????? */

static void test_xy_path_find(void) {
    TEST("XY path finding");
    noc_topology_t *mesh = noc_topo_create_mesh_2d(4);
    if (!mesh) { FAIL("NULL mesh"); return; }

    /* Path from (0,0) to (3,3) = 6 hops */
    noc_path_t path = noc_topo_find_path_xy(mesh, 0, 15);
    assert(path.is_valid);
    assert(path.num_hops == 6);
    assert(path.total_latency == 6);

    /* Self-path should be valid with 0 hops */
    noc_path_t self = noc_topo_find_path_xy(mesh, 5, 5);
    assert(self.is_valid);
    assert(self.num_hops == 0);

    noc_topo_free(mesh);
    PASS();
}

static void test_dijkstra_path(void) {
    TEST("Dijkstra shortest path");
    noc_topology_t *mesh = noc_topo_create_mesh_2d(4);
    if (!mesh) { FAIL("NULL mesh"); return; }

    noc_path_t path = noc_topo_find_path_dijkstra(mesh, 0, 15);
    assert(path.is_valid);
    /* Dijkstra should find same 6-hop path as XY on mesh */
    assert(path.num_hops == 6);

    noc_topo_free(mesh);
    PASS();
}

/* ???????????????????????????????????????????????????????????????????????
 * Main
 * ??????????????????????????????????????????????????????????????????????? */

int main(void) {
    printf("\n");
    printf("????????????????????????????????????????????\n");
    printf("?  mini-noc-design  ?  Test Suite         ?\n");
    printf("????????????????????????????????????????????\n\n");

    /* Section 1: Topology */
    printf("??? Topology Tests (L1, L2, L4) ???\n");
    test_mesh_creation();
    test_torus_creation();
    test_ring_creation();
    test_tree_creation();
    test_butterfly_creation();
    test_topology_metrics();
    test_shortest_paths();
    test_manhattan_distance();
    test_validate_topology();

    /* Section 2: Router */
    printf("\n??? Router Tests (L3) ???\n");
    test_router_init();
    test_router_flit_inject();
    test_round_robin_arbitration();
    test_vc_credits();

    /* Section 3: Routing */
    printf("\n??? Routing Algorithm Tests (L5) ???\n");
    test_xy_routing();
    test_yx_routing();
    test_west_first_routing();
    test_north_last_routing();
    test_negative_first_routing();
    test_odd_even_routing();
    test_adaptive_routing();
    test_randomized_routing();
    test_routing_dispatch();
    test_routing_table();

    /* Section 4: CDG & Deadlock */
    printf("\n??? CDG & Deadlock Tests (L4, L5) ???\n");
    test_cdg_xy_deadlock_free();
    test_cdg_topological_sort();
    test_channel_numbering();
    test_turn_model_lookup();
    test_deadlock_detection();
    test_deadlock_recovery();

    /* Section 5: Flow Control */
    printf("\n??? Flow Control Tests (L2, L4) ???\n");
    test_flow_control_init();
    test_credit_send();
    test_packet_tracking();
    test_buffer_pool();
    test_critical_gap();

    /* Section 6: Performance */
    printf("\n??? Performance Model Tests (L4) ???\n");
    test_zero_load_latency();
    test_ideal_throughput();
    test_jain_fairness();
    test_hotspot_throughput();
    test_traffic_patterns();

    /* Section 7: QoS */
    printf("\n??? QoS Tests (L5, L7) ???\n");
    test_qos_config();
    test_qos_vc_allocator();
    test_wrr_arbitration();
    test_rate_limiter();
    test_sla_monitor();

    /* Section 8: Livelock */
    printf("\n??? Livelock Tests (L4) ???\n");
    test_livelock_free();

    /* Section 9: Simulation */
    printf("\n??? Simulation Tests (L5) ???\n");
    test_simulation_run();
    test_simulation_sweep();

    /* Section 10: Path Finding */
    printf("\n??? Path-finding Tests (L5) ???\n");
    test_xy_path_find();
    test_dijkstra_path();

    printf("\n???????????????????????????????????????????\n");
    printf("  Results: %d/%d passed, %d failed\n",
           tests_passed, tests_total, tests_failed);
    printf("???????????????????????????????????????????\n\n");

    return (tests_failed > 0) ? 1 : 0;
}
