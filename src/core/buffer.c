/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 0x6d72
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "server.h"

#include <string.h>
#include <stdlib.h>

/**
 * used to check whether the given buffer is valid.
 */
#define _checkBuf(b) if((b) == NULL) return
#define _checkBufRet(b, r) if((b) == NULL) return (r)

/**
 * aligns the given buffer size to a multiple of BUFFER_SIZE.
 */
#define _alignBufSize(s) (((s) - ((s) % IO_BUF_SIZE)) \
		+ (((s) % IO_BUF_SIZE > 0) ? IO_BUF_SIZE : 0))

/**
 * resets the buffer to its initial values.
 */
#define _resetBuf(b) ((b)->data = NULL, (b)->len = 0, (b)->size = 0)

/**
 * default allocator function.
 */
static void* _defaultAlloc(void *ptr, size_t newSize)
{
	return realloc(ptr, newSize);
}

/**
 * stores the allocator function.
 */
static bufAlloc_t _alloc = _defaultAlloc;

/**
 * appends the given data to the specified buffer. returns 1 if this operation
 * succeeded or 0 if not.
 */
int bufAppend(buf_t *buf, const void *data, size_t len)
{
	void *newData;
	size_t newLen, newSize;

	/* validate the buffer */
	_checkBufRet(buf, 0);

	/* is there data to append */
	if(data != NULL && len > 0)
	{
		newLen = buf->len + len;
		newSize = newLen;

		/* align the new buffer size */
		newSize = _alignBufSize(newSize);

		/* is it necessary to reallocate memory for the buffer */
		if(newSize > buf->size)
		{
			/* reallocate memory for the new buffer */
			newData = _alloc(buf->data, newSize);

			/* the buffer could not be reallocated */
			if(newData == NULL)
			{
				return 0;
			}

			/* set the pointer to the data */
			buf->data = newData;

			/* set the new size of the buffer */
			buf->size = newSize;
		}

		/* copy the data into the buffer */
		memcpy((void*) (((unsigned char*) buf->data) + buf->len), data, len);

		/* store the new length of the data */
		buf->len = newLen;
	}

	return 1;
}

/**
 * returns the data of the given buffer without removing it from the buffer. the
 * pointer returned must not be free()ed manually.
 */
void *bufPeek(buf_t *buf, size_t *lenDest)
{
	/* validate the buffer */
	_checkBufRet(buf, NULL);

	/* store the length in the given destination */
	*lenDest = buf->len;

	/* return the data of the buffer */
	return buf->data;
}

/**
 * returns the data of the given buffer and resets its data. the size of the
 * data is stored in the second parameter. the data pointed to by the return
 * value must be freed manually with free(). this function may return null if
 * no data is present, the second parameter is left untouched.
 */
void* bufExtract(buf_t *buf, size_t *lenDest)
{
	void *data;

	/* validate the buffer */
	_checkBufRet(buf, NULL);

	data = buf->data;

	/* copy the length of the buffer */
	*lenDest = buf->len;

	/* reset the buffer */
	_resetBuf(buf);

	return data;
}

/**
 * checks whether the given buffer contains data or not. returns 1 if the buffer
 * contains data and 0 if not.
 */
int bufHasData(buf_t *buf)
{
	/* validate the buffer */
	_checkBufRet(buf, 0);

	return buf->data != NULL && buf->len > 0;
}

/**
 * clears the data of the given buffer.
 */
void bufClear(buf_t *buf)
{
	/* validate the buffer */
	_checkBuf(buf);

	/* is there a valid storage location */
	if(buf->data != NULL)
	{
		/* free the storage */
		free(buf->data);
	}

	/* reset the buffer */
	_resetBuf(buf);
}

/**
 * set a new allocator function used for memory allocation. this should be done
 * before any buffer is filled with data.
 */
void bufSetAlloc(bufAlloc_t alloc)
{
	/* there must always be a allocator function */
	if(alloc != NULL)
	{
		_alloc = alloc;
	}
}
