-- ddt lua jit first test

local bench = require("scimark")
local bit = require("bit")

-- no gain on jit? (must actual DO something?)
local
function tight_loop()
	
	local cnt = 5381

	for i=1,10e7 do
		
		cnt = bit.bxor(cnt * 33, i)
	end
	
	print("hash", cnt)

	return cnt
end

function main()

	-- print("profiler on")
	-- ddtProfiler(true, 5)

	local t0 = os.clock()
	
	-- bench.main({"SOR"})
	tight_loop()
	-- bench.main({"FFT"})
	-- bench.main({"-small", "SOR"})
	-- bench.main({"MC"})
	
	local elap_secs = os.clock() - t0
	
	-- ddtProfiler(false)
	-- print("profiler off")
	
	print("elapsed = " .. string.format("%g secs", elap_secs))
end

-- nada mas

