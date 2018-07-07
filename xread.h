// Copyright (c) 2017 Alexey Tourbin
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

#pragma once
#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>

static ssize_t xread(int fd, void *buf, size_t size)
{
    assert(size > 0);
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
