
-- local references for optimisation
local _match = string.match
local _lower = string.lower
local _gsub = string.gsub
local _sub = string.sub
local _toNumber = tonumber
local _toString = tostring
local _urlDecode = decode.url

-- -----------------------------------------------------------------------------
-- defines the possible states of a request parser
-- -----------------------------------------------------------------------------
local _statusRequest = 1
local _statusHeader = 2
local _statusHeaderCheck = 3
local _statusBody = 4
local _statusDone = 5
local _statusError = 6

-- -----------------------------------------------------------------------------
-- define request parser
-- -----------------------------------------------------------------------------
httpRequestParser = {}

-- -----------------------------------------------------------------------------
-- creates a new request parser instance
-- -----------------------------------------------------------------------------
function httpRequestParser.new(maxRequestSize)
	-- adjust the maximum request size (the default is 1mib)
	maxRequestSize = maxRequestSize or (1024 * 1024)

	-- -------------------------------------------------------------------------
	-- stores data that could not be parsed
	-- -------------------------------------------------------------------------
	local _chunk

	-- -------------------------------------------------------------------------
	-- stores the current status of the request parser
	-- -------------------------------------------------------------------------
	local _status

	-- -------------------------------------------------------------------------
	-- stores an error code
	-- -------------------------------------------------------------------------
	local _errorCode

	-- -------------------------------------------------------------------------
	-- stores the request
	-- -------------------------------------------------------------------------
	local _request

	-- -------------------------------------------------------------------------
	-- stores the number of bytes processed by this parser instance.
	-- -------------------------------------------------------------------------
	local _bytesProcessed
	
	-- -------------------------------------------------------------------------
	-- stores the number of times the parser has been used.
	-- -------------------------------------------------------------------------
	local _iterations

	-- -------------------------------------------------------------------------
	-- puts the request parser into the error state.
	-- -------------------------------------------------------------------------
	local function _setError(err)
		_errorCode = err
		_status = _statusError
	end

	-- -------------------------------------------------------------------------
	-- parses the given request line and puts resulting data into the
	-- request object.
	-- -------------------------------------------------------------------------
	local function _parseRequest(requestLine)
		-- parse the request line
		local method, uri, version = _match(
			requestLine, "^%s*([^%s]+) ([^%s]+) HTTP/([%d.]+)$"
		)

		-- is there a valid request line
		if method ~= nil and uri ~= nil and version ~= nil then
			_request.method = _lower(method)
			_request.uri = _urlDecode(uri)
			_request.version = _toNumber(version)
		else
			-- the request line could not be parsed
			_setError(400)
		end
	end

	-- -------------------------------------------------------------------------
	-- parses the given header and appends the resulting data to the
	-- request object.
	-- -------------------------------------------------------------------------
	local function _parseHeader(header)
		-- extract the header name and value
		local name, value = _match(header, "^%s*([^:]+)%s*:%s*(.*)$")

		-- is there a valid header
		if name ~= nil and value ~= nil then
			-- add the header to the request
			_request.headers[_lower(name)] = value
		else
			-- the header could not be parsed
			_setError(400)
		end
	end

	-- -------------------------------------------------------------------------
	-- checks the http header for consistency and validity.
	-- -------------------------------------------------------------------------
	local function _checkHeader()
		-- only head, get, and post requests are allowed
		if _request.method ~= "head"
			and _request.method ~= "get"
			and _request.method ~= "post"
		then
			_setError(501)
			return false
		end

		-- only 1.0 and 1.1 requests are supported
		if _request.version ~= 1.0 and _request.version ~= 1.1 then
			_setError(505)
			return false
		end

		-- is it a http 1.1 request and does it contain a host header
		if _request.version == 1.1 and _request.headers["host"] == nil
		then
			_setError(400)
			return false
		end

		-- is it a post request
		if _request.method == "post" then
			-- if the client wants to post data a content-length header
			-- is required
			local length = _toNumber(_request.headers["content-length"])

			-- the length is not a valid number
			if length == nil then
				_setError(411)
				return false
			end

			-- replace the string with a number
			_request.headers["content-length"] = length
		else
			-- reset the content-length header
			_request.headers["content-length"] = nil
		end

		return true
	end

	-- -------------------------------------------------------------------------
	-- this function is used as a callback for string.gsub().
	-- -------------------------------------------------------------------------
	local function _handleHeaderLine(line)
		-- is this a header
		if _status == _statusHeader then
			-- is this the end of the headers
			if #line == 0 then
				-- switch the state to header check
				_status = _statusHeaderCheck
			else
				-- parse the header
				_parseHeader(line)
			end

		-- is this the request line
		elseif _status == _statusRequest then
			-- parse the request line
			_parseRequest(line)

			-- switch the state to header
			_status = _statusHeader

		-- this is something else; dont touch it
		else
			return nil
		end

		-- remove the current match from the chunk
		return ""
	end

	-- -------------------------------------------------------------------------
	-- processes the header of the request
	-- -------------------------------------------------------------------------
	local function _handleHeader()
		-- is the parser in the header
		if _status == _statusRequest or _status == _statusHeader then
			-- parse the chunk of data
			_chunk = _gsub(_chunk, "([^\r\n]*)\r\n", _handleHeaderLine)
		end

		-- did the parser finish the header
		if _status == _statusHeaderCheck then
			-- check the received data
			if _checkHeader() then
				-- switch to the appropriate status
				if _request.method == "post" then
					_status = _statusBody
				else
					_status = _statusDone
				end
			end
		end
	end

	-- -------------------------------------------------------------------------
	-- processes the body if there is one
	-- -------------------------------------------------------------------------
	local function _handleBody()
		-- should the body be consumed
		if _status == _statusBody then
			-- just append the data to the body
			_request.body = _request.body .. _chunk

			-- is there enough data
			if #_request.body >= _request.headers["content-length"] then
				-- truncate the body to the specified length
				_request.body = _sub(
					_request.body, 1, _request.headers["content-length"]
				)

				-- the request is now done
				_status = _statusDone
			end
		end
	end

	-- the actual parser instance / public api
	local parser = {}

	-- -------------------------------------------------------------------------
	-- resets the parser to initial values
	-- -------------------------------------------------------------------------
	function parser.reset()
		_chunk = ""
		_status = _statusRequest
		_errorCode = 0
		_request = {
			method = "",
			uri = "",
			version = 1.1,
			headers = {},
			body = ""
		}
		_bytesProcessed = 0
	end

	-- -------------------------------------------------------------------------
	-- checks whether the parser is in error state
	-- -------------------------------------------------------------------------
	function parser.hasError()
		return _status == _statusError
	end

	-- -------------------------------------------------------------------------
	-- this method executes the parser and parses the given data. it returns
	-- true if more data is required to complete the request and false if the
	-- request is fully parsed.
	-- -------------------------------------------------------------------------
	function parser.exec(data)
		-- calculate the number of bytes processed
		_bytesProcessed = _bytesProcessed + #data

		-- check if the request is too big
		if _bytesProcessed > maxRequestSize then
			_setError(413)
			return false
		end

		-- merge the chunk with the given data
		_chunk = _chunk .. data

		-- handle the request header
		_handleHeader()

		-- handle the request body
		_handleBody()

		-- the parser needs more data if it isnt done
		if _status ~= _statusDone and _status ~= _statusError then
			return true
		else
			_iterations = _iterations + 1
			return false
		end
	end

	-- -------------------------------------------------------------------------
	-- used to check whether the connection should be kept alive.
	-- -------------------------------------------------------------------------
	function parser.shouldKeepAlive()
		-- version 1.1 of http always keeps connections alive unless
		-- the client does not want that
		if _request.version == 1.1 then
			return not (_lower(_toString(
				_request.headers["connection"])) == "close")
		-- version 1.0 never keeps connections alive unless the
		-- client wants to
		elseif _request.version == 1.0 then
			return _lower(_toString(
				_request.headers["connection"])) == "keep-alive"
		end

		return false
	end

	-- -------------------------------------------------------------------------
	-- returns the error code of the parser.
	-- -------------------------------------------------------------------------
	function parser.getError()
		return _errorCode
	end

	-- -------------------------------------------------------------------------
	-- returns the parsed request.
	-- -------------------------------------------------------------------------
	function parser.getRequest()
		-- the request is only accessible when the parser has
		-- completed parsing
		if _status == _statusDone then
			return _request
		end

		return nil
	end
	
	-- -------------------------------------------------------------------------
	-- returns the iteration count of this parser.
	-- -------------------------------------------------------------------------
	function parser.getIterations()
		return _iterations
	end

	-- set the initial iteration count to 0
	_iterations = 0

	-- reset the request
	parser.reset()

	-- return the parser instance
	return parser
end
