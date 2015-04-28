/*
 * Copyright (c) 2015 Nuand LLC
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef DEVICE_H_
#define DEVICE_H_

#include <stdint.h>
#include "devcfg.h"
#include "common.h"

enum device_type {
    DEVICE_TYPE_SC16Q11_FILE = 0,
    DEVICE_TYPE_BLADERF      = 1,
};

struct device {
    struct devcfg config;
    void *handle;

    /* Receive samples converted to floats */
    int (*rx)(struct device *d, uint64_t *timestamp,
              struct complexf *samples, unsigned int count);

    /* Transmit samples formatted as floats */
    int (*tx)(struct device *d, uint64_t timestamp,
              struct complexf *samples, unsigned int count);

    /* TODO rx_raw and tx_raw for ADC/DAC sample format */

    void (*deinit)(struct device *d);
};

struct device * device_init(const struct devcfg *config,
                            enum device_type type);


static inline int device_rx(struct device *dev, uint64_t *timestamp,
                            struct complexf *samples, unsigned int count)
{
    return dev->rx(dev, timestamp, samples, count);
}

static inline int device_tx(struct device *dev, uint64_t timestamp,
                            struct complexf *samples, unsigned int count)
{
    return dev->tx(dev, timestamp, samples, count);
}

void device_deinit(struct device *dev);

#endif
