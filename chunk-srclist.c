#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <t1ha.h>

static char *read1(void)
{
    char *line = NULL;
    size_t alloc_size = 0;
    ssize_t len;
    errno = 0;
    while ((len = getline(&line, &alloc_size, stdin)) >= 0) {
	if (len > 0 && line[len-1] == '\n')
	    line[--len] = '\0';
	if (len)
	    break;
    }
    if (len < 0) {
	assert(errno == 0);
	return NULL;
    }
    assert(len > 0);
    return line;
}

int main()
{
    struct srpm {
	char *srpm;
	uint64_t hash;
    };
    struct srpm q[8];
    size_t nq = 0;

    void Pop(size_t n)
    {
	assert(n > 0);
	assert(nq >= n);
	for (size_t i = 0; i < n - 1; i++) {
	    char *s = q[i].srpm;
	    fputs(s, stdout);
	    free(s);
	    putchar(' ');
	}
	char *s = q[n-1].srpm;
	puts(s);
	free(s);
	nq -= n;
	memmove(q, q + n, nq * sizeof q[0]);
    }

    uint64_t srpmHash(char *srpm)
    {
	char *dash1 = strrchr(srpm, '-');
	assert(dash1);
	*dash1 = '\0';
	char *dash2 = strrchr(srpm, '-');
	assert(dash2);
	uint64_t h = t1ha(srpm, dash2 - srpm, 0);
	*dash1 = '-';
	return h;
    }

    while ((q[nq].srpm = read1()) != NULL) {
	q[nq].hash = srpmHash(q[nq].srpm);
	nq++;
	switch (nq) {
	case 1:
	    break;
	case 2:
	case 3:
	    if (q[nq-1].hash < q[nq-2].hash)
		break;
	case 4:
	    Pop(nq);
	    break;
	default:
	    assert(!"possible");
	}
    }
    if (nq)
	Pop(nq);

    return 0;
}
