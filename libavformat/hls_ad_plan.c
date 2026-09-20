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

#include <errno.h>
#include <limits.h>
#include <string.h>

#include "libavutil/error.h"
#include "libavutil/mem.h"
#include "libavutil/thread.h"

#include "hls_ad_plan_internal.h"

#define MAX_PLAN_SEGMENTS 10000
#define MAX_SESSION_LENGTH 64
#define MAX_SEGMENT_URL_LENGTH 4096
#define DURATION_TOLERANCE_US 1000

typedef struct StoredSegment {
    char *url;
    int64_t duration_us;
    int64_t byte_range_offset;
    int64_t byte_range_length;
    int remove;
} StoredSegment;

static AVMutex plan_mutex = AV_MUTEX_INITIALIZER;
static struct {
    char *session;
    StoredSegment *segments;
    int count;
    int status;
    int first_unread_index;
} plan;

static void clear_plan(void)
{
    for (int i = 0; i < plan.count; i++)
        av_freep(&plan.segments[i].url);
    av_freep(&plan.segments);
    av_freep(&plan.session);
    plan.count = 0;
    plan.status = AV_HLS_AD_PLAN_UNKNOWN;
    plan.first_unread_index = -1;
}

int avformat_hls_ad_plan_publish(const char *session,
                                 const AVHLSAdSegment *segments, int count)
{
    StoredSegment *copy;
    char *session_copy;
    int has_removal = 0;
    int has_content = 0;
    size_t session_length;

    if (!session || !segments || count < 1 || count > MAX_PLAN_SEGMENTS)
        return AVERROR(EINVAL);
    session_length = strlen(session);
    if (!session_length || session_length > MAX_SESSION_LENGTH)
        return AVERROR(EINVAL);

    copy = av_calloc(count, sizeof(*copy));
    session_copy = av_strdup(session);
    if (!copy || !session_copy) {
        av_free(copy);
        av_free(session_copy);
        return AVERROR(ENOMEM);
    }
    for (int i = 0; i < count; i++) {
        size_t url_length;
        const AVHLSAdSegment *source = &segments[i];
        if (!source->url || source->duration_us <= 0 ||
            source->byte_range_offset < 0 || source->byte_range_length < -1 ||
            (source->remove != 0 && source->remove != 1))
            goto invalid;
        url_length = strlen(source->url);
        if (!url_length || url_length > MAX_SEGMENT_URL_LENGTH)
            goto invalid;
        copy[i].url = av_strdup(source->url);
        if (!copy[i].url)
            goto nomem;
        copy[i].duration_us = source->duration_us;
        copy[i].byte_range_offset = source->byte_range_offset;
        copy[i].byte_range_length = source->byte_range_length;
        copy[i].remove = source->remove;
        has_removal |= source->remove;
        has_content |= !source->remove;
    }
    if (!has_removal || !has_content)
        goto invalid;

    ff_mutex_lock(&plan_mutex);
    clear_plan();
    plan.session = session_copy;
    plan.segments = copy;
    plan.count = count;
    plan.status = AV_HLS_AD_PLAN_PENDING;
    plan.first_unread_index = -1;
    ff_mutex_unlock(&plan_mutex);
    return 0;

nomem:
    for (int i = 0; i < count; i++)
        av_free(copy[i].url);
    av_free(copy);
    av_free(session_copy);
    return AVERROR(ENOMEM);
invalid:
    for (int i = 0; i < count; i++)
        av_free(copy[i].url);
    av_free(copy);
    av_free(session_copy);
    return AVERROR(EINVAL);
}

int avformat_hls_ad_plan_status(const char *session)
{
    int status;
    ff_mutex_lock(&plan_mutex);
    status = session && plan.session && !strcmp(session, plan.session)
                 ? plan.status : AV_HLS_AD_PLAN_UNKNOWN;
    ff_mutex_unlock(&plan_mutex);
    return status;
}

int avformat_hls_ad_plan_first_unread_index(const char *session)
{
    int first_unread_index;
    ff_mutex_lock(&plan_mutex);
    first_unread_index = session && plan.session && !strcmp(session, plan.session) &&
                         plan.status == AV_HLS_AD_PLAN_APPLIED
                             ? plan.first_unread_index : -1;
    ff_mutex_unlock(&plan_mutex);
    return first_unread_index;
}

void avformat_hls_ad_plan_clear(const char *session)
{
    ff_mutex_lock(&plan_mutex);
    if (session && plan.session && !strcmp(session, plan.session))
        clear_plan();
    ff_mutex_unlock(&plan_mutex);
}

void ff_hls_ad_plan_reject(const char *session)
{
    ff_mutex_lock(&plan_mutex);
    if (session && plan.session && !strcmp(session, plan.session) &&
        plan.status == AV_HLS_AD_PLAN_PENDING)
        plan.status = AV_HLS_AD_PLAN_REJECTED;
    ff_mutex_unlock(&plan_mutex);
}

int ff_hls_ad_plan_apply(const char *session, const AVHLSAdSegment *actual,
                         int count, int first_unread_index, uint8_t *remove,
                         int64_t *removed_duration_us)
{
    int64_t duration = 0;
    int result = 0;

    if (!session || !actual || !remove || !removed_duration_us || count < 1 ||
        first_unread_index < 0 || first_unread_index > count)
        return AVERROR(EINVAL);
    *removed_duration_us = 0;
    ff_mutex_lock(&plan_mutex);
    if (!plan.session || strcmp(session, plan.session) ||
        plan.status != AV_HLS_AD_PLAN_PENDING)
        goto finish;
    result = 1;
    if (count != plan.count)
        goto reject;
    for (int i = 0; i < count; i++) {
        const StoredSegment *expected = &plan.segments[i];
        const AVHLSAdSegment *found = &actual[i];
        if (!found->url || found->duration_us <= 0 ||
            strcmp(expected->url, found->url) ||
            expected->byte_range_offset != found->byte_range_offset ||
            expected->byte_range_length != found->byte_range_length ||
            expected->duration_us - found->duration_us > DURATION_TOLERANCE_US ||
            found->duration_us - expected->duration_us > DURATION_TOLERANCE_US)
            goto reject;
    }
    for (int first = 0; first < count;) {
        int end = first;
        if (!plan.segments[first].remove) {
            first++;
            continue;
        }
        while (end < count && plan.segments[end].remove)
            end++;
        /* A range already being read cannot be removed, but later ranges can. */
        if (first < first_unread_index) {
            first = end;
            continue;
        }
        for (int i = first; i < end; i++) {
            if (duration > INT64_MAX - actual[i].duration_us)
                goto reject;
            duration += actual[i].duration_us;
        }
        first = end;
    }
    for (int first = 0; first < count;) {
        int end = first;
        if (!plan.segments[first].remove) {
            first++;
            continue;
        }
        while (end < count && plan.segments[end].remove)
            end++;
        if (first >= first_unread_index)
            memset(remove + first, 1, end - first);
        first = end;
    }
    if (!duration)
        goto reject;
    *removed_duration_us = duration;
    plan.first_unread_index = first_unread_index;
    plan.status = AV_HLS_AD_PLAN_APPLIED;
    goto finish;

reject:
    plan.status = AV_HLS_AD_PLAN_REJECTED;
finish:
    ff_mutex_unlock(&plan_mutex);
    return result;
}
