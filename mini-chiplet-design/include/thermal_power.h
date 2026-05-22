#ifndef THERMAL_POWER_H
#define THERMAL_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define THERMAL_MAX_HOTSPOTS    32
#define THERMAL_MAX_LAYERS      10
#define THERMAL_MAX_C4_BUMP     65536
#define THERMAL_MAX_DECAPS      128

typedef struct {
    double theta_ja;
    double theta_jc;
    double theta_jb;
    double theta_ca;
    double ambient_temp_c;
} thermal_resistance_t;

typedef struct {
    double   x_mm;
    double   y_mm;
    double   width_mm;
    double   height_mm;
    double   power_density_w_per_mm2;
    double   peak_temp_c;
    uint8_t  active;
} hotspot_t;

typedef enum {
    TIM_SOLDER = 0,
    TIM_GREASE,
    TIM_PCM,
    TIM_LIQUID_METAL,
    TIM_GRAPHITE_PAD,
    TIM_SINTERED_SILVER
} tim_material_t;

typedef struct {
    tim_material_t material;
    double  thickness_um;
    double  thermal_conductivity;
    double  bondline_thickness_um;
    double  youngs_modulus_mpa;
    double  cte_ppm_per_k;
    double  max_temp_c;
    double  thermal_resistance_c_per_w;
} tim_spec_t;

typedef struct {
    double  x_mm;
    double  y_mm;
    double  capacitance_nf;
    double  esr_mohm;
    double  esl_ph;
    double  voltage_rating_v;
    uint8_t placed;
} decap_t;

typedef struct {
    double die_temp_c[16];
    double hotspot_peak_c[THERMAL_MAX_HOTSPOTS];
    double junction_temp_c;
    double case_temp_c;
    double ambient_temp_c;
    double total_power_w;
    double cooling_capacity_w;
    double thermal_margin_c;
    uint8_t throttling;
} thermal_state_t;

typedef struct {
    double vdd_v;
    double total_current_a;
    double ir_drop_target_mv;
    double ir_drop_actual_mv;
    double pdn_impedance_mohm;
    double resonance_freq_mhz;
    double decap_total_nf;
    double ripple_mv;
    uint32_t num_c4_bumps;
    double c4_resistance_mohm;
    double c4_inductance_ph;
    double board_resistance_mohm;
    double board_inductance_ph;
} power_delivery_t;

typedef struct {
    thermal_resistance_t theta;
    thermal_state_t      state;
    hotspot_t            hotspots[THERMAL_MAX_HOTSPOTS];
    uint32_t             num_hotspots;
    tim_spec_t           tim;
    power_delivery_t     pdn;
    decap_t              decaps[THERMAL_MAX_DECAPS];
    uint32_t             num_decaps;
    double               die_thickness_um;
    double               die_area_mm2;
    double               power_density_w_per_mm2;
    uint8_t              three_d_stack;
    uint32_t             stack_layers;
    double               inter_layer_tim_c_per_w;
} thermal_power_model_t;

void tp_init(thermal_power_model_t *tm,
             double die_area_mm2, double ambient_temp_c);
void tp_set_thermal_resistance(thermal_power_model_t *tm,
                                double theta_ja, double theta_jc);
void tp_set_tim(thermal_power_model_t *tm, const tim_spec_t *tim);
int  tp_add_hotspot(thermal_power_model_t *tm, const hotspot_t *hs);
int  tp_remove_hotspot(thermal_power_model_t *tm, uint32_t index);

double tp_calc_junction_temp(thermal_power_model_t *tm, double power_w);
double tp_calc_case_temp(const thermal_power_model_t *tm);
double tp_calc_heatspreader_temp(const thermal_power_model_t *tm);
int  tp_check_thermal_throttle(thermal_power_model_t *tm, double max_temp_c);
void tp_mitigate_hotspots(thermal_power_model_t *tm);

void tp_pdn_init(power_delivery_t *pdn, double vdd_v, double total_current_a);
double tp_pdn_ir_drop(const power_delivery_t *pdn);
double tp_pdn_impedance_at_freq(const power_delivery_t *pdn, double freq_mhz);
int  tp_pdn_add_decap(thermal_power_model_t *tm, const decap_t *d);
double tp_pdn_resonance_freq(const power_delivery_t *pdn);
double tp_pdn_ripple(const power_delivery_t *pdn, double switching_freq_mhz,
                      double di_dt_a_per_ns);

double tp_calc_3d_stack_temp(const thermal_power_model_t *tm,
                              const double *layer_powers, uint32_t num_layers);
double tp_calc_cooling_required(const thermal_power_model_t *tm,
                                 double target_temp_c);

void tp_print_state(const thermal_power_model_t *tm);
void tp_print_hotspots(const thermal_power_model_t *tm);
void tp_print_pdn(const thermal_power_model_t *tm);

double tim_select_conductivity(tim_material_t material);
const char *tim_material_name(tim_material_t material);

#ifdef __cplusplus
}
#endif

#endif
