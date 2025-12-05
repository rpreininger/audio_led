#ifndef KISS_FFT_H
#define KISS_FFT_H

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifndef kiss_fft_scalar
#define kiss_fft_scalar float
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    kiss_fft_scalar r;
    kiss_fft_scalar i;
} kiss_fft_cpx;

typedef struct kiss_fft_state* kiss_fft_cfg;

kiss_fft_cfg kiss_fft_alloc(int nfft, int inverse_fft,
                            void * mem, size_t * lenmem);

void kiss_fft(kiss_fft_cfg cfg,
              const kiss_fft_cpx *fin,
              kiss_fft_cpx *fout);

#ifdef __cplusplus
}
#endif
#endif
