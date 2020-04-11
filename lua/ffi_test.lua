#!/usr/bin/env lua

--[[
launch with:

	lua -lffi_test -e 'main()'
]]

local p4_workspace = os.getenv("P4_WORKSPACE")
if (p4_workspace) then
	package.path = package.path .. ";" .. p4_workspace .. "/DebLua/?.lua"
end

-- shell utils
require "lua_shell"

local ffi = require("ffi")

local CWD = ""

---- Main ----------------------------------------------------------------------

function main()

ffi.cdef[[
unsigned long	compressBound(unsigned long sourceLen);
int		compress2(uint8_t *dest, unsigned long *destLen, const uint8_t *source, unsigned long sourceLen, int level);
int		uncompress(uint8_t *dest, unsigned long *destLen, const uint8_t *source, unsigned long sourceLen);
]]

	local zlib = ffi.load(ffi.os == "Windows" and "zlib1" or "z")

	local
	function compress(txt)
		local n = zlib.compressBound(#txt)
		local buf = ffi.new("uint8_t[?]", n)
		local buflen = ffi.new("unsigned long[1]", n)
		local res = zlib.compress2(buf, buflen, txt, #txt, 9)
		assert(res == 0)
		return ffi.string(buf, buflen[0])
	end

	local
	function uncompress(comp, n)
		local buf = ffi.new("uint8_t[?]", n)
		local buflen = ffi.new("unsigned long[1]", n)
		local res = zlib.uncompress(buf, buflen, comp, #comp)
		assert(res == 0)
		return ffi.string(buf, buflen[0])
	end

	-- test
	local txt = string.rep("abcd", 1000)
	print("Uncompressed size: ", #txt)
	
	local c = compress(txt)
	print("Compressed size: ", #c)
	
	local txt2 = uncompress(c, #txt)
	assert(txt2 == txt)

end

