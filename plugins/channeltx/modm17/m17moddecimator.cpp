#include <cmath>
#include "m17moddecimator.h"

M17ModDecimator::M17ModDecimator() :
    mKernel(nullptr),
    mShift(nullptr)
{}

M17ModDecimator::~M17ModDecimator()
{
    delete [] mKernel;
    delete [] mShift;
}

void M17ModDecimator::initialize(
    double decimatedSampleRate,
    double passFrequency,
    unsigned oversampleRatio
)
{
    mDecimatedSampleRate = decimatedSampleRate;
    mRatio = oversampleRatio;
    mOversampleRate = decimatedSampleRate * oversampleRatio;
    double NyquistFreq = decimatedSampleRate / 2;

    // See DSP Guide.
    double Fc = (NyquistFreq + passFrequency) / 2 / mOversampleRate;
    double BW = (NyquistFreq - passFrequency) / mOversampleRate;
    int M = std::ceil(4 / BW);

    if (M % 2) {
        M++;
    }

    unsigned int activeKernelSize = M + 1;
    unsigned int inactiveSize = mRatio - activeKernelSize % mRatio;
    mKernelSize = activeKernelSize + inactiveSize;

    // DSP Guide uses approx. values.  Got these from Wikipedia.
    double a0 = 7938. / 18608., a1 = 9240. / 18608., a2 = 1430. / 18608.;

    // Allocate and initialize the FIR filter kernel.
    delete [] mKernel;
    mKernel = new float[mKernelSize];
    double gain = 0;

    for (unsigned int i = 0; i < inactiveSize; i++) {
        mKernel[i] = 0;
    }

    for (unsigned int i = 0; i < activeKernelSize; i++)
    {
        double y;

        if (i == (unsigned int) M/2) {
            y = 2 * M_PI * Fc;
        } else {
            y = (sin(2 * M_PI * Fc * (i - M / 2)) / (i - M / 2) *
                (a0 - a1 * cos(2 * M_PI * i/ M) + a2 * cos(4 * M_PI / M)));
        }

        gain += y;
        mKernel[inactiveSize + i] = y;
    }

    // Adjust the kernel for unity gain.
    float inv_gain = 1 / gain;

    for (unsigned int i = inactiveSize; i < mKernelSize; i++) {
        mKernel[i] *= inv_gain;
    }

    // Allocate and clear the shift register.
    delete [] mShift;
    mShift = new float[mKernelSize];

    for (unsigned int i = 0; i < mKernelSize; i++) {
        mShift[i] = 0;
    }

    mCursor = 0;
}

// The filter kernel is linear.  Coefficients for oldest samples
// are on the left; newest on the right.
//
// The shift register is circular.  Oldest samples are at cursor;
// newest are just left of cursor.
//
// We have to do the multiply-accumulate in two pieces.
//
//  Kernel
//  +------------+----------------+
//  | 0 .. n-c-1 |   n-c .. n-1   |
//  +------------+----------------+
//   ^            ^                ^
//   0            n-c              n
//
//  Shift Register
//  +----------------+------------+
//  |   n-c .. n-1   | 0 .. n-c-1 |
//  +----------------+------------+
//   ^                ^            ^
//   mShift           shiftp       n

void M17ModDecimator::decimate(int16_t *in, int16_t *out, unsigned int outCount)
{
    // assert(!(mCursor % mRatio));
    // assert(mCursor < mKernelSize);
    unsigned int cursor = mCursor;
    int16_t *inp = in;
    float *shiftp = mShift + cursor;

    for (unsigned int i = 0; i < outCount; i++)
    {
        // Insert mRatio input samples at cursor.
        for (int j = 0; j < mRatio; j++) {
            *shiftp++ = *inp++;
        }

        if ((cursor += mRatio) == mKernelSize)
        {
            cursor = 0;
            shiftp = mShift;
        }

        // Calculate one output sample.
        double acc = 0;
        unsigned int size0 = mKernelSize - cursor;
        unsigned int size1 = cursor;
        const float *kernel1 = mKernel + size0;

        for (unsigned int j = 0; j < size0; j++) {
            acc += shiftp[j] * mKernel[j];
        }

        for (unsigned int j = 0; j < size1; j++) {
            acc += mShift[j] * kernel1[j];
        }

        out[i] = acc;
    }

    mCursor = cursor;
}
