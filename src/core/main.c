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

#include <signal.h>
#include <stdlib.h>
#include <string.h>

/**
 * stores the exit code of the program.
 */
static exitCode_t _exitCode = EXIT_OK;

/**
 * used to check whether the server should restart after it was shut down. a
 * value of 1 means the server should restart and a value of 0 means no restart.
 *
 * by default this value is set to 0.
 */
static int _restart = 0;

/**
 * used to check whether the server is active or not. a value of 1 means the
 * server is active and a value of 0 means the server is not active.
 *
 * the server is by default active.
 */
static int _active = 1;

/**
 * this function is used as a signal handler that only terminates the server.
 */
static void _termSignalHandler(int sigNo)
{
	/* shut down the server */
	_active = 0;

	/* if the server should be terminated a restart is definitely not wanted */
	_restart = 0;
}

/**
 * this function is used as a signal handler that performs a restart the server.
 */
static void _restartSignalHandler(int sigNo)
{
	/* shut down the server */
	_active = 0;

	/* restart the server after it was shut down */
	_restart = 1;
}

/**
 * sets up the signal handling stuff.
 */
static void _prepareSignals(void)
{
	/* register the signal handlers */
	signal(SIGTERM, _termSignalHandler);
	signal(SIGINT, _termSignalHandler);
	signal(SIGHUP, _termSignalHandler);
	signal(SIGUSR1, _restartSignalHandler);
	signal(SIGUSR2, _restartSignalHandler);

	/* ignore SIGCHLD, it is absolutely not needed */
	signal(SIGCHLD, SIG_IGN);
}

/**
 * prepares the server.
 */
static void _prepare(int argc, char **argv)
{
	/* prepare the signals */
	_prepareSignals();

	/* prepare the server */
	serverPrepare();

	/* prepare the lua system */
	if(!providerPrepare(argc, argv))
	{
		/* it was not possible to prepare the provider */
		_exitCode = EXIT_ERROR_PROVIDER;
	}

	/* everything is fine; this is necessary for restarts */
	_exitCode = EXIT_OK;
}

/**
 * executes the server.
 */
static void _exec(void)
{
	/* defines the exit codes for every return value of serverExec() */
	static const exitCode_t resultMapping[] = {
		EXIT_ERROR_SERVER,
		EXIT_OK,
		EXIT_ERROR_NO_CONNECTIONS
	};

	/* start the server */
	if(serverStart())
	{
		/* main loop */
		while(_active && (_exitCode = resultMapping[serverExec()]) == EXIT_OK);
	}

	/* stop the server */
	serverStop();
}

/**
 * shuts down the entire server. this basically means the all the sockets are
 * closed and the module shutdown-function is called.
 */
static void _shutdown(void)
{
	/* shutdown the module */
	providerShutdown();

	/* clean everything from the server */
	serverShutdown();
}

/**
 * main entry point for the application.
 */
int main(int argc, char **argv)
{
	/* prepare everything */
	_prepare(argc, argv);

	/* was everything successfully prepared */
	if(_exitCode == EXIT_OK)
	{
		do
		{
			/* by default the server is active and will not restart */
			_active = 1;
			_restart = 0;

			/* execute the server */
			_exec();
		}
		while(_restart);
	}

	/* shutdown the server */
	_shutdown();

	return _exitCode;
}
