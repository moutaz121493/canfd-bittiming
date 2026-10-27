#include "canfd_bittiming.h"

#include <math.h>

const cfd_limits CFD_MCAN_NOMINAL = {1, 512, 2, 256, 2, 128, 128};
const cfd_limits CFD_MCAN_DATA = {1, 32, 1, 32, 1, 16, 16};

bool cfd_phase_for_brp(uint32_t clock_hz, uint32_t bitrate, double target_sp, const cfd_limits *lim, uint32_t brp,
                       cfd_phase *out)
{
    if (!bitrate || !brp)
        return false;
    uint64_t div = (uint64_t)brp * bitrate;
    if (clock_hz % div)                       /* the bit time must be a whole number of time quanta */
        return false;
    uint64_t n = clock_hz / div;
    if (n < 1u + lim->tseg1_min + lim->tseg2_min || n > 1u + lim->tseg1_max + lim->tseg2_max)
        return false;
    bool found = false;
    for (uint32_t t2 = lim->tseg2_min; t2 <= lim->tseg2_max; t2++) {
        if (n < 1u + t2)
            break;
        uint32_t t1 = (uint32_t)(n - 1 - t2);
        if (t1 < lim->tseg1_min || t1 > lim->tseg1_max)
            continue;
        double sp = (double)(1 + t1) / (double)n;
        double err = fabs(sp - target_sp);
        if (!found || err < out->error - 1e-12) {
            out->brp = brp;
            out->tq = (uint32_t)n;
            out->tseg1 = t1;
            out->tseg2 = t2;
            out->sjw = t2 < lim->sjw_max ? t2 : lim->sjw_max;
            out->sample_point = sp;
            out->error = err;
            found = true;
        }
    }
    return found;
}

/* rank key: sample-point error in 0.1 % steps, then data prescaler, then nominal prescaler, then discovery order */
static int better(const cfd_setting *a, size_t ia, const cfd_setting *b, size_t ib)
{
    long ka = lround((a->nominal.error + a->data.error) * 1000.0), kb = lround((b->nominal.error + b->data.error) * 1000.0);
    if (ka != kb)
        return ka < kb;
    if (a->data.brp != b->data.brp)
        return a->data.brp < b->data.brp;
    if (a->nominal.brp != b->nominal.brp)
        return a->nominal.brp < b->nominal.brp;
    return ia < ib;
}

/* keep the best `max` settings in order (insertion), count all */
static void offer(cfd_setting *out, size_t *order, size_t max, size_t *count, const cfd_setting *s)
{
    size_t idx = (*count)++;
    size_t have = idx < max ? idx : max;
    size_t pos = have;
    while (pos > 0 && better(s, idx, &out[pos - 1], order[pos - 1]))
        pos--;
    if (pos >= max)
        return;
    size_t last = have < max ? have : max - 1;
    for (size_t i = last; i > pos; i--) {
        out[i] = out[i - 1];
        order[i] = order[i - 1];
    }
    out[pos] = *s;
    order[pos] = idx;
}

size_t cfd_solve(uint32_t clock_hz, uint32_t nominal_bps, double nominal_sp, uint32_t data_bps, double data_sp,
                 bool same_prescaler, cfd_setting *out, size_t max)
{
    size_t count = 0;
    size_t order_buf[64];
    size_t *order = order_buf;
    if (max > 64)
        max = 64;                              /* callers rarely need more; keeps the solver allocation-free */
    if (!out)
        max = 0;
    cfd_setting s;
    if (same_prescaler) {
        for (uint32_t b = 1; b <= CFD_MCAN_DATA.brp_max; b++) {
            if (cfd_phase_for_brp(clock_hz, nominal_bps, nominal_sp, &CFD_MCAN_NOMINAL, b, &s.nominal) &&
                cfd_phase_for_brp(clock_hz, data_bps, data_sp, &CFD_MCAN_DATA, b, &s.data))
                offer(out, order, max, &count, &s);
        }
        return count;
    }
    for (uint32_t bn = CFD_MCAN_NOMINAL.brp_min; bn <= CFD_MCAN_NOMINAL.brp_max; bn++) {
        if (!cfd_phase_for_brp(clock_hz, nominal_bps, nominal_sp, &CFD_MCAN_NOMINAL, bn, &s.nominal))
            continue;
        for (uint32_t bd = CFD_MCAN_DATA.brp_min; bd <= CFD_MCAN_DATA.brp_max; bd++)
            if (cfd_phase_for_brp(clock_hz, data_bps, data_sp, &CFD_MCAN_DATA, bd, &s.data))
                offer(out, order, max, &count, &s);
    }
    return count;
}

uint32_t cfd_tdco(const cfd_phase *d) { return d->brp * (1 + d->tseg1); }

bool cfd_tdc_possible(const cfd_phase *d) { return d->brp <= 2; }

uint32_t cfd_mcan_nbtp(const cfd_phase *n)
{
    return ((n->sjw - 1) << 25) | ((n->brp - 1) << 16) | ((n->tseg1 - 1) << 8) | (n->tseg2 - 1);
}

uint32_t cfd_mcan_dbtp(const cfd_phase *d, bool tdc)
{
    return ((uint32_t)(tdc ? 1 : 0) << 23) | ((d->brp - 1) << 16) | ((d->tseg1 - 1) << 8) | ((d->tseg2 - 1) << 4) | (d->sjw - 1);
}

uint32_t cfd_mcan_tdcr(const cfd_phase *d)
{
    uint32_t o = cfd_tdco(d);
    return ((o > 127 ? 127 : o) & 0x7Fu) << 8;
}
