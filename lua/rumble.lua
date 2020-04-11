#!/usr/bin/env lua

--[[

	Rumblepad event reader

]]

local gamepad_dev = '/dev/input/js0'

--------------------------------------------------------------------------------

-- sprintf
local
function sprintf(fmt, ...)
	-- replace %S with "%s"
	local expanded_fmt = fmt:gsub("%%S", "\"%%s\"")

	return expanded_fmt:format(...)
end

-- printf
local
function printf(fmt, ...)

	print(sprintf(fmt, ...))
end

-- assertf	
local
function assertf(f, fmt, ...)
	if (f) then
		return
	end
	if (nil == fmt) then
		fmt = "<missing assertf() fmt>"
	end
	
	assert(f, sprintf(fmt, ...))
end

-- exec
local shell = {}
setmetatable(shell, {__index =
	function(t, func)
		-- print(("_lut %s()"):format(tostring(func)))
		local shell_fn = func.." "
		return	function (...)
			return os.execute(shell_fn..table.concat({...}," "))
		end
	end})

-- piped SINGLE-line res
local pshell = {}
setmetatable(pshell, {__index =
	function(t, func)
		-- print(("_lut %s()"):format(tostring(func)))
		local shell_fn = func.." "
		return	function (...)
			-- return shell_fn..table.concat({...}," ")
			return io.popen(shell_fn..table.concat({...}," ")):read("*l")
		end
	end})

-- piped MULTI-line res, returned as table
local tshell = {}
setmetatable(tshell, {__index =
	function(t, func)
		-- print(("_lut %s()"):format(tostring(func)))
		local shell_fn = func.." "
		return	function (...)
			local ln_t = {}
			io.popen(shell_fn..table.concat({...}," ")):read("*a"):gsub("([^\n]*)\n", function(ln) table.insert(ln_t, ln) end)
			return ln_t
		end
	end})


local log_f = io.open("rumb.log", "w")

local
buttonTab = {}

local
axisNames = {"lstick_x", "lstick_y", "rstick_x", "rstick_y", "hat_x", "hat_y"}

local
axisTab = {}

---- Init States ---------------------------------------------------------------

local
function InitStates()

	for b = 1, 10 do
		buttonTab[b] = 0
	end
	
	for a = 1, 7 do
		axisTab[a] = 0
	end
end

---- Get One event -------------------------------------------------------------

local
function GetOne(f)

	-- 
	-- [0; 3] timestamp in milliseconds
	-- [4; 5] word value (down/up for button, one axis value per axis movement)
	-- [6] type (1=button, 2=axis)
	-- [7] id
	local frame = 8
	
	local s = f:read(frame)
	if (s == nil) then
		return
	end
	
	local val, w2, id, typ
	
	typ = s:byte(7)
	id = s:byte(8)
	
	if (typ == 1) then
		
		id = id + 1
		val = s:byte(5) + (s:byte(6) * 256)
		buttonTab[id] = val
		
		if (val == 1) then
			-- new down
			return id
		else
			-- ignore button up
			return nil
		end
	
	elseif (typ == 2) then
		local u_val = s:byte(5) + (s:byte(6) * 256)
		-- two's complement
		local val = (u_val > 32767) and (u_val - 2^16) or u_val
		
		axisTab[id] = val
		
		local axis_id = 101 + (id * 2) 
		
		if (val < -20000) then
			--left or up
			return axis_id
		elseif (val > 20000) then
			-- right or down
			return axis_id + 1
		else
			-- ignore dead zone
			return nil
		end
	end
end

---- Dump States ---------------------------------------------------------------

local
function DumpStates()

	-- buttons
	local but_s = "buttons: "
	for i = 1, 10 do
		local c = '_'
		if (buttonTab[i] > 0) then
			c = tostring(i)
		end
		
		but_s = but_s .. c .. ' '
	end
	
	local axis_s = "axis: "
	
	for i = 1, 6, 2 do
		axis_s = axis_s .. sprintf("(%05d, %05d) ", axisTab[i], axisTab[i + 1])
	end
	
	printf("%s %s", but_s, axis_s)
end

---- Vlc commands --------------------------------------------------------------

local
cmdLUT = {	[-1] = "quit",
		[9] = "pause",
		[10] = "fullscreen",
		[2] = "voldown 1",
		[4] = "volup 1",
		[3] = "volume 0",
		[5] = "previous",
		[6] = "next",
		
		[101] = "seek -8",
		[102] = "seek 8",
		[103] = "seek 60",
		[104] = "seek -60"
		
		}

---- Main ----------------------------------------------------------------------

function main()

	InitStates()
	
	-- local	f = io.popen("cat "..gamepad_dev, "r")
	-- can read from device directly
	local	f = io.open(gamepad_dev, "rb")
	assert(f)
	
	local loop_f = true
	
	while (loop_f) do
	
		local id = GetOne(f)
		
		DumpStates()
		
		if (id) then
			local cmd = cmdLUT[id]
			
			printf("id %d, cmd %S", id, tostring(cmd))
			
			if (buttonTab[7] == 1) and (buttonTab[8] == 1) then
				id = -1
				cmd = cmdLUT[id]
				printf("id %d, cmd %S", id, tostring(cmd))
				loop_f = false
			end
		end
	end
	
	if (log_f ~= nil) then
		log_f:close()
	end

	os.exit(0)

end

