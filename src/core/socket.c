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

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

/**
 * makes the given socket non-blocking.
 */
static void _makeNonBlocking(int fd)
{
	/* enable non-blocking mode */
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

/**
 * tells the given socket to reuse the address it will be bound to later.
 */
static void _reuseAddr(int fd)
{
	int yes = 1;

	/* let the socket reuse the address it is about to bind to */
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*) &yes, sizeof(yes));
}

/**
 * creates a new server socket descriptor and returns it. returns the new socket
 * descriptor (value >= 0) or -1 in case of an error.
 */
int socketOpenServer(const char *host, const char *port)
{
	/* todo: function to big, should not be more than 50 lines */

	int fd = -1, res;
	struct addrinfo addrInfoHints = {0}, *addrInfo, *curInfo;

	/* set the necessary hints for address resolution */
	addrInfoHints.ai_family = AF_UNSPEC;
	addrInfoHints.ai_socktype = SOCK_STREAM;
	addrInfoHints.ai_flags = AI_PASSIVE;

	/* resolve the specified host and port */
	if((res = getaddrinfo(host, port, &addrInfoHints, &addrInfo)) == 0)
	{
		/* go through the entire info list. use the first info record where it
		 * is possible to bind to */
		for(curInfo=addrInfo;curInfo!=NULL;curInfo=curInfo->ai_next)
		{
			/* create a new socket */
			fd = socket(
				curInfo->ai_family, curInfo->ai_socktype, curInfo->ai_protocol
			);

			/* is there a valid socket */
			if(fd < 0)
			{
				/* socket() failed, make a panic message */
				logWrite("ERROR socket()");
				logWrite(strerror(errno));

				/* not a valid socket, try the next info record */
				continue;
			}

			/* let the socket reuse the address it is about to bind to */
			_reuseAddr(fd);

			/* bind to the address */
			if(bind(fd, curInfo->ai_addr, curInfo->ai_addrlen) < 0)
			{
				/* bind() failed, make a panic message */
				logWrite("ERROR bind()");
				logWrite(strerror(errno));

				/* bind did not work, close the socket and try the next info
				 * record */
				close(fd);

				continue;
			}

			/* socket() and bind() were successfull */
			break;
		}

		/* free the address info */
		freeaddrinfo(addrInfo);

		/* is there a valid and bound socket */
		if(curInfo != NULL)
		{
			/* make the socket non-blocking */
			_makeNonBlocking(fd);

			/* convert the socket to a listening socket */
			if(listen(fd, SOMAXCONN) == 0)
			{
				return fd;
			}

			/* listen() failed, make a panic message */
			logWrite("ERROR listen()");
			logWrite(strerror(errno));

			/* close the socket if it was not possible to convert it to
			 * listening socket */
			close(fd);
		}
		else
		{
			/* none of the results were usable */
			logWrite("ERROR getaddrinfo(): returned unusable results");
		}
	}
	else
	{
		/* getaddrinfo() failed, log the error */
		logWrite("ERROR getaddrinfo()");
		logWrite(gai_strerror(res));
	}

	return INVALID_SOCKET;
}

/**
 * accepts a new client connection on the given server socket. returns either
 * the new socket descriptor (value >= 0) or -1 (INVALID_SOCKET) in case of
 * error.
 */
int socketAccept(int fd)
{
	/* accept the new connection */
	int newFd = accept(fd, NULL, NULL);

	/* is there a valid socket */
	if(newFd >= 0)
	{
		/* make the socket non-blocking */
		_makeNonBlocking(newFd);

		return newFd;
	}
	/* EAGAIN indicates that another process accepted the client connection
	 * first. return a socket value so high that it would never be used.
	 * this was a possible case when it was possible to create multiple
	 * processes. */
	else if(errno == EAGAIN)
	{
		return SOCKET_MAX;
	}

	/* failed to accept a new connection, log the error */
	logWrite("ERROR accept()");
	logWrite(strerror(errno));

	/* something went wrong */
	return INVALID_SOCKET;
}

/**
 * reads data from the socket and stores it in the given buffer. returns 1 if
 * data was read and 0 if not. a return value of 0 can be either an error or EOF
 * was encountered.
 */
int socketRead(int fd, buf_t *buf)
{
	static unsigned char tmpBuf[IO_BUF_SIZE];

	/* read the data (peek it first; the data will not be removed from the
	 * system buffer) */
	ssize_t bytesRead = recv(fd, tmpBuf, sizeof(tmpBuf), MSG_PEEK);

	/* is there any data */
	if(bytesRead > 0)
	{
		/* append the read data to the buffer */
		if(bufAppend(buf, tmpBuf, bytesRead))
		{
			/* remove the previously peeked data from the socket */
			recv(fd, tmpBuf, bytesRead, 0);

			return 1;
		}
	}
	/* is there an error */
	else if(bytesRead < 0)
	{
		/* failed to receive any data, make a panic message */
		logWrite("ERROR recv()");
		logWrite(strerror(errno));
	}

	/* no data or an error occured */

	return 0;
}

/**
 * writes the data stored in the buffer into the specified socket. returns 1 if
 * that was possible and 0 if not. it also returns 1 if no data was stored in
 * the buffer.
 */
int socketWrite(int fd, buf_t *buf)
{
	void *data;
	size_t len;
	ssize_t bytesWritten;

	/* is there data stored in the buffer */
	if(bufHasData(buf))
	{
		/* extract the data from the buffer */
		data = bufExtract(buf, &len);

		/* write the data to the socket */
		bytesWritten = send(fd, data, len, 0);

		/* did an error occur */
		if(bytesWritten < 0)
		{
			/* failed to send any data, make a panic message */
			logWrite("ERROR send()");
			logWrite(strerror(errno));

			bytesWritten = 0;
		}

		/* was everything written to the socket */
		if(((size_t) bytesWritten) < len)
		{
			/* put the part that was not written to the socket back into
			 * the buffer */
			bufAppend(
				buf,
				(void*) (((char*) data) + bytesWritten),
				len - ((size_t) bytesWritten)
			);
		}

		/* free the data */
		free(data);

		/* if no data was written to the socket, return a failure code */
		if(bytesWritten == 0)
		{
			return 0;
		}
	}

	return 1;
}

/**
 * closes the specified socket.
 */
void socketClose(int fd)
{
	/* close the socket */
	close(fd);
}
