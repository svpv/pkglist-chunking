// Copyright (c) 2018, 2019 Alexey Tourbin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "chunker.h"

struct chunker {
    unsigned nq;
    uint64_t q[16];
    enum {
	ST_0, ST_A, ST_AB, ST_ABC, ST_ABCD, ST_ABCDE,
	ST_A2, ST_A3, ST_A4, ST_A5, ST_A6, ST_A7, ST_A8, ST_A9,
	ST_A2B, ST_A3B, ST_A4B,
	ST_A2BC, ST_A3BC, ST_A4BC,
    } st;
    unsigned ret0;
};

struct chunker *chunker_new(void)
{
    struct chunker *C = malloc(sizeof *C);
    if (!C)
	return NULL;
    C->nq = 0;
    C->st = ST_0;
    C->ret0 = 0;
    return C;
}

void chunker_free(struct chunker *C)
{
    free(C);
}

static inline unsigned chunker_ret(struct chunker *C, unsigned ret0, unsigned ret)
{
    if (ret0) {
	assert(C->ret0 == 0);
	assert(ret);
	C->ret0 = ret;
	ret = ret0;
    }
    if (C->ret0) {
	ret0 = C->ret0;
	C->ret0 = ret;
	ret = ret0;
    }
    return ret;
}

unsigned chunker_add(struct chunker *C, uint64_t nameHash)
{
    unsigned ret0 = 0, ret = 0;
    int eq = C->nq ? C->q[C->nq-1] == nameHash : 0;
    C->q[C->nq++] = nameHash;

    switch (C->st) {
    case ST_0: C->st = ST_A; break;
    case ST_A: if (eq) C->st = ST_A2; else C->st = ST_AB; break;

    case ST_A2: if (eq) C->st = ST_A3; else C->st = ST_A2B; break;
    case ST_A3: if (eq) C->st = ST_A4; else C->st = ST_A3B; break;
    case ST_A4: if (eq) C->st = ST_A5; else C->st = ST_A4B; break;
    case ST_A5: if (eq) C->st = ST_A6; else ret = 5, C->st = ST_A; break;
    case ST_A6: if (eq) C->st = ST_A7; else ret = 6, C->st = ST_A; break;
    case ST_A7: if (eq) C->st = ST_A8; else ret = 7, C->st = ST_A; break;
    case ST_A8: if (eq) C->st = ST_A9; else ret = 8, C->st = ST_A; break;

    case ST_A9: if (eq) ret = 8, C->st = ST_A2; else ret = 7, C->st = ST_A2B; break;

    case ST_A2B: if (eq) ret = 2, C->st = ST_A2; else C->st = ST_A2BC; break;
    case ST_A3B: if (eq) ret = 3, C->st = ST_A2; else C->st = ST_A3BC; break;
    case ST_A4B: if (eq) ret = 4, C->st = ST_A2; else C->st = ST_A4BC; break;

    case ST_A2BC: if (eq) ret = 3, C->st = ST_A2; else ret = 2, C->st = ST_ABC; break;
    case ST_A3BC: if (eq) ret = 4, C->st = ST_A2; else ret = 3, C->st = ST_ABC; break;
    case ST_A4BC: if (eq) ret = 5, C->st = ST_A2; else ret = 4, C->st = ST_ABC; break;

    case ST_AB: if (eq) ret = 1, C->st = ST_A2; else C->st = ST_ABC; break;
    case ST_ABC: if (eq) ret = 2, C->st = ST_A2; else C->st = ST_ABCD; break;

    case ST_ABCD:
	if (eq)
	    ret = 3, C->st = ST_A2;
	else if (C->q[1] > C->q[2]) // B > C
	    ret = 2, C->st = ST_ABC;
	else
	    C->st = ST_ABCDE;
	break;

    case ST_ABCDE:
	if (eq)
	    ret0 = 2, ret = 2, C->st = ST_A2;
	else if (C->q[2] > C->q[3]) // C > D
	    ret = 3, C->st = ST_ABC;
	else
	    ret = 2, C->st = ST_ABCD;
	break;
    }

    assert(ret0 + ret <= C->nq);
    C->nq -= ret0 + ret;
    memmove(C->q, C->q + ret0 + ret, C->nq * sizeof C->q[0]);

    return chunker_ret(C, ret0, ret);
}

unsigned chunker_flush(struct chunker *C)
{
    unsigned ret0 = 0, ret = 0;

    switch (C->st) {
    case ST_0: break;
    case ST_A: ret = 1; break;

    case ST_A2: ret = 2; break;
    case ST_A3: ret = 3; break;
    case ST_A4: ret = 4; break;
    case ST_A5: ret = 5; break;
    case ST_A6: ret = 6; break;
    case ST_A7: ret = 7; break;
    case ST_A8: ret = 8; break;
    case ST_A9: ret0 = 7, ret = 2; break;

    case ST_A2B: ret = 3; break;
    case ST_A3B: ret = 4; break;
    case ST_A4B: ret = 5; break;

    case ST_A2BC: ret0 = 2, ret = 2; break;
    case ST_A3BC: ret0 = 3, ret = 2; break;
    case ST_A4BC: ret0 = 4, ret = 2; break;

    case ST_AB: ret = 2; break;
    case ST_ABC: ret = 3; break;
    case ST_ABCD: ret0 = 2, ret = 2; break;

    case ST_ABCDE:
	if (C->q[2] > C->q[3])
	    ret0 = 3, ret = 2;
	else
	    ret0 = 2, ret = 3;
	break;
    }

    assert(C->nq == ret0 + ret);
    C->nq = 0;
    C->st = ST_0;

    return chunker_ret(C, ret0, ret);
}
