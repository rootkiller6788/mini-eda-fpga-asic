#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "io_planning.h"

static void print_section(const char *title)
{
    printf("\n========================================\n");
    printf("  %s\n", title);
    printf("========================================\n");
}

/* -------------------------------------------------------
 * Helper: create a test bank with predefined pins
 * ------------------------------------------------------- */
static void create_test_bank(IoBank *bank, int bank_num, double vcco,
                              const char **pin_names, int num_pins)
{
    io_bank_init(bank, bank_num, vcco);
    for (int i = 0; i < num_pins; i++) {
        IoPin pin;
        io_pin_init(&pin, pin_names[i], bank_num);
        io_bank_add_pin(bank, &pin);
    }
}

/* -------------------------------------------------------
 * Demo 1: I/O Pin Configuration
 * ------------------------------------------------------- */
static void demo_pin_configuration(void)
{
    print_section("Demo 1: I/O Pin Configuration");

    /* Single pin configuration */
    IoPin pin;
    io_pin_init(&pin, "Y11", 34);

    printf("  Default pin: %s\n", pin.pin_name);
    printf("    Bank:       %d\n", pin.bank_number);
    printf("    Direction:  %d (0=input, 1=output, 2=bidir)\n", pin.direction);
    printf("    Std:        %s\n", iostd_to_string(pin.io_standard));
    printf("    Drive:      %d\n", pin.drive_strength);
    printf("    Vcco:       %.1fV\n", pin.bank_vcco_v);

    /* Assign signal */
    io_pin_assign_signal(&pin, "uart_tx", IO_DIR_OUTPUT, IOSTD_LVCMOS33);
    io_pin_set_drive(&pin, DRIVE_16MA);
    printf("\n  Assigned 'uart_tx' (output, LVCMOS33, 16mA):\n");
    printf("    Signal:     %s\n", pin.signal_name);
    printf("    Direction:  %d (output)\n", pin.direction);
    printf("    Drive:      %d (16mA)\n", pin.drive_strength);
    printf("    Assigned:   %s\n", pin.is_assigned ? "YES" : "NO");

    /* LVDS pin pair */
    IoPin p_pin, n_pin;
    io_pin_init(&p_pin, "K20", 34);
    io_pin_init(&n_pin, "J20", 34);
    p_pin.differential_pair = 1;
    n_pin.differential_pair = 0;
    io_pin_assign_signal(&p_pin, "lvds_data_p", IO_DIR_OUTPUT, IOSTD_LVDS);
    io_pin_assign_signal(&n_pin, "lvds_data_n", IO_DIR_OUTPUT, IOSTD_LVDS);

    printf("\n  LVDS pair:\n");
    printf("    P-pin: %s -> %s (std=%s)\n",
           p_pin.pin_name, p_pin.signal_name,
           iostd_to_string(p_pin.io_standard));
    printf("    N-pin: %s -> %s (std=%s)\n",
           n_pin.pin_name, n_pin.signal_name,
           iostd_to_string(n_pin.io_standard));

    /* Clock capable pin */
    IoPin clk_pin;
    io_pin_init(&clk_pin, "R4", 34);
    clk_pin.clk_capable_mask = CLK_CAP_MRCC | CLK_CAP_GC;
    io_pin_assign_signal(&clk_pin, "sys_clk_p", IO_DIR_INPUT, IOSTD_LVCMOS33);
    printf("\n  Clock pin: %s -> %s (CC: MRCC+GC)\n",
           clk_pin.pin_name, clk_pin.signal_name);
}

/* -------------------------------------------------------
 * Demo 2: I/O Bank Management
 * ------------------------------------------------------- */
static void demo_bank_management(void)
{
    print_section("Demo 2: I/O Bank Management");

    /* Create Bank 34 (3.3V) */
    IoBank bank34;
    io_bank_init(&bank34, 34, 3.3);

    const char *bank34_pins[] = {
        "Y11", "AA11", "AB11", "Y12", "AA12", "AB12",
        "Y13", "AA13", "AB13", "Y14", "AA14", "AB14",
        "Y15", "AA15", "AB15", "Y16", "AA16", "AB16",
        "R4",  "T4",  "U4",  "V4"
    };
    int n_b34 = (int)(sizeof(bank34_pins) / sizeof(bank34_pins[0]));
    create_test_bank(&bank34, 34, 3.3, bank34_pins, n_b34);

    printf("  Bank %d initialized with %d pins (Vcco=%.1fV)\n",
           bank34.bank_number, bank34.num_pins, bank34.vcco_target_v);

    /* Assign some signals */
    const char *signals_34[] = {
        "led[0]",  "led[1]",  "led[2]",  "led[3]",
        "sw[0]",   "sw[1]",   "sw[2]",   "sw[3]",
        "btn_cpu_reset", "btn_user"
    };
    int n_sig34 = (int)(sizeof(signals_34) / sizeof(signals_34[0]));
    for (int i = 0; i < n_sig34 && i < bank34.num_pins; i++) {
        int dir = (i < 4) ? IO_DIR_OUTPUT : IO_DIR_INPUT;
        io_bank_assign(&bank34, i, signals_34[i], dir, IOSTD_LVCMOS33);
    }

    /* Create Bank 35 (1.8V) for LVDS */
    IoBank bank35;
    io_bank_init(&bank35, 35, 1.8);
    const char *bank35_pins[] = {
        "K20", "J20", "K21", "J21", "K22", "J22",
        "K23", "J23", "K24", "J24"
    };
    int n_b35 = (int)(sizeof(bank35_pins) / sizeof(bank35_pins[0]));
    create_test_bank(&bank35, 35, 1.8, bank35_pins, n_b35);

    const char *signals_35[] = {
        "lvds_ch0_p", "lvds_ch0_n", "lvds_ch1_p", "lvds_ch1_n",
        "lvds_clk_p", "lvds_clk_n"
    };
    int n_sig35 = (int)(sizeof(signals_35) / sizeof(signals_35[0]));
    for (int i = 0; i < n_sig35 && i < bank35.num_pins; i++) {
        io_bank_assign(&bank35, i, signals_35[i], IO_DIR_OUTPUT, IOSTD_LVDS);
    }

    printf("\n  Bank 34 status:\n");
    printf("    Used: %d/%d pins\n", bank34.num_used_pins, bank34.num_pins);
    printf("    Standard: %s\n", iostd_to_string(bank34.io_standard));

    printf("\n  Bank 35 status:\n");
    printf("    Used: %d/%d pins\n", bank35.num_used_pins, bank35.num_pins);
    printf("    Standard: %s\n", iostd_to_string(bank35.io_standard));

    /* Recalculate SSN */
    io_bank_recalc_ssn(&bank34);
    printf("\n  SSN Bank 34:  %.1f mA\n", bank34.total_ssn_current_ma);
    io_bank_recalc_ssn(&bank35);
    printf("  SSN Bank 35:  %.1f mA\n", bank35.total_ssn_current_ma);
}

/* -------------------------------------------------------
 * Demo 3: SSN Analysis
 * ------------------------------------------------------- */
static void demo_ssn_analysis(void)
{
    print_section("Demo 3: Simultaneous Switching Noise (SSN) Analysis");

    SsnAnalyzer ssn;
    ssn_init(&ssn);

    printf("  Package inductance: %.1f nH\n", ssn.per_pin_inductance_nh);

    /* Different aggressor counts */
    int aggressor_counts[] = { 4, 8, 16, 32 };
    double slew_rates[]    = { 1.0, 0.5, 0.8, 0.4 };

    for (int i = 0; i < 4; i++) {
        SsnAnalyzer sa;
        ssn_init(&sa);

        for (int a = 0; a < aggressor_counts[i]; a++) {
            ssn_add_aggressor(&sa);
        }
        ssn_set_slew(&sa, slew_rates[i]);

        double noise_margin = 200.0;  /* 200 mV */
        int result = ssn_check(&sa, noise_margin);

        printf("\n  Case %d: %d aggressors, %.1fns slew\n",
               i + 1, sa.aggressor_count, sa.aggressor_slew_ns);
        printf("    dI/dt:       %.1f mA/ns\n", sa.dI_dt_ma_per_ns);
        printf("    Est. noise:  %.1f mV\n", sa.victim_noise_mv);
        printf("    Noise margin: %.1f mV\n", sa.noise_margin_mv);
        printf("    Result:      %s\n", result == 0 ? "SAFE" : "FAIL");
    }

    /* Aggressive scenario */
    SsnAnalyzer bad;
    ssn_init(&bad);
    for (int i = 0; i < 40; i++) ssn_add_aggressor(&bad);
    ssn_set_slew(&bad, 0.3);  /* very fast slew */
    int bad_result = ssn_check(&bad, 150.0);
    double est_noise = ssn_estimate_noise(&bad);
    printf("\n  Aggressive scenario: 40 agg, 0.3ns slew\n");
    printf("    Noise estimate: %.1f mV\n", est_noise);
    printf("    Check (150mV margin): %s\n",
           bad_result == 0 ? "SAFE" : "FAIL");
}

/* -------------------------------------------------------
 * Demo 4: Pin Swapping
 * ------------------------------------------------------- */
static void demo_pin_swapping(void)
{
    print_section("Demo 4: Pin Swapping");

    PinSwapper ps;
    pin_swap_init(&ps);

    /* Add swappable pin pairs (simulating data bus bits) */
    int pairs[][2] = {
        {10, 25}, {11, 26}, {12, 27}, {13, 28},
        {14, 29}, {15, 30}, {16, 31}, {17, 32}
    };
    int num_pairs = (int)(sizeof(pairs) / sizeof(pairs[0]));

    for (int i = 0; i < num_pairs; i++) {
        pin_swap_add_pair(&ps, pairs[i][0], pairs[i][1]);
    }

    printf("  Pairs to evaluate: %d\n", ps.group_size);
    for (int i = 0; i < ps.group_size; i++) {
        printf("    Pair %d: pin_%d <-> pin_%d\n",
               i, ps.pin_group_a[i], ps.pin_group_b[i]);
    }

    int worth_swapping = pin_swap_evaluate(&ps);
    printf("\n  Wirelength before: %.0f\n", ps.wirelength_before);
    printf("  Wirelength after:  %.0f\n", ps.wirelength_after);
    printf("  Improvement:       %.1f%%\n", ps.improvement_pct);
    printf("  Worth swapping:    %s (threshold 5%%)\n",
           worth_swapping ? "YES" : "NO");

    /* Apply swap */
    pin_swap_apply(&ps);
    printf("  Swaps performed:   %d\n", ps.swaps_performed);
}

/* -------------------------------------------------------
 * Demo 5: I/O Planner — Full Chip Planning
 * ------------------------------------------------------- */
static void demo_io_planner(void)
{
    print_section("Demo 5: I/O Planner — Full Chip Planning");

    IoPlanner ip;
    io_planner_init(&ip, "xc7k325tffg900-2");

    /* Create and add banks */
    IoBank banks[4];
    int bank_nums[] = { 34, 35, 16, 17 };
    double vccos[]  = { 3.3, 1.8, 3.3, 1.5 };

    for (int b = 0; b < 4; b++) {
        io_bank_init(&banks[b], bank_nums[b], vccos[b]);

        /* Populate with 20 pins each */
        char pin_name[8];
        for (int p = 0; p < 20; p++) {
            snprintf(pin_name, sizeof(pin_name), "%c%d",
                     (char)('A' + p), bank_nums[b]);
            IoPin pin;
            io_pin_init(&pin, pin_name, bank_nums[b]);
            if (p == 0) pin.clk_capable_mask = CLK_CAP_MRCC;
            io_bank_add_pin(&banks[b], &pin);
        }
        io_planner_add_bank(&ip, &banks[b]);
    }

    printf("  Part:     %s\n", ip.part_name);
    printf("  Banks:    %d\n", ip.num_banks);
    printf("  Total pins: %d\n", ip.num_pins_total);

    /* Assign pins by name */
    const char *assignments[][3] = {
        {"A34", "sys_clk_p",   "LVCMOS33"},
        {"B34", "sys_clk_n",   "LVCMOS33"},
        {"C34", "uart_tx",     "LVCMOS33"},
        {"D34", "uart_rx",     "LVCMOS33"},
        {"E34", "spi_sck",     "LVCMOS33"},
        {"F34", "spi_mosi",    "LVCMOS33"},
        {"G34", "spi_miso",    "LVCMOS33"},
        {"H34", "spi_cs_n",    "LVCMOS33"},
        {"I34", "i2c_scl",     "LVCMOS33"},
        {"J34", "i2c_sda",     "LVCMOS33"},
        {"K34", "gpio[0]",     "LVCMOS33"},
        {"L34", "gpio[1]",     "LVCMOS33"},
    };
    int n_assign = (int)(sizeof(assignments) / sizeof(assignments[0]));

    for (int i = 0; i < n_assign; i++) {
        int dir = (i >= 2) ? IO_DIR_OUTPUT : IO_DIR_INPUT;
        int iostd = IOSTD_LVCMOS33;
        if (strcmp(assignments[i][2], "LVDS") == 0) iostd = IOSTD_LVDS;
        int ret = io_planner_assign_pin(&ip, assignments[i][0],
                                         assignments[i][1], dir, iostd);
        if (ret != 0) {
            printf("  WARNING: Pin %s not found in banks\n", assignments[i][0]);
        }
    }

    /* Auto-assign remaining */
    char *auto_signals[] = {
        "led[0]", "led[1]", "led[2]", "led[3]",
        "led[4]", "led[5]", "led[6]", "led[7]",
        "sw[0]", "sw[1]", "sw[2]", "sw[3]",
        "btn_center", "btn_up", "btn_down", "btn_left", "btn_right"
    };
    int n_auto = (int)(sizeof(auto_signals) / sizeof(auto_signals[0]));
    int auto_count = io_planner_auto_assign(&ip, auto_signals, n_auto);
    printf("\n  Auto-assigned: %d pins\n", auto_count);

    /* Validate */
    printf("\n  Validation:\n");
    int errors = io_planner_validate(&ip);
    printf("  Errors: %d\n", errors);

    /* Report */
    io_planner_report(&ip);

    /* Export XDC */
    io_planner_export_xdc(&ip, "demo_io_constraints.xdc");

    /* Cleanup */
    remove("demo_io_constraints.xdc");
}

/* -------------------------------------------------------
 * Demo 6: LVDS Interface Configuration
 * ------------------------------------------------------- */
static void demo_lvds_interface(void)
{
    print_section("Demo 6: LVDS Interface Configuration");

    /* Camera link LVDS at 600 MHz */
    LvdsInterface cam;
    lvds_init(&cam, 600.0, 7);  /* 7:1 serialization */
    lvds_assign_pins(&cam, "K20", "J20");
    lvds_calc_timing(&cam);

    printf("  Camera Link LVDS:\n");
    printf("    Clock:     %.1f MHz\n", cam.clk_freq_mhz);
    printf("    Width:     %d bits\n", cam.bit_width);
    printf("    Data rate: %d Mbps\n", cam.data_rate_mbps);
    printf("    Serialized: %s\n", cam.is_serialized ? "YES (OSERDES)" : "NO");
    printf("    P/N pins:  %s / %s\n", cam.p_pin, cam.n_pin);
    printf("    Setup:     %.3f ns\n", cam.setup_ns);
    printf("    Hold:      %.3f ns\n", cam.hold_ns);
    printf("    Vcm:       %.1f V\n", cam.vcm_v);
    printf("    Vod:       %.0f mV\n", cam.vod_mv);

    /* Low-speed LVDS display interface */
    LvdsInterface disp;
    lvds_init(&disp, 148.5, 1);   /* 1080p @60Hz pixel clock */
    lvds_assign_pins(&disp, "K24", "J24");
    lvds_calc_timing(&disp);

    printf("\n  Display LVDS (1080p):\n");
    printf("    Clock:     %.1f MHz\n", disp.clk_freq_mhz);
    printf("    Data rate: %d Mbps\n", disp.data_rate_mbps);
    printf("    Setup:     %.3f ns\n", disp.setup_ns);
    printf("    Hold:      %.3f ns\n", disp.hold_ns);

    /* High-speed SerDes */
    LvdsInterface serdes;
    lvds_init(&serdes, 1250.0, 8);  /* 10 Gbps */
    lvds_assign_pins(&serdes, "K22", "J22");
    lvds_calc_timing(&serdes);

    printf("\n  High-Speed SerDes:\n");
    printf("    Clock:     %.1f MHz\n", serdes.clk_freq_mhz);
    printf("    Data rate: %d Mbps (%.1f Gbps)\n",
           serdes.data_rate_mbps, (double)serdes.data_rate_mbps / 1000.0);
    printf("    Setup:     %.3f ns\n", serdes.setup_ns);
    printf("    Hold:      %.3f ns\n", serdes.hold_ns);
    printf("    Bit period: %.1f ps\n", 1000.0 / (double)serdes.data_rate_mbps * 1000.0);
}

/* -------------------------------------------------------
 * Demo 7: I/O Standard Compatibility
 * ------------------------------------------------------- */
static void demo_io_standard_compatibility(void)
{
    print_section("Demo 7: I/O Standard & Vcco Compatibility");

    printf("  I/O Standard Reference:\n");
    printf("  %-16s %6s %s\n", "Standard", "Vcco", "Termination");
    printf("  %-16s %6s %s\n", "----------------", "-----", "-----------");

    for (int i = 0; i < IOSTD_NUM; i++) {
        const char *term = "None";
        if (i >= IOSTD_LVDS && i <= IOSTD_LVDS_EXT) term = "100-ohm diff";
        else if (i >= IOSTD_SSTL15 && i <= IOSTD_SSTL18_II) term = "50-ohm to VTT";
        else if (i >= IOSTD_HSTL_I && i <= IOSTD_HSTL_II) term = "50-ohm to VTT";
        else if (i == IOSTD_POD12) term = "50-ohm to Vcco/2";

        printf("  %-16s %5.1fV %s\n",
               iostd_names[i], iostd_vcco[i], term);
    }

    printf("\n  Bank Vcco Rules:\n");
    printf("    - All I/Os in a bank share Vcco\n");
    printf("    - Inputs can tolerate higher Vcco\n");
    printf("    - LVDS requires Vcco=2.5V (or external Vcm)\n");
    printf("    - SSTL/HSTL require VREF for inputs\n");
    printf("    - DCI termination available in HP banks\n");
}

int main(void)
{
    printf("==================================================\n");
    printf("  mini-fpga-dev: I/O Planning Demo\n");
    printf("  Version: %s\n", IO_PLANNING_VERSION);
    printf("==================================================\n");

    printf("\nSupported I/O standards: %d\n", IOSTD_NUM);
    printf("Max banks: %d, Max pins: %d\n\n", MAX_IO_BANKS, MAX_TOTAL_PINS);

    demo_pin_configuration();
    demo_bank_management();
    demo_ssn_analysis();
    demo_pin_swapping();
    demo_io_planner();
    demo_lvds_interface();
    demo_io_standard_compatibility();

    printf("\n==================================================\n");
    printf("  All I/O planning demos completed.\n");
    printf("==================================================\n");
    return 0;
}
