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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "device.h"
#include "conversions.h"

#define DECLARE_DEVICE_TYPE(type) \
    void * type##_init(const struct devcfg *config); \
    void type##_deinit(struct device *dev); \
    int type##_rx(struct device *d, uint64_t *timestamp, \
                  struct complexf *samples, unsigned int count); \
    int type##_tx(struct device *d, uint64_t timestamp, \
                  struct complexf *samples, unsigned int count); \

#define SETUP_FN_PTRS(device, type) \
do { \
    device->deinit = type##_deinit; \
    device->rx = type##_rx; \
    device->tx = type##_tx; \
} while (0)


DECLARE_DEVICE_TYPE(file)
DECLARE_DEVICE_TYPE(bladerf_dev)

struct device * device_init(const struct devcfg *config,
                            enum device_type type)
{
    struct device *ret = NULL;

    ret = calloc(1, sizeof(ret[0]));
    if (ret == NULL) {
        perror("calloc");
        return NULL;
    }

    switch (type) {
        case DEVICE_TYPE_SC16Q11_FILE:
            ret->handle = file_init(config);
            SETUP_FN_PTRS(ret, file);
            break;

        case DEVICE_TYPE_BLADERF:
            ret->handle = bladerf_dev_init(config);
            SETUP_FN_PTRS(ret, bladerf_dev);
            break;

        default:
            fprintf(stderr, "Invalid device type; %d\n", type);
            free(ret);
            ret = NULL;
    }

    if (ret != NULL && ret->handle != NULL) {
        memcpy(&ret->config, config, sizeof(ret->config));

        if (config->device_specifier) {
            ret->config.device_specifier = strdup(config->device_specifier);
            if (ret->config.device_specifier == NULL) {
                free(ret);
                ret = NULL;
            }
        }
    } else {
        device_deinit(ret);
        ret = NULL;
    }

    return ret;
}

void device_deinit(struct device *dev)
{
    if (dev != NULL) {
        free((void*)dev->config.device_specifier);
        dev->config.device_specifier = NULL;
        dev->deinit(dev);
        free(dev);
    }
}
