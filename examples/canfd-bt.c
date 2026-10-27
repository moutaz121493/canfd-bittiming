/* Command-line front end: canfd-bt <clock MHz> <nominal kbit/s> <nominal SP %> <data kbit/s> <data SP %> [--any-brp]
 * Example: canfd-bt 40 500 80 2000 75 */
#include "canfd_bittiming.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 6) {
        fprintf(stderr, "usage: %s <clock MHz> <nominal kbit/s> <nominal SP %%> <data kbit/s> <data SP %%> [--any-brp]\n", argv[0]);
        return 2;
    }
    uint32_t clk = (uint32_t)(atof(argv[1]) * 1e6 + 0.5), nb = (uint32_t)(atof(argv[2]) * 1e3 + 0.5), db = (uint32_t)(atof(argv[4]) * 1e3 + 0.5);
    double nsp = atof(argv[3]) / 100.0, dsp = atof(argv[5]) / 100.0;
    bool same = !(argc > 6 && !strcmp(argv[6], "--any-brp"));
    cfd_setting s[5];
    size_t n = cfd_solve(clk, nb, nsp, db, dsp, same, s, 5);
    if (!n) {
        printf("no exact setting: the clock cannot be divided into these bit rates with whole time quanta\n");
        return 1;
    }
    const cfd_phase *p = &s[0].nominal, *d = &s[0].data;
    bool tdc = db > 1000000u && cfd_tdc_possible(d);
    printf("%zu valid settings, best:\n", n);
    printf("  nominal  brp %3u  tq %3u = 1 + %3u + %3u  sjw %3u  sample point %.2f %%\n", p->brp, p->tq, p->tseg1, p->tseg2, p->sjw, p->sample_point * 100);
    printf("  data     brp %3u  tq %3u = 1 + %3u + %3u  sjw %3u  sample point %.2f %%\n", d->brp, d->tq, d->tseg1, d->tseg2, d->sjw, d->sample_point * 100);
    printf("  TDC      %s, TDCO = %u clock cycles\n", tdc ? "on" : (db > 1000000u ? "not possible (data prescaler > 2)" : "not needed"), cfd_tdco(d));
    printf("  M_CAN    NBTP 0x%08X  DBTP 0x%08X  TDCR 0x%08X\n", cfd_mcan_nbtp(p), cfd_mcan_dbtp(d, tdc), cfd_mcan_tdcr(d));
    printf("  Linux    ip link set can0 up type can bitrate %u sample-point %.3f dbitrate %u dsample-point %.3f fd on\n",
           nb, p->sample_point, db, d->sample_point);
    return 0;
}
