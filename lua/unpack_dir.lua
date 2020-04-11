#!/usr/bin/env lua

--[[
launch with:

	lua -lunpack_dir -e 'main()'
]]


local posix = require "posix"

local p4_workspace = os.getenv("P4_WORKSPACE")
if (p4_workspace) then
	package.path = package.path .. ";" .. p4_workspace .. "/DebLua/?.lua"
end

-- shell utils
require "lua_shell"

local CWD = ""

---- main ---------------------------------------------------------------------

function main()

	local f_list = Util.CollectDirFilenames("/media/vm/_mp3_check_dupes")
	
	Log.Init(CWD .. "unpack_dir.log")
	
	Log.SetTimestamp("%H:%M:%S > ")
	
	local f_list = Util.CollectDirFilenames("/media/vm/_mp3_check_dupes", "*.bz2")
	-- local f_list = Util.CollectDirFilenames("/media/vm/_mp3_check_dupes")
	-- local f_list = Util.CollectDirFilenames("/media/vm/_mp3_check_dupes/Fakes - disc 1 (dZihan & Kamien)")
	
	local n_total = #f_list
	local filtered_t = {}
	
	for k, v in ipairs(f_list) do
		
		if (true) then -- v:find("(%.bz2)$")) then
			print(k, n_total, v)
			table.insert(filtered_t, v)
		end
	end
	
	Log.f("total %d, filtered %d", #f_list, #filtered_t)
	Log.f("done")
	
	-- getkey('\n\npress a key to exit\n')
end















