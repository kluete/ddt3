
function main()

	local src = io.open("/home/plaufenb/test.torrent", "r"):read("*a")

	local n = src:len()
	
	print("src len = " .. n)

end

