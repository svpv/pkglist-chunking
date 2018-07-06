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
    struct srpm stack[8];
    size_t nstack = 0;

    void Pop(size_t n)
    {
	assert(n > 0);
	assert(nstack >= n);
	for (size_t i = 0; i < n - 1; i++) {
	    char *s = stack[i].srpm;
	    fputs(s, stdout);
	    free(s);
	    putchar(' ');
	}
	char *s = stack[n-1].srpm;
	puts(s);
	free(s);
	nstack -= n;
	memmove(stack, stack + n, nstack * sizeof stack[0]);
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

    bool Funny2(void)
    {
	return stack[0].hash > stack[1].hash;
    }

    while ((stack[nstack].srpm = read1()) != NULL) {
	stack[nstack].hash = srpmHash(stack[nstack].srpm);
	nstack++;
	switch (nstack) {
	case 1:
	    break;
	case 2:
	    if (Funny2())
		Pop(2);
	    break;
	case 3:
	    Pop(3);
	    break;
	default:
	    assert(!"possible");
	}
    }
    if (nstack)
	Pop(nstack);

    return 0;
}
