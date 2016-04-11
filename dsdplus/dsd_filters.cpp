///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "dsd_filters.h"

namespace DSDplus
{

// DMR filter
const float DSDFilters::xcoeffs[] =
{  -0.0083649323f, -0.0265444850f, -0.0428141462f, -0.0537571943f,
        -0.0564141052f, -0.0489161045f, -0.0310068662f, -0.0043393881f,
        +0.0275375106f, +0.0595423283f, +0.0857543325f, +0.1003565948f,
        +0.0986944931f, +0.0782804830f, +0.0395670487f, -0.0136691535f,
        -0.0744390415f, -0.1331834575f, -0.1788967208f, -0.2005995448f,
        -0.1889627181f, -0.1378439993f, -0.0454976231f, +0.0847488694f,
        +0.2444859269f, +0.4209222342f, +0.5982295474f, +0.7593684540f,
        +0.8881539892f, +0.9712773915f, +0.9999999166f, +0.9712773915f,
        +0.8881539892f, +0.7593684540f, +0.5982295474f, +0.4209222342f,
        +0.2444859269f, +0.0847488694f, -0.0454976231f, -0.1378439993f,
        -0.1889627181f, -0.2005995448f, -0.1788967208f, -0.1331834575f,
        -0.0744390415f, -0.0136691535f, +0.0395670487f, +0.0782804830f,
        +0.0986944931f, +0.1003565948f, +0.0857543325f, +0.0595423283f,
        +0.0275375106f, -0.0043393881f, -0.0310068662f, -0.0489161045f,
        -0.0564141052f, -0.0537571943f, -0.0428141462f, -0.0265444850f,
        -0.0083649323f, };

// NXDN filter
const float DSDFilters::nxcoeffs[] =
{ +0.031462429f, +0.031747267f, +0.030401148f, +0.027362877f, +0.022653298f,
        +0.016379869f, +0.008737200f, +0.000003302f, -0.009468531f,
        -0.019262057f, -0.028914291f, -0.037935027f, -0.045828927f,
        -0.052119261f, -0.056372283f, -0.058221106f, -0.057387924f,
        -0.053703443f, -0.047122444f, -0.037734535f, -0.025769308f,
        -0.011595336f, +0.004287292f, +0.021260954f, +0.038610717f,
        +0.055550276f, +0.071252765f, +0.084885375f, +0.095646450f,
        +0.102803611f, +0.105731303f, +0.103946126f, +0.097138329f,
        +0.085197939f, +0.068234131f, +0.046586711f, +0.020828821f,
        -0.008239664f, -0.039608255f, -0.072081234f, -0.104311776f,
        -0.134843790f, -0.162160200f, -0.184736015f, -0.201094346f,
        -0.209863285f, -0.209831516f, -0.200000470f, -0.179630919f,
        -0.148282051f, -0.105841323f, -0.052543664f, +0.011020985f,
        +0.083912428f, +0.164857408f, +0.252278939f, +0.344336996f,
        +0.438979335f, +0.534000832f, +0.627109358f, +0.715995947f,
        +0.798406824f, +0.872214756f, +0.935487176f, +0.986548646f,
        +1.024035395f, +1.046939951f, +1.054644241f, +1.046939951f,
        +1.024035395f, +0.986548646f, +0.935487176f, +0.872214756f,
        +0.798406824f, +0.715995947f, +0.627109358f, +0.534000832f,
        +0.438979335f, +0.344336996f, +0.252278939f, +0.164857408f,
        +0.083912428f, +0.011020985f, -0.052543664f, -0.105841323f,
        -0.148282051f, -0.179630919f, -0.200000470f, -0.209831516f,
        -0.209863285f, -0.201094346f, -0.184736015f, -0.162160200f,
        -0.134843790f, -0.104311776f, -0.072081234f, -0.039608255f,
        -0.008239664f, +0.020828821f, +0.046586711f, +0.068234131f,
        +0.085197939f, +0.097138329f, +0.103946126f, +0.105731303f,
        +0.102803611f, +0.095646450f, +0.084885375f, +0.071252765f,
        +0.055550276f, +0.038610717f, +0.021260954f, +0.004287292f,
        -0.011595336f, -0.025769308f, -0.037734535f, -0.047122444f,
        -0.053703443f, -0.057387924f, -0.058221106f, -0.056372283f,
        -0.052119261f, -0.045828927f, -0.037935027f, -0.028914291f,
        -0.019262057f, -0.009468531f, +0.000003302f, +0.008737200f,
        +0.016379869f, +0.022653298f, +0.027362877f, +0.030401148f,
        +0.031747267f, +0.031462429f, };

DSDFilters::DSDFilters()
{
    for (int i=0; i < NZEROS+1; i++) {
        xv[i] = 0.0f;
    }

    for (int i=0; i < NXZEROS+1; i++) {
        nxv[i] = 0.0f;
    }
}

DSDFilters::~DSDFilters()
{
}

short DSDFilters::dmr_filter(short sample)
{
    return dsd_input_filter(sample, 1);
}

short DSDFilters::nxdn_filter(short sample)
{
    return dsd_input_filter(sample, 2);
}

short DSDFilters::dsd_input_filter(short sample, int mode)
{
    float sum;
    int i;
    float gain;
    int zeros;
    float *v;
    const float *coeffs;

    switch (mode)
    {
    case 1:
        gain = ngain;
        v = xv;
        coeffs = xcoeffs;
        zeros = NZEROS;
        break;
    case 2:
        gain = nxgain;
        v = nxv;
        coeffs = nxcoeffs;
        zeros = NXZEROS;
        break;
    default:
        return sample;
    }

    for (i = 0; i < zeros; i++) {
        v[i] = v[i + 1];
    }

    v[zeros] = sample; // unfiltered sample in
    sum = 0.0f;

    for (i = 0; i <= zeros; i++) {
        sum += (coeffs[i] * v[i]);
    }

    return sum / ngain; // filtered sample out
}

} // namespace dsdplus

