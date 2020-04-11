--------------------------------------------------------------------------------------------------------------
-- print_rt_up.lua
-- print nodes connected to the selected rendertarget (walk backwards up the tree)
-- if there's no rendertarget then find the root of mesh nodes
--
-- copyright (c) 2014 OTOY
-- author John Cooke www.ultrafish.com
--------------------------------------------------------------------------------------------------------------
local DO_RAW_ATTRIBS = true
local DO_PRINT_GC = false        -- print lua memory usage
local DO_PRINT_LOG = false		-- print to log file if true
local orgPrint = print
local firstprint = true
function print( str)

	str = tostring(str)
	
	if (DO_PRINT_LOG) then
		local docname = "print_nodes_from_rt.log"
		local flag = "a"
		if (firstprint) then
			flag = "w"
			firstprint = false
		end
		local doc, errMsg = io.open(docname, flag)
		if doc then 
			if (str == nil) then
				str = ""
			end
			doc:write( str.."\n")
			io.close( doc)
		end
	end
	orgPrint( str)
end

--------------------------------------------------------------------------------------------------------------
function dump( obj, name, indent)
	
	indent = indent or 0
	local first = indent == 0
	local	indent_str = string.rep("  ", indent)

	if (type(obj) ~= "table") then	

		local str
		if (type(obj) == "number") then
			str = string.format("%.2f", obj)
		else
			str = tostring(obj)
		end
		local name = name or ""
		print(indent_str..name.." = "..str)
		return indent
	end
	if (name) then
		print(indent_str..name)
	end
	indent = indent + 1
	for name,elem in pairs(obj) do
	
		dump(elem, name, indent)
	end
	
	if (first) then
		print(" ")
	end
	return indent
end

--------------------------------------------------------------------------------------------------------------
local function valString(value)

	local str
	if (type(value) == "number" and math.fmod(value,1) ~= 0) then
		str = string.format("%.2f", value)
	else
		str = tostring(value)
	end
	return str
end

--------------------------------------------------------------------------------------------------------------
local function getValueString(value)

	if (type(value) ~= "table") then
		return valString(value)
	end
	if (#value > 32) then
		return "[BIGTABLE] len = "..#value
	end	

	local str = ""
	for _,v in ipairs(value) do
	
		local s
		if (type(v) == "table") then
			s = getValueString(v)
		else
			s = valString(v)
		end
		if (str == "") then
			str = s
		else
			str = str..", "..s
		end
	end
	return str
end

--------------------------------------------------------------------------------------------------------------
local nodeTypeDontDive = {
	[octane.nodeType.NT_BOOL] = true, 
	[octane.nodeType.NT_ENUM] = true, 
	[octane.nodeType.NT_FLOAT] = true, 
	[octane.nodeType.NT_IMAGE_RESOLUTION] = true, 
	[octane.nodeType.NT_INT] = true, 
	[octane.nodeType.NT_IN_BOOL] = true, 
	[octane.nodeType.NT_IN_ENUM] = true, 
	[octane.nodeType.NT_IN_FLOAT] = true, 
	[octane.nodeType.NT_IN_INT] = true, 
	[octane.nodeType.NT_IN_OFFSET] = true, 
	[octane.nodeType.NT_OUT_BOOL] = true, 
	[octane.nodeType.NT_OUT_ENUM] = true, 
	[octane.nodeType.NT_OUT_FLOAT] = true, 
	[octane.nodeType.NT_OUT_INT] = true, 
	[octane.nodeType.NT_OUT_OFFSET] = true, 	
	[octane.nodeType.NT_SUN_DIRECTION] = true,
	[octane.nodeType.NT_IMAGE_RESOLUTION] = true,
	[octane.nodeType.NT_TEX_RGB] = true, 	
	[octane.nodeType.NT_TEX_FLOAT] = true, 	
}
--------------------------------------------------------------------------------------------------------------
local attribRawType = {
	["verticesPerPoly"] = true, 
    ["vertices"] = true, 
    ["polyVertexSpeed"] = true, 
    ["polyVertexIndices"] = true, 
    ["normals"] = true, 
    ["polyNormalIndices"] = true, 
    ["smoothGroups"] = true,  
    ["uvws"] = true, 
    ["polyUvwIndices"] = true, 
    ["materialNames"] = true, 
    ["polyMaterialIndices"] = true, 
    ["objectNames"] = true, 
    ["polyObjectIndices"] = true, 
    ["verticesPerHair"] = true, 
    ["hairVertices"] = true, 
    ["hairVertexSpeed"] = true, 
    ["hairThickness"] = true, 
    ["hairUVs"] = true, 
    ["hairMaterialIndices"] = true, 
    ["hairObjectIndices"] = true, 
    ["buffer"] = true,
    ["transforms"] = true,
}

--------------------------------------------------------------------------------------------------------------
local function printNodes(nodes, printLevel, printType, indent, label)

	if (nodes == nil) then
		return
	end

	-- setup nil args
	printType = printType or 0
	indent = indent or 0
	label = label or ""
	-- build an indent string
	local indent_str = string.rep("  ", indent)
	
	for _, node in ipairs(nodes) do

		-- print node name
		local props = {}
		if (true) then
			local p = octane.node.getProperties( node )
	--		print( indent_str..label..p.name.." : type = "..p.type.." : "..tostring(node))
			print( indent_str..label..p.name.." : type = "..p.type)

			-- node properties
			if (printLevel >= 3 and (printType == 0 or printType == node.type)) then
				print( indent_str.."  PROPERTIES" )
				for n,pp in pairs(p) do
					print( indent_str.."    "..n.." = "..getValueString(pp) )
				end
			end
			props.type = p.type
			props.attributeNames = p.attributeNames
			props.pinNames = p.pinNames
			props.isLinker = p.isLinker
		end
		-- try to be clever with memory usage
		collectgarbage()
		collectgarbage()
		if (DO_PRINT_GC) then
			print(indent_str.."    ".."LUA MEMORY = "..math.floor(collectgarbage("count")).." kb")
		end
		-- node info
		if (printLevel >= 4 and (printType == 0 or printType == node.type)) then
			print( indent_str.."  INFO" )
			local info = octane.node.getNodeInfo(node)
			for n,p in pairs(info) do
				print( indent_str.."    "..n.." = "..getValueString(p) )
			end		
		end
		-- try to be clever with memory usage
		collectgarbage()
		collectgarbage()
		if (DO_PRINT_GC) then
			print(indent_str.."    ".."LUA MEMORY = "..math.floor(collectgarbage("count")).." kb")
		end

		-- fixups 
		local usePinIds = false
		if (props.type == octane.nodeType.NT_GEO_GROUP) then
		-- sometimes GEO_GROUP pin names are 1, 2, 3... 
		-- and that confuses getConnectedNode into thinking the name is an id
		-- see \\DROBO-4\Public\octane_data\instance-tests\scenegraph.ocs
			usePinIds = true	
		end
		-- node attributes
		if ((printLevel >= 5 or printLevel == 1) and (printType == 0 or printType == node.type)) then
			local attribs = props.attributeNames
			if (attribs and #attribs > 0) then
				print( indent_str.."  ATTRIBUTES")
				for _, name in ipairs(attribs) do
				
					local useRaw = attribRawType[name]
					if (useRaw and DO_RAW_ATTRIBS) then
                        print(indent_str.."    "..name.." = USERAW")
					else
						local attrib = octane.node.getAttribute( node, name )
						print( indent_str.."    "..name.." = "..getValueString(attrib))
					end
					-- try to be clever with memory usage
					collectgarbage()
					collectgarbage()	
					if (DO_PRINT_GC) then
						print(indent_str.."    LUA MEMORY = "..math.floor(collectgarbage("count")).." kb")
					end
				end	
			end
		end
		-- node pins
		local pins = props.pinNames
		if (pins and #pins > 0) then
			for i,name in ipairs(pins) do
				local n
				if (usePinIds) then
					n = octane.node.getConnectedNodeIx( node, i)
				else
					n = octane.node.getConnectedNode( node, name)
				end

				local dive = true
				if (n and n.isNode) then
					if (true) then
						local p = octane.node.getProperties( n )
						dive = nodeTypeDontDive[p.type] ~= true or printLevel >= 6
					end
					-- try to be clever with memory usage
					collectgarbage()
					collectgarbage()	
					if (DO_PRINT_GC) then
						print(indent_str.."  LUA MEMORY = "..math.floor(collectgarbage("count")).." kb")
					end
			
					if (dive) then
						printNodes( { n }, printLevel, printType, indent+1, 'PIN "'..name..'" = ')
					elseif (printLevel >= 1 and (printType == 0 or printType == node.type)) then
						local value
					
						if (usePinIds) then
							value = octane.node.getPinValueIx(node, i)
						else
--							print("PIN "..name)
							value = octane.node.getPinValue(node, name)
						end
						print( indent_str.."  "..name.." = "..getValueString(value) )
					end
					-- try to be clever with memory usage
					collectgarbage()
					collectgarbage()		
					if (DO_PRINT_GC) then
						print(indent_str.."  LUA MEMORY = "..math.floor(collectgarbage("count")).." kb")
					end
					
				else
--					print(indent_str.."  "..name.." = NO NODE")
				end
			end
		end
	end
end

--------------------------------------------------------------------------------------------------------------
-- is there a viable rendertarget in the list?
local function checkRenderTarget( nodes)

	for _,node in ipairs(nodes) do
	
		-- if its a rendertarget
		if (node.isNode and node.type == octane.nodeType.NT_RENDERTARGET) then
		
			-- and has a mesh connected
			local n = octane.node.getConnectedNode(node, "mesh")
			if (n) then
				-- use it
				return node
			end
		end
	end
	return nil
end
	
--------------------------------------------------------------------------------------------------------------
-- get the currently selected rendertarget, or the first one connected to a mesh
local function getRenderTarget()

	local nodes = octane.project.getSelection()
	local rt = checkRenderTarget( nodes)
	
	if (not rt) then
		local sceneGraph = octane.project.getSceneGraph( )
		local nodes = octane.nodegraph.findNodes( sceneGraph, octane.nodeType.NT_RENDERTARGET, true )	
		rt = checkRenderTarget( nodes)
	end
	return rt
end

--------------------------------------------------------------------------------------------------------------
-- get the base ancestor of node
local function getRoot( node, indent)

	indent = indent or 0
--[[
	local first = indent == 0
	local indent_str = string.rep("  ", indent)

	local props = octane.node.getProperties( node )
	print( indent_str..props.name.." : type = "..props.type.." : "..tostring(node))

	local pins = props.pinNames
	if (pins and #pins > 0) then
		for i,name in ipairs(pins) do
			local n = octane.node.getConnectedNode( node, name)
			print( indent_str.."  pin "..name.." "..tostring(n))
		end
	end
]]	
	local dn = octane.node.getDestinationNodes( node)
	
	if (dn and #dn > 0) then
		return getRoot( dn[1].node, indent+1)
	end
	return node
end

--------------------------------------------------------------------------------------------------------------
-- find the most useful root nodes
local function getRootNodes()

	-- first pick a selected rendertarget
	-- or the first rendertarget with a mesh input
	local node = getRenderTarget()
	local nodes
	if (node) then
		nodes = { node }
	else
		local sceneGraph = octane.project.getSceneGraph( )
	
		-- otherwise find the root of the mesh nodes in the scene
		nodes = octane.nodegraph.findNodes( sceneGraph, octane.nodeType.NT_GEO_MESH, true )

		-- remove duplicates
		local roots = {}
		for _,n in ipairs(nodes) do
			
			local rn = getRoot( n)
			-- how to create unique keys?
			local props = octane.node.getProperties(rn)
			local key = props.name..tostring(props.type)
--			local key = rn
			roots[key] = rn
		end
		-- build a node array
		nodes = {}
		for _,n in pairs(roots) do
			table.insert(nodes, n)
--			break
		end
		if (#nodes == 0) then
			nodes = octane.nodegraph.findNodes( sceneGraph, octane.nodeType.NT_RENDERTARGET, true )	
		end
		print("NODE COUNT = "..#nodes)
	end
	return nodes
end

-------------------------------------------------------------------------------------------------------------
--printNodes( getRootNodes(), 4, octane.nodeType.NT_IMAGE_RESOLUTION )	
printNodes( getRootNodes(), 5)

print("\nAnimationTimeSpan")
local sceneGraph = octane.project.getSceneGraph( )
local timespan = octane.nodegraph.getAnimationTimeSpan( sceneGraph)
dump( timespan)

--------------------------------------------------------------------------------------------------------------
--------------------------------------------------------------------------------------------------------------