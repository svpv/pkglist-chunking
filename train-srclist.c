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
    struct srpmBlob stack[8];
    size_t nstack = 0;

    void *p = samples.samplesBuffer;

    void Pop(size_t n)
    {
	assert(n > 0);
	assert(nstack >= n);

	void *p0 = p;
	for (size_t i = 0; i < n; i++) {
	    p = mempcpy(p, stack[i].blob, stack[i].blobSize);
	    free(stack[i].blob);
	}
	samples.samplesSizes[samples.nbSamples++] = p - p0;

	nstack -= n;
	memmove(stack, stack + n, nstack * sizeof stack[0]);
    }

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
    };
    char dict[112<<10];
    size_t dictSize = ZDICT_optimizeTrainFromBuffer_cover(dict, sizeof dict,
	    samples.samplesBuffer, samples.samplesSizes, samples.nbSamples,
	    &params);
    assert(dictSize == sizeof dict);
    fprintf(stderr, "best parameters: d=%u k=%u\n", params.d, params.k);

    if (!xwrite(1, dict, dictSize))
	assert(!!!"write failed");
    return 0;
}
