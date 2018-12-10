#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <t1ha.h>
#include <zstd.h>
#include "xread.h"
#include "xwrite.h"
#include "rpmblob.h"
#include "shingle.h"

int main(int argc, char **argv)
{
    int level = 3;
    const char *dictFile = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "1::2::3::4::5::6::7::8::9::D:")) != -1) {
	switch (opt) {
	case '1':
	    if (!optarg)
		level = 1;
	    else {
		assert(*optarg >= '0' && *optarg <= '9');
		assert(optarg[1] == '\0');
		level = 10 + *optarg - '0';
	    }
	    break;
	case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9':
	    if (optarg)
		assert(!"supported levels > 19");
	    level = opt - '0';
	    break;
	case 'D':
	    dictFile = optarg;
	    break;
	default:
	    fprintf(stderr, "Usage: %s <IN >OUT\n", argv[0]);
	    return 2;
	}
    }

    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    assert(cctx);

    ZSTD_CDict *cdict = NULL;
    if (dictFile) {
	char buf[(1<<20)+1];
	int fd = open(dictFile, O_RDONLY);
	assert(fd);
	ssize_t ret = xread(fd, buf, sizeof buf);
	assert(ret > 0);
	assert(ret < sizeof buf);
	cdict = ZSTD_createCDict(buf, ret, level);
	assert(cdict);
    }

    struct rpmBlob q[8];
    size_t nq = 0;

    void Pop(size_t n)
    {
	assert(n > 0);
	assert(nq >= n);

	size_t totalSize = q[0].blobSize;
	for (size_t i = 1; i < n; i++)
	    totalSize += q[i].blobSize;
	size_t nameoff = q[0].name - (char *) q[0].blob;
	q[0].blob = realloc(q[0].blob, totalSize + ZSTD_COMPRESSBOUND(totalSize));
	assert(q[0].blob);
	q[0].name = q[0].blob + nameoff;

	void *p = q[0].blob + q[0].blobSize;
	for (int i = 1; i < n; i++)
	    p = mempcpy(p, q[i].blob, q[i].blobSize);
	assert(p - q[0].blob == totalSize);

	size_t zsize;
	if (cdict)
	    zsize = ZSTD_compress_usingCDict(cctx, p, ZSTD_COMPRESSBOUND(totalSize),
					     q[0].blob, totalSize, cdict);
	else
	    zsize = ZSTD_compressCCtx(cctx, p, ZSTD_COMPRESSBOUND(totalSize),
				      q[0].blob, totalSize, level);
	assert(zsize > 0);
	assert(zsize <= ZSTD_COMPRESSBOUND(totalSize));

	if (!xwrite(1, p, zsize))
	    assert(!!!"write failed");

	uint64_t hi;
	uint64_t lo = t1ha2_atonce128(&hi, p, zsize, 0);
	fprintf(stderr, "%016" PRIx64 "%016" PRIx64 "\t%zu\t%s", lo, hi, zsize, q[0].name);
	for (size_t i = 1; i < n; i++) {
	    fprintf(stderr, " %s", q[i].name);
	    free(q[i].blob);
	    free(q[i].shi);
	}
	fprintf(stderr, "\n");
	free(q[0].blob);
	free(q[0].shi);

	nq -= n;
	memmove(q, q + n, nq * sizeof q[0]);
    }

    // 2+ headers per chunk.
    while (readRpmBlob(&q[nq])) {
	nq++;
	switch (nq) {
	case 1: case 2: break;
	case 3: if (q[1].nameHash > q[2].nameHash) Pop(2); break;
	case 4: if (q[2].nameHash > q[3].nameHash) Pop(3); break;
	case 5: if (q[3].nameHash > q[4].nameHash) Pop(4); break;
	case 6: if (q[4].nameHash > q[5].nameHash) Pop(5); break;
	case 7: if (q[5].nameHash > q[6].nameHash) Pop(6); break;
	case 8: if (q[6].nameHash > q[7].nameHash) Pop(7); else Pop(8); break;
	default: assert(!"possible");
	}
    }
    if (nq)
	Pop(nq);
    return 0;
}
