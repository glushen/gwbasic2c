#include "core/bool.c"
gw_int unequal_s(const char* a, const char* b) {
    return to_bool(strcmp(a, b) != 0);
}