/*
 * HLS ad removal plan validation
 * Copyright (c) 2026 FongMi
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef AVFORMAT_HLS_AD_PLAN_INTERNAL_H
#define AVFORMAT_HLS_AD_PLAN_INTERNAL_H

#include <stdint.h>

#include "hls_ad_plan.h"

/* Copies only complete, not-yet-opened ad ranges into remove. Returns 1 when
 * a plan was consumed, 0 when none is pending, or a negative AVERROR. */
int ff_hls_ad_plan_apply(const char *session, const AVHLSAdSegment *actual,
                         int count, int first_unread_index, uint8_t *remove,
                         int64_t *removed_duration_us);
void ff_hls_ad_plan_reject(const char *session);

#endif
