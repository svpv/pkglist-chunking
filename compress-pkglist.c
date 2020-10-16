#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <t1ha.h>
#include <zstd.h>
#include "xread.h"
#include "xwrite.h"
#include "rpmblob.h"
#include "chunker.h"

int main(int argc, char **argv)
{
    int level = 19;
    int wlog = -1;
    const char *dictFile = NULL;

    enum {
	OPT_HELP = 256,
	OPT_SEED,
	OPT_WLOG,
    };
    static const struct option longopts[] = {
	{ "help",    no_argument,       NULL, OPT_HELP },
	{ "seed",    required_argument, NULL, OPT_SEED },
	{ "wlog",    required_argument, NULL, OPT_WLOG },
	{  NULL,     0,                 NULL,  0  },
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "1::2::3::4::5::6::7::8::9::D:", longopts, NULL)) != -1) {
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
	case OPT_SEED:
	    assert(*optarg >= '0' && *optarg <= '9');
	    rpmBlobSeed = strtoull(optarg, NULL, 0);
	    break;
	case OPT_WLOG:
	    wlog = atoi(optarg);
	    assert(wlog > 0);
	    break;
	default:
	    fprintf(stderr, "Usage: %s <IN >OUT\n", argv[0]);
	    return 2;
	}
    }

    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    assert(cctx);
    size_t zret = ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, level);
    assert(!ZSTD_isError(zret));
    if (wlog > 0) {
	zret = ZSTD_CCtx_setParameter(cctx, ZSTD_c_windowLog, wlog);
	assert(!ZSTD_isError(zret));
    }
    if (dictFile) {
	char buf[(1<<20)+1];
	int fd = open(dictFile, O_RDONLY);
	assert(fd);
	ssize_t ret = xread(fd, buf, sizeof buf);
	assert(ret > 0);
	assert(ret < sizeof buf);
	zret = ZSTD_CCtx_loadDictionary(cctx, buf, ret);
	assert(!ZSTD_isError(zret));
    }

    struct rpmBlob q[16];
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

	size_t zsize = ZSTD_compress2(cctx, p, ZSTD_COMPRESSBOUND(totalSize),
				      q[0].blob, totalSize);
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

    struct chunker *C = chunker_new();
    assert(C);

    // 2+ headers per chunk.
    while (readRpmBlob(&q[nq])) {
	size_t n = chunker_add(C, q[nq++].nameHash);
	if (n)
	    Pop(n);
    }
    while (1) {
	size_t n = chunker_flush(C);
	if (!n)
	    break;
	Pop(n);
    }
    return 0;
}
