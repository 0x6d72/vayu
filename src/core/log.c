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

#include <stdio.h>

/**
 * default log callback, which writes the message to stdout.
 */
static void _logStdout(const char *msg)
{
	/* write the message to stdout */
	(void) puts(msg);
}

/**
 * stores the callback function.
 */
static logCallback_t _callback = _logStdout;

/**
 * overwrites the current log callback function. NULL disables the log
 * completely.
 */
void logSetCallback(logCallback_t callback)
{
	/* overwrite the callback with the new function */
	_callback = callback;
}

/**
 * writes the given message to the log.
 */
void logWrite(const char* msg)
{
	/* is there a valid callback */
	if(_callback != NULL)
	{
		/* log the message with the given callback function */
		_callback(msg);
	}
}
