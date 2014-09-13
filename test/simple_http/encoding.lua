
-- local references for optimisation
local _gsub = string.gsub
local _char = string.char
local _byte = string.byte
local _format = string.format
local _toNumber = tonumber

-- -----------------------------------------------------------------------------
-- converts the the given hex number to a char.
-- -----------------------------------------------------------------------------
local function _hexToChar(num)
	return _char(_toNumber(num, 16))
end

-- -----------------------------------------------------------------------------
-- converts the given character into the hex number.
-- -----------------------------------------------------------------------------
local function _charToHex(char)
	return _format("%%%02X", _byte(char))
end

-- -----------------------------------------------------------------------------
-- this table contains all encode functions.
-- -----------------------------------------------------------------------------
encode = {}

-- -----------------------------------------------------------------------------
-- this table contains all decode functions.
-- -----------------------------------------------------------------------------
decode = {}

-- -----------------------------------------------------------------------------
-- this function encodes the given data with url encoding.
-- -----------------------------------------------------------------------------
function encode.url(data)
	return _gsub(_gsub(data, "([^%w ])", _charToHex), " ", "+")
end

-- -----------------------------------------------------------------------------
-- decode the given data with the url algorithm
-- -----------------------------------------------------------------------------
function decode.url(data)
	return _gsub(_gsub(data, "+", " "), "%%(%x%x)", _hexToChar)
end
