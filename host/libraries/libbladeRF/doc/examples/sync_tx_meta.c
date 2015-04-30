/*
 * Example of TX synchronous interface usage with metadata
 *
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2014 Nuand LLC
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
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <libbladeRF.h>
#include "example_common.h"

/* Just a dummy routine to produce a tone at the carrier frequency */
static void produce_samples(int16_t *samples, unsigned int num_samples)
{
    static bool data_populated = false;
    unsigned int i;

    if (data_populated) {
        return;
    }

    for (i = 0; i < 2 * num_samples; i += 2) {
        samples[i]      = 2000;
        samples[i + 1]  = 2000;
    }
}

/** [wait_for_timestamp] */
int wait_for_timestamp(struct bladerf *dev, bladerf_module module,
                       uint64_t timestamp, unsigned int timeout_ms)
{
    int status;
    uint64_t curr_ts = 0;
    unsigned int slept_ms = 0;
    bool done;

    do {
        status = bladerf_get_timestamp(dev, module, &curr_ts);
        done = (status != 0) || curr_ts >= timestamp;

        if (!done) {
            if (slept_ms > timeout_ms) {
                done = true;
                status = BLADERF_ERR_TIMEOUT;
            } else {
                usleep(10000);
                slept_ms += 10;
            }
        }
    } while (!done);

    return status;
}
/** [wait_for_timestamp] */

/** [tx_meta_init] */
int16_t * init(struct bladerf *dev, int16_t num_samples)
{
    int status = -1;

    /* "User" buffer that we store our modulated samples in, and its
     * associated size, in units of samples. Recall that for the
     * SC16Q11 format (native to the ADCs), one sample = two int16_t values.
     *
     * When using the bladerf_sync_* functions, the buffer size isn't
     * restricted to multiples of any particular size.
     *
     * The value for `num_samples` has no major restrictions here, while the
     * `buffer_size` below must be a multiple of 1024.
     */
    int16_t *samples;

    /* These items configure the underlying asynch stream used by the the sync
     * interface. The "buffer" here refers to those used internally by worker
     * threads, not the `samples` buffer above. */
    const unsigned int num_buffers = 32;
    const unsigned int buffer_size = 2048;
    const unsigned int num_transfers = 16;
    const unsigned int timeout_ms  = 1000;

    samples = malloc(num_samples * 2 * sizeof(int16_t));
    if (samples == NULL) {
        perror("malloc");
        goto error;
    }

    /* Configure the device's TX module for use with the sync interface.
     * SC16 Q11 samples *with* metadata are used. */
    status = bladerf_sync_config(dev,
                                 BLADERF_MODULE_TX,
                                 BLADERF_FORMAT_SC16_Q11_META,
                                 num_buffers,
                                 buffer_size,
                                 num_transfers,
                                 timeout_ms);

    if (status != 0) {
        fprintf(stderr, "Failed to configure TX sync interface: %s\n",
                bladerf_strerror(status));

        goto error;
    }

    /* We must always enable the TX module *after* calling
     * bladerf_sync_config(), and *before* attempting to TX samples via
     * bladerf_sync_tx(). */
    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, true);
    if (status != 0) {
        fprintf(stderr, "Failed to enable RX module: %s\n",
                bladerf_strerror(status));

        goto error;
    }

    status = 0;

error:
    if (status != 0) {
        free(samples);
        samples = NULL;
    }

    return samples;
}

/** [tx_meta_deinit] */
void deinit(struct bladerf *dev, int16_t *samples)
{
    printf("\nDeinitalizing device.\n");

    /* Disable TX module, shutting down our underlying TX stream */
    int status = bladerf_enable_module(dev, BLADERF_MODULE_TX, false);
    if (status != 0) {
        fprintf(stderr, "Failed to disable TX module: %s\n",
                bladerf_strerror(status));
    }

    /* Deinitialize and free resources */
    free(samples);
    bladerf_close(dev);
}
/** [tx_meta_deinit] */

/** [tx_meta_now_example] */
/*
 * TODO move this to the doc page
 *
 * Here we'll send an entire burst of samples in one function call.
 * Note that it is possible to spread this transmission out across multiple
 * calls.
 *
 * For example, one could call bladerf_sync_tx multiple times, with the
 * flags set as follows:
 *
 * Call #1:
 *      Start the burst, noting that the timestamp field should be ignored
 *      and that the samples should be sent immediately.
 *
 *      flags: BLADERF_META_FLAG_TX_BURST_START | BLADERF_META_FLAG_TX_NOW
 *
 * Call #2 - #n-1:
 *      Continue to send data, with flags = 0
 *
 * Call #n:
 *      On the last call, set the BLADERF_META_FLAG_TX_BURST_END flag bit.
 *      It's often convenient to just send 2 or more 0+0j samples with
 *      this flag set, rather than track which data call is the final one.
 */
int sync_tx_meta_now_example(struct bladerf *dev, int16_t *samples,
                             unsigned int num_samples, unsigned int tx_count,
                             unsigned int timeout_ms)
{
    int status = 0;
    struct bladerf_metadata meta;
    unsigned int i;

    memset(&meta, 0, sizeof(meta));

    /* Send entire burst worth of samples in one function call */
    meta.flags = BLADERF_META_FLAG_TX_BURST_START |
                 BLADERF_META_FLAG_TX_NOW |
                 BLADERF_META_FLAG_TX_BURST_END;

    for (i = 0; i < tx_count && status == 0; i++) {
        /* Fetch or produce IQ samples...*/
        produce_samples(samples, num_samples);

        status = bladerf_sync_tx(dev, samples, num_samples, &meta, timeout_ms);
        if (status != 0) {
            fprintf(stderr, "TX failed: %s\n", bladerf_strerror(status));
        } else {
            uint64_t curr_ts;

            status = bladerf_get_timestamp(dev, BLADERF_MODULE_TX, &curr_ts);
            if (status != 0) {
                fprintf(stderr, "Failed to get current TX timestamp: %s\n",
                        bladerf_strerror(status));
            } else {
                printf("TX'd at approximately t=%016"PRIu64"\n", curr_ts);
            }

            /* Delay next transmission by approximately 5 ms
             *
             * This is a very imprecise, "quick and dirty" means to do so in
             * cases where no particular intra-burst time is required. */
            usleep(5000);
        }
    }

    /* Wait for samples to be TX'd before completing.  */
    if (status == 0) {
        status = bladerf_get_timestamp(dev, BLADERF_MODULE_TX, &meta.timestamp);
        if (status != 0) {
            fprintf(stderr, "Failed to get current TX timestamp: %s\n",
                    bladerf_strerror(status));
            return status;
        } else {
            status = wait_for_timestamp(dev, BLADERF_MODULE_TX,
                                        meta.timestamp + 2 * num_samples,
                                        timeout_ms);
            if (status != 0) {
                fprintf(stderr, "Failed to wait for timestamp.\n");
            }
        }
    }

    return status;
}
/** [tx_meta_now_example] */

int sync_tx_meta_sched_example(struct bladerf *dev,
                             int16_t *samples, unsigned int num_samples,
                             unsigned int tx_count, unsigned int samplerate,
                             unsigned int timeout_ms)
{
    int status = 0;
    unsigned int i;
    struct bladerf_metadata meta;

    memset(&meta, 0, sizeof(meta));

    /* Send entire burst worth of samples in one function call */
    meta.flags = BLADERF_META_FLAG_TX_BURST_START |
                 BLADERF_META_FLAG_TX_BURST_END;

    /* Retrieve the current timestamp so we can schedule our transmission
     * in the future. */
    status = bladerf_get_timestamp(dev, BLADERF_MODULE_TX, &meta.timestamp);
    if (status != 0) {
        fprintf(stderr, "Failed to get current TX timestamp: %s\n",
                bladerf_strerror(status));
        return status;
    } else {
        printf("\nCurrent TX timestamp: 0x%016"PRIx64"\n", meta.timestamp);
    }

    for (i = 0; i < tx_count && status == 0; i++) {
        /* Get sample to transmit... */
        produce_samples(samples, num_samples);

        /* Schedule burst 5 ms into the future */
        meta.timestamp += samplerate / 200;

        status = bladerf_sync_tx(dev, samples, num_samples, &meta, timeout_ms);
        if (status != 0) {
            fprintf(stderr, "TX failed: %s\n", bladerf_strerror(status));
            return status;
        } else {
            printf("TX'd @ t=%016"PRIu64"\n", meta.timestamp);
        }
    }

    /* Wait for samples to finish being transmitted. */
    if (status == 0) {
        meta.timestamp += 2 * (samplerate / 200);

        status = wait_for_timestamp(dev, BLADERF_MODULE_TX,
                                    meta.timestamp, timeout_ms);

        if (status != 0) {
            fprintf(stderr, "Failed to wait for timestamp.\n");
        }
    }

    return status;
}
/** [example_snippet] */

static void usage(const char *argv0) {
    printf("Usage: %s [device specifier]\n\n", argv0);
}


int main(int argc, char *argv[])
{
    int status = -1;
    struct bladerf *dev = NULL;
    const char *devstr = NULL;
    int16_t *samples = NULL;

    const unsigned int num_samples = 4096;
    const unsigned int tx_count = 15;
    const unsigned int timeout_ms = 2500;

    if (argc == 2) {
        if (!strcasecmp("-h", argv[1]) || !strcasecmp("--help", argv[1])) {
            usage(argv[0]);
            return 0;
        } else {
            devstr = argv[1];
        }
    } else if (argc > 1) {
        usage(argv[0]);
        return -1;
    }

    dev = example_init(devstr);
    if (dev) {
        samples = init(dev, num_samples);
        if (samples != NULL) {
            printf("Running TX meta \"now\" example...\n");
            status = sync_tx_meta_now_example(dev,
                                              samples, num_samples,
                                              tx_count,
                                              timeout_ms);


            if (status == 0) {
                printf("Running TX meta scheduled example...\n");
                status = sync_tx_meta_sched_example(dev,
                                                    samples, num_samples,
                                                    tx_count,
                                                    EXAMPLE_SAMPLERATE,
                                                    timeout_ms);
            }
        }

        deinit(dev, samples);
    }

    return status;
}
