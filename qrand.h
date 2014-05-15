#ifndef _QRAND_H
#define _QRAND_H

/* Random number generator 
 * 
 * From Numerical Recipes, note that if you are making more
 * than about 1e12 calls, this has too short a period */

#include <math.h>

static unsigned long long QRAND_V = 4101842887655102017LL;

void sqrand(unsigned long long seed) {
    QRAND_V ^= seed;
}

// long
unsigned long long lqrand() {
    QRAND_V ^= QRAND_V >> 21;
    QRAND_V ^= QRAND_V << 35;
    QRAND_V ^= QRAND_V >> 4;
    return QRAND_V*2685821657736338717LL;
}

/* float x1, x2, w, y1, y2; */

/* do { */
/*         x1 = 2.0 * ranf() - 1.0; */
/*         x2 = 2.0 * ranf() - 1.0; */
/*         w = x1 * x1 + x2 * x2; */
/* } while ( w >= 1.0 ); */

/* w = sqrt( (-2.0 * log( w ) ) / w ); */
/* y1 = x1 * w; */
/* y2 = x2 * w; */

// double
double dqrand() {
    return 5.42101086242752217E-20 * lqrand();
}

// normally distributed number
static double _QRAND_SECOND = 0;
double qrand_normal() {
    double x1,x2,w,y1,out;
    if (_QRAND_SECOND == 0.0) {
        do {
            x1 = 2.0 * dqrand() - 1.0; 
            x2 = 2.0 * dqrand() - 1.0; 
            w = x1 * x1 + x2 * x2;
        } while ( w >= 1.0 );
        w = sqrt( (-2.0 * log( w ) )/ w);
        y1 = x1 * w;
        _QRAND_SECOND = x2 * w;
        out = y1;
    } else {
        out = _QRAND_SECOND;
        _QRAND_SECOND = 0.0;
    }
    return out;
}

// Draw an exponential random number
double inline qrand_exponential(double const lambda) {
    double u = dqrand();
    return -log(u)/lambda;
}


// return an integer
unsigned int qrand() {
    return (unsigned int)lqrand();
}

unsigned int inthash(unsigned int x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x);
    return x;
}

#endif
