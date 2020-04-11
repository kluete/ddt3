# test ignored line

require "ddt_sub_file"

g_var = 2
local local_tab = {"abc", "de", "xxx"}
local xxx = {"tab2", "de", "xxx"}

function genfunc(n2)

	local cnt = 1234
	local print = print			-- copy global function to upvalue

	return function()
		
		local prims = "pre-_ENV"

		a = 15			-- global
		print("prims", prims)
		
		_ENV = { _G = _G }	-- environment override

		a = 1			-- "envars"
		half_global = 2

		local b0 = a		-- copy envar
		local b1 = _G.a		-- copy global
		
		_G.print("a", a)
		_G.print("prims", prims)
		
		print(cnt)
		
		print("n2", n2)		-- print generator arg
	end
end



