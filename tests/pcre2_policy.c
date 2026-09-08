#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include <pcre2.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint32_t enabled = 99;
    if (pcre2_config(PCRE2_CONFIG_JIT, &enabled) != 0 || enabled != KUMWE_EXPECTED_PCRE2_JIT) {
        fprintf(stderr, "PCRE2 JIT violates the qualified target policy: %u\n", enabled);
        return 1;
    }
    puts("PCRE2 target JIT policy verified against the actual compiled library");
    return 0;
}
