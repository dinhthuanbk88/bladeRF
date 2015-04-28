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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "device.h"
#include "conversions.h"

#define BUF_LEN_SAMPLES 1024

#ifdef ENABLE_FILE_DEV_DEBUG_MSG
#   define DBG(...) fprintf(stderr, "[DEV:FILE]  " __VA_ARGS__)
#else
#   define DBG(...)
#endif

#define ERR(...) fprintf(stderr, "[DEV:FILE]  " __VA_ARGS__)

struct ch_vars {
    int16_t buf[2 * BUF_LEN_SAMPLES];
    uint64_t timestamp;
};

struct file_dev_handle {
    FILE *file;
    struct ch_vars rx;
    struct ch_vars tx;
};

void * file_init(const struct devcfg *config)
{
    struct file_dev_handle *handle;

    handle = calloc(1, sizeof(handle[0]));
    if (handle == NULL) {
        return NULL;
    }

    handle->file = fopen(config->device_specifier, "r+b");
    if (handle->file == NULL) {
        free(handle);
        handle = NULL;
    }

    return handle;
}

void file_deinit(struct device *dev)
{
    struct file_dev_handle *f;

    if (dev != NULL) {
        f = (struct file_dev_handle *) dev->handle;

        if (f != NULL) {
            if (f->file != NULL) {
                fclose(f->file);
            }

            free(f);
        }
    }
}

int file_rx(struct device *d, uint64_t *timestamp,
            struct complexf *samples, unsigned int count)
{
    struct file_dev_handle *f = (struct file_dev_handle *) d->handle;
    size_t n, total_read, to_read;

    total_read = 0;

    DBG("Read of %zd samples requested.\n", count);

    while (total_read < count) {
        const size_t diff = count - total_read;
        to_read = diff > BUF_LEN_SAMPLES ? BUF_LEN_SAMPLES : diff;

        n = fread(f->rx.buf, 2 * sizeof(int16_t), to_read, f->file);

        DBG("Read %zd/%zd samples...\n", to_read, n);

        if (feof(f->file)) {
            DBG("EOF hit. Read %zd/%zd samples\n", n, to_read);
            return 1;
        } else if (ferror(f->file)) {
            fprintf(stderr, "File I/O error.\n");
            return -1;
        } else if (n != to_read) {
            fprintf(stderr, "Unexpected truncated read.\n");
            return -1;
        }

        sc16q11_to_complexf(f->rx.buf, samples, n);

        samples += n;
        total_read += n;
    }

    *timestamp = f->rx.timestamp;
    f->rx.timestamp += total_read;

    return 0;
}

int file_tx(struct device *d, uint64_t timestamp,
            struct complexf *samples, unsigned int count)
{
    struct file_dev_handle *f = (struct file_dev_handle *) d->handle;
    size_t n, total_written, to_write;

    total_written = 0;

    /* TODO: Fill zeros for timestamp discontinuity */

    while (total_written < count) {
        const size_t diff = count - total_written;
        to_write = diff > BUF_LEN_SAMPLES ? BUF_LEN_SAMPLES : diff;

        complexf_to_sc16q11(samples, f->tx.buf, to_write);

        n = fwrite(f->tx.buf, 2 * sizeof(int16_t), to_write, f->file);
        if (n != to_write) {
            return -1;
        }

        samples += n;
        total_written += n;

    }

    return 0;
}
