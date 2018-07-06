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

struct srpmBlob {
    void *blob;
    size_t blobSize;
    const char *pkgname;
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
    return true;
}

int main()
{
    struct srpmBlob b;
    while (read1(&b)) {
	if (!xwrite(1, b.blob, b.blobSize))
	    assert(!!!"write failed");
	free(b.blob);
    }
    return 0;
}
