/*
 * HLS ad break detection from parsed media-playlist metadata.
 * Copyright (c) 2026 FongMi
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
#ifndef AVFORMAT_HLS_AD_DETECT_H
#define AVFORMAT_HLS_AD_DETECT_H

#include <stdint.h>

typedef struct FFHLSAdSegment {
    const char *url;
    const char *key;
    const char *init_url;
    int64_t duration;
    int64_t offset;
    int64_t size;
    uint8_t iv[16];
    int key_type;
    int discontinuity;
    int cue_ad;
} FFHLSAdSegment;

typedef struct FFHLSAdProbeResult {
    int64_t size;
    double frame_rate;
    double frame_rate_margin;
    int width;
    int height;
} FFHLSAdProbeResult;

/* Returns the number of removable segments, or zero for ambiguous playlists. */
int ff_hls_ad_detect(const FFHLSAdSegment *segments, int count,
                     int has_cue, int valid_cue, int allow_repeated_blocks,
                     uint8_t *remove);

/* Confirms a discontinuity-bounded interior block from measured segment media. */
int ff_hls_ad_confirm_window(const FFHLSAdSegment *segments,
                              const FFHLSAdProbeResult *results,
                              int count, int first, int end);

#endif
