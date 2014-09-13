
-- stores all socket parsers
local _socketList = {}

local function _serialize(v, i)
	i = i or 0
	local r
	local t = type(v)
	if t == "table" then
		local w, wn = string.rep("  ", i), string.rep("  ", i + 1)
		local sr = {}
		for sk, sv in pairs(v) do
			sr[#sr+1] = string.format(
				"%s[%s] => %s", wn, _serialize(sk), _serialize(sv, i + 1)
			)
		end
		r = string.format("<%s> {\n%s%s\n%s}", t, table.concat(sr, ",\n"), wn, w)
	elseif t == "string" then
		r = string.format("<%s> %q", t, v)
	else
		r = string.format("<%s> %s", t, tostring(v))
	end
	return r
end

-- handles the request
local function _handleRequest(context, parser)

	local request = parser.getRequest()
	local keepAlive = parser.shouldKeepAlive()
	
	-- default response
	local response = {
		status = 200,
		headers = {
			["content-type"] = "text/plain"
		},
		body = _serialize(request)
	}

	-- only allow five requests per connections
	if parser.getIterations() > 5 then
		keepAlive = false
	end

	-- add the appropriate headers if keep alive is turned of
	if not keepAlive then
		response.headers["connection"] = "close"
	elseif request.version == 1.0 then
		response.headers["connection"] = "Keep-Alive"
	end

	-- write the response
	context.oBuf:append(
		httpResponseBuilder.buildResponse(response, request.version)
	)

	-- without keep alive the connection will be terminated
	if not keepAlive then
		-- close the connection
		server.closeSocket(context.cFd)
		
		-- remove the parser
		_socketList[context.cFd] = nil
	else
		-- reset the parser
		parser.reset()
	end
end

-- set the callback
server.setCallback(function (context)
	-- react to a read event
	if context.event == "socket_read" then
		-- is there a parser for this connection
		if _socketList[context.cFd] == nil then
			-- no parser present create a new one
			_socketList[context.cFd] = httpRequestParser.new()
		end
	
		-- get the parser for this connection
		local parser = _socketList[context.cFd]
		
		-- is the request done
		if not parser.exec(context.iBuf:extract()) then
			-- handle the request
			return _handleRequest(context, parser)
		end
		
	-- react to an idle event
	elseif context.event == "idle" then
		collectgarbage();
	end
end)

-- add the server
server.addServerSocket("0.0.0.0", "12345")
