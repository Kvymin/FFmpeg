/*
 * HLS ad removal plans for the Android player
 * Copyright (c) 2026 FongMi
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef AVFORMAT_HLS_AD_PLAN_H
#define AVFORMAT_HLS_AD_PLAN_H

#include <stdint.h>

typedef struct AVHLSAdSegment {
    const char *url;
    int64_t duration_us;
    int64_t byte_range_offset;
    int64_t byte_range_length;
    int remove;
} AVHLSAdSegment;

enum AVHLSAdPlanStatus {
    AV_HLS_AD_PLAN_UNKNOWN = -1,
    AV_HLS_AD_PLAN_PENDING,
    AV_HLS_AD_PLAN_APPLIED,
    AV_HLS_AD_PLAN_REJECTED,
};

/* Copies a plan for the current Android player session. A new publication
 * replaces the previous plan. The HLS demuxer verifies the entire playlist
 * before removing any unopened segment. */
int avformat_hls_ad_plan_publish(const char *session,
                                 const AVHLSAdSegment *segments, int count);
int avformat_hls_ad_plan_status(const char *session);
void avformat_hls_ad_plan_clear(const char *session);

#endif
