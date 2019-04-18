#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

void shuffle(void **a, size_t n)
{
    static uint64_t rnd = 0xe220a8397b1dcdaf;
    for (size_t i = n - 1; i > 0; i--) {
	size_t j = rnd % (i + 1);
	rnd = rnd * 0x5851f42d4c957f2d + 0x14057b7ef767814f;
	void *tmp = a[i];
	a[i] = a[j];
	a[j] = tmp;
    }
}

#include <zpkglist.h>
#include <arpa/inet.h>

size_t calcBlobSize(struct HeaderBlob *blob)
{
    return 8 + 16 * ntohl(blob->il) + ntohl(blob->dl);
}

void **read_headers(int fd, size_t *np)
{
    struct zpkglistReader *z;
    const char *err[2];
    int rc = zpkglistFdopen(&z, fd, err);
    assert(rc > 0);

    void **a = NULL;
    size_t n = 0;
    while (1) {
	struct HeaderBlob *blob;
	ssize_t blobSize = zpkglistNextMalloc(z, &blob, NULL, err);
	if (blobSize == 0)
	    break;
	assert(blobSize > 0);
	assert(blobSize == calcBlobSize(blob));
	if ((n & (n - 1)) == 0)
	    a = realloc(a, 1 + 2 * n * sizeof a[0]), assert(a);
	a[n++] = blob;
    }

    return *np = n, a;
}

static void write_magic(FILE *fp)
{
    static const unsigned char magic[8] = {
	0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00
    };
    size_t ret = fwrite(magic, 1, 8, fp);
    assert(ret == 8);
}

static void write_blob(void *blob, FILE *fp)
{
    size_t size = calcBlobSize(blob);
    assert(size > 0), assert(size < (256<<20));
    size_t ret = fwrite(blob, 1, size, fp);
    assert(ret == size);
}

void write_headers(void **a, size_t n, FILE *fp)
{
    for (size_t i = 0; i < n; i++) {
	write_magic(fp);
	write_blob(a[i], fp);
    }
    int rc = fflush(fp);
    assert(rc == 0);
}

int main()
{
    size_t n;
    void **a = read_headers(0, &n);
    shuffle(a, n);
    write_headers(a, n, stdout);
    return 0;
}
