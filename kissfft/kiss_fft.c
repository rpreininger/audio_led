#include "kiss_fft.h"

struct kiss_fft_state {
    int nfft;
    int inverse;
};

kiss_fft_cfg kiss_fft_alloc(int nfft, int inverse_fft,
                            void * mem, size_t * lenmem)
{
    kiss_fft_cfg st = (kiss_fft_cfg) malloc(sizeof(struct kiss_fft_state));
    st->nfft = nfft;
    st->inverse = inverse_fft;
    return st;
}

void kiss_fft(kiss_fft_cfg cfg,
              const kiss_fft_cpx *fin,
              kiss_fft_cpx *fout)
{
    int n = cfg->nfft;
    int inverse = cfg->inverse ? -1 : 1;

    for (int k = 0; k < n; k++) {
        kiss_fft_scalar sumr = 0;
        kiss_fft_scalar sumi = 0;

        for (int t = 0; t < n; t++) {
            float angle = 2 * M_PI * t * k / n * inverse;
            float wr = cosf(angle);
            float wi = sinf(angle);
            sumr += fin[t].r * wr - fin[t].i * wi;
            sumi += fin[t].r * wi + fin[t].i * wr;
        }

        fout[k].r = sumr;
        fout[k].i = sumi;
    }
}
