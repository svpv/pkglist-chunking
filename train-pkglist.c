#include <stdio.h>
#define ZDICT_STATIC_LINKING_ONLY
#include <zdict.h>
#include "xwrite.h"
#include "rpmblob.h"
#include "chunker.h"

struct samples {
    size_t nbSamples;
    size_t samplesSizes[1<<20];
    char samplesBuffer[1<<30];
} samples;

static void load(void)
{
    struct rpmBlob q[16];
    size_t nq = 0;

    void *p = samples.samplesBuffer;

    void Pop(size_t n)
    {
	assert(n > 0);
	assert(nq >= n);

	void *p0 = p;
	for (size_t i = 0; i < n; i++) {
	    p = mempcpy(p, q[i].blob, q[i].blobSize);
	    free(q[i].blob);
	}
	samples.samplesSizes[samples.nbSamples++] = p - p0;

	nq -= n;
	memmove(q, q + n, nq * sizeof q[0]);
    }

    struct chunker *C = chunker_new();
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
    chunker_free(C);
}

int main()
{
    assert(!isatty(0));
    assert(!isatty(1));

    load();

    ZDICT_cover_params_t params = {
	.d = 6,
	.nbThreads = 2,
	.zParams.notificationLevel = 3,
	.zParams.compressionLevel = 19,
    };
    char dict[512<<10];
    size_t dictSize = ZDICT_optimizeTrainFromBuffer_cover(dict, sizeof dict,
	    samples.samplesBuffer, samples.samplesSizes, samples.nbSamples,
	    &params);
    assert(dictSize == sizeof dict);
    fprintf(stderr, "best parameters: d=%u k=%u\n", params.d, params.k);

    if (!xwrite(1, dict, dictSize))
	assert(!!!"write failed");
    return 0;
}
