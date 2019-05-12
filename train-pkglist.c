#include <stdio.h>
#include <getopt.h>
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

int main(int argc, char **argv)
{
    assert(!isatty(0));
    assert(!isatty(1));

    int level = 19;
    int verbose = 1;
    bool fast = false;
    int d = 0, k = 0;

    enum {
	OPT_HELP = 256,
	OPT_SEED,
	OPT_FAST,
    };
    static const struct option longopts[] = {
	{ "help",    no_argument,       NULL, OPT_HELP },
	{ "verbose", no_argument,       NULL, 'v' },
	{ "seed",    required_argument, NULL, OPT_SEED },
	{ "fast",    no_argument,       NULL, OPT_FAST },
	{  NULL,     0,                 NULL,  0  },
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "1::2::3::4::5::6::7::8::9::d:k:v", longopts, NULL)) != -1) {
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
	case 'd':
	    d = atoi(optarg);
	    assert(d > 0);
	    break;
	case 'k':
	    k = atoi(optarg);
	    assert(k > 0);
	    break;
	case 'v':
	    verbose++;
	    break;
	case OPT_SEED:
	    assert(*optarg >= '0' && *optarg <= '9');
	    rpmBlobSeed = strtoull(optarg, NULL, 0);
	    break;
	case OPT_FAST:
	    fast = 1;
	    break;
	default:
	    fprintf(stderr, "Usage: %s <pkglist >dict\n", argv[0]);
	    return 2;
	}
    }

    load();

    ZDICT_cover_params_t params = {
	.d = d, .k = k,
	.nbThreads = 2,
	.zParams.compressionLevel = level,
	.zParams.notificationLevel = verbose,
    };
    ZDICT_fastCover_params_t fastParams = {
	.d = d, .k = k,
	.nbThreads = 2,
	.zParams.compressionLevel = level,
	.zParams.notificationLevel = verbose,
	.splitPoint = 1.0,
    };
    char dict[512<<10];
    size_t dictSize;
    if (fast) {
	if (d && k)
	    dictSize = ZDICT_trainFromBuffer_fastCover(dict, sizeof dict,
		samples.samplesBuffer, samples.samplesSizes, samples.nbSamples, fastParams);
	else {
	    dictSize = ZDICT_optimizeTrainFromBuffer_fastCover(dict, sizeof dict,
		samples.samplesBuffer, samples.samplesSizes, samples.nbSamples, &fastParams);
	    fprintf(stderr, "best parameters: d=%u k=%u\n", fastParams.d, fastParams.k);
	}
	assert(dictSize >= (64<<10) && dictSize <= sizeof dict);
    }
    else {
	if (d && k)
	    dictSize = ZDICT_trainFromBuffer_cover(dict, sizeof dict,
		samples.samplesBuffer, samples.samplesSizes, samples.nbSamples, params);
	else {
	    dictSize = ZDICT_optimizeTrainFromBuffer_cover(dict, sizeof dict,
		samples.samplesBuffer, samples.samplesSizes, samples.nbSamples, &params);
	    fprintf(stderr, "best parameters: d=%u k=%u\n", params.d, params.k);
	}
	assert(dictSize == sizeof dict);
    }

    if (!xwrite(1, dict, dictSize))
	assert(!!!"write failed");
    return 0;
}
