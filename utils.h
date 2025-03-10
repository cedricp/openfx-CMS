#pragma once

#include <math.h>

class logEncode
{
    // Forward
    double m, b, klog, kb;
    // Backward
    double kinv, mkb, mb, linsinv, mlino, minv;
    // Common
    double scale, offset, logbreak;
public:    
    logEncode(double min, double max, double logslope = 1, double logoffset = 0, double linslope = 1, double linoffset = 0, double logbase = 2)
    {
        // Forward log encoding
        m = linslope;
        b = linoffset;
        klog = logslope / log2(logbase);
        kb = logoffset;
        logbreak = 0;
        logbreak = pow(2.0, min);
        
        // Backward log encoding
        kinv = log2(logbase) / logslope;
        mkb = -logoffset;
        mb = -linoffset;
        minv = 1.0 / logslope;
        linsinv =  1.0 / linslope;
        mlino = -linoffset;
        
        fit(min, max, 0, 1);
    }

    void fit(double oldmin, double oldmax, double newmin, double newmax)
    {
        double denom = oldmax - oldmin;
        if (denom == 0){
            return;
        }
        scale = (newmax - newmin) / denom;
        offset = (newmin * oldmax - newmax * oldmin) / denom;
    }

    double apply(double in)
    {
        double out = in * m + b;
        if (in >= logbreak){
            out = std::max(DBL_EPSILON, out);
            out = log2(out);
            out = out * klog + kb;
            out *= scale;
            out += offset;
        }
        return out;
    }

    double apply_backward(double in)
    {
        in -= offset;
        in /= scale;
        return pow(2.0, (in + mkb) * kinv);
    }
};