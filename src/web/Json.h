#pragma once

#include <stddef.h>

// Dopisuje tekst w cudzyslowach JSON (z ucieczka znakow) od pozycji n, zwraca nowa dlugosc.
inline size_t jsonString(char* out, size_t len, size_t n, const char* s) {
    if (n + 2 >= len) return n;
    out[n++] = '"';
    for (; *s && n + 3 < len; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            out[n++] = '\\';
            out[n++] = c;
        } else if (c >= 0x20) {
            out[n++] = c;
        }
    }
    out[n++] = '"';
    out[n] = 0;
    return n;
}
