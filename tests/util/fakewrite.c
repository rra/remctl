/*
 * Fake write and writev functions for testing xwrite and xwritev.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2000-2002, 2004, 2017, 2023 Russ Allbery <eagle@eyrie.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>
#include <portable/uio.h>

#include <errno.h>

#include <tests/util/fakewrite.h>
#include <util/macros.h>


/*
 * All the data is actually written into this buffer.  We use write_offset
 * to track how far we've written.
 */
char write_buffer[256];
size_t write_offset = 0;

/*
 * If write_interrupt is non-zero, then half of the calls to write or writev
 * will fail, returning -1 with errno set to EINTR.
 */
int write_interrupt = false;

/*
 * If write_fail is true, all writes or writevs will return 0, indicating no
 * progress in writing out the buffer.
 */
bool write_fail = false;


/*
 * Accept a write request and write only the first 32 bytes of it into
 * write_buffer (or as much as will fit), returning the amount written.
 */
ssize_t
fake_write(int fd UNUSED, const void *data, size_t n)
{
    size_t total, left;

    if (write_fail)
        return 0;
    if (write_interrupt && (write_interrupt++ % 2) == 0) {
        errno = EINTR;
        return -1;
    }
    if (write_offset >= sizeof(write_buffer)) {
        errno = ENOSPC;
        return 0;
    }
    left = sizeof(write_buffer) - write_offset;
    if (left > 32)
        left = 32;
    total = (n < left) ? n : left;
    memcpy(write_buffer + write_offset, data, total);
    write_offset += total;
    return total;
}


/*
 * Accept a pwrite request and write only the first 32 bytes of it into
 * write_buffer at the specified offset (or as much as will fit), returning
 * the amount written.
 */
ssize_t
fake_pwrite(int fd UNUSED, const void *data, size_t n, off_t offset)
{
    size_t total, left;

    if (write_fail)
        return 0;
    if (write_interrupt && (write_interrupt++ % 2) == 0) {
        errno = EINTR;
        return -1;
    }
    if (offset >= (ssize_t) sizeof(write_buffer)) {
        errno = ENOSPC;
        return -1;
    }
    left = sizeof(write_buffer) - offset;
    if (left > 32)
        left = 32;
    total = (n < left) ? n : left;
    memcpy(write_buffer + offset, data, total);
    return total;
}


/*
 * Accept an xwrite request and write only the first 32 bytes of it into
 * write_buffer (or as much as will fit), returning the amount written.
 */
ssize_t
fake_writev(int fd UNUSED, const struct iovec *iov, int iovcnt)
{
    int i;
    size_t left, n, total;

    if (write_fail)
        return 0;
    if (write_interrupt && (write_interrupt++ % 2) == 0) {
        errno = EINTR;
        return -1;
    }
    left = sizeof(write_buffer) - write_offset;
    if (left > 32)
        left = 32;
    total = 0;
    for (i = 0; i < iovcnt && left != 0; i++) {
        n = ((size_t) iov[i].iov_len < left) ? (size_t) iov[i].iov_len : left;
        memcpy(write_buffer + write_offset, iov[i].iov_base, n);
        write_offset += n;
        total += n;
        left -= n;
    }
    return total;
}
