/*
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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>

#include "libavformat/hls.c"

static int check_playlist(const char *line, int position,
                          int allow_repeated, int valid_cue)
{
    AVFormatContext *format = avformat_alloc_context();
    HLSContext hls = { .ctx = format };
    FFIOContext io;
    char *manifest = av_asprintf(
        "#EXTM3U\n"
        "#EXT-X-TARGETDURATION:4\n"
        "%s\n"
        "#EXTINF:4,\n"
        "before.ts\n"
        "#EXT-X-DISCONTINUITY\n"
        "#EXTINF:4,\n"
        "%s\n"
        "ad.ts\n"
        "#EXT-X-DISCONTINUITY\n"
        "#EXTINF:4,\n"
        "after.ts\n"
        "#EXT-X-ENDLIST\n"
        "%s\n", position == 0 ? line : "",
                  position == 1 ? line : "",
                  position == 2 ? line : "");
    int ret = 1;

    if (!format || !manifest)
        goto end;
    format->priv_data = &hls;
    ffio_init_read_context(&io, (const uint8_t *)manifest, strlen(manifest));
    if (parse_playlist(&hls, "http://test.invalid/index.m3u8", NULL, &io.pub) < 0)
        goto end;
    if (hls.n_playlists != 1 || !hls.playlists[0]->finished ||
        hls.playlists[0]->n_segments != 3 ||
        !hls.playlists[0]->segments[1]->discontinuity ||
        !hls.playlists[0]->segments[2]->discontinuity ||
        hls.playlists[0]->allow_repeated_ad_blocks != allow_repeated ||
        hls.playlists[0]->valid_cue != valid_cue) {
        fprintf(stderr, "HLS comment/tag handling mismatch: '%s'\n", line);
        goto end;
    }
    ret = 0;

end:
    free_playlist_list(&hls);
    free_variant_list(&hls);
    free_rendition_list(&hls);
    if (format)
        format->priv_data = NULL;
    avformat_free_context(format);
    av_free(manifest);
    return ret;
}

int main(void)
{
    static const struct {
        const char *line;
        int allow_repeated;
        int valid_cue;
    } cases[] = {
        { "",                             1, 1 },
        { "#",                            1, 1 },
        { "# ordinary comment",           1, 1 },
        { "# ppvod-ad-injected v1",        1, 1 },
        { "# comment with #EXT text",     1, 1 },
        { "#ext-lowercase-comment",       1, 1 },
        { "#EXT-X-VERSION:3",             1, 1 },
        { "#EXT-X-INDEPENDENT-SEGMENTS",  1, 1 },
        { "#EXT-X-UNKNOWN:1",             0, 1 },
        { "#EXT-X-SKIP:SKIPPED-SEGMENTS=1", 0, 0 },
    };

    for (int i = 0; i < FF_ARRAY_ELEMS(cases); i++)
        for (int position = 0; position < 3; position++)
            if (check_playlist(cases[i].line, position,
                               cases[i].allow_repeated, cases[i].valid_cue))
                return 1;
    return 0;
}
