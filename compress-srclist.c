#include <stdbool.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>

static ssize_t xread(int fd, void *buf, size_t size)
{
    assert(size);
    assert(size <= SSIZE_MAX);

    ssize_t total = 0;
    do {
	ssize_t ret = read(fd, buf, size);
	if (ret < 0) {
	    if (errno == EINTR)
		continue;
	    return -1;
	}
	if (ret == 0)
	    break;
	assert((size_t) ret <= size);
	buf = (char *) buf + ret;
	size -= ret;
	total += ret;
    } while (size);

    return total;
}

static bool xwrite(int fd, const void *buf, size_t size)
{
    assert(size);
    bool zero = false;
    do {
	ssize_t ret = write(fd, buf, size);
	if (ret < 0) {
	    if (errno == EINTR)
		continue;
	    return false;
	}
	if (ret == 0) {
	    if (zero) {
		// write keeps returning zero
		errno = EAGAIN;
		return false;
	    }
	    zero = true;
	    continue;
	}
	zero = false;
	assert(ret <= size);
	buf = (char *) buf + ret;
	size -= ret;
    } while (size);

    return true;
}

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <rpm/rpmtag.h>
#include <t1ha.h>

struct srpmBlob {
    void *blob;
    size_t blobSize;
    const char *pkgname;
    uint64_t hash;
};

static bool read1(struct srpmBlob *b)
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
    b->pkgname = &data[off];
    b->hash = t1ha(b->pkgname, strlen(b->pkgname), 0);
    return true;
}

#include <stdio.h>
#include <inttypes.h>
#include <zstd.h>

int main()
{
    struct srpmBlob stack[8];
    size_t nstack = 0;

    void Pop(size_t n)
    {
	assert(n > 0);
	assert(nstack >= n);

	size_t totalSize = stack[0].blobSize;
	for (size_t i = 1; i < n; i++)
	    totalSize += stack[i].blobSize;
	size_t pkgnameoff = stack[0].pkgname - (char *) stack[0].blob;
	stack[0].blob = realloc(stack[0].blob, totalSize + ZSTD_COMPRESSBOUND(totalSize));
	assert(stack[0].blob);
	stack[0].pkgname = stack[0].blob + pkgnameoff;

	void *p = stack[0].blob + stack[0].blobSize;
	for (int i = 1; i < n; i++)
	    p = mempcpy(p, stack[i].blob, stack[i].blobSize);
	assert(p - stack[0].blob == totalSize);

#define level 3
	size_t zsize = ZSTD_compress(p, ZSTD_COMPRESSBOUND(totalSize),
				     stack[0].blob, totalSize, level);
	assert(zsize > 0);
	assert(zsize <= ZSTD_COMPRESSBOUND(totalSize));

	if (!xwrite(1, p, zsize))
	    assert(!!!"write failed");

	uint64_t hi;
	uint64_t lo = t1ha2_atonce128(&hi, p, zsize, 0);
	fprintf(stderr, "%016" PRIx64 "%016" PRIx64 "\t%zu\t%s", lo, hi, zsize, stack[0].pkgname);
	for (size_t i = 1; i < n; i++) {
	    fprintf(stderr, " %s", stack[i].pkgname);
	    free(stack[i].blob);
	}
	fprintf(stderr, "\n");
	free(stack[0].blob);

	nstack -= n;
	memmove(stack, stack + n, nstack * sizeof stack[0]);
    }

#if 0
    // One header per chunk.
    while (read1(stack))
	Pop(++nstack);
#else
    // 2+ headers per chunk.
    while (read1(&stack[nstack])) {
	nstack++;
	switch (nstack) {
	case 1:
	    break;
	case 2:
	case 3:
	    if (stack[nstack-1].hash < stack[nstack-2].hash)
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
