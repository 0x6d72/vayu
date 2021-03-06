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

#include "../lua/lua.h"
#include "../lua/lauxlib.h"
#include "../lua/lualib.h"

#include <stdlib.h>
#include <string.h>

/**
 * defines the prefix used for all values stored in the registry.
 */
#define _SERVER_REGISTRY_PREFIX "_s."

/**
 * defines the index for the server callback in the lua registry.
 */
#define _SERVER_CALLBACK_INDEX _SERVER_REGISTRY_PREFIX "scb"

/**
 * defines the index for the context table in the lua registry.
 */
#define _CONTEXT_INDEX _SERVER_REGISTRY_PREFIX "ctx"

/**
 * defines the index for the log callback function in the lua registry.
 */
#define _LOG_CALLBACK_INDEX _SERVER_REGISTRY_PREFIX "lcb"

/**
 * defines the type name for all buffer objects.
 */
#define _BUF_TYPE_NAME _SERVER_REGISTRY_PREFIX "buf"

/**
 * stores the used lua state.
 */
static lua_State *_state;

/**
 * lua wrapper function for bufPeek().
 */
static int _luaBufPeek(lua_State *state)
{
	void *data;
	size_t len;

	/* get the buffer from the arguments */
	buf_t **bufPtr = luaL_checkudata(state, 1, _BUF_TYPE_NAME);

	/* does the buffer has data */
	if(bufHasData(*bufPtr))
	{
		/* extract the actual data from the buffer */
		data = bufPeek(*bufPtr, &len);

		/* push the actual data onto the stack */
		lua_pushlstring(state, data, len);
	}
	else
	{
		/* use nil instead of data if there is nothing */
		lua_pushnil(state);
	}

	return 1;
}

/**
 * lua wrapper function for bufExtract().
 */
static int _luaBufExtract(lua_State *state)
{
	void *data;
	size_t len;

	/* get the buffer from the arguments */
	buf_t **bufPtr = luaL_checkudata(state, 1, _BUF_TYPE_NAME);

	/* does the buffer has data */
	if(bufHasData(*bufPtr))
	{
		/* extract the actual data from the buffer */
		data = bufExtract(*bufPtr, &len);

		/* push the actual data onto the stack */
		lua_pushlstring(state, data, len);

		/* free the extracted data */
		free(data);
	}
	else
	{
		/* use nil instead of data if there is nothing in the buffer */
		lua_pushnil(state);
	}

	return 1;
}

/**
 * lua wrapper function for bufAppend().
 */
static int _luaBufAppend(lua_State *state)
{
	const void *data;
	size_t len;

	/* get the buffer from the function arguments */
	buf_t **bufPtr = luaL_checkudata(state, 1, _BUF_TYPE_NAME);

	/* get the data to append from the lua stack */
	data = luaL_checklstring(state, 2, &len);

	/* append the data to the buffer and push the result onto stack */
	lua_pushboolean(state, bufAppend(*bufPtr, data, len));

	return 1;
}

/**
 * lua wrapper function for bufClear().
 */
static int _luaBufClear(lua_State *state)
{
	/* get the buffer from the function arguments */
	buf_t **bufPtr = luaL_checkudata(state, 1, _BUF_TYPE_NAME);

	/* clear the buffer */
	bufClear(*bufPtr);

	return 0;
}

/**
 * lua wrapper function for bufHasData().
 */
static int _luaBufHasData(lua_State *state)
{
	/* get the buffer from the function arguments */
	buf_t **bufPtr = luaL_checkudata(state, 1, _BUF_TYPE_NAME);

	/* check whether the buffer has data and push the result onto the lua
	 * stack */
	lua_pushboolean(state, bufHasData(*bufPtr));

	return 1;
}

/**
 * pushes the given buffer onto the lua stack.
 */
static void _pushBuf(buf_t *buf)
{
	buf_t **bufPtr;

	/* is there a valid buffer */
	if(buf != NULL)
	{
		/* create a new user data object on the lua stack */
		bufPtr = (buf_t**) lua_newuserdata(_state, sizeof(buf_t*));

		/* set the pointer to the buffer */
		*bufPtr = buf;

		/* assign the metatable to the buffer object */
		luaL_setmetatable(_state, _BUF_TYPE_NAME);
	}
	else
	{
		/* if there is no valid buffer push nil onto the stack */
		lua_pushnil(_state);
	}
}

/**
 * registers the buffer api with lua.
 */
static void _registerBufferApi(void)
{
	/* possible lua buffer functions */
	const luaL_Reg funcs[] = {
		{"peek", _luaBufPeek},
		{"extract", _luaBufExtract},
		{"append", _luaBufAppend},
		{"clear", _luaBufClear},
		{"hasData", _luaBufHasData},
		{NULL, NULL}
	};

	/* create the new meta table for the buffer types */
	luaL_newmetatable(_state, _BUF_TYPE_NAME);

	/* store the functions in the meta table */
	luaL_setfuncs(_state, funcs, 0);

	/* store an index meta field in the metatable to allow accessing the
	 * functions */
	lua_pushliteral(_state, "__index");
	lua_pushvalue(_state, -2);
	lua_rawset(_state, -3);

	/* remove the metatable from the stack */
	lua_pop(_state, 1);
}

/**
 * returns the string representation of the given event.
 */
static const char* _getEventStr(event_t event)
{
	/* defines the possible event names */
	static const char* names[EVENT_COUNT] = {
		"start",
		"stop",
		"idle",
		"socket_accept",
		"socket_read",
		"socket_write",
		"socket_close",
	};

	/* is it a valid event */
	if(event >= 0 && event < EVENT_COUNT)
	{
		/* return the event name */
		return names[event];
	}

	/* invalid event */
	return NULL;
}

/**
 * pushes the given socket fd onto the stack. if the socket is invalid nil will
 * be used as the socket fd.
 */
static void _pushSocketFd(lua_State *state, int fd)
{
	/* check whether the socket is invalid or not */
	if(fd == INVALID_SOCKET)
	{
		/* if the socket is invalid push nil onto the stack */
		lua_pushnil(state);
	}
	else
	{
		/* if it is a valid socket fd push it onto the stack */
		lua_pushinteger(state, (lua_Integer) fd);
	}
}

/**
 * pushes the context as lua table onto the stack.
 */
static void _pushContext(eventContext_t *context)
{
	/* create a new context table */
	luaL_getsubtable(_state, LUA_REGISTRYINDEX, _CONTEXT_INDEX);

	/* store the event in the table */
	lua_pushliteral(_state, "event");
	lua_pushstring(_state, _getEventStr(context->event));
	lua_rawset(_state, -3);

	/* store the client socket in the data table */
	lua_pushliteral(_state, "sFd");
	_pushSocketFd(_state, context->sFd);
	lua_rawset(_state, -3);

	/* store the client socket in the data table */
	lua_pushliteral(_state, "cFd");
	_pushSocketFd(_state, context->cFd);
	lua_rawset(_state, -3);

	/* store the input buffer in the data table */
	lua_pushliteral(_state, "iBuf");
	_pushBuf(context->iBuf);
	lua_rawset(_state, -3);

	/* store the output buffer in the data table */
	lua_pushliteral(_state, "oBuf");
	_pushBuf(context->oBuf);
	lua_rawset(_state, -3);
}

/**
 * used as the real socket callback function.
 */
static int _luaServerCallback(eventContext_t *context)
{
	int result = 0;

	/* get the callback function from the registry */
	lua_pushliteral(_state, _SERVER_CALLBACK_INDEX);
	lua_rawget(_state, LUA_REGISTRYINDEX);

	/* push the context onto the stack */
	_pushContext(context);

	/* invoke the callback function */
	if(lua_pcall(_state, 1, 1, 0) != LUA_OK)
	{
		/* the function caused an error */
		logWrite("ERROR lua_pcall()");
		logWrite(lua_tostring(_state, -1));

		/* remove the error message from the stack */
		lua_pop(_state, 1);

		/* in case of an error shutdown the socket */
		serverCloseSocket(context->cFd);
	}
	else
	{
		/* get the result of the function */
		result = lua_toboolean(_state, -1);

		/* remove the result from the stack */
		lua_pop(_state, 1);
	}

	return result;
}

/**
 * lua wrapper function for serverSetCallback().
 */
static int _luaServerSetCallback(lua_State *state)
{
	/* the first argument of this function must be a lua function */
	luaL_checktype(state, 1, LUA_TFUNCTION);

	/* store the function in the lua registry */
	lua_pushliteral(state, _SERVER_CALLBACK_INDEX);
	lua_pushvalue(state, 1);
	lua_rawset(state, LUA_REGISTRYINDEX);

	/* set the accept callback */
	serverSetCallback(_luaServerCallback);

	return 0;
}

/**
 * lua wrapper function for serverOpenSocket().
 */
static int _luaServerOpenSocket(lua_State *state)
{
	/* add the server to the system and push the result onto the lua stack */
	_pushSocketFd(state, serverOpenSocket(
		luaL_checkstring(state, 1),
		luaL_checkstring(state, 2)
	));

	return 1;
}

/**
 * lua wrapper function for serverCloseSocket().
 */
static int _luaServerCloseSocket(lua_State *state)
{
	/* close the socket */
	serverCloseSocket(luaL_checkint(state, 1));

	return 0;
}

/**
 * lua wrapper function for serverGetSocketAddr().
 */
static int _luaServerGetSocketAddr(lua_State *state)
{
	int port;
	const char *host;

	/* get the address */
	if(serverGetSocketAddr(luaL_checkint(state, 1), &host, &port))
	{
		/* push the address and port onto the stack */
		lua_pushstring(state, host);
		lua_pushinteger(state, (lua_Integer) port);

		return 2;
	}

	return 0;
}

/**
 * lua wrapper function for serverChangeDir().
 */
static int _luaServerChangeDir(lua_State *state)
{
	/* change the current working directory */
	lua_pushboolean(state, serverChangeDir(luaL_checkstring(state, 1)));

	return 1;
}

/**
 * lua wrapper function for serverIsPrivileged().
 */
static int _luaServerIsPrivileged(lua_State *state)
{
	/* check if the process has privileged rights */
	lua_pushboolean(state, serverIsPrivileged());

	return 1;
}

/**
 * lua wrapper function for serverChangeUser().
 */
static int _luaServerChangeUser(lua_State *state)
{
	/* try to change the user of the process */
	lua_pushboolean(state, serverChangeUser(luaL_checkstring(state, 1)));

	return 1;
}

/**
 * lua wrapper function for serverJail().
 */
static int _luaServerJail(lua_State *state)
{
	/* create the jail */
	lua_pushboolean(state, serverJail(luaL_checkstring(state, 1)));

	return 1;
}

/**
 * lua wrapper function for serverChangeUserAndJail().
 */
static int _luaServerChangeUserAndJail(lua_State *state)
{
	/* change the user and establish a root jail */
	lua_pushboolean(state, serverChangeUserAndJail(
		luaL_checkstring(state, 1),
		luaL_checkstring(state, 2)
	));

	return 1;
}

/**
 * lua wrapper function for serverDaemonize().
 */
static int _luaServerDaemonize(lua_State *state)
{
	/* turn the process into a daemon */
	lua_pushboolean(state, serverDaemonize());

	return 1;
}

/**
 * registers the server api with lua.
 */
static void _registerServerApi(void)
{
	/* possible lua server functions */
	const luaL_Reg funcs[] = {
		{"setCallback", _luaServerSetCallback},
		{"openSocket", _luaServerOpenSocket},
		{"closeSocket", _luaServerCloseSocket},
		{"getSocketAddr", _luaServerGetSocketAddr},
		{"changeDir", _luaServerChangeDir},
		{"isPrivileged", _luaServerIsPrivileged},
		{"changeUser", _luaServerChangeUser},
		{"jail", _luaServerJail},
		{"changeUserAndJail", _luaServerChangeUserAndJail},
		{"daemonize", _luaServerDaemonize},
		{NULL, NULL}
	};

	/* create the server api and make it accessible */
	luaL_newlib(_state, funcs);
	lua_setglobal(_state, "server");
}

/**
 * function that is used as the lua log function. calls internally the script
 * defined log function.
 */
static void _luaLogCallback(const char *msg)
{
	/* get the callback function from the registry */
	lua_pushliteral(_state, _LOG_CALLBACK_INDEX);
	lua_rawget(_state, LUA_REGISTRYINDEX);

	/* push the context onto the stack */
	lua_pushstring(_state, msg);

	/* invoke the callback function */
	if(lua_pcall(_state, 1, 0, 0) != LUA_OK)
	{
		/* just remove the error message from the stack to prevent an endless
		 * loop. calling logWrite() at this point would produce the same error
		 * over and over again. */
		lua_pop(_state, 1);
	}
}

/**
 * lua wrapper function for logSetCallback().
 */
static int _luaLogSetCallback(lua_State *state)
{
	/* if nothing was given, disable logging */
	if(lua_isnoneornil(state, 1))
	{
		/* disable logging with NULL */
		logSetCallback(NULL);
	}
	else
	{
		/* the first argument of this function must be a lua function */
		luaL_checktype(state, 1, LUA_TFUNCTION);

		/* store the function in the lua registry */
		lua_pushliteral(state, _LOG_CALLBACK_INDEX);
		lua_pushvalue(state, 1);
		lua_rawset(state, LUA_REGISTRYINDEX);

		/* set the callback */
		logSetCallback(_luaLogCallback);
	}

	return 0;
}

/**
 * lua wrapper function for logWrite().
 */
static int _luaLogWrite(lua_State *state)
{
	/* write the log message */
	logWrite(luaL_checkstring(state, 1));

	return 0;
}

/**
 * registers the log api with lua.
 */
static void _registerLogApi(void)
{
	/* possible lua server functions */
	const luaL_Reg funcs[] = {
		{"setCallback", _luaLogSetCallback},
		{"write", _luaLogWrite},
		{NULL, NULL}
	};

	/* create the log api and make it accessible */
	luaL_newlib(_state, funcs);
	lua_setglobal(_state, "log");
}

/**
 * registers the server api with lua.
 */
static void _registerApi(void)
{
	/* register the buffer api */
	_registerBufferApi();

	/* register the server api */
	_registerServerApi();

	/* register the log api */
	_registerLogApi();
}

/**
 * prepares the lua provider by executing the specified file. returns 1 in case
 * of success and 0 in case of error.
 */
int providerPrepare(int argc, char **argv)
{
	/* is there a file argument */
	if(argc > 1)
	{
		/* create a new lua state */
		if((_state = luaL_newstate()) != NULL)
		{
			/* open the default lua libraries */
			luaL_openlibs(_state);

			/* register the server api to lua */
			_registerApi();

			/* execute the lua script file */
			if(luaL_dofile(_state, argv[1]) == LUA_OK)
			{
				/* everything went good so far */
				return 1;
			}

			/* failed to parse or execute the file, log the error */
			logWrite("ERROR luaL_dofile()");
			logWrite(lua_tostring(_state, -1));

			/* remove the error message from the stack */
			lua_pop(_state, 1);
		}
		else
		{
			/* cannot create a lua state */
			logWrite("ERROR luaL_newstate(): unable to create lua state");
		}
	}
	else
	{
		/* no cmd line argument */
		logWrite("ERROR no lua file provided");
	}

	return 0;
}

/**
 * shuts the lua system down.
 */
void providerShutdown(void)
{
	/* is there a valid state */
	if(_state != NULL)
	{
		/* close the lua state */
		lua_close(_state);
	}
}
