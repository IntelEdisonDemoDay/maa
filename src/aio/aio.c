/*
 * Author: Nandkishor Sonar
 * Copyright (c) 2014 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include "aio.h"
#include "maa_internal.h"

struct _aio {
    unsigned int channel;
    int adc_in_fp;
};

static maa_result_t aio_get_valid_fp(maa_aio_context dev)
{
    char file_path[64]= "";

    //Open file Analog device input channel raw voltage file for reading.
    snprintf(file_path, 64, "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw",
        dev->channel );

    dev->adc_in_fp = open(file_path, O_RDONLY);
    if (dev->adc_in_fp == -1) {
	fprintf(stderr, "Failed to open Analog input raw file %s for "
	    "reading!\n", file_path); return( MAA_ERROR_INVALID_RESOURCE);
    }

    return MAA_SUCCESS;
}

/** Initialise an Analog input, connected to the specified channel
 *
 * @param aio_channel Analog input channel to read
 *
 * @returns pointer to maa_aio_context structure  after initialisation of
 * Analog input pin connected to the device successfully, else returns NULL.
 */
maa_aio_context maa_aio_init(unsigned int aio_channel)
{
    int checked_pin = maa_setup_aio(aio_channel);
    if (checked_pin < 0) {
        switch(checked_pin) {
            case -1:
                fprintf(stderr, "Invalid analog input channel %d specified\n",
                        aio_channel);
                return NULL;
            case -2:
                fprintf(stderr, "Failed to set-up analog input channel %d "
                        "multiplexer\n", aio_channel);
                return NULL;
            case -3:
                fprintf(stderr, "Platform not initialised");
                return NULL;
            default: return NULL;
        }
    }

    //Create ADC device connected to specified channel
    maa_aio_context dev = malloc(sizeof(struct _aio));
    if (dev == NULL) {
        fprintf(stderr, "Insufficient memory for specified Analog input channel "
            "%d\n", aio_channel);
        return NULL;
    }
    dev->channel = checked_pin;

    //Open valid  analog input file and get the pointer.
    if (MAA_SUCCESS != aio_get_valid_fp(dev)) {
        free(dev);
        return NULL;
    }

    return dev;
}

/** Read the input voltage, represented as an unsigned short in the range [0x0,
 * 0xFFFF]
 *
 * @param pointer to maa_aio_context structure  initialised by
 * maa_aio_init()
 *
 * @returns
 *   unsigned 16 bit int representing the current input voltage, normalised to
 *   a 16-bit value
 */
uint16_t maa_aio_read(maa_aio_context dev)
{
    char buffer[16];
    unsigned int shifter_value = 0;

    if (dev->adc_in_fp == -1) {
        aio_get_valid_fp(dev);
    }

    lseek(dev->adc_in_fp, 0, SEEK_SET);
    if (read(dev->adc_in_fp, buffer, sizeof(buffer)) < 1) {
        fprintf(stderr, "Failed to read a sensible value\n");
    }
    lseek(dev->adc_in_fp, 0, SEEK_SET);

    errno = 0;
    char *end;
    uint16_t analog_value = (uint16_t) strtoul(buffer, &end, 10);
    if (end == &buffer[0]) {
        fprintf(stderr, "%s is not a decimal number\n", buffer);
    }
    else if (errno != 0) {
        fprintf(stderr, "errno was set\n");
    }

    /* Adjust the raw analog input reading to supported resolution value*/
    if (ADC_RAW_RESOLUTION_BITS > ADC_SUPPORTED_RESOLUTION_BITS) {
        shifter_value = ADC_RAW_RESOLUTION_BITS - ADC_SUPPORTED_RESOLUTION_BITS;
        analog_value =  analog_value >> shifter_value;
    } else {
        shifter_value = ADC_SUPPORTED_RESOLUTION_BITS - ADC_RAW_RESOLUTION_BITS;
        analog_value = analog_value << shifter_value;
    }

    return analog_value;
}

/** Close the analog input and free context memory
 *
 * @param dev - the analog input context
 *
 * @return maa result type.
 */
maa_result_t maa_aio_close(maa_aio_context dev)
{
    if (NULL != dev)
        free(dev);

    return(MAA_SUCCESS);
}
