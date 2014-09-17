/**
 * Copyright (c) 2014 Markus Richter <markus.richter-os@web.de>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __SERVER_H
#define __SERVER_H

#include <stddef.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/select.h>

/**
 * defines the maximum number of sockets that can be active concurrently. this
 * value must never exceed FD_SETSIZE.
 */
#define SOCKET_MAX FD_SETSIZE

/**
 * defines the value of an invalid socket.
 */
#define INVALID_SOCKET (-1)

/**
 * defines the default idle timeout in seconds.
 */
#ifndef DEFAULT_IDLE_TIMEOUT
#define DEFAULT_IDLE_TIMEOUT (1)
#endif

/**
 * defines the default size of input/output buffers.
 */
#ifndef IO_BUF_SIZE
#define IO_BUF_SIZE (1024)
#endif

/**
 * defines the possible exit codes for the server.
 */
typedef enum {

	/* everything ok, normal shutdown */
	EXIT_OK,

	/* used to indicate an error with the server itself */
	EXIT_ERROR_SERVER,

	/* no open connections, server can not work without connections */
	EXIT_ERROR_NO_CONNECTIONS,

	/* there was an error with the provider */
	EXIT_ERROR_PROVIDER

} exitCode_t;

/**
 * defines the function signature for the allocator function used for buffers.
 */
typedef void* (*bufAlloc_t)(void*, size_t);

/**
 * defines the structure of a buffer used mainly for socket i/o (but of course
 * they can be used for anything else).
 */
typedef struct {

	/* stores the actual data of the buffer */
	void* data;

	/* stores the length of the data stored in the buffer */
	size_t len;

	/* stores the actual size of the buffer, this is always greater or equal
	 * to len */
	size_t size;

} buf_t;

/**
 * defines the possible callback events.
 */
typedef enum {

	/* triggered when the server is starting up. for this event no fields of
	 * the context are used. */
	EVENT_START,

	/* this event is triggered when the server is being stopped. it does not
	 * use any of the context fields. */
	EVENT_STOP,

	/* this event is triggered when the server is doing nothing special at the
	 * moment. it does not use any of the context fields. */
	EVENT_IDLE,

	/* triggered when the server accepts new client connection. all fields of
	 * the context are filled with the appropriate data. */
	EVENT_SOCKET_ACCEPT,

	/* this event is triggered when there was data read on a socket. it is only
	 * used for client sockets. all fields of the context, except the sFd-field,
	 * are used. */
	EVENT_SOCKET_READ,

	/* this event is used when data was written on a socket. it uses all fields,
	 * except the sFd-field, of the context. */
	EVENT_SOCKET_WRITE,

	/* triggered when the server closes a socket. it uses only cFd and sFd
	 * fields of the context. */
	EVENT_SOCKET_CLOSE,

	/* this must always be the last in the enumeration. it is used to determine
	 * how many callback types exist. it is NOT used as an event. */
	EVENT_COUNT

} event_t;

/**
 * defines the structure of the context used by the callbacks.
 */
typedef struct {

	/* stores the type of the callback */
	event_t event;

	/* stores the server and client socket */
	int sFd, cFd;

	/* stores the i/o buffers of the client socket */
	buf_t *iBuf, *oBuf;

} eventContext_t;

/**
 * defines the signature of the callback. the return value depends on the
 * specific type of the callback.
 */
typedef void (*callback_t)(eventContext_t*);

/* --- buffer api ----------------------------------------------------------- */

/**
 * appends the given data to the specified buffer. returns 1 if this operation
 * succeeded or 0 if not.
 */
int bufAppend(buf_t* , const void*, size_t);

/**
 * returns the data of the given buffer without removing it from the buffer. the
 * pointer returned must not be free()ed manually.
 */
void *bufPeek(buf_t*, size_t*);

/**
 * returns the data of the given buffer and resets its data. the size of the
 * data is stored in the second parameter. the data pointed to by the return
 * value must be freed manually with free(). this function may return null if
 * no data is present, the second parameter is left untouched.
 */
void* bufExtract(buf_t*, size_t*);

/**
 * checks whether the given buffer contains data or not. returns 1 if the buffer
 * contains data and 0 if not.
 */
int bufHasData(buf_t*);

/**
 * clears the data of the given buffer.
 */
void bufClear(buf_t*);

/**
 * set a new allocator function used for memory allocation. this should be done
 * before any buffer is filled with data.
 */
void bufSetAlloc(bufAlloc_t);

/* --- socket api ----------------------------------------------------------- */

/**
 * creates a new server socket descriptor and returns it. returns the new socket
 * descriptor (value >= 0) or -1 in case of an error.
 */
int socketOpenServer(const char*, const char*);

/**
 * accepts a new client connection on the given server socket. returns either
 * the new socket descriptor (value >= 0) or -1 in case of error.
 */
int socketAccept(int);

/**
 * reads data from the socket and stores it in the given buffer. returns 1 if
 * data was read and 0 if not. a return value of 0 can be either an error or EOF
 * was encountered.
 */
int socketRead(int, buf_t*);

/**
 * writes the data stored in the buffer into the specified socket. returns 1 if
 * that was possible and 0 if not. it also returns 1 if no data was stored in
 * the buffer.
 */
int socketWrite(int, buf_t*);

/**
 * closes the specified socket.
 */
void socketClose(int);

/* --- server api ----------------------------------------------------------- */

/**
 * prepares the server. this means all the internal structures are reset to its
 * initial values.
 */
void serverPrepare(void);

/**
 * starts the server. this is basically the invocation of the start event.
 */
void serverStart(void);

/**
 * executes the server. returns 1 in case of success, 2 if there are no open
 * sockets and 0 in case of an error.
 */
int serverExec(void);

/**
 * stops the server. this means all sockets are closed and and the stop event
 * is fired.
 */
void serverStop(void);

/**
 * used to set a callback for the server.
 */
void serverSetCallback(callback_t);

/**
 * returns the callback currently in use.
 */
callback_t serverGetCallback(void);

/**
 * adds a new server socket to the system. returns the socket descriptor if
 * everything is ok and INVALID_SOCKET if not.
 */
int serverOpenSocket(const char*, const char*);

/**
 * used to close the given socket. this basically sets the keep-alive value of
 * the given socket to zero, which than leads to a closed socket. closing
 * sockets is only possible with client connections.
 */
void serverCloseSocket(int);

/**
 * changes the current working directory of the server process. returns 1 if it
 * was possible to change the directory and 0 if not.
 */
int serverChangeDir(const char*);

/**
 * this function is used to change the effective and real user and group id to
 * the specified user. the user is specified by his name. returns 1 if it was
 * possible to change the user and 0 if not.
 */
int serverChangeUser(const char*);

/**
 * this function is used to establish a root jail for the current process.
 * returns 1 if the jail was successfully set up and 0 if not.
 */
int serverJail(const char*);

/**
 * this is a combination function of serverChangeUser() and serverJail(). it is
 * necessary because these two functions won't work together. after a chroot()
 * the server can't read /etc/passwd to switch to the specified user. after
 * setuid() it is not possible to call chroot(). this function returns 1 if
 * everything was successfull and 0 if not.
 */
int serverChangeUserAndJail(const char*, const char*);

/**
 * turns the current process into a daemon process. returns 1 if that was
 * successfull and 0 if not. if this function returns 0 the process should be
 * terminated because something might have changed e.g. stdin, stdout and stderr
 * point to /dev/null.
 *
 * due to the fact that /dev/null will be used for stdin, stdout and stderr,
 * this function must be called before serverJail() or serverChangeUserAndJail()
 */
int serverDaemonize(void);

/**
 * shuts the server down. this function literally does nothing.
 */
void serverShutdown(void);

/* --- provider api --------------------------------------------------------- */

/**
 * prepares the provider. the provider is responsible for setting up network
 * connections and provide a callback for them.
 *
 * this function should return 1 when everything was successfully set up and 0
 * if not.
 */
int providerPrepare(int, char**);

/**
 * shuts the provider down. this function should reverse everything that
 * providerPrepare() did.
 */
void providerShutdown(void);

/* --- util api ------------------------------------------------------------- */

/**
 * this function is used to print error messages to stderr. it is used in the
 * same way printf() is used.
 */
void error(const char*, ...);

#endif
