#include "core/bool.c"
gw_int geq_d(double a, double b) {
    return to_bool(a >= b);
}