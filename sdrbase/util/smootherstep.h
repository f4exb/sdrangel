/*
 * smootherstep.h
 *
 *  Created on: Jul 25, 2017
 *      Author: f4exb
 */

#ifndef SDRBASE_UTIL_SMOOTHERSTEP_H_
#define SDRBASE_UTIL_SMOOTHERSTEP_H_

class StepFunctions
{
public:
    static float smootherstep(float x)
    {
        if (x == 1.0f) {
            return 1.0f;
        } else if (x == 0.0f) {
            return 0.0f;
        }

        double x3 = x * x * x;
        double x4 = x * x3;
        double x5 = x * x4;

        return (float) (6.0*x5 - 15.0*x4 + 10.0*x3);
    }
};

#endif /* SDRBASE_UTIL_SMOOTHERSTEP_H_ */
