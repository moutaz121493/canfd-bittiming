/*
 * canfd_bittiming: CAN FD bit-timing calculator for Bosch M_CAN-style controllers (C99, no dependencies, no malloc).
 *
 * Given the CAN clock, nominal and data bit rates and target sample points, find every exact setting
 * (whole time quanta, within the controller limits), rank them, and encode the best one into the
 * M_CAN NBTP / DBTP / TDCR registers, including transmitter delay compensation (TDC).
 *
 * Ranking (same as the calculator on https://moutazm.com/projects/can-tools/):
 *   1. combined sample-point error, in 0.1 % steps
 *   2. lower data prescaler (TDC needs data prescaler 1 or 2; a short time quantum resynchronises better)
 *   3. lower nominal prescaler
 * Within one prescaler, TSEG2 is chosen so the sample point is closest to the target (first found wins on a tie).
 * SJW = TSEG2 (capped by the controller limit). Sync segment = 1 time quantum.
 */
#ifndef CANFD_BITTIMING_H
#define CANFD_BITTIMING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Controller limits for one phase (inclusive ranges). */
typedef struct {
    uint32_t brp_min, brp_max;
    uint32_t tseg1_min, tseg1_max;
    uint32_t tseg2_min, tseg2_max;
    uint32_t sjw_max;
} cfd_limits;

/* Bosch M_CAN, the same values as the Linux m_can driver (m_can_bittiming_const / m_can_data_bittiming_const). */
extern const cfd_limits CFD_MCAN_NOMINAL;
extern const cfd_limits CFD_MCAN_DATA;

/* One phase of a solution. tq = 1 + tseg1 + tseg2 time quanta per bit. */
typedef struct {
    uint32_t brp, tq, tseg1, tseg2, sjw;
    double sample_point;   /* (1 + tseg1) / tq */
    double error;          /* |sample_point - target| */
} cfd_phase;

typedef struct {
    cfd_phase nominal, data;
} cfd_setting;

/*
 * Best TSEG1/TSEG2 split for one prescaler. Returns false if the bit time is not a whole number of time quanta
 * or no split fits the limits.
 */
bool cfd_phase_for_brp(uint32_t clock_hz, uint32_t bitrate, double target_sp, const cfd_limits *lim, uint32_t brp,
                       cfd_phase *out);

/*
 * All valid (nominal, data) combinations, best first. Writes at most `max` settings to `out` (may be NULL if
 * max is 0) and returns how many exist in total. same_prescaler = true pairs only equal prescalers
 * (CiA 601-3 recommendation).
 */
size_t cfd_solve(uint32_t clock_hz, uint32_t nominal_bps, double nominal_sp, uint32_t data_bps, double data_sp,
                 bool same_prescaler, cfd_setting *out, size_t max);

/* Transmitter delay compensation: secondary sample point offset in CAN clock cycles = data brp x (1 + tseg1). */
uint32_t cfd_tdco(const cfd_phase *data);
/* ISO 11898-1 allows TDC only with a data prescaler of 1 or 2. */
bool cfd_tdc_possible(const cfd_phase *data);

/* M_CAN register values (fields hold value - 1, TDCO as is, capped at 127). */
uint32_t cfd_mcan_nbtp(const cfd_phase *nominal);
uint32_t cfd_mcan_dbtp(const cfd_phase *data, bool tdc_enable);
uint32_t cfd_mcan_tdcr(const cfd_phase *data);

#ifdef __cplusplus
}
#endif
#endif
