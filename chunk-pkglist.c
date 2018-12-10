#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/auxv.h>
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

int main(int argc, char **argv)
{
    struct rpm {
	char *rpm;
	char *srpm;
	uint64_t nameHash;
    };
    struct rpm q[8];
    size_t nq = 0;

    void Pop(size_t n)
    {
	assert(n > 0);
	assert(nq >= n);
	for (size_t i = 0; i < n - 1; i++) {
	    char *s = q[i].rpm;
	    fputs(s, stdout);
	    free(s);
	    putchar(' ');
	}
	char *s = q[n-1].rpm;
	puts(s);
	free(s);
	nq -= n;
	memmove(q, q + n, nq * sizeof q[0]);
    }

    int ha = 0;
    int opt;
    while ((opt = getopt(argc, argv, "h:")) != -1)
	switch (opt) {
	case 'h':
	    ha = atoi(optarg);
	    assert(ha || *optarg == '0');
	    break;
	default:
	    assert(!!!"unknown option");
	}

    uint64_t seed = 0xe220a8397b1dcdaf;
    for (int i = 0; i < ha; i++)
	seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;

    uint64_t nameHash(char *srpm)
    {
	char *dash1 = strrchr(srpm, '-');
	assert(dash1);
	*dash1 = '\0';
	char *dash2 = strrchr(srpm, '-');
	assert(dash2);
	uint64_t h = t1ha2_atonce(srpm, dash2 - srpm, seed);
	*dash1 = '-';
	return h;
    }

    while ((q[nq].rpm = read1()) != NULL) {
	char *tab = strchr(q[nq].rpm, '\t'); assert(tab);
	*tab = '\0'; q[nq].srpm = tab + 1;
	q[nq].nameHash = nameHash(q[nq].srpm);
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

    return 0;
}
