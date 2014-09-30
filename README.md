# Vayu

Vayu is a small event based TCP server written in c utilizing LUA as the scripting language. It complies fully to c89 except the socket stuff which is posix compliant.

## How to build?

Just execute the build script in ./bin:

```
$ cd ./bin
$ ./build.sh
```

You can also build vayu by yourself by grabbing all c-files and passing them to a c compiler (any c compiler you like). Like this for example:

```
$ cd ./bin
$ tcc -lm $(find ../src -name "*.c")
```

Pass the compiler any parameters you want. **-lm is mandatory (on linux at least) to compile lua**.

You can even compile vayu without lua support. All you need to do is to exclude the files ./src/core/lua.c and ./src/lua/*.c and provide a file which contains the two functions `providerPrepare()` and `providerShutdown()`. See `./src/core/server.h` for the declaration.

```
$ cd ./bin
$ tcc $(find ../src/core -name "*.c" -and -not -name "lua.c") ../../mycode/my_vayu_provider.c
```

## How to execute?

If you have successfully build vayu call it like this:

```
$ ./vayu LUA_SCRIPT
```

where LUA_SCRIPT is the lua script file containing the entire server logic.

## C Interface

The entire c-interface is documented in `./src/core/server.h`.

## LUA Interface

### Server

**server.setCallback(callback)**

Used to set the event callback to the given function. The callback function has the signature `bool callback(table context)`. `context` is a table with the following format:

```lua
{
    -- the event type. can be on of:
    -- "start"           when the server starts
    -- "stop"            when the server stops
    -- "idle"            when the server has nothing to do at the moment
    -- "socket_accept"   when the server accepted a new client connection
    -- "socket_read"     when data is ready to read from the input buffer
    -- "socket_write"    when data was sent to the client
    -- "socket_close"    when the socket was closed
    ["event"] = string,

    -- the server socket descriptor (the listening socket).
    -- only set on "socket_accept" or "socket_close" otherwise nil
    ["sFd"] = number,

    -- the client socket descriptor (the socket connected to the client)
    -- only set on "socket_accept", "socket_read", "socket_write" or "socket_close"
    -- otherwise nil
    ["cFd"] = number,

    -- the next two fields contain the data buffer for the client socket.
    -- they only contain a buffer object when the client socket descriptor
    -- is set otherwise they contain nil. see the buffer section for more details.
    ["iBuf"] = buffer,
    ["oBuf"] = buffer
}
```

**server.openSocket(host, port)**

Opens a new server socket. `host` defines the host address either in numeric representation or a domain name. `port` defines the port number either as a number or a service name ("www" for port 80). Returns the descriptor of the new server socket.

**server.closeSocket(socket)**

takes a socket descriptor and closes the socket associated with that descriptor. this will create a "socket_close" event for that particular socket. if the output buffer contains data it will first written to the client.

**server.getSocketAddr(socket)**

returns the address and port associated with the given socket. returns two values the first one contains the host in numeric representation and the second one contains the port number.

**server.changeDir(dir)**

Changes the current working directory of the server process to the specified directory. Returns true if it was successfull and false if not.

**server.isPrivileged()**

Checks whether the server has privileged (root) rights. Returns true if the process has root-rights and false if not.

**server.changeUser(user)**

Changes the effective and real user of the server process. the user is specified by his name. Returns true if it was successfull to change the user or false otherwise.

**server.jail(dir)**

Used to set up a chroot-jail in the specified directory. Returns true if it was successfull to set up the jail and false if not.

**server.changeUserAndJail(user, dir)**

A combination of `server.changeUser()` and `server.jail()`. This function is usefull when the chroot-jail does not have the `/etc/passwd` file. Returns true if everything was successfull and false if not.

**server.daemonize()**

Turns the server process into a background process without a controlling terminal. Returns true if was successfull and false if not.

### Buffer

**buffer:peek()**

Get the data from the buffer without removing it from the buffer. Returns the data as a string.

**buffer:extract()**

Get the data that is stored in the buffer and clear it. Returns the data as a string.

**buffer:append(data)**

Appends the data to the buffer.

**buffer:clear()**

Empties the buffer and discards its contents.

**buffer:hasData()**

Used to check whether the buffer contains data or not. Returns true if the buffer contains data and false if not.

### Log

**log.setCallback(callback)**

Sets the callback to write log message. The callback has the signature `void callback(string msg)`.

**log.write(msg)**

Logs the given message using previously defined callback.
