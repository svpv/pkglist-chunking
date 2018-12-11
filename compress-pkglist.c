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
    ZSTD_CDict *fastdict = NULL;
    if (dictFile) {
	char buf[(1<<20)+1];
	int fd = open(dictFile, O_RDONLY);
	assert(fd);
	ssize_t ret = xread(fd, buf, sizeof buf);
	assert(ret > 0);
	assert(ret < sizeof buf);
	cdict = ZSTD_createCDict(buf, ret, level);
	assert(cdict);
	fastdict = level > 6 ? ZSTD_createCDict(buf, ret, 6) : cdict;
    }

    struct rpmBlob q[9];
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

    size_t fastCompress(const void *in, size_t isize, void *out, size_t osize)
    {
	size_t zsize;
	if (fastdict)
	    zsize = ZSTD_compress_usingCDict(cctx, out, osize, in, isize, cdict);
	else
	    zsize = ZSTD_compressCCtx(cctx, out, osize, in, isize, level > 6 ? 6: level);
	assert(zsize > 0);
	assert(zsize < INT32_MAX);
	return zsize;
    }

    void PopX(size_t n)
    {
	if (q[n].nameHash == q[n+1].nameHash)
	    return Pop(n);
	size_t size12 = q[n-1].blobSize + q[n].blobSize;
	size_t size23 = q[n].blobSize + q[n+1].blobSize;
	size_t alloc = ZSTD_COMPRESSBOUND(size12 > size23 ? size12 : size23);
	void *buf = malloc(alloc); assert(buf);
	size_t price1 = fastCompress(q[n-1].blob, q[n-1].blobSize, buf, alloc);
	size_t price3 = fastCompress(q[n+1].blob, q[n+1].blobSize, buf, alloc);
	size_t nameoff = q[n-1].name - (char *) q[n-1].blob;
	q[n-1].blob = realloc(q[n-1].blob, q[n-1].blobSize + q[n].blobSize); assert(q[n-1].blob);
	q[n-1].name = q[n-1].blob + nameoff;
	nameoff = q[n].name - (char *) q[n].blob;
	q[n].blob = realloc(q[n].blob, q[n].blobSize + q[n+1].blobSize); assert(q[n].blob);
	q[n].name = q[n].blob + nameoff;
	memcpy(q[n-1].blob + q[n-1].blobSize, q[n].blob, q[n].blobSize);
	memcpy(q[n].blob + q[n].blobSize, q[n+1].blob, q[n+1].blobSize);
	size_t price12 = fastCompress(q[n-1].blob, size12, buf, alloc);
	size_t price23 = fastCompress(q[n].blob, size23, buf, alloc);
	double ratio = (price12 + price3) / (double)(price1 + price23);
	if (ratio < 0.97)
	    Pop(n+1);
	else
	    Pop(n);
	free(buf);
    }

    // 2+ headers per chunk.
    while (readRpmBlob(&q[nq])) {
	switch (nq++) {
	case 0: case 1: case 2: break;
  has3: case 3: if (q[1].nameHash > q[2].nameHash) PopX(2); break;
  has4: case 4: if (q[2].nameHash > q[3].nameHash) PopX(3); break;
  has5: case 5: if (q[3].nameHash > q[4].nameHash) PopX(4); break;
  has6: case 6: if (q[4].nameHash > q[5].nameHash) PopX(5); break;
  has7: case 7: if (q[5].nameHash > q[6].nameHash) PopX(6); break;
	case 8: if (q[6].nameHash > q[7].nameHash) PopX(7);
	   else if (q[7].nameHash != q[8].nameHash) Pop(8);
	   else if (q[6].nameHash != q[7].nameHash) Pop(7);
	   else if (q[5].nameHash != q[6].nameHash) { Pop(6); goto has3; }
	   else if (q[4].nameHash != q[5].nameHash) { Pop(5); goto has4; }
	   else if (q[3].nameHash != q[4].nameHash) { Pop(4); goto has5; }
	   else if (q[2].nameHash != q[3].nameHash) { Pop(3); goto has6; }
	   else if (q[1].nameHash != q[2].nameHash) { Pop(2); goto has7; }
	   else Pop(7);
	   break;
	default: assert(!"possible");
	}
    }
    if (nq)
	Pop(nq);
    return 0;
}
