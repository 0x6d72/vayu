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

#include "server.h"

#include <pwd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/**
 * defines the structure of a socket.
 */
typedef struct {

	/* two buffers one for input and one for output */
	buf_t iBuf, oBuf;

	/* used to check whether the socket should be kept alive. a value of 0
	 * means the socket should not be kept alive, every other value means keep
	 * it alive. */
	unsigned int keepAlive : 1;

	/* used to check whether the socket is a server or not */
	unsigned int isServer : 1;

} _socket_t;

/**
 * stores the callback used by the server.
 */
static callback_t _callback;

/**
 * the actual array of sockets.
 */
static _socket_t _sockets[SOCKET_MAX];

/**
 * the two sets sockets used for select().
 */
static fd_set _socketReadSet, _socketWriteSet;

/**
 * stores the highest descriptor of all active sockets.
 */
static int _highestSocket;

/**
 * invokes the callback function with the specified context data.
 */
static void _invokeCallback(
	event_t event, int sFd, int cFd, buf_t *iBuf, buf_t *oBuf
)
{
	/* context to use for the callback */
	static eventContext_t context;

	/* is there a valid callback for the specified type */
	if(_callback != NULL)
	{
		/* prepare the context */
		context.event = event;
		context.sFd = sFd;
		context.cFd = cFd;
		context.iBuf = iBuf;
		context.oBuf = oBuf;

		/* invoke the callback */
		_callback(&context);
	}
}

/**
 * checks whether the socket descriptor is valid or not. returns 1 if it is
 * valid and 0 if not.
 */
static int _isValidSocket(int fd)
{
	return fd > INVALID_SOCKET && fd < SOCKET_MAX;
}

/**
 * adds the given socket descriptor to the socket list and the read set.
 * returns 1 in case of success and 0 in case of error.
 */
static int _addSocket(int fd)
{
	_socket_t *socket;

	/* check whether the socket fd is valid */
	if(_isValidSocket(fd))
	{
		/* get the socket data */
		socket = _sockets + fd;

		/* by default a socket should be kept alive */
		socket->keepAlive = 1;

		/* by default it is not a server */
		socket->isServer = 0;

		/* reset the input and output buffer */
		bufClear(&(socket->iBuf));
		bufClear(&(socket->oBuf));

		/* add the socket to the read set */
		FD_SET(fd, &_socketReadSet);

		/* adjust the highest socket descriptor */
		if(fd > _highestSocket)
		{
			_highestSocket = fd;
		}

		return 1;
	}

	return 0;
}

/**
 * returns the highest descriptor of all active sockets. if there are no active
 * sockets it returns INVALID_SOCKET.
 */
static int _findHighestSocket(void)
{
	int i;

	/* go through all possible sockets */
	for(i=_highestSocket-1;i>=0;--i)
	{
		if(FD_ISSET(i, &_socketReadSet))
		{
			return i;
		}
	}

	return INVALID_SOCKET;
}

/**
 * removes the socket from the socket list and the read and write set. returns
 * 1 in case of success and 0 in case of error.
 */
static void _removeSocket(int fd)
{
	/* get the socket data */
	_socket_t *socket = _sockets + fd;

	/* invoke the callback for this socket */
	if(socket->isServer)
	{
		/* invoke the callback for server sockets */
		_invokeCallback(EVENT_SOCKET_CLOSE, fd, INVALID_SOCKET, NULL, NULL);
	}
	else
	{
		/* invoke the callback for server sockets */
		_invokeCallback(EVENT_SOCKET_CLOSE, INVALID_SOCKET, fd, NULL, NULL);
	}

	/* remove the socket data */
	socket->keepAlive = 0;
	socket->isServer = 0;

	/* clear the i/o buffers */
	bufClear(&(socket->iBuf));
	bufClear(&(socket->oBuf));

	/* remove the descriptor from all sets */
	FD_CLR(fd, &_socketReadSet);
	FD_CLR(fd, &_socketWriteSet);

	/* is this the socket with the highest descriptor */
	if(fd == _highestSocket)
	{
		/* if so find the next highest descriptor */
		_highestSocket = _findHighestSocket();
	}

	/* close the socket after it was removed */
	socketClose(fd);
}

/**
 * removes all sockets from the server.
 */
static void _removeAllSockets(void)
{
	int fd;

	/* go through all active sockets */
	for(fd=0;fd<=_highestSocket;++fd)
	{
		/* is this socket active */
		if(FD_ISSET(fd, &_socketReadSet))
		{
			/* remove the socket from the system */
			_removeSocket(fd);
		}
	}
}

/**
 * enables writing on the specified socket. returns 1 if it was possible and 0
 * if not.
 */
static void _enableSocketWrite(int fd)
{
	/* add the socket to the write set */
	FD_SET(fd, &_socketWriteSet);
}

/**
 * disables socket writing for the specified socket. for server sockets this has
 * no effect. returns 1 in case of success and 0 in case of error.
 */
static void _disableSocketWrite(int fd)
{
	/* remove the socket from the write set */
	FD_CLR(fd, &_socketWriteSet);
}

/**
 * used to evaluate the result of the callback function and to check whether the
 * client socket should be closed or not.
 */
static void _checkClientSocket(int cFd)
{
	/* if there is data to write put the socket in the write set */
	if(bufHasData(&(_sockets[cFd].oBuf)))
	{
		/* enable writing on this client */
		_enableSocketWrite(cFd);
	}
	/* should the socket be kept alive */
	else if(!_sockets[cFd].keepAlive)
	{
		/* it should not be kept alive, remove and close it then */
		_removeSocket(cFd);
	}
}

/**
 * accepts a new client connection and adds it to the system, it also invokes
 * the callback of the new socket. if it was possible to accept the connection
 * but not adding it to the system, the socket be closed silently.
 */
static void _handleServerInput(int sFd)
{
	/* accept a new connection */
	int cFd = socketAccept(sFd);

	/* is there a valid new socket descriptor */
	if(cFd >= 0)
	{
		/* add the new client connection */
		if(_addSocket(cFd))
		{
			/* invoke the callback of the new client socket */
			_invokeCallback(
				EVENT_SOCKET_ACCEPT,
				sFd,
				cFd,
				&(_sockets[cFd].iBuf),
				&(_sockets[cFd].oBuf)
			);

			/* check the client socket */
			_checkClientSocket(cFd);
		}
		else
		{
			/* it was not possible to the add the new client socket to the
			 * system, close it then */
			socketClose(cFd);
		}
	}
	else
	{
		/* it was not possible to accept a new connection, close the server
		 * socket then */
		_removeSocket(sFd);
	}
}

/**
 * reads data from the specified socket and stores it in the input buffer of the
 * socket. it also invokes the callback when there was data read. the socket
 * will be closed when EOF was read.
 */
static void _handleClientInput(int cFd)
{
	/* read data from the socket */
	if(socketRead(cFd, &(_sockets[cFd].iBuf)))
	{
		/* invoke the callback for this socket */
		_invokeCallback(
			EVENT_SOCKET_READ,
			INVALID_SOCKET,
			cFd,
			&(_sockets[cFd].iBuf),
			&(_sockets[cFd].oBuf)
		);

		/* check the client socket */
		_checkClientSocket(cFd);

		return;
	}

	/* there was no data read, close the socket then */
	_removeSocket(cFd);
}

/**
 * handles input on the specified socket. for servers a new connection will be
 * accepted and for clients data will be read and stored in the input buffers.
 *
 * this function will only be invoked when there really is data to read from the
 * socket or if there is a new connection to acceopt.
 */
static void _handleInput(int fd)
{
	/* is this a server or a client */
	if(_sockets[fd].isServer)
	{
		/* handle server input, this means accept a new connection */
		_handleServerInput(fd);
	}
	else
	{
		/* handle client input */
		_handleClientInput(fd);
	}
}

/**
 * writes data from the output buffer to the socket (the client). returns 1 if
 * data was sent and 0 if not.
 *
 * this function will only be invoked when there is data to write and when the
 * socket is ready to write.
 */
static void _handleOutput(int cFd)
{
	/* get the socket data */
	_socket_t *socket = _sockets + cFd;

	/* write the data from the output buffer to the socket */
	if(socketWrite(cFd, &(socket->oBuf)))
	{
		/* invoke the socket write callback */
		_invokeCallback(
			EVENT_SOCKET_WRITE,
			INVALID_SOCKET,
			cFd,
			&(socket->iBuf),
			&(socket->oBuf)
		);

		/* is there any data left in the buffer */
		if(!bufHasData(&(socket->oBuf)))
		{
			/* no data left in the buffer, disable writing on this socket */
			_disableSocketWrite(cFd);

			/* should the socket kept alive */
			if(!socket->keepAlive)
			{
				goto end;
			}
		}

		return;
	}

	end:

	/* something went wrong or the socket should not be kept alive, in either
	 * case close the socket */
	_removeSocket(cFd);
}

/**
 * prepares the server. this means all the internal structures are reset to its
 * initial values.
 */
void serverPrepare(void)
{
	/* reset the callback */
	_callback = NULL;

	/* reset the socket data */
	memset(_sockets, 0, sizeof(_sockets));

	/* clear the socket sets */
	FD_ZERO(&_socketReadSet);
	FD_ZERO(&_socketWriteSet);

	/* reset the highest socket */
	_highestSocket = INVALID_SOCKET;
}

/**
 * starts the server. this is basically the invocation of the start event.
 */
void serverStart(void)
{
	/* invoke the start event */
	_invokeCallback(EVENT_START, INVALID_SOCKET, INVALID_SOCKET, NULL, NULL);
}

/**
 * executes the server. returns 1 in case of success, 2 if there are no open
 * sockets and 0 in case of an error.
 */
int serverExec(void)
{
	static int result, fd;
	static struct timeval timeout;
	static fd_set readSet, writeSet;

	/* are there any sockets */
	if(_highestSocket < 0)
	{
		/* there are no sockets for select() */
		return 2;
	}

	/* set the default timeout for select */
	timeout.tv_sec = DEFAULT_IDLE_TIMEOUT;
	timeout.tv_usec = 0;

	/* use the global socket sets */
	readSet = _socketReadSet;
	writeSet = _socketWriteSet;

	/* wait for changes on the sockets */
	result = select(_highestSocket + 1, &readSet, &writeSet, NULL, &timeout);

	/* are there any sockets with changes */
	if(result > 0)
	{
		/* go thtough every possible */
		for(fd=0;fd<=_highestSocket;++fd)
		{
			/* is this socket ready for reading */
			if(FD_ISSET(fd, &readSet))
			{
				/* handle the socket read */
				_handleInput(fd);
			}

			/* is this socket ready for writing */
			if(FD_ISSET(fd, &writeSet))
			{
				/* handle the socket writing */
				_handleOutput(fd);
			}
		}
	}
	/* error or signal interrupt (which is displayed as an error) */
	else if(result < 0)
	{
		/* being interrupted by a signal is not considered an error */
		if(errno != EINTR)
		{
			/* select failed, log the error */
			error("select(): %s", strerror(errno));

			/* a real error did occur return the appropriate error code */
			return 0;
		}
	}
	/* nothing to do at the moment */
	else
	{
		/* invoke the idle callback */
		_invokeCallback(EVENT_IDLE, INVALID_SOCKET, INVALID_SOCKET, NULL, NULL);
	}

	return 1;
}

/**
 * stops the server. this means all sockets are closed and and the stop event
 * is fired.
 */
void serverStop(void)
{
	/* remove all sockets */
	_removeAllSockets();

	/* invoke the shutdown event */
	_invokeCallback(EVENT_STOP, INVALID_SOCKET, INVALID_SOCKET, NULL, NULL);
}

/**
 * used to set a callback for the server.
 */
void serverSetCallback(callback_t callback)
{
	/* overwrite the old callback function */
	_callback = callback;
}

/**
 * returns the callback currently in use.
 */
callback_t serverGetCallback(void)
{
	return _callback;
}

/**
 * adds a new server socket to the system. returns the socket descriptor if
 * everything is ok and INVALID_SOCKET if not.
 */
int serverAddServerSocket(const char *host, const char *port)
{
	/* create a new server socket descriptor */
	int fd = socketOpenServer(host, port);

	/* due to the fact that the new descriptor is unique it is sufficient
	 * to check the validity and not if there is a slot left in the socket
	 * list */
	if(_addSocket(fd))
	{
		/* mark this socket as a server */
		_sockets[fd].isServer = 1;

		return fd;
	}

	/* close the socket descriptor if there is one */
	if(fd >= 0)
	{
		/* close the socket */
		socketClose(fd);
	}

	return INVALID_SOCKET;
}

/**
 * used to close the given socket. this basically sets the keep-alive value of
 * the given socket to zero, which than leads to a closed socket. closing
 * sockets is only possible with client connections.
 */
void serverCloseSocket(int fd)
{
	/* is there a valid socket */
	if(_isValidSocket(fd) && !_sockets[fd].isServer)
	{
		/* set the keep-alive value to zero */
		_sockets[fd].keepAlive = 0;

		/* enable writing on the socket, so that it can be closed as soon as
		 * possible */
		_enableSocketWrite(fd);
	}
}

/**
 * this function is used to change the effective and real user and group id to
 * the specified user. the user is specified by his name. returns 1 if it was
 * possible to change the user and 0 if not.
 */
int serverChangeUser(const char *user)
{
	/* get the user data */
	struct passwd *rec = getpwnam(user);

	/* was a user found with that particular name */
	if(rec != NULL)
	{
		/* change the group id first */
		if(setgid(rec->pw_gid) == 0)
		{
			/* now try to change the user id */
			if(setuid(rec->pw_uid) == 0)
			{
				return 1;
			}
		}
	}

	return 0;
}

/**
 * turns the current process into a daemon process. returns 1 if that was
 * successfull and 0 if not. if this function returns 0 the process should be
 * terminated because something might have changed e.g. stdin, stdout and stderr
 * point to /dev/null.
 */
int serverDaemonize(void)
{
	pid_t id;

	/* clear the file creation mask */
	umask(0);

	/* close stdin, stdout and stderr */
	close(0);
	close(1);
	close(2);

	/* reopen stdin with /dev/null */
	if(open("/dev/null", O_RDWR) == 0)
	{
		/* now reopen stdout and stderr by duplicating stdin */
		dup(0);
		dup(0);

		/* change the current working directory to / */
		if(chdir("/") == 0)
		{
			/* create a new process  */
			id = fork();

			/* is this the new process */
			if(id == 0)
			{
				/* make this process the session leader */
				setsid();

				return 1;
			}
			else if(id > 0)
			{
				/* just close the parent process */
				exit(0);
			}
		}
	}

	return 0;
}

/**
 * this function is used to establish a root jail for the current process.
 * returns 1 if the jail was successfully set up and 0 if not.
 */
int serverJail(const char *dir)
{
	/* create the root jail */
	return chroot(dir) == 0 ? 1 : 0;
}

/**
 * shuts the server down. this function literally does nothing.
 */
void serverShutdown(void)
{
}
