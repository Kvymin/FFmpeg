/*
 * Copyright (c) 2026 FongMi
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <string.h>

#include "libavformat/hls_ad_plan_internal.h"

static const AVHLSAdSegment segments[] = {
    { "https://example.test/open.ts", 4000000, 0, -1, 0 },
    { "https://example.test/ad.ts",   2000000, 0, -1, 1 },
    { "https://example.test/main.ts", 5000000, 0, -1, 0 },
};

static int check(int condition, const char *message)
{
    if (condition)
        return 0;
    fprintf(stderr, "%s\n", message);
    return 1;
}

static int test_future_segment(void)
{
    uint8_t remove[3] = { 0 };
    int64_t duration = -1;

    if (check(!avformat_hls_ad_plan_publish("future", segments, 3), "publish") ||
        check(avformat_hls_ad_plan_status("future") == AV_HLS_AD_PLAN_PENDING,
              "pending status") ||
        check(ff_hls_ad_plan_apply("future", segments, 3, 1, remove, &duration) == 1,
              "apply") ||
        check(remove[0] == 0 && remove[1] == 1 && remove[2] == 0,
              "only the ad is removed") ||
        check(duration == 2000000, "removed duration") ||
        check(avformat_hls_ad_plan_status("future") == AV_HLS_AD_PLAN_APPLIED,
              "applied status") ||
        check(avformat_hls_ad_plan_first_unread_index("future") == 1,
              "applied frontier"))
        return 1;
    avformat_hls_ad_plan_clear("future");
    return 0;
}

static int test_already_read_segment(void)
{
    uint8_t remove[3] = { 0 };
    int64_t duration = -1;

    if (check(!avformat_hls_ad_plan_publish("read", segments, 3), "publish read") ||
        check(ff_hls_ad_plan_apply("read", segments, 3, 2, remove, &duration) == 1,
              "consume read plan") ||
        check(avformat_hls_ad_plan_status("read") == AV_HLS_AD_PLAN_REJECTED,
              "read ad rejected") ||
        check(avformat_hls_ad_plan_first_unread_index("read") == -1,
              "rejected frontier unavailable") ||
        check(!remove[0] && !remove[1] && !remove[2] && !duration,
              "read ad has no effect"))
        return 1;
    avformat_hls_ad_plan_clear("read");
    return 0;
}

static int test_changed_playlist(void)
{
    AVHLSAdSegment changed[3];
    uint8_t remove[3] = { 0 };
    int64_t duration = -1;

    memcpy(changed, segments, sizeof(changed));
    changed[2].url = "https://example.test/changed.ts";
    if (check(!avformat_hls_ad_plan_publish("changed", segments, 3),
              "publish changed") ||
        check(ff_hls_ad_plan_apply("changed", changed, 3, 1,
                                    remove, &duration) == 1,
              "consume changed plan") ||
        check(avformat_hls_ad_plan_status("changed") == AV_HLS_AD_PLAN_REJECTED,
              "changed playlist rejected") ||
        check(!remove[1] && !duration, "changed playlist has no effect"))
        return 1;
    avformat_hls_ad_plan_clear("changed");
    return 0;
}

static int test_changed_byte_range(void)
{
    AVHLSAdSegment changed[3];
    uint8_t remove[3] = { 0 };
    int64_t duration = -1;

    memcpy(changed, segments, sizeof(changed));
    changed[1].byte_range_length = 1024;
    if (check(!avformat_hls_ad_plan_publish("range", segments, 3),
              "publish range") ||
        check(ff_hls_ad_plan_apply("range", changed, 3, 1,
                                    remove, &duration) == 1,
              "consume range plan") ||
        check(avformat_hls_ad_plan_status("range") == AV_HLS_AD_PLAN_REJECTED,
              "changed byte range rejected") ||
        check(!remove[1] && !duration, "changed range has no effect"))
        return 1;
    avformat_hls_ad_plan_clear("range");
    return 0;
}

static int test_changed_duration(void)
{
    AVHLSAdSegment changed[3];
    uint8_t remove[3] = { 0 };
    int64_t duration = -1;

    memcpy(changed, segments, sizeof(changed));
    changed[1].duration_us += 2000;
    if (check(!avformat_hls_ad_plan_publish("duration", segments, 3),
              "publish duration") ||
        check(ff_hls_ad_plan_apply("duration", changed, 3, 1,
                                    remove, &duration) == 1,
              "consume duration plan") ||
        check(avformat_hls_ad_plan_status("duration") == AV_HLS_AD_PLAN_REJECTED,
              "changed duration rejected") ||
        check(!remove[1] && !duration, "changed duration has no effect"))
        return 1;
    avformat_hls_ad_plan_clear("duration");
    return 0;
}

static int test_mixed_past_and_future_ads(void)
{
    const AVHLSAdSegment two_ads[] = {
        { "https://example.test/opening1.ts", 2000000, 0, -1, 1 },
        { "https://example.test/opening2.ts", 2000000, 0, -1, 1 },
        segments[2],
        { "https://example.test/ad2.ts", 2000000, 0, -1, 1 },
        { "https://example.test/end.ts", 5000000, 0, -1, 0 },
    };
    uint8_t remove[5] = { 0 };
    int64_t duration = -1;

    if (check(!avformat_hls_ad_plan_publish("mixed", two_ads, 5),
              "publish two ads") ||
        check(ff_hls_ad_plan_apply("mixed", two_ads, 5, 1,
                                    remove, &duration) == 1,
              "consume mixed plan") ||
        check(avformat_hls_ad_plan_status("mixed") == AV_HLS_AD_PLAN_APPLIED,
              "future ad remains applicable") ||
        check(avformat_hls_ad_plan_first_unread_index("mixed") == 1,
              "partial plan frontier") ||
        check(!remove[0] && !remove[1] && remove[3] && duration == 2000000,
              "only the complete future ad is removed"))
        return 1;
    avformat_hls_ad_plan_clear("mixed");
    return 0;
}

static int test_all_ads_rejected(void)
{
    AVHLSAdSegment all_ads[3];

    memcpy(all_ads, segments, sizeof(all_ads));
    for (int i = 0; i < 3; i++)
        all_ads[i].remove = 1;
    return check(avformat_hls_ad_plan_publish("all", all_ads, 3) < 0,
                 "all segments cannot be removed");
}

static int test_stale_session(void)
{
    if (check(!avformat_hls_ad_plan_publish("old", segments, 3), "publish old") ||
        check(!avformat_hls_ad_plan_publish("new", segments, 3), "publish new"))
        return 1;
    avformat_hls_ad_plan_clear("old");
    if (check(avformat_hls_ad_plan_status("old") == AV_HLS_AD_PLAN_UNKNOWN,
              "old session is stale") ||
        check(avformat_hls_ad_plan_status("new") == AV_HLS_AD_PLAN_PENDING,
              "stale clear preserves new session"))
        return 1;
    avformat_hls_ad_plan_clear("new");
    return 0;
}

int main(void)
{
    return test_future_segment() || test_already_read_segment() ||
           test_changed_playlist() || test_changed_byte_range() ||
           test_changed_duration() ||
           test_mixed_past_and_future_ads() || test_all_ads_rejected() ||
           test_stale_session();
}
