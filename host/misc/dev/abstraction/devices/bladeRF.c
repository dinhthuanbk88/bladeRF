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
#include <inttypes.h>
#include <assert.h>
#include <libbladeRF.h>
#include "common.h"
#include "device.h"
#include "devcfg.h"
#include "conversions.h"
#include "minmax.h"

#ifdef ENABLE_BLADERF_DEV_DEBUG_MSG
#   define DBG(...) fprintf(stderr, "[DEV:BLADERF] " __VA_ARGS__)
#else
#   define DBG(...)
#endif

struct ch_params {
    bool enabled;

    int16_t *buf;
    size_t buf_len;
};

struct bladerf_dev {
    struct bladerf *dev;
    struct devcfg config;

    unsigned int timeout;

    struct ch_params rx;
    struct ch_params tx;
};

static void deinit(struct bladerf_dev *handle)
{
    if (handle != NULL) {
        if (handle->dev != NULL) {
            bladerf_enable_module(handle->dev, BLADERF_MODULE_RX, false);
            bladerf_enable_module(handle->dev, BLADERF_MODULE_TX, false);
            bladerf_close(handle->dev);
        }

        free(handle->rx.buf);
        free(handle->tx.buf);
        free(handle);
    }
}

void bladerf_dev_deinit(struct device *dev)
{
    if (dev != NULL) {
        struct bladerf_dev *handle = (struct bladerf_dev *) dev->handle;
        deinit(handle);
    }
}

void * bladerf_dev_init(const struct devcfg *config)
{
    int status = -1;
    struct bladerf_dev *handle;

    handle = calloc(1, sizeof(handle[0]));
    if (handle == NULL) {
        perror("calloc");
        goto out;
    }

    memcpy(&handle->config, config, sizeof(handle->config));
    handle->config.device_specifier = NULL;

    handle->timeout = config->sync_timeout_ms;
    handle->rx.buf_len = config->samples_per_buffer;
    handle->tx.buf_len = config->samples_per_buffer;

    handle->rx.enabled = false;
    handle->tx.enabled = false;

    handle->rx.buf = malloc(2 * sizeof(int16_t) * handle->rx.buf_len);
    if (handle->rx.buf == NULL) {
        perror("malloc");
        goto out;
    }

    handle->tx.buf = malloc(2 * sizeof(int16_t) * handle->tx.buf_len);
    if (handle->tx.buf == NULL) {
        perror("malloc");
        goto out;
    }

    status = bladerf_open(&handle->dev, config->device_specifier);
    if (status != 0) {
        fprintf(stderr, "Unable to open device: %s\n",
                bladerf_strerror(status));
    }

    status = devcfg_apply(handle->dev, config);

out:
    if (status != 0) {
        deinit(handle);
        handle = NULL;
    }

    return handle;
}

static int init_module(struct bladerf_dev *handle, bladerf_module module)
{
    int status;

    status = bladerf_sync_config(handle->dev, module,
                                 BLADERF_FORMAT_SC16_Q11,
                                 handle->config.num_buffers,
                                 handle->config.samples_per_buffer,
                                 handle->config.num_transfers,
                                 handle->config.stream_timeout_ms);

    if (status != 0) {
        return status;
    }

    status = bladerf_enable_module(handle->dev, module, true);

    /* TODO: Get current TX timestamp and advance it a bit into the future? */

    if (status == 0) {
        switch (module) {
            case BLADERF_MODULE_RX:
                handle->rx.enabled = true;
                break;

            case BLADERF_MODULE_TX:
                handle->tx.enabled = true;
                break;

            default:
                assert(!"Invalid module\n");
                status = -1;
        }
    }

    return status;
}

int bladerf_dev_rx(struct device *d, uint64_t *timestamp,
                   struct complexf *samples, unsigned int count)
{
    int status = 0;
    unsigned int to_read, total_read;
    struct bladerf_metadata meta;
    struct bladerf_dev *handle = (struct bladerf_dev *) d->handle;

    *timestamp = 0;

    meta.timestamp = 0;
    meta.flags = BLADERF_META_FLAG_RX_NOW;
    meta.status = 0;
    meta.actual_count = 0;


    if (!handle->rx.enabled) {
        status = init_module(handle, BLADERF_MODULE_RX);
        if (status != 0) {
            fprintf(stderr, "Failed to init RX module: %s\n",
                    bladerf_strerror(status));

            return status;
        }

        DBG("Initialized RX...\n");
    }

    total_read = 0;

    while (status == 0 && total_read < count) {
        to_read = uint_min(handle->rx.buf_len, count - total_read);

        DBG("Reading %u samples...\n", to_read);

        status = bladerf_sync_rx(handle->dev,
                                 handle->rx.buf, to_read,
                                 &meta, handle->timeout);

        if (status != 0) {
            fprintf(stderr, "RX failure: %s\n", bladerf_strerror(status));
            continue;
        }

        if (meta.status != 0) {
            if (meta.status & BLADERF_META_STATUS_OVERRUN) {
                fprintf(stderr, "RX overrun detected @ ts=%"PRIu64"\n",
                        meta.timestamp);
            } else {
                fprintf(stderr, "Unknown metadata status: 0x%08x\n",
                        meta.status);
            }
        }

        DBG("Actually read %u samples...\n", meta.actual_count);

        if (*timestamp == 0) {
            *timestamp = meta.timestamp;
        }

        sc16q11_to_complexf(handle->rx.buf, samples, meta.actual_count);

        samples += meta.actual_count;
        total_read += meta.actual_count;
    }

    return status;
}

int bladerf_dev_tx(struct device *d, uint64_t timestamp,
                   struct complexf *samples, unsigned int count)
{
    int status = -1;
    struct bladerf_dev *handle = (struct bladerf_dev *) d->handle;

    /* Supress "never read" warning from Clang's scan-build */
    (void) handle;

    assert(!"bladeRF TX via dev interface not implemented yet.\n");
    return status;
}
