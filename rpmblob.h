// Copyright (c) 2018 Alexey Tourbin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <rpm/rpmtag.h>
#include <t1ha.h>
#include "xread.h"

struct rpmBlob {
    void *blob;
    size_t blobSize;
    const char *name;
    const char *srpm;
    uint64_t nameHash;
    struct shingles *shi;
};

static bool readRpmBlob(struct rpmBlob *b)
{
    unsigned lead[4];
    ssize_t ret = xread(0, lead, sizeof lead);
    if (ret == 0)
	return false;
    assert(ret == sizeof lead);

    const unsigned char magic[8] = { 0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00 };
    assert(memcmp(lead, magic, 8) == 0);

    unsigned il = ntohl(lead[2]);
    unsigned dl = ntohl(lead[3]);
    assert(il-1 < (64<<10));
    assert(dl-1 < (16<<20));

    b->blobSize = 16 + 16 * il + dl;
    b->blob = malloc(b->blobSize);
    assert(b->blob);
    memcpy(b->blob, lead, 16);
    ret = xread(0, b->blob + 16, b->blobSize - 16);
    assert(ret == b->blobSize - 16);

    struct HeaderEntry {
	int tag;
	int type;
	unsigned off;
	unsigned cnt;
    };
    struct HeaderEntry *ee = b->blob + 16;
    assert(il > 1);
    assert(ee[1].tag == htonl(RPMTAG_NAME));
    assert(ee[1].type == htonl(RPM_STRING_TYPE));
    unsigned off = ntohl(ee[1].off);
    assert(off < dl);
    char *data = (void *) (ee + il);
    b->name = &data[off];
    struct HeaderEntry *e = ee + 2, *eend = ee + il;
    for (; e < eend; e++)
	if (e->tag == htonl(RPMTAG_SOURCERPM))
	    break;
    assert(e < eend);
    off = ntohl(e->off);
    assert(off < dl);
    b->srpm = &data[off];
    b->nameHash = t1ha(b->srpm, strlen(b->srpm), 0);
    b->shi = NULL;
    return true;
}
