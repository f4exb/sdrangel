
#define _USE_MATH_DEFINES
#include <math.h>

#include "dsd.h"

#include "p25p1_heuristics.h"

/**
 * This module is dedicated to improve the accuracy of the digitizer. The digitizer is the piece of code that
 * translates an analog value to an actual symbol, in the case of P25, a dibit.
 * It implements a simple Gaussian classifier. It's based in the assumption that the analog values from the
 * signal follow normal distributions, one single distribution for each symbol.
 * Analog values for the dibit "0" will fit into a Gaussian bell curve with a characteristic mean and std
 * distribution and the same goes for dibits "1", "2" and "3."
 * Hopefully those bell curves are well separated from each other so we can accurately discriminate dibits.
 * If we could model the Gaussian of each dibit, then given an analog value, the dibit whose Gaussian fits
 * better is the most likely interpretation for that value. By better fit we can calculate the PDF
 * (probability density function) for the Gaussian, the one with the highest value is the best fit.
 *
 * The approach followed here to model the Gaussian for each dibit is to use the error corrected information
 * as precise oracles. P25 uses strong error correction, some dibits are doubly protected by Hamming/Golay
 * and powerful Reed-Solomon codes. If a sequence of dibits clears the last Reed-Solomon check, we can be
 * quite confident that those values are correct. We can use the analog values for those cleared dibits to
 * calculate mean and std deviation of our Gaussians. With this we are ready to calculate the PDF of a new
 * unknown analog value when required.
 * Values that don't clear the Reed-Solomon check are discarded.
 * This implementation uses a circular buffer to keep track of the N latest cleared analog dibits so we can
 * adapt to changes in the signal.
 * A modification was made to improve results for C4FM signals. See next block comment.
 */

/**
 * In the C4FM P25 recorded files from the "samples" repository, it can be observed that there is a
 * correlation between the correct dibit associated for a given analog value and the value of the previous
 * dibit. For instance, in one P25 recording, the dibits "0" come with an average analog signal of
 * 3829 when the previous dibit was also "0," but if the previous dibit was a "3" then the average
 * analog signal is 6875. These are the mean and std deviations for the full 4x4 combinations of previous and
 * current dibits:
 *
 * 00: count: 200 mean:    3829.12 sd:     540.43    <-
 * 01: count: 200 mean:   13352.45 sd:     659.74
 * 02: count: 200 mean:   -5238.56 sd:    1254.70
 * 03: count: 200 mean:  -13776.50 sd:     307.41
 * 10: count: 200 mean:    3077.74 sd:    1059.00
 * 11: count: 200 mean:   11935.11 sd:     776.20
 * 12: count: 200 mean:   -6079.46 sd:    1003.94
 * 13: count: 200 mean:  -13845.43 sd:     264.42
 * 20: count: 200 mean:    5574.33 sd:    1414.71
 * 21: count: 200 mean:   13687.75 sd:     727.68
 * 22: count: 200 mean:   -4753.38 sd:     765.95
 * 23: count: 200 mean:  -12342.17 sd:    1372.77
 * 30: count: 200 mean:    6875.23 sd:    1837.38    <-
 * 31: count: 200 mean:   14527.99 sd:     406.85
 * 32: count: 200 mean:   -3317.61 sd:    1089.02
 * 33: count: 200 mean:  -12576.08 sd:    1161.77
 * ||          |             |              |
 * ||          |             |              \_std deviation
 * ||          |             |
 * ||          |             \_mean of the current dibit
 * ||          |
 * ||          \_number of dibits used to calculate mean and std deviation
 * ||
 * |\_current dibit
 * |
 * \_previous dibit
 *
 * This effect is not observed on QPSK or GFSK signals, there the mean values are quite consistent regardless
 * of the previous dibit.
 *
 * The following define enables taking the previous dibit into account for C4FM signals. Comment out
 * to disable.
 */
#define USE_PREVIOUS_DIBIT

/**
 * There is a minimum of cleared analog values we need to produce a meaningful mean and std deviation.
 */
#define MIN_ELEMENTS_FOR_HEURISTICS 10


//Uncomment to disable the behaviour of this module.
//#define DISABLE_HEURISTICS

/**
 * The value of the previous dibit is only taken into account on the C4FM modulation. QPSK and GFSK are
 * not improved by this technique.
 */
static int use_previous_dibit(int rf_mod)
{
    // 0: C4FM modulation
    // 1: QPSK modulation
    // 2: GFSK modulation

    // Use previoud dibit information when on C4FM
    return (rf_mod == 0)? 1 : 0;
}

/**
 * Update the model of a symbol's Gaussian with new information.
 * \param heuristics Pointer to the P25Heuristics module with all the needed state information.
 * \param previous_dibit The cleared previous dibit value.
 * \param original_dibit The current dibit as it was interpreted initially.
 * \param dibit The current dibit. Will be different from original_dibit if the FEC fixed it.
 * \param analog_value The actual analog signal value from which the original_dibit was derived.
 */
static void update_p25_heuristics(P25Heuristics* heuristics, int previous_dibit, int original_dibit, int dibit, int analog_value)
{
    float mean;
    int old_value;
    float old_mean;

    SymbolHeuristics* sh;
    int number_errors;

#ifndef USE_PREVIOUS_DIBIT
    previous_dibit = 0;
#endif

    // Locate the Gaussian (SymbolHeuristics structure) we are going to update
    sh = &(heuristics->symbols[previous_dibit][dibit]);

    // Update the circular buffers of values
    old_value = sh->values[sh->index];
    old_mean = sh->means[sh->index];

    // Update the BER statistics
    number_errors = 0;
    if (original_dibit != dibit) {
        if ((original_dibit == 0 && dibit == 3) || (original_dibit == 3 && dibit == 0) ||
            (original_dibit == 1 && dibit == 2) || (original_dibit == 2 && dibit == 1)) {
            // Interpreting a "00" as "11", "11" as "00", "01" as "10" or "10" as "01" counts as 2 errors
            number_errors = 2;
        } else {
            // The other 8 combinations count (where original_dibit != dibit) as 1 error.
            number_errors = 1;
        }
    }
    update_error_stats(heuristics, 2, number_errors);

    // Update the running mean and variance. This is to calculate the PDF faster when required
    if (sh->count >= HEURISTICS_SIZE) {
        sh->sum -= old_value;
        sh->var_sum -= (((float)old_value) - old_mean) * (((float)old_value) - old_mean);
    }
    sh->sum += analog_value;

    sh->values[sh->index] = analog_value;
    if (sh->count < HEURISTICS_SIZE) {
        sh->count++;
    }
    mean = sh->sum / ((float)sh->count);
    sh->means[sh->index] = mean;
    if (sh->index >= (HEURISTICS_SIZE-1)) {
        sh->index = 0;
    } else {
        sh->index++;
    }

    sh->var_sum += (((float)analog_value) - mean) * (((float)analog_value) - mean);
}

void contribute_to_heuristics(int rf_mod, P25Heuristics* heuristics, AnalogSignal* analog_signal_array, int count)
{
    int i;
    int use_prev_dibit;

#ifdef USE_PREVIOUS_DIBIT
    use_prev_dibit = use_previous_dibit(rf_mod);
#else
    use_prev_dibit = 0;
#endif

    for (i=0; i<count; i++) {
        int use;
        int prev_dibit;

        if (use_prev_dibit) {
            if (analog_signal_array[i].sequence_broken) {
                // The sequence of dibits was broken here so we don't have reliable information on the actual
                // value of the previous dibit. Don't use this value.
                use = 0;
            } else {
                use = 1;
                // The previous dibit is the corrected_dibit of the previous element
                prev_dibit = analog_signal_array[i-1].corrected_dibit;
            }
        } else {
            use = 1;
            prev_dibit = 0;
        }

        if (use) {
            update_p25_heuristics(heuristics, prev_dibit, analog_signal_array[i].dibit,
                    analog_signal_array[i].corrected_dibit, analog_signal_array[i].value);
        }
    }
}

/**
 * Initializes the symbol's heuristics state.
 * \param sh The SymbolHeuristics structure to initialize.
 */
static void initialize_symbol_heuristics(SymbolHeuristics* sh)
{
    sh->count = 0;
    sh->index = 0;
    sh->sum = 0;
    sh->var_sum = 0;
}

void initialize_p25_heuristics(P25Heuristics* heuristics)
{
    int i, j;
    for (i=0; i<4; i++) {
        for (j=0; j<4; j++) {
            initialize_symbol_heuristics(&(heuristics->symbols[i][j]));
        }
    }
    heuristics->bit_count = 0;
    heuristics->bit_error_count = 0;
}

/**
 * Important method to calculate the PDF (probability density function) of the Gaussian.
 * TODO: improve performance. Since we are calculating this value to compare it with other PDF we can
 * simplify very much. We don't really need to know the actual PDF value, just which Gaussian's got the
 * highest PDF, which is a simpler problem.
 */
static float evaluate_pdf(SymbolHeuristics* se, int value)
{
    float x = (se->count*((float)value) - se->sum);
    float y = -0.5F*x*x/(se->count*se->var_sum);
    float pdf = sqrtf(se->count / se->var_sum) * expf(y) / sqrtf(2.0F * ((float) M_PI));

    return pdf;
}


/**
 * Logging of the internal PDF values for a given analog value and previous dibit.
 */
static void debug_log_pdf(P25Heuristics* heuristics, int previous_dibit, int analog_value)
{
    int i;
    float pdfs[4];

    for (i=0; i<4; i++) {
        pdfs[i] = evaluate_pdf(&(heuristics->symbols[previous_dibit][i]), analog_value);
    }

    fprintf(stderr, "v: %i, (%e, %e, %e, %e)\n", analog_value, pdfs[0], pdfs[1], pdfs[2], pdfs[3]);
}

int estimate_symbol(int rf_mod, P25Heuristics* heuristics, int previous_dibit, int analog_value, int* dibit)
{
    int valid;
    int i;
    float pdfs[4];


#ifdef USE_PREVIOUS_DIBIT
    int use_prev_dibit = use_previous_dibit(rf_mod);

    if (use_prev_dibit == 0)
      {
        // Ignore
        previous_dibit = 0;
      }
#else
	// Use previous_dibit as it comes.
#endif

    valid = 1;

    // Check if we have enough values to model the Gaussians for each symbol involved.
    for (i=0; i<4; i++) {
        if (heuristics->symbols[previous_dibit][i].count >= MIN_ELEMENTS_FOR_HEURISTICS) {
            pdfs[i] = evaluate_pdf(&(heuristics->symbols[previous_dibit][i]), analog_value);
        } else {
            // Not enough data, we don't trust this result
            valid = 0;
            break;
        }
    }

    if (valid) {
        // Find the highest pdf
        int max_index;
        float max;

        max_index = 0;
        max = pdfs[0];
        for (i=1; i<4; i++) {
            if (pdfs[i] > max) {
                max_index = i;
                max = pdfs[i];
            }
        }

        // The symbol is the one with the highest pdf
        *dibit = max_index;
    }

#ifdef DISABLE_HEURISTICS
    valid = 0;
#endif

    return valid;
}

/**
 * Logs the internal state of the heuristic's state. Good for debugging.
 */
static void debug_print_symbol_heuristics(int previous_dibit, int dibit, SymbolHeuristics* sh)
{
    float mean, sd;
    int k;
    int n;

    n = sh->count;
    if (n == 0)
      {
        mean = 0;
        sd = 0;
      }
    else
      {
        mean = sh->sum/n;
        sd = sqrtf(sh->var_sum / ((float) n));
      }
    fprintf(stderr, "%i%i: count: %2i mean: % 10.2f sd: % 10.2f", previous_dibit, dibit, sh->count, mean, sd);
    /*
    fprintf(stderr, "(");
    for (k=0; k<n; k++)
      {
        if (k != 0)
          {
            fprintf(stderr, ", ");
          }
        fprintf(stderr, "%i", sh->values[k]);
      }
    fprintf(stderr, ")");
    */
    fprintf(stderr, "\n");

}

void debug_print_heuristics(P25Heuristics* heuristics)
{
  int i,j;

  fprintf(stderr, "\n");

  for(i=0; i<4; i++)
    {
      for(j=0; j<4; j++)
        {
          debug_print_symbol_heuristics(i, j, &(heuristics->symbols[i][j]));
        }
    }
}

void update_error_stats(P25Heuristics* heuristics, int bits, int errors)
{
    heuristics->bit_count += bits;
    heuristics->bit_error_count += errors;

    // Normalize to avoid overflow in the counters
    if ((heuristics->bit_count & 1) == 0 && (heuristics->bit_error_count & 1) == 0) {
        // We can divide both values by 2 safely. We just care about their ratio, not the actual value
        heuristics->bit_count >>= 1;
        heuristics->bit_error_count >>= 1;
    }
}

float get_P25_BER_estimate(P25Heuristics* heuristics)
{
    float ber;
    if (heuristics->bit_count == 0) {
        ber = 0.0F;
    } else {
        ber = ((float)heuristics->bit_error_count) * 100.0F / ((float)heuristics->bit_count);
    }
    return ber;
}
