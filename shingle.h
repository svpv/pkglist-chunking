#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "qsort.h"

struct shingles {
    size_t n;
    uint32_t hh[];
};

static struct shingles *shingle(const char *data, size_t size)
{
    struct shingles *shi = malloc(sizeof(size_t) + size / 2 * sizeof(uint32_t));
    assert(shi);
    if (size < 8) {
	shi->n = 0;
	return shi;
    }

    size_t n = 0;
    uint32_t *hh = shi->hh;
    for (size_t i = 0; i <= size - 8; i += 2) {
	uint64_t x;
	memcpy(&x, &data[i], sizeof x);
	uint64_t h = x * UINT64_C(0xEC99BF0D8372CAAB) >> 31;
	if (h <= UINT32_MAX)
	    hh[n++] = h;
	i += h % 3;
    }

    uint32_t tmp;
#define SHI_LESS(i, j) hh[i] < hh[j]
#define SHI_SWAP(i, j) tmp = hh[i], hh[i] = hh[j], hh[j] = tmp
    QSORT(n, SHI_LESS, SHI_SWAP);

    // uniq
    size_t i = 0, j = 0;
    hh[j++] = hh[i++];
    for (; i < n; i++)
	if (hh[i] != hh[i-1])
	    hh[j++] = hh[i];
    shi->n = j;
    return shi;
}

size_t shimilar(const struct shingles *shi1, const struct shingles *shi2)
{
    size_t n1 = shi1->n, n2 = shi2->n;
    if (n1 == 0 || n2 == 0)
	return 0;
    size_t i1 = 0, i2 = 0;
    const uint32_t *hh1 = shi1->hh, *hh2 = shi2->hh;
    size_t cnt = 0;
    while (1) {
	if (hh1[i1] < hh2[i2]) {
	    i1++;
	    if (i1 == n1) break;
	}
	else if (hh1[i1] > hh2[i2]) {
	    i2++;
	    if (i2 == n2) break;
	}
	else {
	    cnt++, i1++, i2++;
	    if (i1 == n1) break;
	    if (i2 == n2) break;
	}
    }
    return cnt;
}
