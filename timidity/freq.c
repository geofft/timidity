/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define NUM_WIDE_PEAKS_TO_KEEP 99999	/* keep only the first n humps */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "timidity.h"
#include "instrum.h"
#include "freq.h"
#include "fft4g.h"

static float *floatdata, *magdata, *logmagdata, *cepstrum;
static int *ip;
static float *w;
static uint32 oldfftsize = 0;
static float pitchmags[129];
static double pitchbins[129];
static double pitchbins_cepstrum[129];
static int *fft1_bin_to_pitch;



/* middle C = pitch 60 = 261.6 Hz
   freq     = 13.75 * exp((pitch - 9) / 12 * log(2))
   pitch    = 9 - log(13.75 / freq) * 12 / log(2)
            = -36.37631656 + 17.31234049 * log(freq)
*/
float pitch_freq_table[129] = {
    8.17579892, 8.66195722, 9.17702400, 9.72271824, 10.3008612, 10.9133822,
    11.5623257, 12.2498574, 12.9782718, 13.7500000, 14.5676175, 15.4338532,

    16.3515978, 17.3239144, 18.3540480, 19.4454365, 20.6017223, 21.8267645,
    23.1246514, 24.4997147, 25.9565436, 27.5000000, 29.1352351, 30.8677063,

    32.7031957, 34.6478289, 36.7080960, 38.8908730, 41.2034446, 43.6535289,
    46.2493028, 48.9994295, 51.9130872, 55.0000000, 58.2704702, 61.7354127,

    65.4063913, 69.2956577, 73.4161920, 77.7817459, 82.4068892, 87.3070579,
    92.4986057, 97.9988590, 103.826174, 110.000000, 116.540940, 123.470825,

    130.812783, 138.591315, 146.832384, 155.563492, 164.813778, 174.614116,
    184.997211, 195.997718, 207.652349, 220.000000, 233.081881, 246.941651,

    261.625565, 277.182631, 293.664768, 311.126984, 329.627557, 349.228231,
    369.994423, 391.995436, 415.304698, 440.000000, 466.163762, 493.883301,

    523.251131, 554.365262, 587.329536, 622.253967, 659.255114, 698.456463,
    739.988845, 783.990872, 830.609395, 880.000000, 932.327523, 987.766603,

    1046.50226, 1108.73052, 1174.65907, 1244.50793, 1318.51023, 1396.91293,
    1479.97769, 1567.98174, 1661.21879, 1760.00000, 1864.65505, 1975.53321,

    2093.00452, 2217.46105, 2349.31814, 2489.01587, 2637.02046, 2793.82585,
    2959.95538, 3135.96349, 3322.43758, 3520.00000, 3729.31009, 3951.06641,

    4186.00904, 4434.92210, 4698.63629, 4978.03174, 5274.04091, 5587.65170,
    5919.91076, 6271.92698, 6644.87516, 7040.00000, 7458.62018, 7902.13282,

    8372.01809, 8869.84419, 9397.27257, 9956.06348, 10548.0818, 11175.3034,
    11839.8215, 12543.8540, 13289.7503
};



float pitch_freq_ub_table[129] = {
    8.41536811, 8.91577194, 9.44593133, 10.0076156, 10.6026994, 11.2331687,
    11.9011277, 12.6088056, 13.3585642, 14.1529058, 14.9944813, 15.8860996,

    16.8307362, 17.8315439, 18.8918627, 20.0152313, 21.2053988, 22.4663375,
    23.8022554, 25.2176112, 26.7171284, 28.3058115, 29.9889626, 31.7721992,

    33.6614724, 35.6630878, 37.7837253, 40.0304625, 42.4107977, 44.9326750,
    47.6045109, 50.4352224, 53.4342568, 56.6116230, 59.9779253, 63.5443983,

    67.3229449, 71.3261755, 75.5674506, 80.0609251, 84.8215954, 89.8653499,
    95.2090217, 100.870445, 106.868514, 113.223246, 119.955851, 127.088797,

    134.645890, 142.652351, 151.134901, 160.121850, 169.643191, 179.730700,
    190.418043, 201.740890, 213.737027, 226.446492, 239.911701, 254.177593,

    269.291780, 285.304702, 302.269802, 320.243700, 339.286382, 359.461400,
    380.836087, 403.481779, 427.474054, 452.892984, 479.823402, 508.355187,

    538.583559, 570.609404, 604.539605, 640.487400, 678.572763, 718.922799,
    761.672174, 806.963558, 854.948108, 905.785968, 959.646805, 1016.71037,

    1077.16712, 1141.21881, 1209.07921, 1280.97480, 1357.14553, 1437.84560,
    1523.34435, 1613.92712, 1709.89622, 1811.57194, 1919.29361, 2033.42075,

    2154.33424, 2282.43762, 2418.15842, 2561.94960, 2714.29105, 2875.69120,
    3046.68869, 3227.85423, 3419.79243, 3623.14387, 3838.58722, 4066.84149,

    4308.66847, 4564.87523, 4836.31684, 5123.89920, 5428.58211, 5751.38240,
    6093.37739, 6455.70846, 6839.58487, 7246.28775, 7677.17444, 8133.68299,

    8617.33694, 9129.75046, 9672.63368, 10247.7984, 10857.1642, 11502.7648,
    12186.7548, 12911.4169, 13679.1697
};



float pitch_freq_lb_table[129] = {
    7.94304979, 8.41536811, 8.91577194, 9.44593133, 10.0076156, 10.6026994,
    11.2331687, 11.9011277, 12.6088056, 13.3585642, 14.1529058, 14.9944813,

    15.8860996, 16.8307362, 17.8315439, 18.8918627, 20.0152313, 21.2053988,
    22.4663375, 23.8022554, 25.2176112, 26.7171284, 28.3058115, 29.9889626,

    31.7721992, 33.6614724, 35.6630878, 37.7837253, 40.0304625, 42.4107977,
    44.9326750, 47.6045109, 50.4352224, 53.4342568, 56.6116230, 59.9779253,

    63.5443983, 67.3229449, 71.3261755, 75.5674506, 80.0609251, 84.8215954,
    89.8653499, 95.2090217, 100.870445, 106.868514, 113.223246, 119.955851,

    127.088797, 134.645890, 142.652351, 151.134901, 160.121850, 169.643191,
    179.730700, 190.418043, 201.740890, 213.737027, 226.446492, 239.911701,

    254.177593, 269.291780, 285.304702, 302.269802, 320.243700, 339.286382,
    359.461400, 380.836087, 403.481779, 427.474054, 452.892984, 479.823402,

    508.355187, 538.583559, 570.609404, 604.539605, 640.487400, 678.572763,
    718.922799, 761.672174, 806.963558, 854.948108, 905.785968, 959.646805,

    1016.71037, 1077.16712, 1141.21881, 1209.07921, 1280.97480, 1357.14553,
    1437.84560, 1523.34435, 1613.92712, 1709.89622, 1811.57194, 1919.29361,

    2033.42075, 2154.33424, 2282.43762, 2418.15842, 2561.94960, 2714.29105,
    2875.69120, 3046.68869, 3227.85423, 3419.79243, 3623.14387, 3838.58722,

    4066.84149, 4308.66847, 4564.87523, 4836.31684, 5123.89920, 5428.58211,
    5751.38240, 6093.37739, 6455.70846, 6839.58487, 7246.28775, 7677.17444,

    8133.68299, 8617.33694, 9129.75046, 9672.63368, 10247.7984, 10857.1642,
    11502.7648, 12186.7548, 12911.4169
};



/* (M)ajor,		rotate back 1,	rotate back 2
   (m)inor,		rotate back 1,	rotate back 2
   (d)iminished minor,	rotate back 1,	rotate back 2
   (f)ifth,		rotate back 1,	rotate back 2
*/
int chord_table[4][3][3] = {
    0, 4, 7,     -5, 0, 4,     -8, -5, 0,
    0, 3, 7,     -5, 0, 3,     -9, -5, 0,
    0, 3, 6,     -6, 0, 3,     -9, -6, 0,
    0, 5, 7,     -5, 0, 5,     -7, -5, 0
};



/* write the chord type to *chord, returns the root note of the chord */
int assign_chord(double *pitchbins, int *chord)
{

    int type, subtype;
    int pitches[3];
    int i, j, n;
    double val;
    int start = 0;

    *chord = -1;

    /* count local maxima, take first 3 */
    for (i = LOWEST_PITCH, n = 0; n < 3 && i <= HIGHEST_PITCH; i++)
    {
	val = pitchbins[i];
	if (val)
	{
	    /* reached the end of a wide peak */
	    if (i == HIGHEST_PITCH || !pitchbins[i + 1])
	    {
		for (j = start; j <= i; j++)
		{
		    /* throw out all but local maxima */
		    val = pitchbins[j];
		    if (j && pitchbins[j - 1] < val &&
			j < HIGHEST_PITCH && pitchbins[j + 1] < val)
		    {
			pitches[n++] = j;
			if (n == 3)
			    break;
		    }
		}
	    }
	}
	else
	    start = i + 1;
    }

    for (subtype = 0; subtype < 3; subtype++)
    {
	for (type = 0; type < 4; type++)
	{
	    for (i = 0, n = 0; i < 3; i++)
	    {
		if (i == subtype)
		    continue;

		if (pitches[i] - pitches[subtype] ==
		    chord_table[type][subtype][i])
			n++;
	    }
	    if (n == 2)
	    {
		*chord = 3 * type + subtype;
		return pitches[subtype];
	    }
	}
    }

    return -1;
}



/* initialize FFT arrays for the frequency analysis */
int freq_initialize_fft_arrays(Sample *sp)
{

    uint32 i, padding;
    uint32 length, newlength;
    unsigned int rate;
    sample_t *origdata;

    rate = sp->sample_rate;
    length = sp->data_length >> FRACTION_BITS;
    origdata = sp->data;

    /* copy the sample to a new float array */
    floatdata = (float *) malloc(length * sizeof(float));
    for (i = 0; i < length; i++)
	floatdata[i] = origdata[i];

    /* length must be a power of 2 */
    /* set it to smallest power of 2 >= rate */
    /* if you make it bigger than this, freq magnitudes get too difused for
       good cepstrum weighting */
    newlength = pow(2, ceil(log(length) / log(2)));
    if (newlength > length)
    {
	floatdata = realloc(floatdata, newlength * sizeof(float));
	memset(floatdata + length, 0, (newlength - length) * sizeof(float));
    }
    length = newlength;
    if (length < rate)
    {
	padding = pow(2, ceil(log(rate) / log(2))) - length;
	floatdata = realloc(floatdata, (length + padding) * sizeof(float));
	memset(floatdata + length, 0, padding * sizeof(float));
	length += padding;
    }
    else if (length > pow(2, ceil(log(rate) / log(2))))
	length = pow(2, ceil(log(rate) / log(2)));

    /* allocate FFT arrays */
    /* calculate sin/cos and fft1_bin_to_pitch tables */
    if (length != oldfftsize)
    {
        float f0;
    
        if (oldfftsize > 0)
        {
            free(magdata);
            free(logmagdata);
            free(cepstrum);
            free(ip);
            free(w);
            free(fft1_bin_to_pitch);
        }
        magdata = (float *) malloc(length * sizeof(float));
        logmagdata = (float *) malloc(length * sizeof(float));
        cepstrum = (float *) malloc(length * sizeof(float));
        ip = (int *) malloc(2 + sqrt(length) * sizeof(int));
        *ip = 0;
        w = (float *) malloc((length >> 1) * sizeof(float));
        fft1_bin_to_pitch = malloc((length >> 1) * sizeof(float));

        for (i = 1, f0 = (float) rate / length; i < (length >> 1); i++) {
            fft1_bin_to_pitch[i] = assign_pitch_to_freq(i * f0);
        }
    }
    oldfftsize = length;

    /* zero out arrays that need it */
    memset(pitchmags, 0, 129 * sizeof(float));
    memset(pitchbins, 0, 129 * sizeof(double));
    memset(pitchbins_cepstrum, 0, 129 * sizeof(double));
    memset(logmagdata, 0, length * sizeof(float));

    return(length);
}



/* return the frequency of the sample */
/* max of 1.0 - 2.0 seconds of audio is analyzed, depending on sample rate */
/* samples < 1 second are padded to the max length for higher fft accuracy */
float freq_fourier(Sample *sp, int *chord)
{

    uint32 length, length0;
    int32 maxoffset, minoffset, minoffset1, minoffset2;
    int32 minbin, maxbin;
    int32 bin, bestbin, largest_peak;
    int32 i, n;
    unsigned int rate;
    int dist, bestdist;
    int pitch, minpitch, maxpitch;
    sample_t *origdata;
    float f0, mag, maxmag;
    sample_t amp, oldamp, maxamp;
    int32 maxpos;
    double sum, weightsum, maxsum;
    double maxcepstrum;
    float refinedbin;
    float freq;
    float minfreq, maxfreq, newminfreq, newmaxfreq;


    rate = sp->sample_rate;
    length = length0 = sp->data_length >> FRACTION_BITS;
    origdata = sp->data;

    length = freq_initialize_fft_arrays(sp);

    /* base frequency of the FFT */
    f0 = (float) rate / length;

    /* get maximum amplitude */
    maxamp = 0;
    for (i = 0; i < length0; i++)
    {
	amp = abs(origdata[i]);
	if (amp >= maxamp)
	{
	    maxamp = amp;
	    maxpos = i;
	}
    }

    /* go out 2 zero crossings starting from maxpos */
    minoffset1 = 0;
    for (n = 0, oldamp = maxamp, i = maxpos - 1; i >= 0 && n < 2; i--)
    {
	amp = origdata[i];
	if ((amp * oldamp < 0) ||
	    (maxamp * amp > 0 && !oldamp) || (maxamp * oldamp > 0 && !amp))
		n++;
	oldamp = amp;
    }
    if (n == 2)
	minoffset1 = maxpos - i;
    minoffset2 = 0;
    for (n = 0, oldamp = maxamp, i = maxpos + 1; i < length0 && n < 2; i++)
    {
	amp = origdata[i];
	if ((amp * oldamp < 0) ||
	    (maxamp * amp > 0 && !oldamp) || (maxamp * oldamp > 0 && !amp))
		n++;
	oldamp = amp;
    }
    if (n == 2)
	minoffset2 = i - maxpos;

    minoffset = minoffset1;
    if (minoffset2 > minoffset1)
	minoffset = minoffset2;

    if (minoffset < 2)
	minoffset = 2;
    maxoffset = rate / pitch_freq_table[LOWEST_PITCH] + 2;

    /* don't go beyond the end of the sample */
    if (maxoffset > (length >> 1))
	maxoffset = (length >> 1);

    minfreq = (float) rate / maxoffset;
    maxfreq = (float) rate / minoffset;
    if (maxfreq >= (rate >> 1)) maxfreq = (rate >> 1) - 1;

    /* perform the in place FFT */
    rdft(length, 1, floatdata, ip, w);

    /* calc the magnitudes */
    for (i = 2; i < length; i++)
    {
	mag = floatdata[i++];
	mag *= mag;
	mag += floatdata[i] * floatdata[i];
	magdata[i >> 1] = sqrt(mag);
    }

    /* bin the pitches */
    maxmag = 0;
    for (i = 1; i < (length >> 1); i++)
    {
	mag = magdata[i];

	pitch = fft1_bin_to_pitch[i];
	pitchbins[pitch] += mag;
	if (pitch && mag > maxmag)
	    maxmag = mag;
	if (mag > pitchmags[pitch])
	    pitchmags[pitch] = mag;
    }

    /* ingore lowest pitch when determinging largest peak */
    for (i = LOWEST_PITCH + 1, maxsum = -42; i <= HIGHEST_PITCH; i++)
    {
	sum = pitchbins[i];
	if (sum > maxsum)
	{
	    maxsum = sum;
	    largest_peak = i;
	}
    }

    /* remove all pitches below threshold */
    for (i = LOWEST_PITCH; i <= HIGHEST_PITCH; i++)
    {
	if (pitchbins[i] / maxsum < 0.1 || pitchmags[i] / maxmag < 0.2)
	    pitchbins[i] = 0;
    }

    /* zero out any peak that has LOWEST_PITCH in it */
    for (i = LOWEST_PITCH; pitchbins[i] && i <= HIGHEST_PITCH; i++)
    {
	pitchbins[i] = 0;
    }

    /* keep only the first NUM_WIDE_PEAKS_TO_KEEP wide peaks */
    newminfreq = minfreq;
    newmaxfreq = maxfreq;
    minpitch = -1;
    for (i = LOWEST_PITCH, n = 0; i <= HIGHEST_PITCH; i++)
    {
	if (pitchbins[i])
	{
	    if (n < NUM_WIDE_PEAKS_TO_KEEP)
	    {
		if (i == HIGHEST_PITCH || !pitchbins[i + 1])
		    n++;
		if (minpitch < 0)
		{
		    freq = pitch_freq_lb_table[i];
		    if (freq > minfreq)
			newminfreq = freq;
		    minpitch = i;
		}
		freq = pitch_freq_ub_table[i];
		if (freq < maxfreq)
		    newmaxfreq = freq;
		maxpitch = i;
	    }
	    else
		pitchbins[i] = 0;
	}
    }

    minfreq = newminfreq;
    maxfreq = newmaxfreq;

    minbin = minfreq / f0;
    if (!minbin)
	minbin = 1;
    maxbin = ceil(maxfreq / f0);

    /* skip ahead to final freq refinement if it is a chord */
    if ((pitch = assign_chord(pitchbins, chord)) >= 0)
    {
	minfreq = pitch_freq_lb_table[pitch];
	maxfreq = pitch_freq_ub_table[pitch];
	largest_peak = pitch;
	goto fourier_refine_bin;
    }

    /* filter out all "noise" from magnitude array */
    /* save it into where we will take the logs */
    for (i = minbin, n = 0; i <= maxbin; i++)
    {
	pitch = fft1_bin_to_pitch[i];
	if (pitchbins[pitch])
	{
	    logmagdata[i] = magdata[i];
	    n++;
	}
    }

    /* whoa!, there aren't any strong peaks at all !!! bomb early */
    if (!n)
	return 260;

    /* check for remaining points */
    for (i = maxbin, n = 0; i >= minbin; i--)
	if (logmagdata[i] && i * f0 <= maxfreq)
	    n++;

    /* uh oh, this is not a well behaved sample */
    if (!n)
    {
	/* set maxfreq to highest peak */
	for (i = HIGHEST_PITCH; i >= 0; i--)
	{
	    if (pitchbins[i])
		break;
	}
	maxfreq = pitch_freq_ub_table[i];
    }

    /* take the log10 of all the magnitudes */
    for (i = minbin; i <= maxbin; i++)
    {
	if (!logmagdata[i])
	    logmagdata[i] = 0;
	else
	    logmagdata[i] = log10(logmagdata[i]);
    }

    /* take FFT of the log magnitude array */
    rdft(length, 1, logmagdata, ip, w);

    minbin = ceil(rate / maxfreq);
    if (!minbin)
	minbin = 1;
    maxbin = rate / minfreq;

    /* uh oh, bin bounds are switched due to poor resolution */
    if (maxbin < minbin)
    {
	bin = minbin;
	minbin = maxbin;
	maxbin = bin;
    }

    /* calculate the cepstrum, find maximum */
    for (i = (minbin << 1), maxcepstrum = -42; i <= (maxbin << 1); i++)
    {
	mag = logmagdata[i++];
	mag *= mag;
	mag += logmagdata[i] * logmagdata[i];
	cepstrum[i >> 1] = mag;

	if (mag > maxcepstrum)
	{
	    maxcepstrum = mag;
	    bestbin = i >> 1;
	}
    }

    /* sum up cepstrum pitch peak areas */
    for (i = minbin, n = 0; i <= maxbin; i++)
    {
	freq = (float) rate / i;
	pitch = assign_pitch_to_freq(freq);

	if (pitchbins[pitch] && freq >= minfreq && freq <= maxfreq)
	{
	    pitchbins_cepstrum[pitch] += cepstrum[i];
	    n++;
	}
    }

    /* uh oh, no good cepstrum peaks, might be bad FFT resolution? */
    /* just choose the pitch closest to the biggest one */
    if (!n)
    {
	pitch = assign_pitch_to_freq((float) rate / bestbin);
	for (i = minpitch, bestdist = 129; i <= maxpitch; i++)
	{
	    if (!pitchbins[i])
		continue;

	    dist = abs(i - pitch);
	    if (dist < bestdist)
	    {
		bestdist = dist;
		largest_peak = i;
	    }
	}
	minfreq = pitch_freq_lb_table[largest_peak];
	maxfreq = pitch_freq_ub_table[largest_peak];
	goto fourier_refine_bin;
    }

    /* weight cepstrum pitch peaks by the maximum magnitude of the pitch */
    /* this doesn't work so well when fft length > 2*rate, which is why
       only a maximum of 2 seconds of audio is analyzed */
    for (i = minpitch; i <= maxpitch; i++)
    {
	if (pitchbins_cepstrum[i])
	    pitchbins_cepstrum[i] *= pitchmags[i];
    }

    /* find largest cepstrum pitch peak */
    for (i = minpitch, maxsum = -42; i <= maxpitch; i++)
    {
	sum = pitchbins_cepstrum[i];
	if (sum > maxsum)
	{
	    maxsum = sum;
	    largest_peak = i;
	}
    }

  fourier_refine_bin:;

    minbin = minfreq / f0;
    if (!minbin)
	minbin = 1;
    maxbin = ceil(maxfreq / f0);

    /* do a weighted average of the frequencies within the pitch peak */
    weightsum = sum = 0;
    for (i = minbin, maxmag = -42; i <= maxbin; i++)
    {
	pitch = fft1_bin_to_pitch[i];
	if (pitch == largest_peak)
	{
	    mag = magdata[i];
	    if (mag > maxmag)
	    {
		maxmag = mag;
		bestbin = i;
	    }
	    sum += mag;
	    weightsum += i * mag;
	}
	else if (pitch > largest_peak)
	    break;
    }
    refinedbin = weightsum / sum;

    freq = refinedbin * f0;

    free(floatdata);

    return (freq);
}



int assign_pitch_to_freq(float freq)
{

    int pitch;

    /* round to nearest integer using: ceil(fraction - 0.5) */
    /* -0.5 is already added into the first constant below */
    pitch = ceil(-36.87631656f + 17.31234049f * log(freq));

    /* min and max pitches */
    if (pitch < LOWEST_PITCH) pitch = LOWEST_PITCH;
    else if (pitch > HIGHEST_PITCH) pitch = HIGHEST_PITCH;

    return pitch;
}
