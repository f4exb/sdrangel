/*---------------------------------------------------------------------------*\

  FILE........: fsk.c
  AUTHOR......: Brady O'Brien
  DATE CREATED: 7 January 2016

  C Implementation of 2/4FSK modulator/demodulator, based on octave/fsk_horus.m

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2016 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

/*---------------------------------------------------------------------------*\

                               DEFINES

\*---------------------------------------------------------------------------*/

/* P oversampling rate constant -- should probably be init-time configurable */
#define horus_P 8

/* Define this to enable EbNodB estimate */
/* This needs square roots, may take more cpu time than it's worth */
#define EST_EBNO

/* This is a flag for the freq. estimator to use a precomputed/rt computed hann window table
   On platforms with slow cosf, this will produce a substantial speedup at the cost of a small
    amount of memory
*/
#define USE_HANN_TABLE

/* This flag turns on run-time hann table generation. If USE_HANN_TABLE is unset,
    this flag has no effect. If USE_HANN_TABLE is set and this flag is set, the
    hann table will be allocated and generated when fsk_init or fsk_init_hbr is
    called. If this flag is not set, a hann function table of size fsk->Ndft MUST
    be provided. On small platforms, this can be used with a precomputed table to
    save memory at the cost of flash space.
*/
#define GENERATE_HANN_TABLE_RUNTIME

/* Turn off table generation if on cortex M4 to save memory */
#ifdef CORTEX_M4
#undef USE_HANN_TABLE
#endif

/*---------------------------------------------------------------------------*\

                               INCLUDES

\*---------------------------------------------------------------------------*/

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "fsk.h"
#include "comp_prim.h"
#include "kiss_fftr.h"
#include "modem_probe.h"

namespace FreeDV
{

/*---------------------------------------------------------------------------*\

                               FUNCTIONS

\*---------------------------------------------------------------------------*/

static void stats_init(struct FSK *fsk);

#ifdef USE_HANN_TABLE
/*
   This is used by fsk_create and fsk_create_hbr to generate a hann function
   table
*/
static void fsk_generate_hann_table(struct FSK* fsk){
    int Ndft = fsk->Ndft;
    int i;

    /* Set up complex oscilator to calculate hann function */
    COMP dphi = comp_exp_j((2*M_PI)/((float)Ndft-1));
    COMP rphi = {.5,0};

    rphi = cmult(cconj(dphi),rphi);

    for (i = 0; i < Ndft; i++)
    {
        rphi = cmult(dphi,rphi);
        float hannc = .5-rphi.real;
        //float hann = .5-(.5*cosf((2*M_PI*(float)(i))/((float)Ndft-1)));
        fsk->hann_table[i] = hannc;
    }
}
#endif



/*---------------------------------------------------------------------------*\

  FUNCTION....: fsk_create_hbr
  AUTHOR......: Brady O'Brien
  DATE CREATED: 11 February 2016

  Create and initialize an instance of the FSK modem. Returns a pointer
  to the modem state/config struct. One modem config struct may be used
  for both mod and demod. returns NULL on failure.

\*---------------------------------------------------------------------------*/

struct FSK * fsk_create_hbr(int Fs, int Rs,int P,int M, int tx_f1, int tx_fs)
{
    struct FSK *fsk;
    int i;
    int memold;
    int Ndft = 0;
    /* Number of symbols in a processing frame */
    int nsyms = 48;
    /* Check configuration validity */
    assert(Fs > 0 );
    assert(Rs > 0 );
    assert(tx_f1 > 0);
    assert(tx_fs > 0);
    assert(P > 0);
    /* Ts (Fs/Rs) must be an integer */
    assert( (Fs%Rs) == 0 );
    /* Ts/P (Fs/Rs/P) must be an integer */
    assert( ((Fs/Rs)%P) == 0 );
    assert( M==2 || M==4);

    fsk = (struct FSK*) malloc(sizeof(struct FSK));
    if(fsk == NULL) return NULL;


    /* Set constant config parameters */
    fsk->Fs = Fs;
    fsk->Rs = Rs;
    fsk->Ts = Fs/Rs;
    fsk->burst_mode = 0;
    fsk->N = fsk->Ts*nsyms;
    fsk->P = P;
    fsk->Nsym = nsyms;
    fsk->Nmem = fsk->N+(2*fsk->Ts);
    fsk->f1_tx = tx_f1;
    fsk->fs_tx = tx_fs;
    fsk->nin = fsk->N;
    fsk->mode = M==2 ? MODE_2FSK : MODE_4FSK;
    fsk->Nbits = M==2 ? fsk->Nsym : fsk->Nsym*2;

    /* Find smallest 2^N value that fits Fs for efficient FFT */
    /* It would probably be better to use KISS-FFt's routine here */
    for(i=1; i; i<<=1)
        if((fsk->N)&i)
            Ndft = i;

    fsk->Ndft = Ndft;

    fsk->est_min = Rs/4;
    if(fsk->est_min<0) fsk->est_min = 0;

    fsk->est_max = (Fs/2)-Rs/4;

    fsk->est_space = Rs-(Rs/5);

    /* Set up rx state */

    for( i=0; i<M; i++)
        fsk->phi_c[i] = comp_exp_j(0);

    memold = (4*fsk->Ts);

    fsk->nstash = memold;
    fsk->samp_old = (COMP*) malloc(sizeof(COMP)*memold);
    if(fsk->samp_old == NULL){
        free(fsk);
        return NULL;
    }

    for(i=0;i<memold;i++) {
        fsk->samp_old[i].real = 0;
        fsk->samp_old[i].imag = 0;
    }

    fsk->fft_cfg = kiss_fft_alloc(fsk->Ndft,0,NULL,NULL);
    if(fsk->fft_cfg == NULL){
        free(fsk->samp_old);
        free(fsk);
        return NULL;
    }

    fsk->fft_est = (float*)malloc(sizeof(float)*fsk->Ndft/2);
    if(fsk->fft_est == NULL){
        free(fsk->samp_old);
        free(fsk->fft_cfg);
        free(fsk);
        return NULL;
    }

    #ifdef USE_HANN_TABLE
        #ifdef GENERATE_HANN_TABLE_RUNTIME
            fsk->hann_table = (float*)malloc(sizeof(float)*fsk->Ndft);
            if(fsk->hann_table == NULL){
                free(fsk->fft_est);
                free(fsk->samp_old);
                free(fsk->fft_cfg);
                free(fsk);
                return NULL;
            }
            fsk_generate_hann_table(fsk);
        #else
            fsk->hann_table = NULL;
        #endif
    #endif

    for(i=0;i<fsk->Ndft/2;i++)fsk->fft_est[i] = 0;

    fsk->norm_rx_timing = 0;

    /* Set up tx state */
    fsk->tx_phase_c = comp_exp_j(0);

    /* Set up demod stats */
    fsk->EbNodB = 0;

    for( i=0; i<M; i++)
        fsk->f_est[i] = 0;

    fsk->ppm = 0;

    fsk->stats = (struct MODEM_STATS*)malloc(sizeof(struct MODEM_STATS));
    if(fsk->stats == NULL){
        free(fsk->fft_est);
        free(fsk->samp_old);
        free(fsk->fft_cfg);
        free(fsk);
        return NULL;
    }
    stats_init(fsk);
    fsk->normalise_eye = 1;

    return fsk;
}


#define HORUS_MIN 800
#define HORUS_MAX 2500
#define HORUS_MIN_SPACING 100

/*---------------------------------------------------------------------------*\

  FUNCTION....: fsk_create
  AUTHOR......: Brady O'Brien
  DATE CREATED: 7 January 2016

  Create and initialize an instance of the FSK modem. Returns a pointer
  to the modem state/config struct. One modem config struct may be used
  for both mod and demod. returns NULL on failure.

\*---------------------------------------------------------------------------*/

struct FSK * fsk_create(int Fs, int Rs,int M, int tx_f1, int tx_fs)
{
    struct FSK *fsk;
    int i;
    int Ndft = 0;
    int memold;

    /* Check configuration validity */
    assert(Fs > 0 );
    assert(Rs > 0 );
    assert(tx_f1 > 0);
    assert(tx_fs > 0);
    assert(horus_P > 0);
    /* Ts (Fs/Rs) must be an integer */
    assert( (Fs%Rs) == 0 );
    /* Ts/P (Fs/Rs/P) must be an integer */
    assert( ((Fs/Rs)%horus_P) == 0 );
    assert( M==2 || M==4);

    fsk = (struct FSK*) malloc(sizeof(struct FSK));
    if(fsk == NULL) return NULL;

    Ndft = 1024;

    /* Set constant config parameters */
    fsk->Fs = Fs;
    fsk->Rs = Rs;
    fsk->Ts = Fs/Rs;
    fsk->N = Fs;
    fsk->burst_mode = 0;
    fsk->P = horus_P;
    fsk->Nsym = fsk->N/fsk->Ts;
    fsk->Ndft = Ndft;
    fsk->Nmem = fsk->N+(2*fsk->Ts);
    fsk->f1_tx = tx_f1;
    fsk->fs_tx = tx_fs;
    fsk->nin = fsk->N;
    fsk->mode = M==2 ? MODE_2FSK : MODE_4FSK;
    fsk->Nbits = M==2 ? fsk->Nsym : fsk->Nsym*2;
    fsk->est_min = HORUS_MIN;
    fsk->est_max = HORUS_MAX;
    fsk->est_space = HORUS_MIN_SPACING;

    /* Set up rx state */
    for( i=0; i<M; i++)
        fsk->phi_c[i] = comp_exp_j(0);

    memold = (4*fsk->Ts);

    fsk->nstash = memold;
    fsk->samp_old = (COMP*) malloc(sizeof(COMP)*memold);
    if(fsk->samp_old == NULL){
        free(fsk);
        return NULL;
    }

    for(i=0;i<memold;i++) {
        fsk->samp_old[i].real = 0.0;
        fsk->samp_old[i].imag = 0.0;
    }

    fsk->fft_cfg = kiss_fft_alloc(Ndft,0,NULL,NULL);
    if(fsk->fft_cfg == NULL){
        free(fsk->samp_old);
        free(fsk);
        return NULL;
    }

    fsk->fft_est = (float*)malloc(sizeof(float)*fsk->Ndft/2);
    if(fsk->fft_est == NULL){
        free(fsk->samp_old);
        free(fsk->fft_cfg);
        free(fsk);
        return NULL;
    }

    #ifdef USE_HANN_TABLE
        #ifdef GENERATE_HANN_TABLE_RUNTIME
            fsk->hann_table = (float*)malloc(sizeof(float)*fsk->Ndft);
            if(fsk->hann_table == NULL){
                free(fsk->fft_est);
                free(fsk->samp_old);
                free(fsk->fft_cfg);
                free(fsk);
                return NULL;
            }
            fsk_generate_hann_table(fsk);
        #else
            fsk->hann_table = NULL;
        #endif
    #endif

    for(i=0;i<Ndft/2;i++)fsk->fft_est[i] = 0;

    fsk->norm_rx_timing = 0;

    /* Set up tx state */
    fsk->tx_phase_c = comp_exp_j(0);

    /* Set up demod stats */
    fsk->EbNodB = 0;

    for( i=0; i<M; i++)
        fsk->f_est[i] = 0;

    fsk->ppm = 0;

    fsk->stats = (struct MODEM_STATS*)malloc(sizeof(struct MODEM_STATS));

    if(fsk->stats == NULL){
        free(fsk->fft_est);
        free(fsk->samp_old);
        free(fsk->fft_cfg);
        free(fsk);
        return NULL;
    }
    stats_init(fsk);
    fsk->normalise_eye = 1;

    return fsk;
}

/* make sure stats have known values in case monitoring process reads stats before they are set */

static void stats_init(struct FSK *fsk) {
    /* Take a sample for the eye diagrams */
    int i,j,m;
    int P = fsk->P;
    int M = fsk->mode;

    /* due to oversample rate P, we have too many samples for eye
       trace.  So lets output a decimated version */

    /* asserts below as we found some problems over-running eye matrix */

    /* TODO: refactor eye tracing code here and in fsk_demod */

    int neyesamp_dec = ceil(((float)P*2)/MODEM_STATS_EYE_IND_MAX);
    int neyesamp = (P*2)/neyesamp_dec;
    assert(neyesamp <= MODEM_STATS_EYE_IND_MAX);
    fsk->stats->neyesamp = neyesamp;

    int eye_traces = MODEM_STATS_ET_MAX/M;

    fsk->stats->neyetr = fsk->mode*eye_traces;
    for(i=0; i<eye_traces; i++) {
        for (m=0; m<M; m++){
            for(j=0; j<neyesamp; j++) {
                assert((i*M+m) < MODEM_STATS_ET_MAX);
                fsk->stats->rx_eye[i*M+m][j] = 0;
           }
        }
    }

    fsk->stats->rx_timing = fsk->stats->snr_est = 0;

}


void fsk_set_nsym(struct FSK *fsk,int nsyms){
    assert(nsyms>0);
    int Ndft,i;
    Ndft = 0;

    /* Set constant config parameters */
    fsk->N = fsk->Ts*nsyms;
    fsk->Nsym = nsyms;
    fsk->Nmem = fsk->N+(2*fsk->Ts);
    fsk->nin = fsk->N;
    fsk->Nbits = fsk->mode==2 ? fsk->Nsym : fsk->Nsym*2;

    /* Find smallest 2^N value that fits Fs for efficient FFT */
    /* It would probably be better to use KISS-FFt's routine here */
    for(i=1; i; i<<=1)
        if((fsk->N)&i)
            Ndft = i;

    fsk->Ndft = Ndft;

    free(fsk->fft_cfg);
    free(fsk->fft_est);

    fsk->fft_cfg = kiss_fft_alloc(Ndft,0,NULL,NULL);
    fsk->fft_est = (float*)malloc(sizeof(float)*fsk->Ndft/2);

    for(i=0;i<Ndft/2;i++)fsk->fft_est[i] = 0;

}

/* Set the FSK modem into burst demod mode */

void fsk_enable_burst_mode(struct FSK *fsk,int nsyms){
    fsk_set_nsym(fsk,nsyms);
    fsk->nin = fsk->N;
    fsk->burst_mode = 1;
}

void fsk_clear_estimators(struct FSK *fsk){
    int i;
    /* Clear freq estimator state */
    for(i=0; i < (fsk->Ndft/2); i++){
        fsk->fft_est[i] = 0;
    }
    /* Reset timing diff correction */
    fsk->nin = fsk->N;
}

uint32_t fsk_nin(struct FSK *fsk){
    return (uint32_t)fsk->nin;
}

void fsk_destroy(struct FSK *fsk){
    free(fsk->fft_cfg);
    free(fsk->samp_old);
    free(fsk->stats);
    free(fsk);
}

void fsk_get_demod_stats(struct FSK *fsk, struct MODEM_STATS *stats){
    /* copy from internal stats, note we can't overwrite stats completely
       as it has other states rqd by caller, also we want a consistent
       interface across modem types for the freedv_api.
    */

    stats->clock_offset = fsk->stats->clock_offset;
    stats->snr_est = fsk->stats->snr_est;           // TODO: make this SNR not Eb/No
    stats->rx_timing = fsk->stats->rx_timing;
    stats->foff = fsk->stats->foff;

    stats->neyesamp = fsk->stats->neyesamp;
    stats->neyetr = fsk->stats->neyetr;
    memcpy(stats->rx_eye, fsk->stats->rx_eye, sizeof(stats->rx_eye));
    memcpy(stats->f_est, fsk->stats->f_est, fsk->mode*sizeof(float));

    /* these fields not used for FSK so set to something sensible */

    stats->sync = 0;
    stats->nr = fsk->stats->nr;
    stats->Nc = fsk->stats->Nc;
}

/*
 * Set the minimum and maximum frequencies at which the freq. estimator can find tones
 */
void fsk_set_est_limits(struct FSK *fsk,int est_min, int est_max){

    fsk->est_min = est_min;
    if(fsk->est_min<0) fsk->est_min = 0;

    fsk->est_max = est_max;
}

/*
 * Internal function to estimate the frequencies of the two tones within a block of samples.
 * This is split off because it is fairly complicated, needs a bunch of memory, and probably
 * takes more cycles than the rest of the demod.
 * Parameters:
 * fsk - FSK struct from demod containing FSK config
 * fsk_in - block of samples in this demod cycles, must be nin long
 * freqs - Array for the estimated frequencies
 * M - number of frequency peaks to find
 */
void fsk_demod_freq_est(struct FSK *fsk, COMP fsk_in[],float *freqs,int M){
    int Ndft = fsk->Ndft;
    int Fs = fsk->Fs;
    int nin = fsk->nin;
    int i,j;
    float hann;
    float max;
    float tc;
    int imax;
    kiss_fft_cfg fft_cfg = fsk->fft_cfg;
    int *freqi = new int[M];
    int f_min,f_max,f_zero;

    /* Array to do complex FFT from using kiss_fft */
    kiss_fft_cpx *fftin  = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx)*Ndft);
    kiss_fft_cpx *fftout = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx)*Ndft);

    #ifndef USE_HANN_TABLE
    COMP dphi = comp_exp_j((2*M_PI)/((float)Ndft-1));
    COMP rphi = {.5,0};
    rphi = cmult(cconj(dphi),rphi);
    #endif

    f_min  = (fsk->est_min*Ndft)/Fs;
    f_max  = (fsk->est_max*Ndft)/Fs;
    f_zero = (fsk->est_space*Ndft)/Fs;

    /* scale averaging time constant based on number of samples */
    tc = 0.95*Ndft/Fs;

    int samps;
    int fft_samps;
    int fft_loops = nin / Ndft;

    for (j = 0; j < fft_loops; j++)
    {
        /* 48000 sample rate (for example) will have a spare */
        /* 896 samples besides the 46 "Ndft" samples, so adjust */

        samps = (nin - ((j + 1) * Ndft));
        fft_samps = (samps >= Ndft) ? Ndft : samps;

        /* Copy FSK buffer into reals of FFT buffer and apply a hann window */
        for(i=0; i<fft_samps; i++){
            #ifdef USE_HANN_TABLE
            hann = fsk->hann_table[i];
            #else
            //hann = 1-cosf((2*M_PI*(float)(i))/((float)fft_samps-1));
            rphi = cmult(dphi,rphi);
            hann = .5-rphi.real;
            #endif
            fftin[i].r = hann*fsk_in[i+Ndft*j].real;
            fftin[i].i = hann*fsk_in[i+Ndft*j].imag;
        }

        /* Zero out the remaining slots on spare samples */
        for(; i<Ndft;i++){
            fftin[i].r = 0;
            fftin[i].i = 0;
        }

        /* Do the FFT */
        kiss_fft(fft_cfg,fftin,fftout);

        /* Find the magnitude^2 of each freq slot and stash away in the real
        * value, so this only has to be done once. Since we're only comparing
        * these values and only need the mag of 2 points, we don't need to do
        * a sqrt to each value */
        for(i=0; i<Ndft/2; i++){
            fftout[i].r = (fftout[i].r*fftout[i].r) + (fftout[i].i*fftout[i].i) ;
        }

        /* Zero out the minimum and maximum ends */
        for(i=0; i<f_min; i++){
            fftout[i].r = 0;
        }
        for(i=f_max-1; i<Ndft/2; i++){
            fftout[i].r = 0;
        }
        /* Mix back in with the previous fft block */
        /* Copy new fft est into imag of fftout for frequency divination below */
        for(i=0; i<Ndft/2; i++){
            fsk->fft_est[i] = (fsk->fft_est[i]*(1-tc)) + (sqrtf(fftout[i].r)*tc);
            fftout[i].i = fsk->fft_est[i];
        }
    }

    modem_probe_samp_f("t_fft_est", fsk->fft_est, Ndft/2);

    max = 0;
    /* Find the M frequency peaks here */
    for(i=0; i<M; i++){
        imax = 0;
        max = 0;
        for(j=0;j<Ndft/2;j++){
            if(fftout[j].i > max){
                max = fftout[j].i;
                imax = j;
            }
        }
        /* Blank out FMax +/-Fspace/2 */
        f_min = imax - f_zero;
        f_min = f_min < 0 ? 0 : f_min;
        f_max = imax + f_zero;
        f_max = f_max > Ndft ? Ndft : f_max;
        for(j=f_min; j<f_max; j++)
            fftout[j].i = 0;

        /* Stick the freq index on the list */
        freqi[i] = imax;
    }

    /* Gnome sort the freq list */
    /* My favorite sort of sort*/
    i = 1;
    while(i<M){
        if(freqi[i] >= freqi[i-1]) i++;
        else{
            j = freqi[i];
            freqi[i] = freqi[i-1];
            freqi[i-1] = j;
            if(i>1) i--;
        }
    }

    /* Convert freqs from indices to frequencies */
    for(i=0; i<M; i++){
        freqs[i] = (float)(freqi[i])*((float)Fs/(float)Ndft);
    }

    delete[] freqi;
    free(fftin);
    free(fftout);
}

void fsk2_demod(struct FSK *fsk, uint8_t rx_bits[], float rx_sd[], COMP fsk_in[]){
    int N = fsk->N;
    int Ts = fsk->Ts;
    int Rs = fsk->Rs;
    int Fs = fsk->Fs;
    int nsym = fsk->Nsym;
    int nin = fsk->nin;
    int P = fsk->P;
    int Nmem = fsk->Nmem;
    int M = fsk->mode;
    int i, j, m, dc_i, cbuf_i;
    float ft1;
    int nstash = fsk->nstash;

    COMP* *f_int = new COMP*[M];    /* Filtered and downsampled symbol tones */
    COMP *t = new COMP[M];          /* complex number temps */
    COMP t_c;           /* another complex temp */
    COMP *phi_c = new COMP[M];
    COMP phi_ft;
    int nold = Nmem-nin;

    COMP *dphi = new COMP[M];
    COMP dphift;
    float rx_timing,norm_rx_timing,old_norm_rx_timing,d_norm_rx_timing,appm;
    int using_old_samps;

    COMP* sample_src;
    COMP* f_intbuf_m;

    float *f_est = new float[M];
    float fc_avg, fc_tx;
    float meanebno,stdebno,eye_max;
    int neyesamp,neyeoffset;

    #ifdef MODEMPROBE_ENABLE
    char mp_name_tmp[20]; /* Temporary string for modem probe trace names */
    #endif

    //for(size_t jj = 0; jj<nin; jj++){
    //    fprintf(stderr,"%f,j%f,",fsk_in[jj].real,fsk_in[jj].imag);
    //}

    /* Load up demod phases from struct */
    for(m = 0; m < M; m++) {
        phi_c[m] = fsk->phi_c[m];
    }

    /* Estimate tone frequencies */
    fsk_demod_freq_est(fsk,fsk_in,f_est,M);
    modem_probe_samp_f("t_f_est",f_est,M);


    /* Allocate circular buffer for integration */
    f_intbuf_m = (COMP*) malloc(sizeof(COMP)*Ts);

    /* allocate memory for the integrated samples */
    for( m=0; m<M; m++){
        /* allocate memory for the integrated samples */
        /* Note: This must be kept after fsk_demod_freq_est for memory usage reasons */
        f_int[m] = (COMP*) malloc(sizeof(COMP)*(nsym+1)*P);
    }

    /* If this is the first run, we won't have any valid f_est */
    /* TODO: add first_run flag to FSK to make negative freqs possible */
    if(fsk->f_est[0]<1){
        for( m=0; m<M; m++)
            fsk->f_est[m] = f_est[m];
    }

    /* Initalize downmixers for each symbol tone */
    for( m=0; m<M; m++){
        /* Back the stored phase off to account for re-integraton of old samples */
        dphi[m] = comp_exp_j(-2*(Nmem-nin-(Ts/P))*M_PI*((fsk->f_est[m])/(float)(Fs)));
        phi_c[m] = cmult(dphi[m],phi_c[m]);
        //fprintf(stderr,"F%d = %f",m,fsk->f_est[m]);

        /* Figure out how much to nudge each sample downmixer for every sample */
        dphi[m] = comp_exp_j(2*M_PI*((fsk->f_est[m])/(float)(Fs)));
    }

    /* Integrate and downsample for symbol tones */
    for(m=0; m<M; m++){
        /* Copy buffer pointers in to avoid second buffer indirection */
        float f_est_m = f_est[m];
        COMP* f_int_m = &(f_int[m][0]);
        COMP dphi_m = dphi[m];

        dc_i = 0;
        cbuf_i = 0;
        sample_src = &(fsk->samp_old[nstash-nold]);
        using_old_samps = 1;

        /* Pre-fill integration buffer */
        for(dc_i=0; dc_i<Ts-(Ts/P); dc_i++){
            /* Switch sample source to new samples when we run out of old ones */
            if(dc_i>=nold && using_old_samps){
                sample_src = &fsk_in[0];
                dc_i = 0;
                using_old_samps = 0;

                /* Recalculate delta-phi after switching to new sample source */
                phi_c[m] = comp_normalize(phi_c[m]);
                dphi_m = comp_exp_j(2*M_PI*((f_est_m)/(float)(Fs)));
            }
            /* Downconvert and place into integration buffer */
            f_intbuf_m[dc_i]=cmult(sample_src[dc_i],cconj(phi_c[m]));

            #ifdef MODEMPROBE_ENABLE
            snprintf(mp_name_tmp,19,"t_f%zd_dc",m+1);
            modem_probe_samp_c(mp_name_tmp,&f_intbuf_m[dc_i],1);
            #endif
            /* Spin downconversion phases */
            phi_c[m] = cmult(phi_c[m],dphi_m);
        }
        cbuf_i = dc_i;

        /* Integrate over Ts at offsets of Ts/P */
        for(i=0; i<(nsym+1)*P; i++){
            /* Downconvert and Place Ts/P samples in the integration buffers */
            for(j=0; j<(Ts/P); j++,dc_i++){
                /* Switch sample source to new samples when we run out of old ones */
                if(dc_i>=nold && using_old_samps){
                    sample_src = &fsk_in[0];
                    dc_i = 0;
                    using_old_samps = 0;

                    /* Recalculate delta-phi after switching to new sample source */
                    phi_c[m] = comp_normalize(phi_c[m]);
                    dphi_m = comp_exp_j(2*M_PI*((f_est_m)/(float)(Fs)));
                }
                /* Downconvert and place into integration buffer */
                f_intbuf_m[cbuf_i+j]=cmult(sample_src[dc_i],cconj(phi_c[m]));

                #ifdef MODEMPROBE_ENABLE
                snprintf(mp_name_tmp,19,"t_f%zd_dc",m+1);
                modem_probe_samp_c(mp_name_tmp,&f_intbuf_m[cbuf_i+j],1);
                #endif
                /* Spin downconversion phases */
                phi_c[m] = cmult(phi_c[m],dphi_m);

            }

            /* Dump internal samples */
            cbuf_i += Ts/P;
            if(cbuf_i>=Ts) cbuf_i = 0;

            /* Integrate over the integration buffers, save samples */
            float it_r = 0;
            float it_i = 0;
            for(j=0; j<Ts; j++){
                it_r += f_intbuf_m[j].real;
                it_i += f_intbuf_m[j].imag;
            }
            f_int_m[i].real = it_r;
            f_int_m[i].imag = it_i;
        }
    }

    /* Save phases back into FSK struct */
    for(m=0; m<M; m++){
        fsk->phi_c[m] = phi_c[m];
        fsk->f_est[m] = f_est[m];
    }

    /* Stash samples away in the old sample buffer for the next round of bit getting */
    memcpy((void*)&(fsk->samp_old[0]),(void*)&(fsk_in[nin-nstash]),sizeof(COMP)*nstash);

    /* Fine Timing Estimation */
    /* Apply magic nonlinearity to f1_int and f2_int, shift down to 0,
     * extract angle */

    /* Figure out how much to spin the oscillator to extract magic spectral line */
    dphift = comp_exp_j(2*M_PI*((float)(Rs)/(float)(P*Rs)));
    phi_ft.real = 1;
    phi_ft.imag = 0;
    t_c=comp0();
    for(i=0; i<(nsym+1)*P; i++){
        /* Get abs^2 of fx_int[i], and add 'em */
        ft1 = 0;
        for( m=0; m<M; m++){
            ft1 += (f_int[m][i].real*f_int[m][i].real) + (f_int[m][i].imag*f_int[m][i].imag);
        }

        /* Down shift and accumulate magic line */
        t_c = cadd(t_c,fcmult(ft1,phi_ft));

        /* Spin the oscillator for the magic line shift */
        phi_ft = cmult(phi_ft,dphift);
    }

    /* Check for NaNs in the fine timing estimate, return if found */
    /* otherwise segfaults happen */
    if( std::isnan(t_c.real) || std::isnan(t_c.imag)){
        return;
    }

    /* Get the magic angle */
    norm_rx_timing =  atan2f(t_c.imag,t_c.real)/(2*M_PI);
    rx_timing = norm_rx_timing*(float)P;

    old_norm_rx_timing = fsk->norm_rx_timing;
    fsk->norm_rx_timing = norm_rx_timing;

    /* Estimate sample clock offset */
    d_norm_rx_timing = norm_rx_timing - old_norm_rx_timing;

    /* Filter out big jumps in due to nin change */
    if(fabsf(d_norm_rx_timing) < .2){
        appm = 1e6*d_norm_rx_timing/(float)nsym;
        fsk->ppm = .9*fsk->ppm + .1*appm;
    }

    /* Figure out how many samples are needed the next modem cycle */
    /* Unless we're in burst mode */
    if(!fsk->burst_mode){
        if(norm_rx_timing > 0.25)
            fsk->nin = N+Ts/2;
        else if(norm_rx_timing < -0.25)
            fsk->nin = N-Ts/2;
        else
            fsk->nin = N;
    }

    modem_probe_samp_f("t_norm_rx_timing",&(norm_rx_timing),1);
    modem_probe_samp_i("t_nin",&(fsk->nin),1);

    /* Re-sample the integrators with linear interpolation magic */
    int low_sample = (int)floorf(rx_timing);
    float fract = rx_timing - (float)low_sample;
    int high_sample = (int)ceilf(rx_timing);

    /* Vars for finding the max-of-4 for each bit */
    float *tmax = new float[M];

    #ifdef EST_EBNO
    meanebno = 0;
    stdebno = 0;
    #endif

    /* FINALLY, THE BITS */
    /* also, resample fx_int */
    for(i = 0; i < nsym; i++)
    {
        int st = (i+1)*P;
        for( m=0; m<M; m++){
            t[m] =           fcmult(1-fract,f_int[m][st+ low_sample]);
            t[m] = cadd(t[m],fcmult(  fract,f_int[m][st+high_sample]));
            /* Figure mag^2 of each resampled fx_int */
            tmax[m] = (t[m].real*t[m].real) + (t[m].imag*t[m].imag);
        }

        float max = tmax[0]; /* Maximum for figuring correct symbol */
        float min = tmax[0];
        int sym = 0; /* Index of maximum */
        for( m=0; m<M; m++){
            if(tmax[m]>max){
                max = tmax[m];
                sym = m;
            }
            if(tmax[m]<min){
                min = tmax[m];
            }
        }

        /* Get the actual bit */
        if(rx_bits != NULL){
            /* Get bits for 2FSK and 4FSK */
            /* TODO: Replace this with something more generic maybe */
            if(M==2){
                rx_bits[i] = sym==1;                /* 2FSK. 1 bit per symbol */
            }else if(M==4){
                rx_bits[(i*2)+1] = (sym&0x1);       /* 4FSK. 2 bits per symbol */
                rx_bits[(i*2)  ] = (sym&0x2)>>1;
            }
        }

        /* Produce soft decision symbols */
        if(rx_sd != NULL){
            /* Convert symbols from max^2 into max */
            for( m=0; m<M; m++)
                tmax[m] = sqrtf(tmax[m]);

            if(M==2){
                rx_sd[i] = tmax[0] - tmax[1];
            }else if(M==4){
                /* TODO: Find a soft-decision mode that works for 4FSK */
                min = sqrtf(min);
                rx_sd[(i*2)+1] = - tmax[0] ;  /* Bits=00 */
                rx_sd[(i*2)  ] = - tmax[0] ;
                rx_sd[(i*2)+1]+=   tmax[1] ;  /* Bits=01 */
                rx_sd[(i*2)  ]+= - tmax[1] ;
                rx_sd[(i*2)+1]+= - tmax[2] ;  /* Bits=10 */
                rx_sd[(i*2)  ]+=   tmax[2] ;
                rx_sd[(i*2)+1]+=   tmax[3] ;  /* Bits=11 */
                rx_sd[(i*2)  ]+=   tmax[3] ;
            }
        }
        /* Accumulate resampled int magnitude for EbNodB estimation */
        /* Standard deviation is calculated by algorithm devised by crafty soviets */
        #ifdef EST_EBNO
        /* Accumulate the square of the sampled value */
        ft1 = max;
        stdebno += ft1;

        /* Figure the abs value of the max tone */
        meanebno += sqrtf(ft1);
        #endif
        /* Soft output goes here */
    }

    delete[] tmax;

    #ifdef EST_EBNO

    /* Calculate mean for EbNodB estimation */
    meanebno = meanebno/(float)nsym;

    /* Calculate the std. dev for EbNodB estimate */
    stdebno = (stdebno/(float)nsym) - (meanebno*meanebno);
    /* trap any negative numbers to avoid NANs flowing through */
    if (stdebno > 0.0) {
        stdebno = sqrt(stdebno);
    } else {
        stdebno = 0.0;
    }

    fsk->EbNodB = -6+(20*log10f((1e-6+meanebno)/(1e-6+stdebno)));
    #else
    fsk->EbNodB = 1;
    #endif

    /* Write some statistics to the stats struct */

    /* Save clock offset in ppm */
    fsk->stats->clock_offset = fsk->ppm;

    /* Calculate and save SNR from EbNodB estimate */

    fsk->stats->snr_est = .5*fsk->stats->snr_est + .5*fsk->EbNodB;//+ 10*log10f(((float)Rs)/((float)Rs*M));

    /* Save rx timing */
    fsk->stats->rx_timing = (float)rx_timing;

    /* Estimate and save frequency offset */
    fc_avg = (f_est[0]+f_est[1])/2;
    fc_tx = (fsk->f1_tx+fsk->f1_tx+fsk->fs_tx)/2;
    fsk->stats->foff = fc_tx-fc_avg;

    /* Take a sample for the eye diagrams ---------------------------------- */

    /* due to oversample rate P, we have too many samples for eye
       trace.  So lets output a decimated version.  We use 2P
       as we want two symbols worth of samples in trace  */

    int neyesamp_dec = ceil(((float)P*2)/MODEM_STATS_EYE_IND_MAX);
    neyesamp = (P*2)/neyesamp_dec;
    assert(neyesamp <= MODEM_STATS_EYE_IND_MAX);
    fsk->stats->neyesamp = neyesamp;

    #ifdef I_DONT_UNDERSTAND
    neyeoffset = high_sample+1+(P*28); /* WTF this line? Where does "28" come from ?                           */
    #endif                             /* ifdef-ed out as I am afraid it will index out of memory as P changes */
    neyeoffset = high_sample+1;

    int eye_traces = MODEM_STATS_ET_MAX/M;
    int ind;

    fsk->stats->neyetr = fsk->mode*eye_traces;
    for( i=0; i<eye_traces; i++){
        for ( m=0; m<M; m++){
            for(j=0; j<neyesamp; j++) {
               /*
                  2*P*i...........: advance two symbols for next trace
                  neyeoffset......: centre trace on ideal timing offset, peak eye opening
                  j*neweyesamp_dec: For 2*P>MODEM_STATS_EYE_IND_MAX advance through integrated
                                    samples newamp_dec at a time so we dont overflow rx_eye[][]
               */
               ind = 2*P*i + neyeoffset + j*neyesamp_dec;
               assert((i*M+m) < MODEM_STATS_ET_MAX);
               assert(ind < (nsym+1)*P);
               fsk->stats->rx_eye[i*M+m][j] = cabsolute(f_int[m][ind]);
            }
        }
    }

    if (fsk->normalise_eye) {
        eye_max = 0;
        /* Normalize eye to +/- 1 */
        for(i=0; i<M*eye_traces; i++)
            for(j=0; j<neyesamp; j++)
                if(fabsf(fsk->stats->rx_eye[i][j])>eye_max)
                    eye_max = fabsf(fsk->stats->rx_eye[i][j]);

        for(i=0; i<M*eye_traces; i++)
            for(j=0; j<neyesamp; j++)
                fsk->stats->rx_eye[i][j] = fsk->stats->rx_eye[i][j]/eye_max;
    }

    fsk->stats->nr = 0;
    fsk->stats->Nc = 0;

    for(i=0; i<M; i++) {
        fsk->stats->f_est[i] = f_est[i];
    }

    /* Dump some internal samples */
    modem_probe_samp_f("t_EbNodB",&(fsk->EbNodB),1);
    modem_probe_samp_f("t_ppm",&(fsk->ppm),1);
    modem_probe_samp_f("t_rx_timing",&(rx_timing),1);

    #ifdef MODEMPROBE_ENABLE
    for( m=0; m<M; m++){
        snprintf(mp_name_tmp,19,"t_f%zd_int",m+1);
        modem_probe_samp_c(mp_name_tmp,f_int[m],(nsym+1)*P);
        snprintf(mp_name_tmp,19,"t_f%zd",m+1);
        modem_probe_samp_f(mp_name_tmp,&f_est[m],1);
    }
    #endif

    for( m=0; m<M; m++){
        free(f_int[m]);
    }
    free(f_intbuf_m);

    delete[] f_est;
    delete[] dphi;
    delete[] phi_c;
    delete[] t;
    delete[] f_int;
}

void fsk_demod(struct FSK *fsk, uint8_t rx_bits[], COMP fsk_in[]){
    fsk2_demod(fsk,rx_bits,NULL,fsk_in);
}

void fsk_demod_sd(struct FSK *fsk, float rx_sd[], COMP fsk_in[]){
    fsk2_demod(fsk,NULL,rx_sd,fsk_in);
}

void fsk_mod(struct FSK *fsk,float fsk_out[],uint8_t tx_bits[]){
    COMP tx_phase_c = fsk->tx_phase_c; /* Current complex TX phase */
    int f1_tx = fsk->f1_tx;         /* '0' frequency */
    int fs_tx = fsk->fs_tx;         /* space between frequencies */
    int Ts = fsk->Ts;               /* samples-per-symbol */
    int Fs = fsk->Fs;               /* sample freq */
    int M = fsk->mode;
    COMP *dosc_f = new COMP[M];     /* phase shift per sample */
    COMP dph;                       /* phase shift of current bit */
    int i, j, m, bit_i, sym;

    /* Init the per sample phase shift complex numbers */
    for( m=0; m<M; m++){
        dosc_f[m] = comp_exp_j(2*M_PI*((float)(f1_tx+(fs_tx*m))/(float)(Fs)));
    }

    bit_i = 0;
    for( i=0; i<fsk->Nsym; i++){
        sym = 0;
        /* Pack the symbol number from the bit stream */
        for( m=M; m>>=1; ){
            uint8_t bit = tx_bits[bit_i];
            bit = (bit==1)?1:0;
            sym = (sym<<1)|bit;
            bit_i++;
        }
        /* Look up symbol phase shift */
        dph = dosc_f[sym];
        /* Spin the oscillator for a symbol period */
        for(j=0; j<Ts; j++){
            tx_phase_c = cmult(tx_phase_c,dph);
            fsk_out[i*Ts+j] = 2*tx_phase_c.real;
        }

    }

    /* Normalize TX phase to prevent drift */
    tx_phase_c = comp_normalize(tx_phase_c);

    /* save TX phase */
    fsk->tx_phase_c = tx_phase_c;

    delete[] dosc_f;
}

void fsk_mod_c(struct FSK *fsk,COMP fsk_out[],uint8_t tx_bits[]){
    COMP tx_phase_c = fsk->tx_phase_c; /* Current complex TX phase */
    int f1_tx = fsk->f1_tx;         /* '0' frequency */
    int fs_tx = fsk->fs_tx;         /* space between frequencies */
    int Ts = fsk->Ts;               /* samples-per-symbol */
    int Fs = fsk->Fs;               /* sample freq */
    int M = fsk->mode;
    COMP *dosc_f = new COMP[M];     /* phase shift per sample */
    COMP dph;                       /* phase shift of current bit */
    int i, j, bit_i, sym;
    int m;

    /* Init the per sample phase shift complex numbers */
    for( m=0; m<M; m++){
        dosc_f[m] = comp_exp_j(2*M_PI*((float)(f1_tx+(fs_tx*m))/(float)(Fs)));
    }

    bit_i = 0;
    for( i=0; i<fsk->Nsym; i++){
        sym = 0;
        /* Pack the symbol number from the bit stream */
        for( m=M; m>>=1; ){
            uint8_t bit = tx_bits[bit_i];
            bit = (bit==1)?1:0;
            sym = (sym<<1)|bit;
            bit_i++;
        }
        /* Look up symbol phase shift */
        dph = dosc_f[sym];
        /* Spin the oscillator for a symbol period */
        for(j=0; j<Ts; j++){
            tx_phase_c = cmult(tx_phase_c,dph);
            fsk_out[i*Ts+j] = fcmult(2,tx_phase_c);
        }
    }

    /* Normalize TX phase to prevent drift */
    tx_phase_c = comp_normalize(tx_phase_c);

    /* save TX phase */
    fsk->tx_phase_c = tx_phase_c;

    delete[] dosc_f;
}


/* Modulator that assume an external VCO.  The output is a voltage
   that changes for each symbol */

void fsk_mod_ext_vco(struct FSK *fsk, float vco_out[], uint8_t tx_bits[]) {
    int f1_tx = fsk->f1_tx;         /* '0' frequency */
    int fs_tx = fsk->fs_tx;         /* space between frequencies */
    int Ts = fsk->Ts;               /* samples-per-symbol */
    int M = fsk->mode;
    int i, j, m, sym, bit_i;

    bit_i = 0;
    for(i=0; i<fsk->Nsym; i++) {
        /* generate the symbol number from the bit stream,
           e.g. 0,1 for 2FSK, 0,1,2,3 for 4FSK */

        sym = 0;

        /* unpack the symbol number from the bit stream */

        for( m=M; m>>=1; ){
            uint8_t bit = tx_bits[bit_i];
            bit = (bit==1)?1:0;
            sym = (sym<<1)|bit;
            bit_i++;
        }

        /*
           Map 'sym' to VCO frequency
           Note: drive is inverted, a higher tone drives VCO voltage lower
         */

        //fprintf(stderr, "i: %d sym: %d freq: %f\n", i, sym, f1_tx + fs_tx*(float)sym);
        for(j=0; j<Ts; j++) {
            vco_out[i*Ts+j] = f1_tx + fs_tx*(float)sym;
        }
    }
}

void fsk_stats_normalise_eye(struct FSK *fsk, int normalise_enable) {
    fsk->normalise_eye = normalise_enable;
}

} // freeDV





