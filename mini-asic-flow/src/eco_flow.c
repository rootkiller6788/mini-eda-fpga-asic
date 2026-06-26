#include "eco_flow.h"
#include "floorplan_cts.h"
#include <stdio.h>
#include <string.h>

void eco_flow_init(eco_flow_t *eco, eco_type_t type)
{
    memset(eco, 0, sizeof(*eco));
    eco->eco_type          = type;
    eco->current_iteration = 0;
    eco->max_iterations    = 3;
    eco->next_id           = 0;
}

void eco_flow_set_max_iterations(eco_flow_t *eco, int max_iter)
{
    eco->max_iterations = max_iter;
}

static int eco_push_operation(eco_flow_t *eco, eco_op_type_t op_type,
                              const char *cell_name, const char *from_net,
                              const char *to_net)
{
    eco_operation_t *op;
    if (eco->operation_count >= MAX_ECO_OPS) return -1;
    op = &eco->operations[eco->operation_count++];
    memset(op, 0, sizeof(*op));
    op->id      = eco->next_id++;
    op->op_type  = op_type;
    op->iteration = eco->current_iteration;
    if (cell_name) strncpy(op->cell_name, cell_name, sizeof(op->cell_name) - 1);
    if (from_net)  strncpy(op->from_net, from_net, sizeof(op->from_net) - 1);
    if (to_net)    strncpy(op->to_net, to_net, sizeof(op->to_net) - 1);
    op->state = ECO_STATE_PENDING;
    return 0;
}

int eco_add_cell(eco_flow_t *eco, const char *cell_name,
                 double x_um, double y_um)
{
    int rc = eco_push_operation(eco, ECO_OP_ADD_CELL, cell_name, NULL, NULL);
    if (rc == 0) {
        eco->operations[eco->operation_count - 1].x_um = x_um;
        eco->operations[eco->operation_count - 1].y_um = y_um;
        eco->cells_added++;
    }
    return rc;
}

int eco_remove_cell(eco_flow_t *eco, const char *cell_name)
{
    int rc = eco_push_operation(eco, ECO_OP_REMOVE_CELL, cell_name, NULL, NULL);
    if (rc == 0) eco->cells_removed++;
    return rc;
}

int eco_reconnect_net(eco_flow_t *eco, const char *from_net,
                      const char *to_net, const char *cell, int pin)
{
    int rc = eco_push_operation(eco, ECO_OP_RECONNECT, cell, from_net, to_net);
    if (rc == 0) {
        eco->operations[eco->operation_count - 1].pin_index = pin;
        eco->nets_changed++;
    }
    return rc;
}

int eco_insert_buffer(eco_flow_t *eco, const char *net_name,
                      int stage, int drive_strength)
{
    int rc = eco_push_operation(eco, ECO_OP_INSERT_BUFFER, net_name, NULL, NULL);
    if (rc == 0) {
        eco->operations[eco->operation_count - 1].stage     = stage;
        eco->operations[eco->operation_count - 1].new_drive_strength = drive_strength;
        eco->buffers_inserted++;
    }
    return rc;
}

int eco_change_drive_strength(eco_flow_t *eco, const char *cell_name,
                               int new_strength)
{
    int rc = eco_push_operation(eco, ECO_OP_CHANGE_DRIVE, cell_name, NULL, NULL);
    if (rc == 0) {
        eco->operations[eco->operation_count - 1].new_drive_strength = new_strength;
        eco->cells_added++;
        eco->cells_removed++;
    }
    return rc;
}

int eco_tie_pin(eco_flow_t *eco, const char *cell, int pin, int tie_high)
{
    int rc = eco_push_operation(eco,
                                tie_high ? ECO_OP_TIE_HIGH : ECO_OP_TIE_LOW,
                                cell, NULL, NULL);
    if (rc == 0) {
        eco->operations[eco->operation_count - 1].pin_index = pin;
    }
    return rc;
}

int eco_apply_operations(eco_flow_t *eco)
{
    int i, applied = 0;
    for (i = 0; i < eco->operation_count; i++) {
        if (eco->operations[i].state == ECO_STATE_PENDING) {
            eco->operations[i].state = ECO_STATE_APPLIED;
            applied++;
        }
    }
    return applied;
}

int eco_verify_operations(eco_flow_t *eco)
{
    int i, verified = 0, failed = 0;
    for (i = 0; i < eco->operation_count; i++) {
        if (eco->operations[i].state == ECO_STATE_APPLIED) {
            eco->operations[i].state = ECO_STATE_VERIFIED;
            verified++;
        }
    }
    if (failed > 0 && eco->current_iteration < eco->max_iterations) {
        eco->current_iteration++;
    }
    return verified;
}

int eco_revert_operation(eco_flow_t *eco, int op_index)
{
    if (op_index < 0 || op_index >= eco->operation_count) return -1;
    if (eco->operations[op_index].state == ECO_STATE_REVERTED) return -1;
    eco->operations[op_index].state = ECO_STATE_REVERTED;
    return 0;
}

int eco_metal_only_add_route(eco_flow_t *eco, const char *cell,
                              int layer, double x1, double y1,
                              double x2, double y2)
{
    int rc = eco_push_operation(eco, ECO_OP_RECONNECT, cell, NULL, NULL);
    if (rc == 0) {
        eco_operation_t *op = &eco->operations[eco->operation_count - 1];
        op->x_um = x1;
        op->y_um = y1;
        op->metal_layer = layer;
    }
    (void)x2; (void)y2;
    return rc;
}

void spare_cell_init(spare_cell_array_t *spa, double density_pct,
                     double col_spacing, double row_spacing)
{
    memset(spa, 0, sizeof(*spa));
    spa->spare_density_pct = density_pct;
    spa->column_spacing_um = col_spacing;
    spa->row_spacing_um    = row_spacing;
    spa->spare_strategy    = 0;
}

int spare_cell_place(spare_cell_array_t *spa, const floorplan_t *fp,
                     int cell_count)
{
    int i, cols_per_row, rows;
    double x, y;
    if (cell_count > MAX_SPARE_CELLS) cell_count = MAX_SPARE_CELLS;
    cols_per_row = (int)(fp->core_width_um / spa->column_spacing_um);
    if (cols_per_row < 1) cols_per_row = 1;
    rows = (cell_count + cols_per_row - 1) / cols_per_row;

    for (i = 0; i < cell_count; i++) {
        int r = i / cols_per_row;
        int c = i % cols_per_row;
        spare_cell_t *sc = &spa->spare_cells[i];
        sc->id          = i;
        snprintf(sc->cell_name, sizeof(sc->cell_name), "SPARE_%d", i);
        sc->is_used     = 0;
        sc->is_broken   = 0;
        sc->x_um        = (double)c * spa->column_spacing_um + 10.0;
        sc->y_um        = (double)r * spa->row_spacing_um + 10.0;
        sc->input_pin_count  = 2;
        sc->output_pin_count = 1;
    }
    spa->spare_count = cell_count;
    (void)rows;
    return cell_count;
}

int spare_cell_allocate(spare_cell_array_t *spa, const char *cell_name,
                         double x_um, double y_um)
{
    int i;
    double best_dist = 1e18;
    int    best_idx  = -1;
    for (i = 0; i < spa->spare_count; i++) {
        if (!spa->spare_cells[i].is_used && !spa->spare_cells[i].is_broken) {
            double dx = spa->spare_cells[i].x_um - x_um;
            double dy = spa->spare_cells[i].y_um - y_um;
            double dist = dx * dx + dy * dy;
            if (dist < best_dist) {
                best_dist = dist;
                best_idx  = i;
            }
        }
    }
    if (best_idx >= 0) {
        spa->spare_cells[best_idx].is_used = 1;
        strncpy(spa->spare_cells[best_idx].cell_name, cell_name,
                sizeof(spa->spare_cells[best_idx].cell_name) - 1);
    }
    return best_idx;
}

int spare_cell_release(spare_cell_array_t *spa, int spare_id)
{
    if (spare_id < 0 || spare_id >= spa->spare_count) return -1;
    spa->spare_cells[spare_id].is_used = 0;
    return 0;
}

int spare_cell_count_available(const spare_cell_array_t *spa)
{
    int i, count = 0;
    for (i = 0; i < spa->spare_count; i++) {
        if (!spa->spare_cells[i].is_used && !spa->spare_cells[i].is_broken) {
            count++;
        }
    }
    return count;
}

int spare_cell_count_used(const spare_cell_array_t *spa)
{
    int i, count = 0;
    for (i = 0; i < spa->spare_count; i++) {
        if (spa->spare_cells[i].is_used) count++;
    }
    return count;
}

int eco_flow_save(const eco_flow_t *eco, const char *filename)
{
    FILE *f = fopen(filename, "w");
    int i;
    if (!f) return -1;
    fprintf(f, "ECO %d %d %d\n", eco->eco_type, eco->current_iteration,
            eco->max_iterations);
    fprintf(f, "STATS %d %d %d %d\n", eco->cells_added, eco->cells_removed,
            eco->nets_changed, eco->buffers_inserted);
    for (i = 0; i < eco->operation_count; i++) {
        const eco_operation_t *op = &eco->operations[i];
        fprintf(f, "OP %d %d %s %s %s %d %d\n",
                op->id, op->op_type, op->cell_name, op->from_net,
                op->to_net, op->state, op->iteration);
    }
    fclose(f);
    return 0;
}

int eco_flow_load(eco_flow_t *eco, const char *filename)
{
    FILE *f = fopen(filename, "r");
    char hdr[32];
    int i, count;
    if (!f) return -1;
    fscanf(f, "%s %d %d %d", hdr, (int*)&eco->eco_type,
           &eco->current_iteration, &eco->max_iterations);
    fscanf(f, "%s %d %d %d %d", hdr, &eco->cells_added, &eco->cells_removed,
           &eco->nets_changed, &eco->buffers_inserted);
    for (i = 0; i < MAX_ECO_OPS; i++) {
        eco_operation_t *op = &eco->operations[i];
        if (fscanf(f, "%s %d %d %s %s %s %d %d", hdr, &op->id,
                   (int*)&op->op_type, op->cell_name, op->from_net,
                   op->to_net, (int*)&op->state, &op->iteration) != 8) break;
        count++;
    }
    eco->operation_count = count;
    eco->next_id = count;
    fclose(f);
    return 0;
}

int eco_flow_signoff(const eco_flow_t *eco)
{
    int i;
    for (i = 0; i < eco->operation_count; i++) {
        if (eco->operations[i].state != ECO_STATE_VERIFIED &&
            eco->operations[i].state != ECO_STATE_REVERTED) {
            return 0;
        }
    }
    return 1;
}

void eco_flow_report(const eco_flow_t *eco)
{
    int i;
    printf("=== ECO Flow Report ===\n");
    printf("Type: %d  Iterations: %d/%d\n",
           eco->eco_type, eco->current_iteration, eco->max_iterations);
    printf("Added: %d  Removed: %d  Nets changed: %d  Buffers: %d\n",
           eco->cells_added, eco->cells_removed,
           eco->nets_changed, eco->buffers_inserted);
    printf("Sign-off: %s\n", eco_flow_signoff(eco) ? "PASS" : "PENDING");
    printf("Operations: %d\n", eco->operation_count);
    for (i = 0; i < eco->operation_count && i < 20; i++) {
        printf("  [%d] id=%d type=%d cell=%s state=%d iter=%d\n",
               i, eco->operations[i].id, eco->operations[i].op_type,
               eco->operations[i].cell_name, eco->operations[i].state,
               eco->operations[i].iteration);
    }
    if (eco->operation_count > 20) printf("  ... +%d more\n",
                                          eco->operation_count - 20);
}

void spare_cell_report(const spare_cell_array_t *spa)
{
    printf("=== Spare Cell Report ===\n");
    printf("Total: %d  Used: %d  Available: %d  Broken: %d\n",
           spa->spare_count,
           spare_cell_count_used(spa),
           spare_cell_count_available(spa),
           spa->spare_count - spare_cell_count_available(spa) -
           spare_cell_count_used(spa));
    printf("Density: %.1f%%  Col spacing: %.1f um  Row spacing: %.1f um\n",
           spa->spare_density_pct, spa->column_spacing_um,
           spa->row_spacing_um);
}
