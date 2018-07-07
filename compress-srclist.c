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
#include "srpmblob.h"

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
	char buf[(128<<10)+1];
	int fd = open(dictFile, O_RDONLY);
	assert(fd);
	ssize_t ret = xread(fd, buf, sizeof buf);
	assert(ret > 0);
	// Dictionaries should be smaller that 128K, otherwise literal stats
	// for Huffman coding in the first 128K block wouldn't make sense.
	assert(ret < sizeof buf);
	cdict = ZSTD_createCDict(buf, ret, level);
	assert(cdict);
    }

    struct srpmBlob stack[8];
    size_t nstack = 0;

    void Pop(size_t n)
    {
	assert(n > 0);
	assert(nstack >= n);

	size_t totalSize = stack[0].blobSize;
	for (size_t i = 1; i < n; i++)
	    totalSize += stack[i].blobSize;
	size_t nameoff = stack[0].name - (char *) stack[0].blob;
	stack[0].blob = realloc(stack[0].blob, totalSize + ZSTD_COMPRESSBOUND(totalSize));
	assert(stack[0].blob);
	stack[0].name = stack[0].blob + nameoff;

	void *p = stack[0].blob + stack[0].blobSize;
	for (int i = 1; i < n; i++)
	    p = mempcpy(p, stack[i].blob, stack[i].blobSize);
	assert(p - stack[0].blob == totalSize);

	size_t zsize;
	if (cdict)
	    zsize = ZSTD_compress_usingCDict(cctx, p, ZSTD_COMPRESSBOUND(totalSize),
					     stack[0].blob, totalSize, cdict);
	else
	    zsize = ZSTD_compressCCtx(cctx, p, ZSTD_COMPRESSBOUND(totalSize),
				      stack[0].blob, totalSize, level);
	assert(zsize > 0);
	assert(zsize <= ZSTD_COMPRESSBOUND(totalSize));

	if (!xwrite(1, p, zsize))
	    assert(!!!"write failed");

	uint64_t hi;
	uint64_t lo = t1ha2_atonce128(&hi, p, zsize, 0);
	fprintf(stderr, "%016" PRIx64 "%016" PRIx64 "\t%zu\t%s", lo, hi, zsize, stack[0].name);
	for (size_t i = 1; i < n; i++) {
	    fprintf(stderr, " %s", stack[i].name);
	    free(stack[i].blob);
	}
	fprintf(stderr, "\n");
	free(stack[0].blob);

	nstack -= n;
	memmove(stack, stack + n, nstack * sizeof stack[0]);
    }

#if 0
    // One header per chunk.
    while (readSrpmBlob(stack))
	Pop(++nstack);
#else
    // 2+ headers per chunk.
    while (readSrpmBlob(&stack[nstack])) {
	nstack++;
	switch (nstack) {
	case 1:
	    break;
	case 2:
	case 3:
	    if (stack[nstack-1].nameHash < stack[nstack-2].nameHash)
		break;
	    // fall through
	case 4:
	    Pop(nstack);
	    break;
	default:
	    assert(!"possible");
	}
    }
    if (nstack)
	Pop(nstack);
#endif
    return 0;
}
