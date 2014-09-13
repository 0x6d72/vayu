
-- local references for optimisation
local _gsub = string.gsub
local _upper = string.upper
local _lower = string.lower
local _format = string.format
local _concat = table.concat
local _toNumber = tonumber
local _pairs = pairs

-- -----------------------------------------------------------------------------
-- defines the http status codes.
-- -----------------------------------------------------------------------------
local _responseCodes = {

	-- information
	[100] = "Continue",
	[101] = "Switching Protocols",

	-- success
	[200] = "OK",
	[201] = "Created",
	[202] = "Accepted",
	[203] = "Non-Authoritative Information",
	[204] = "No Content",
	[205] = "Reset Content",
	[206] = "Partial Content",

	-- redirection
	[300] = "Multiple Choices",
	[301] = "Moved Permanently",
	[302] = "Found",
	[303] = "See Other",
	[304] = "Not Modified",
	[305] = "Use Proxy",
	[306] = "Unused",
	[307] = "Temporary Redirect",

	-- client errors
	[400] = "Bad Request",
	[401] = "Unauthorized",
	[402] = "Payment Required",
	[403] = "Forbidden",
	[404] = "Not Found",
	[405] = "Method Not Allowed",
	[406] = "Not Acceptable",
	[407] = "Proxy Authentication Required",
	[408] = "Request Time-out",
	[409] = "Conflict",
	[410] = "Gone",
	[411] = "Length Required",
	[412] = "Precondition Failed",
	[413] = "Request Entity Too Large",
	[414] = "Request-URI Too Long",
	[415] = "Unsupported Media Type",
	[416] = "Requested range not satisfiable",
	[417] = "Expectation Failed",
	[418] = "I'm a teapot",

	-- server errors
	[500] = "Internal Server Error",
	[501] = "Not Implemented",
	[502] = "Bad Gateway",
	[503] = "Service Unavailable",
	[504] = "Gateway Time-out",
	[505] = "HTTP Version not supported"
}

-- -----------------------------------------------------------------------------
-- formats the given header name and returns a prettier version of it.
-- -----------------------------------------------------------------------------
local function _formatHeader(header)
	-- format the header
	return _gsub(_gsub(_lower(header), "^%a", _upper),  "%-%a", _upper)
end

-- -----------------------------------------------------------------------------
-- this object is used to create http response messages.
-- -----------------------------------------------------------------------------
httpResponseBuilder = {}

-- -----------------------------------------------------------------------------
-- creates the http response with the given response object. the object must
-- look like the following:
--	{
--		status = 200,
--		headers = {
--			["connection"] = "close",
--			["content-length"] = 4,
--			["content-type"] = "text/plain",
--			...
--		}
--		body = "test"
--	}
-- the content-length header is automatically set accordingly to the body.
-- -----------------------------------------------------------------------------
function httpResponseBuilder.buildResponse(response, version)
	-- recalculate the content length
	response.headers["content-length"] = #response.body

	-- normalize the version
	version = _toNumber(version) or 1.1

	local responseText
	local lines = {}

	-- build the response line
	lines[#lines + 1] = _format(
		"HTTP/%.1f %d %s",
		version,
		response.status,
		_responseCodes[response.status]
	)

	-- add the other headers
	for key, val in _pairs(response.headers) do
		lines[#lines + 1] = _formatHeader(key) .. ": " .. val
	end

	-- add blanks to the header
	lines[#lines + 1] = ""
	lines[#lines + 1] = ""

	-- build and return the response text
	return _concat(lines, "\r\n") .. response.body
end
