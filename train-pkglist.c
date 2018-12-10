#include <stdio.h>
#define ZDICT_STATIC_LINKING_ONLY
#include <zdict.h>
#include "xwrite.h"
#include "srpmblob.h"

struct samples {
    size_t nbSamples;
    size_t samplesSizes[1<<20];
    char samplesBuffer[1<<30];
} samples;

static void load(void)
{
    struct srpmBlob q[8];
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

    while (readSrpmBlob(&q[nq])) {
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
