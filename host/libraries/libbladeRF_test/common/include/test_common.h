#ifndef TEST_COMMON_H_
#define TEST_COMMON_H_

#include <stdint.h>
#include "host_config.h"
#include "rel_assert.h"

/**
 * Initialize a seed for use with randval_update
 *
 * @param[out]   state      PRNG state
 * @param[in]    seed       PRNG seed value. Should not be 0.
 *
 */
static inline void randval_init(uint64_t *state, uint64_t seed) {
    if (seed == 0) {
        seed = UINT64_MAX;
    }

    *state = seed;
}

/**
 * Get the next PRNG value, using a simple xorshift implementation
 *
 * @param[inout] state      PRNG state
 *
 * @return  next PRNG value
 */
static inline uint64_t randval_update(uint64_t *state)
{
    assert(*state != 0);
    (*state) ^= (*state) >> 12;
    (*state) ^= (*state) << 25;
    (*state) ^= (*state) >> 27;
    (*state) *= 0x2545f4914f6cdd1d;
    return *state;
}

#endif
