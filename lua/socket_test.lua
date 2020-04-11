-- lua socket test

package.path = package.path .. ";/usr/share/lua/5.1/?.lua"
package.cpath = package.cpath .. ";/usr/lib/x86_64-linux-gnu/lua/5.1/?.so"

-- load namespace
local socket = require("socket")

function main()

	-- create a TCP socket and bind it to the local host, at any port
	local server = assert(socket.bind("*", 4444))
	
	-- find out which port the OS chose for us
	local ip, port = server:getsockname()
	
	print('use "telnet localhost ' .. port .. '"')
	
	local done_f
	
	while (not done_f) do
		-- wait for a connection from any client
		local client = server:accept()
		-- make sure we don't block waiting for this client's line
		client:settimeout(2)
		
		-- receive the line
		local line, err = client:receive()
		
		-- if there was no error, send it back to the client
		if not err then
			client:send('echoing "' .. line .. '"\n')
			done_f = true
		else
			print('timed out, looping')
		end
		
		-- done with client, close the object
		client:close()
	end
	
	print("socket test DONE")	
end

-- nada mas
