
#ifndef P25P1_HEURISTICS_H_030dd3530b7546abbb56f8dd1e66a2f6
#define P25P1_HEURISTICS_H_030dd3530b7546abbb56f8dd1e66a2f6

#define HEURISTICS_SIZE 200
typedef struct
{
  int values[HEURISTICS_SIZE];
  float means[HEURISTICS_SIZE];
  int index;
  int count;
  float sum;
  float var_sum;
} SymbolHeuristics;

typedef struct
{
  unsigned int bit_count;
  unsigned int bit_error_count;
  SymbolHeuristics symbols[4][4];
} P25Heuristics;

typedef struct
{
    int value;
    int dibit;
    int corrected_dibit;
    int sequence_broken;
} AnalogSignal;

#ifdef __cplusplus
extern "C"{
#endif

/**
 * Initializes the heuristics state.
 * \param heuristics The P25Heuristics structure to initialize.
 */
void initialize_p25_heuristics(P25Heuristics* heuristics);

/**
 * Important method that estimates the most likely symbol for a given analog signal value and previous dibit.
 * This is called by the digitizer.
 * \param rf_mod Indicates the modulation used. The previous dibit is only used on C4FM.
 * \param heuristics Pointer to the P25Heuristics module with all the needed state information.
 * \param previous_dibit The previous dibit.
 * \param analog_value The signal's analog value we want to interpret as a dibit.
 * \param dibit Address were to store the estimated dibit.
 * \return A boolean set to true if we are able to estimate a dibit. The reason why we might not be able
 * to estimate it is because we don't have enough information to model the Gaussians (not enough data
 * has been passed to contribute_to_heuristics).
 */
int estimate_symbol(int rf_mod, P25Heuristics* heuristics, int previous_dibit, int analog_value, int* dibit);

/**
 * Log some useful information on the heuristics state.
 */
void debug_print_heuristics(P25Heuristics* heuristics);

/**
 * This method contributes valuable information from dibits whose value we are confident is correct. We take
 * the dibits and corresponding analog signal values to model the Gaussians for each dibit (and previous
 * dibit if enabled).
 * \param rf_mod Indicates the modulation used. The previous dibit is only used on C4FM.
 * \param heuristics Pointer to the P25Heuristics module with all the needed state information.
 * \param analog_signal_array Sequence of AnalogSignal which contain the cleared dibits and analog values.
 * \param count number of cleared dibits passed (= number of elements to use from analog_signal_array).
 */
void contribute_to_heuristics(int rf_mod, P25Heuristics* heuristics, AnalogSignal* analog_signal_array, int count);

/**
 * Updates the estimate for the BER (bit error rate). Mind this is method is not called for every single
 * bit in the data stream but only for those bits over which we have an estimate of its error rate,
 * specifically the bits that are protected by Reed-Solomon codes.
 * \param heuristics The heuristics state.
 * \param bits The number of bits we have read.
 * \param errors The number of errors we estimate in those bits.
 */
void update_error_stats(P25Heuristics* heuristics, int bits, int errors);

/**
 * Returns the estimate for the BER (bit error rate).
 * \return The estimated BER. This is just the percentage of errors over the processed bits.
 */
float get_P25_BER_estimate(P25Heuristics* heuristics);

#ifdef __cplusplus
}
#endif

#endif // P25P1_HEURISTICS_H_030dd3530b7546abbb56f8dd1e66a2f6
