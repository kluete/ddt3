-- Lua DDT debugger demo

require "ddt_sub_file"

require "mini_func"

-- require "posix"

j = 4567

-- crashell()

g_tab = {"global2", "ibz1", "sonica3"}

function new_obj()

	local obj = {	m_Cnt = 0,
			Add = function (self, v)
				self.m_Cnt = self.m_Cnt + v
			end
		}

	function obj:Sub(v)
		self.m_Cnt = self.m_Cnt - v
	end

	return obj
end 

function tailor()

	print("tailor")
end

function arg_test(arg1, arg2, arg3)

	if (false) then
	
	end

	-- return print("fixed arg test")
	return tailor()
end

function var_arg_test(arg1, ...)

	print("test varargs")
end

function sleep(n_secs)

	print("test snoozing for " .. n_secs .. " seconds (test UI live)")
	
	local t0 = os.time()
	
	while (os.difftime(os.time(), t0) < n_secs) do
		-- nop
	end
end

-- test assert() behavior (test 1st arg, pass others upstream)
function assertest()

	return "assert", "test", "pass", "through"
end

-- upvalues test (locals view)
local upval2 = "upval ref'ed by closure"

function gen_closure(upv)

	local upv_inst1 = upv
	
	return function()
			local up_bind = upval2
			local loc = 2
			gloc2 = 102
			print(upv_inst1)
			print(upval2)
		end
end

-- coroutine test
function cofn()

	print("inside coroutine")
	
	-- breakpoint()

	print("returning to main lua_State...")
	
	coroutine.yield()
end

function part1(dumie)

	print("welcome to this small lua ddt demo...")
	
	subfile_func("passing")
	
	g = "global var"
	
	print("vanilla table members test")
	-- local tab = {entry1 = "it's a string", [1] = "index", flag = true}
	local tab = {"it's a string", "index", true}
	
	print("test functions in table")
	local fnLUT = {print, function() return 3 end, subfunction1}

	print("generate function with _ENV override")
	local ffn = genfunc(g)
	ffn()

	print("test object notation")
	local obj = new_obj()

	obj:Add(20)
	obj:Sub(5)

	print("test fixed args / varargs")
	arg_test("arg1", 2, true)
	
	var_arg_test("arg1", 2, true)

	print("test closure")
	local clozur =	function(cloz_var)
				print(cloz_var)
			end
	
	clozur("kloz")

	print("test changing variable highlight")
	local new_var = 1

	new_var = 2

	-- test non-ASCII string
	print("embedded zeroes are fine")
	local non_printable_string = "I'm not print\000able"

	non_printable_string = "I'm unprint\000able and changed"
		
	-- test multi-line string
	print("inspect multi-liner")
	local multiline_string = [[ line1
	line2
	line3
	]]
	
	print("test table self-reference")
	table.insert(tab, tab)
	
	print("meta-table test")
	function add(op1, op2)
	
		return op1 + op2
	end
	
	local mt = {__add = add}
	setmetatable(tab, mt)
	
	tab.entry1 = "overwrite"

	print("test variable shadowing")
	local
	function test_shadow()

		local new_var = "shadow"

		print(new_var)
		
		-- only sees upstream if is used?
		print(tab.entry1)
	end
	
	test_shadow()

	local deep = gen_closure("limbo")

	local array = {"one", "two", "three"}

	deep()

	array[2] = "whatever"
	
	print("boolean table keys")
	local bool_tab = {[true] = "c'est vrai", [false] = "du tout"}

	local co = coroutine.create(cofn)
	coroutine.resume(co)

	print("test assert() behavior")
	tab.subtab = {sub = tab}
	local ass_ret, r1, r2, r3 = assert(assertest())
	
	print("part1 done")
end

function part2_subfunction1()
	
	-- snoozing
	sleep(5)
	
	-- press F10 to step OVER the function
	subfunction1("passing")
end	

function part3_subfile()
	-- assert(false)
	subfile_func()
end

function part4_subfunction2()
	-- press F11 to step INTO the function
	subfunction1("passing2")
end

function part5_loop()

	print("entering loop")
	-- switch to watch view
	-- step through a few loop iterations with F10
	-- note the variable color as it is changing
	-- press F5 to let it run until the breakpoint is triggered
	for i = 1, 10000 do
		
		print(i, j)
		i = i + 1
		j = j + 1
	end
end

function part6_runtime_error()
	-- press F5 to run until the illegal function is called
	print("calling an illegal function now...")

	-- switch to the console view to see the error logged
	-- switch to the traceback view
	-- double-click on stack entry
	-- click the abort button to quit
	illegalfunction()
end

function subfunction1(arg)
	
	-- press F11 repeatedly until exits both functions
	local	vis_var = "visible"
	
	local
	function subfunction2()
		
		print("two levels deep")
	end
	
	subfunction2()
end

g_Functions = {part1, part2_subfunction1, part3_subfile, part4_subfunction2, part5_loop, part6_runtime_error, nil}

function main()

	local f = false
	local b = "bite"

	-- assert(f, b)

	for k, fn in ipairs(g_Functions) do
	
		if (not fn) then
			break
		end
		
		fn("dummy")
	end
end

-- nada mas

