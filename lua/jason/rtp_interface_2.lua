-- rtp_interface.lua
-- Manages loading RTP interface files for panels. Interface files use XML and are usually
-- saved by the Layout Editor tool, however they can be hand-generated for small projects.
-- Currently there should only be one Interface object in an application.
-------------------------------------------------------------------------------
-- $Id: //depot/bin/RTPShell4/scripts/class/rtp_interface_2.lua#47 $
-- $Date: 2014/05/22 $
-- $Change: 47580 $
-- $Author: Bzysberg $
-------------------------------------------------------------------------------

require "xml_parser"
require "psd_parser"

---- Globals ------------------------------------------------------------------

local Class_Data =
{
	name = "Interface",
	super = "Object",
	script = "rtp_interface_2.lua",
	
	omitFromToolbox = true,
	
	-- TODO: Set this to a brief description of your class, this gets shown in a tooltip or status-bar control when your class is selected in layout editor.
	description = "User Interface manager containing definitions of all controls and screens for an application.",

	properties =
	{
		{ name="FileName", datatype="string", default="none", description="XML file name from which this interface is created", edit={ group="Interface", }, },
		{ name="Name", datatype="string", default="Interface1", description="Name of this interface class", edit={ group="Interface", }, },
		{ name="ForceUniqueNames", datatype="boolean", default=true, description="Whether or not to append the interface name to the name of each panel to force them to be unique.", edit={ visible=false } },
		{ name="Size", datatype="vector2", default={x=0, y=0}, description="Size of this interface, defaults to zero size which means use the InterfaceSize field of Applicaton class (for backwards compatibility).", edit={ group="Interface", }, },
		{ name="ScaleToFit", datatype="boolean", default=true, description="Whether or not this interface auto-scales to fit the presenter size.", edit={ group="Interface", }, },
		{ name="StartScreen", datatype="string", default="none", description="XML screen node that is the screen to display by default.", edit={ group="Interface", }, },
	},
}

local Class_PrivateData =
{
	-- Styles: A style is a set of properties for a specific class that just override the defaults for whatever control is using it.
	styles = {},		-- array (use ipairs on this)
	stylesByName = {},	-- key = style name, value = index into styles[]

	-- Controls: A control is an instance of a class derived from Object, when serializing we save only the properties of the class that
	-- differ from the default values for that class.
	controls = {},		-- array (use ipairs on this)
	controlsByName = {},	-- key = control name, value = control
	controlsByType = {},	-- key = control Class name, value = table of controls

	-- Screens: A screen specifies which controls are visible with optional property overrides for each control, kinda like a layer comp in a PSD file.
	screens = {},		-- array (use ipairs on this)
	screensByName = {},	-- key = screen name, value = screen
}

PreRegisterClass(Class_Data, Class_PrivateData)

local SuperClass = _G[Class_Data.super] or {}
_G[Class_Data.super] = SuperClass


---- PostInit -----------------------------------------------------------------
-- This loads the XML file if one was specified in the constructor arguments.
function Interface:PostInit()
	SuperClass.PostInit(self)

	local fname = self:GetFileName()

	if fname ~= "none" then
		-- FileName must be in the asset path
		local fullPath = AssetLib.GetFilePath(fname)
		self:LoadXML(fullPath)
	end
end

---- ImportPSD ----------------------------------------------------------------
-- * filename - string - Name of the PSD file to load (full path name)
-- * imgSavePath - string - Optional - Name of the folder to save PNG images to, if not given PNG images will not be saved.
-- This function parses a PSD file and merges its contents into this interface. Should only be used by
-- the Layout Editor tool.
function Interface:ImportPSD(filename, imgSavePath)
	ParsePSD(filename, self, imgSavePath)
end

---- LoadXML ------------------------------------------------------------------
-- * filename - string - Name of the XML file to load (full path name)
-- * rootPanel - Panel to which all top-level controls defined in the interface will be added to, this is required.
-- * postLoadCallback - function - Optional - Function to call after the XML has been loaded into Lua tables (but before it has been processed).
-- Loads the given XML file into this interface. The XML defines controls and screens that the interface 
-- will display.
function Interface:LoadXML(filename, rootPanel, postLoadCallback)
	printf("LoadXMLInterface %s",filename)
	--collectgarbage("collect")

	local asset_path, fn = SplitPathAndName(filename)
	
	-- Show progress dialog in Layout Editor tool
	if gIsLayoutEditor then
		-- Last param means user can't cancel (maybe eventually...)
		ShowProgressDialog(string.format("Loading Interface '%s'", fn), "Parsing XML...", false)
	end
	
	local xml = XmlParser:ParseXmlFile(filename)
	local iface = nil

	-- Parsing XML Stage: 25% = XML loaded, 50% = Styles parsed, 75% = Controls parsed, 99% = Screens parsed
	UpdateProgressDialog(nil, 25)
	
	if xml then
		-- Parse XML into Lua Tables (organized into tables similar to Class_PrivateData above)
		iface = self:GetInterfaceFromXML( xml, rootPanel )
	else
		printf("Interface.LoadXML: Error parsing file: %s", filename)
	end

	if postLoadCallback then
		-- Caller can use this to perform extra validation on XML before interface gets initialized with it.
		-- Return true to reject the entire interface, or make corrections by editing the first parameter in place.
		if postLoadCallback(self, iface, xml) then
			iface = nil
		end
	end

	if iface then
		-- Create actual class objects from the interface info we loaded
		self:InitializeComponents(iface, rootPanel)

		-- Now that we've successfully loaded the interface, change the FileName property.
		local asset_path, fn = SplitPathAndName(filename)
		self:SetFileName( fn )

		return true
	end

	printf("Interface.LoadXML: Error initializing components!")

	UpdateProgressDialog(nil, 100)	-- close the progress dialog on error!
	
	return false
end

---- InitializeComponents -----------------------------------------------------
-- * iface - Lua tables loaded from XML containing info about the interface.
-- Creates class instances of controls defined in the interface. This should be made into a private helper function.
function Interface:InitializeComponents(iface, root)
	--Load styles
	if iface.styles then
		UpdateProgressDialog("Creating Styles...", 1)
		
		local count = #iface.styles
		
		for i,style in ipairs(iface.styles) do
			-- Convert strings to proper value types for various options (Style nodes need to have ClassName property to indicate the class of the UI element it describes)
			TypeConvertProperties(style)

			-- Styles contain options for a specific class but do not have an instance created for them, used for CSS style
			-- behavior of defining options that can be applied to various controls.
			self:AddStyle(style)
			
			UpdateProgressDialog( nil, math.floor(99 * (i/count)) )
		end
	end

	--Create controls used within this interface
	if iface.controls then
		UpdateProgressDialog("Creating Controls...", 1)
		
		local count = #iface.controls
		
		for i,ctrl in ipairs(iface.controls) do
			-- Convert strings to proper value types for various options
			TypeConvertProperties(ctrl)

			local styleName = ctrl.Style
			
			if styleName and styleName ~= "none" then
				local style = self.stylesByName[styleName]
				
				if style then
					-- Merge the style info into this control properties, without overwriting any however as the control properties override the style.
					table.merge(ctrl, style, false)
				end
			end
			
			local c = CreateClassInstance(ctrl.ClassName, ctrl)

			if c then
				self:AddControl( c )
			end
			
			UpdateProgressDialog( nil, math.floor(99 * (i/count)) )
		end
	end

	-- Post load pass for panels that need to initialize their children
	for _,ctrl in ipairs(self.controls) do
		ctrl:PostLoad()
	end
	
	UpdateProgressDialog( "Creating Screens...", 1 )
	
	-- Load interface screens
	local start_screen = nil

	-- The "<default>" screen is always created first, this holds the default state of the controls (always!)
	local screen = CreateScreen( { Name="<default>" } )
	if gIsLayoutEditor then
		root = Panel_FindByName("LayoutDoc")
	end
	if root then
		screen:SetEntry(root, nil, true, true)
		self.rootPanel = root
	end
	self:AddScreen(screen)
	
	if iface.screens then
		local count = #iface.screens
		
		for i=1,count do
			local screeninfo = iface.screens[i]
			
			screeninfo.ClassName = "Screen"
			TypeConvertProperties(screeninfo)
			screeninfo.ClassName = nil

			screen = CreateScreen( screeninfo )
			
			-- Set the state for each control to the state used in the screen.
			local nentries = #screeninfo.Entries
			for j=1,nentries do
				local entry = screeninfo.Entries[j]
				local panel = Panel_FindByName(entry.Name)
				
				if panel then
					entry.ClassName = panel.ClassName
					TypeConvertProperties(entry)
					entry.ClassName = nil
                                
					screen:SetEntry( panel, entry )
					--printf("Added Entry: %i", iEntry)
				elseif screen:GetDelayBinding() then
					-- Add an entry to the screen by name (not requiring the panel, for delayed binding screens)
					screen:SetEntry( entry.Name, entry )
				else
					printf("Invalid Entry: Panel '%s' not found! (from screen '%s')", tostring(entry.Name), screen:GetName())
				end
			end

			self:AddScreen(screen)

			if self:GetStartScreen() == screen:GetName() then
				start_screen = screen
			end
			
			UpdateProgressDialog( nil, math.floor(99 * (i/count)) )
		end
	end

	-- Default to first screen if none of them are set as the starting screen
	if not start_screen then
		start_screen = self.screens[2]
	end

	if start_screen then
		self:SetScreen( start_screen:GetName() )
	end

	-- Presumably setting the value to 100% will hide the progress dialog since it's finished now
	UpdateProgressDialog(nil, 100)
	
	collectgarbage("collect")
end

---- DoDelayedBinding ---------------------------------------------------------
-- Perform initialization for screens whose panels did not exist at load-time. This should be removed since screens
-- no longer require their panels to exist when they are created.
function Interface:DoDelayedBinding( filename )
	-- Screens already exist but we need to lookup the panels for each entry and convert values to the correct datatypes instead of XML strings.
	for _,screen in ipairs(self.screens) do
		if screen:GetDelayBinding() then
			for i,entry in ipairs(screen.Entries) do
				local panel = Panel_FindByName(entry.Name)
				
				if panel then
					entry.ClassName = panel.ClassName
					TypeConvertProperties(entry)
					entry.ClassName = nil
					
					screen:SetEntry(entry.Name, entry)
				end
			end
		end
	end
end

---- GetInterfaceFromXML ------------------------------------------------------
-- * root - table - Root of the raw XML that was loaded.
-- * rootPanel - The root panel to which top-level controls will be added.
-- Converts table given by XMLParser script into an interface table. This should be a private helper function.
function Interface:GetInterfaceFromXML(root, rootPanel)
	local iface={}
	local rootName = nil

	if rootPanel then
		rootName = rootPanel:GetName()
	end

	-- Allow Interface root tag to contain attributes that get set on the interface object.
	if root.Attributes then
		local iface_props = { ClassName="Interface", }

		for attr,value in pairs(root.Attributes) do
			iface_props[attr]=value
		end

		TypeConvertProperties(iface_props)

		self:SetInfo(iface_props)
	end

	for _,xmlNode in pairs(root.ChildNodes) do
		-- Load styles
		if(xmlNode.Name=="Styles") then
			if iface.styles==nil then
				iface.styles={}
			end
			self:LoadStyles(xmlNode, iface.styles)
		end
		
		-- Load controls
		if(xmlNode.Name=="Controls") then
			if iface.controls==nil then
				iface.controls={}
			end
			-- LoadControls is recursive, so it'd be hard to keep a consistent percentage of completeness without first traversing the whole thing to get a combined count.
			-- We'll just update the progress bar as the top-level controls get parsed instead.
			local count = #xmlNode.ChildNodes
			for i,ctrlNode in pairs(xmlNode.ChildNodes) do
				self:LoadControls(ctrlNode,rootName,iface.controls)
				UpdateProgressDialog( nil, 50 + math.floor(25 * (i/count)) )
			end
		end

		-- Load screens
		if(xmlNode.Name=="Screens") then
			if iface.screens==nil then
				iface.screens={}
			end
			local count = #xmlNode.ChildNodes
			for i,screenNode in pairs(xmlNode.ChildNodes) do
				self:LoadScreen(screenNode,iface.screens)
				UpdateProgressDialog( nil, 75 + math.floor(24 * (i/count)) )
			end
		end
	end

	if self:GetScaleToFit() and rootPanel then
		local rootSizeX, rootSizeY = rootPanel:GetSize()
		local isize = self:GetSize()

		-- If Size=(0,0) then default to interface size defined by the application.
		if isize.x == 0 and isize.y == 0 then
			if gRtpApplication then
				isize = gRtpApplication:GetInterfaceSize()
				self:SetSize(isize)
			end
		end

		if isize.x ~= 0 and isize.y ~= 0 then
			local scaleX = rootSizeX / self.Size.x
			local scaleY = rootSizeY / self.Size.y

			-- TODO: Change Scale to a vector2 type so we can support non-uniform scale, for now just use whichever is the smaller of the two and center
			-- on the other axis.
			if scaleX < scaleY then
				rootPanel:SetScale(scaleX)
				local scaledH = scaleX * self.Size.y
				local posY = (rootSizeY - scaledH) / 2
				rootPanel:SetPosition( {x=0, y=posY} )
			elseif scaleX > scaleY then
				rootPanel:SetScale( scaleY )
				local scaledW = scaleY * self.Size.x
				local posX = (rootSizeX - scaledW) / 2
				rootPanel:SetPosition( {x=posX, y=0} )
			else
				-- Even scale!
				rootPanel:SetScale( scaleX )
			end
		end
	end

	return iface
end

---- CreateControl ------------------------------------------------------------
-- * info - table - Info about the control to create (key/value table containing settings for properties)
-- * container - string - Optional - Name of the parent panel to use for this control if not specified in info (defaults to "LayoutDoc" if not given)
-- This function creates a new control (Panel derived class) and adds it to this interface.
function Interface:CreateControl(info, container)
	info.Visible = info.Visible or true
	info.Container = info.Container or container or "LayoutDoc"

	printf("Creating Object: "..info.Name)

	local c = CreateClassInstance(info.ClassName, info)

	if c then
		self:AddControl(c)
	end

	return c
end

---- LoadScreen ---------------------------------------------------------------
-- * screenNode - table - Table from XMLParser of the screen node being loaded.
-- * screens - array - Screen array to add the new screen info to.
-- This function converts the raw XML for a screen into screen info table that will later be
-- used to create the screen. This is called internally when loading an XML file and should be
-- made private.
function Interface:LoadScreen(screenNode, screens)
	-- For now just store info in tables (screen objects get created in InitializeControls after controls have been created)
	local screen = {}

	-- Copy screen attributes
	for att,value in pairs(screenNode.Attributes) do
		screen[att] = value
	end

	-- Copy entries for the screen
	screen.Entries = {}

	for _,entryNode in ipairs(screenNode.ChildNodes) do
		if entryNode.Name == "Entry" then
			local entry = {}

			for att,value in pairs(entryNode.Attributes) do
				entry[att] = value
			end

			table.insert(screen.Entries, entry)
		end
	end

	table.insert(screens, screen)
end

---- LoadStyles ---------------------------------------------------------------
-- * style_node - table - Table from XMLParser of the Style node being loaded.
-- * styles - array - Styles array to add the new style info to.
-- This converts the raw XML for a Style node into the Lua info table used to construct a style.
-- This is called internally when loading an XML file and should be made private.
function Interface:LoadStyles(style_node,styles)
	local count = #style_node.ChildNodes
	
	-- Styles node contains one child Style node for each style that has attributes for each property in that style, ClassName and Name attributes are required!
	for i,styleNode in ipairs(style_node.ChildNodes) do
		if styleNode.Name == "Style" then
			local style = {}

			for att,value in pairs(styleNode.Attributes) do
				style[att] = value
			end

			if not style.Name then
				printf("Warning: Encountered a style without a Name field, discarding it!")
			elseif not style.ClassName then
				printf("Warning: Style (%s) needs to have a ClassName field, discarding it!", tostring(style.Name))
			else
				table.insert(styles, style)
			end
		else
			printf("Warning: Encountered unexpected node type (%s) in the Styles list!", styleNode.Name)
		end
		
		-- 25-50% from parsing styles
		UpdateProgressDialog( nil, 25 + math.floor(25 * (i/count)) )
	end
end

---- LoadControls -------------------------------------------------------------
-- * control_node - table - Table from XMLParser of the Control node being loaded.
-- * container_name - string - Optional - Name of the container (parent panel) to add this control to.
-- * controls - array - Controls array to add the new control info to.
-- This recursively populates Lua info tables used to construct controls from the XML nodes.
-- This is called internally when loading an XML file and should be made private.
function Interface:LoadControls(control_node,container_name,controls)
	--Load the current ctrl
	local ctrl={}
	ctrl.ClassName=control_node.Name

	if container_name~=nil then
		ctrl.Container=container_name
	end

	--setup ctrl properties
	for attr,value in pairs(control_node.Attributes) do
		--!!Create controls with unique name by appending Interface name to All controls
		--this way we can have multiple interface from one XML file in different states.!!
		if self.ForceUniqueNames and attr=="Name" then
			ctrl[attr]=value.."_"..self:GetName()
		else
			ctrl[attr]=value
		end
	end

	table.insert(controls, ctrl)

	if #control_node.ChildNodes<1 then
		return
	else
		for _,ctrlNode in pairs(control_node.ChildNodes) do
			self:LoadControls(ctrlNode,ctrl.Name,controls)
		end
	end
end

---- SaveInterface ------------------------------------------------------------
-- This is the original code to save this interface to an XML file. The saving code has been revised and
-- moved into scripts specific to the LayoutEditor project, so this should be removed soon.
function Interface:SaveInterface(filename, rootPanel)
	local start_screen = self:GetCurrentScreen()
	local path,fn = SplitPathAndName(filename)
	
	-- Save interface to XML file
	--local fullPath = filename
	local fullPath = path.."__"..fn

	local f = io.open(fullPath, "w")

	if not f then
		printf("SaveInterface(): Error opening file for writing: %s", fullPath)
		return
	end
	
	ShowProgressDialog( string.format("Saving Interface '%s'", fn), "Saving Header...", false )
	
	f:write("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n")

	-- Write out the Interface XML node (don't close because it has child nodes)
	self:WriteXML(f, "", false)

	UpdateProgressDialog( nil, 99 )
	
	if self.styles and #self.styles > 0 then
		f:write("\t<Styles>\n")
		
		UpdateProgressDialog( "Saving Styles...", 1 )
		
		local count = #self.styles
		
		for i,style in ipairs(self.styles) do
			-- Open node with class name
			f:write( "\t\t<Style " )

			-- Alphabetize by property name so they appear in consistent order in the XML
			local frontkeys = { "Name", "ClassName", }
			local keylist = FrontSortKeys(style, frontkeys)

			-- Write out properties as attributes
			for _,name in ipairs(keylist) do
				local val = style[name]
				
				-- Always save class name and style name
				local save = (name == "ClassName") or (name == "Name")
				
				-- Redundancy check here at class level
				if not save then
					local def = self:GetCtrlPropertyDefault(name, "class", nil, nil, nil, style.ClassName)
					-- If default value was found and is the same as this value then we don't need to write it, otherwise we do
					save = not ((def ~= nil) and table.compare(val, def))
				end
				
				if save then
					f:write( string.format("%s=\"%s\" ", name, ValueToXMLString(style, name, val)) )
				end
			end

			-- Close node
			f:write( "/>\n" )
			
			UpdateProgressDialog( nil, 99 * (i/count) )
		end
		
		f:write("\t</Styles>\n")
	end
	
	-- Apply the default screen for the Controls section to be written as the specified default state
	self:SetScreen("<default>")
	
	UpdateProgressDialog( "Saving Controls...", 1 )
	
	f:write("\t<Controls>\n")

	if (not rootPanel) and gIsLayoutEditor then
		rootPanel = Panel_FindByName("LayoutDoc")
	end

	if rootPanel then
		local childCount = rootPanel:GetChildCount()

		for i=1,childCount do
			local child = rootPanel:GetChild(i)

			UpdateProgressDialog( nil, 99 * (i/childCount) )
			
			child:WriteXML(f, "\t\t")
		end
	end

	f:write("\t</Controls>\n")

	UpdateProgressDialog( "Reducing Screens...", 1 )
	
	f:write("\t<Screens>\n")

	-- We need to do one pass through ALL of the screens and for each control save any properties that are different from the Controls section. BUT... before we can do that we
	-- need to support the "<default>" screen that specifies the values for the Controls section without actually writing out that screen itself... then build the list of properties
	-- that differ, and in a second pass through screens for each control write out all of the properties that were found to differ in any screen, these need to be written in every screen.
	local ctrl_fields = {}
	local count = #self.screens
	
	for i,screen in ipairs(self.screens) do
		if screen:GetName() ~= "<default>" then
			local entryCount = screen:GetNumEntries()
			
			for i=1,entryCount do
				local entry = screen:GetEntry(i)
				local fields = ctrl_fields[entry.Name]
				
				if not fields then
					fields = { Name=true, }
					ctrl_fields[entry.Name] = fields
				end
				
				for key,val in pairs(entry) do
					if screen:GetDelayBinding() then
						fields[key] = true
						fields.le_NotEmpty = true
					elseif (not fields[key]) and (key ~= "ClassName") then
						local def = self:GetCtrlPropertyDefault(key, "control", nil, entry.Name)
						
						-- Set the field for this control to true if it's value differs from the default value in this screen
						if not ((def ~= nil) and table.compare(val, def)) then
							fields[key] = true
							fields.le_NotEmpty = true	-- this flag indicates this entry has at least 1 field to be written other than Name (with le_ prefix which we'll reserve for this tool)
						end
					end
				end
			end
		end
		
		UpdateProgressDialog( nil, 99 * (i/count) )
	end
	
	UpdateProgressDialog( "Saving Screens...", 1 )
	
	-- Now at this point ctrl_fields[] can be indexed by control name to get a table whose keys are the names of fields that need to be saved,
	-- so go ahead and save just those fields in each screen for each control.
	for i,screen in ipairs(self.screens) do
		-- Default screen does not get saved to the XML
		if screen:GetName() ~= "<default>" then
			screen:WriteXML(f, "\t\t")

			local entryCount = screen:GetNumEntries()

			for i=1,entryCount do
				local entry = screen:GetEntry(i)
				local panel = Panel_FindByName(entry.Name)
				local fields = ctrl_fields[entry.Name]

				if not fields.le_NotEmpty then
					-- Skip this panel because all of it's properties matches the default values in all screens so it never changes.
				elseif panel or screen:GetDelayBinding() then
					entry.Container = nil
					
					if panel then
						entry.ClassName = panel.ClassName
					end
					
					-- Open node with class name
					f:write( "\t\t\t<Entry " )

					-- Alphabetize by property name so they appear in consistent order in the XML
					local frontkeys = { "Name", "Visible", "Position", "Width", "Height" }
					local keylist = FrontSortKeys(entry, frontkeys)

					-- Write out properties as attributes
					for _,name in ipairs(keylist) do
						if name ~= "ClassName" then
							local val = entry[name]
							local save = fields[name]
							
							if save then
								if panel then
									f:write( string.format("%s=\"%s\" ", name, ValueToXMLString(entry, name, val)) )
								else
									-- No panel indicates a delayed binding so values remain strings in this case and just write them out as they are
									f:write( string.format("%s=\"%s\" ", name, val) )
								end
							end
						end
					end

					-- Close node
					f:write( "/>\n" )
				else
					-- Print the error in the log file (still continue saving however)
					printf("SaveInterface ERROR: Panel not found!: %s (from screen '%s')", tostring(entry.Name), screen:GetName())
				end
			end

			f:write("\t\t</Screen>\n")
		end
		
		UpdateProgressDialog( nil, 99 * (i/count) )
	end

	f:write("\t</Screens>\n")

	f:write("</Interface>\n")
	f:flush()
	f:close()

	os.remove(filename)
	os.rename(fullPath, filename)
	
	-- Close progress dialog now
	UpdateProgressDialog( nil, 100 )
	
	-- Restore the screen that we were at before the save started
	self:SetScreen(start_screen)
	
	-- Last segment is the file name (wait until successful save to change the FileName property)
	local asset_path, fn = SplitPathAndName(filename)
	self:SetFileName( fn )
end

---- SetFileName --------------------------------------------------------------
-- * name - string - Name of the XML file this Interface will be saved to or was loaded from.
-- This sets the FileName property associated with this Interface, really only used by the Layout Editor tool
-- so it knows what file to save to by default when the Interface gets saved.
function Interface:SetFileName(name)
	if self.FileName ~= name then
		self.FileName = name

		if gIsLayoutEditor then
			Window.SetName("Layout Editor - ["..name.."]")
		end
	end
end

---- SetScreen ----------------------------------------------------------------
-- * name - string - Name of the screen to apply.
-- Call this function to apply the given screen to the controls in this interface. Returns true on success or false
-- if a screen with the given name was not found.
function Interface:SetScreen(name)
	printf("SetScreen : %s", name)

	local current_screen = self.screensByName[name]

	if (current_screen) then
		current_screen:Apply()
		self.current_screen = name
		return true
	end

	return false
end

---- GetScreen ----------------------------------------------------------------
-- Returns the name of the current screen (the screen most recently activated by a ''SetScreen'' call).
function Interface:GetScreen()
	return self.current_screen
end

---- GetCtrlByName ------------------------------------------------------------
-- * ctrlName - string - Name of the control to look up.
-- This function returns the Panel object of the control in this interface with the given name, or nil if
-- no control with this name exists in this interface.
function Interface:GetCtrlByName(ctrlName)
	if self.ForceUniqueNames then
		ctrlName=ctrlName.."_"..self:GetName()
	end

	return self.controlsByName[ctrlName]
end

---- GetCtrlsByType -----------------------------------------------------------
-- * className - string - Name of the class for the control type you want to retrieve.
-- This function returns all controls in this Interface that are of the given class. Only controls that are
-- directly of this class type will be returned (does not return controls of a derived class). For example
-- calling this with "Panel" will return only plain Panel controls, not ALL controls in the Interface.
function Interface:GetCtrlsByType(className)
	return self.controlsByType[className]
end

---- GetCtrlPropertyDefault ---------------------------------------------------
-- * propName - string - Name of the class property whose value to retrieve.
-- * level - string - Level at which to look up the default value, searches highest level first then steps to next lower level until found, values can be:
-- ** "class" = Return the default value for the property from the Lua class definition. (lowest level)
-- ** "style" = Return the default value for the property from the Style used by the control.
-- ** "control" = Return the default value for the property from the Controls section of the interface.
-- ** "screen" = Return the default value for the property from the Screens section of the currently active screen (or indicated screen if 4th param is supplied). (highest level)
-- * screenName - string - Optional - Name of the screen to look in when "screen" level is used, defaults to currently active screen of omitted.
-- * ctrlName - string - Name of the control to look for this property in, this is required if the level is anything above "class".
-- * styleName - string - Optional - Name of the style to look in if the "style" level or above is used, if omitted it will default to the style used by the control given in ctrlName.
-- * className - string - Optional - Name of the class to look when searching for a class default value, if omitted it will default to the class used by the control given in ctrlName.
-- This returns the default value of a property for a control, it is used by the Layout Editor tool to help reduce the amount of XML saved by omitting
-- properties that would match the default value from screen definitions and in the controls and styles sections. RTP interactives will likely never use this.
function Interface:GetCtrlPropertyDefault(propName, level, screenName, ctrlName, styleName, className)
	if type(propName) ~= "string" then
		error "GetCtrlPropertyDefault(): ERROR - Invalid propName !!!"
	end
	
	-- Default to "control" level and current screen if searching screens.
	level = level or "control"
	screenName = screenName or self.current_screen
	
	local ctrl
	
	if ctrlName then
		ctrl = self:GetCtrlByName(ctrlName)
	end
	
	-- Style and ClassName come from the control (if not explicitly given but there is a control given)
	if ctrl then
		if not className then
			className = ctrl.ClassName
		end
		if not styleName then
			styleName = ctrl:GetStyle()
			
			if styleName == "none" then
				styleName = nil
			end
		end
	end
	
	-- Check for the value of this property in order of: screen, control, style, class
	if level == "screen" then
		local scrn = self.screensByName[screenName]
		
		if scrn then
			local entry = scrn:GetEntry(ctrlName)
			
			if entry and (entry[propName] ~= nil) then
				return entry[propName]
			end
		end
		
		level = "control"
	end
	
	if level == "control" then
		-- State of controls stored in the "<default>" screen (not yet implemented, so this will just always fall through)
		local scrn = self.screensByName["<default>"]
		
		if scrn and ctrlName then
			local entry = scrn:GetEntry(ctrlName)
			
			if entry and (entry[propName] ~= nil) then
				return entry[propName]
			end
		end
		
		level = "style"
	end
	
	if level == "style" then
		if styleName then
			local style = self.stylesByName[styleName]
			
			if style and (style[propName] ~= nil) then
				return style[propName]
			end
		end
	end
	
	-- "class" level is the default behavior so just always done if we get this far.
	if className then
		local cdata = FindClassData(className)
		
		if cdata.defaultprops[propName] then
			return cdata.defaultprops[propName].default
		end
	end
end

---- GetCurrentScreen ---------------------------------------------------------
-- Returns the currently set screen, this is just another name for the GetScreen function but unfortunately both
-- of them are in use by projects so we'll just keep both functions around for now at least.
function Interface:GetCurrentScreen()
	return self.current_screen
end

---- OnNameChange -------------------------------------------------------------
-- * obj - Object - The Panel or Screen object whose name is being changed.
-- * oldName - string - The previous name of the given Object.
-- * newName - string - The new name the given Object is being assigned.
-- This is called when a Panel or Screen is renamed. The Panel class calls this automatically in the SetName field.
-- The call for Screens are currently being done in code specific to the Layout Editor project. Therefore RTP apps
-- cannot change the name of Screens in an Interface, and while renaming controls would work you, should avoid doing this.
function Interface:OnNameChange(obj, oldName, newName)
	if obj:IsDerivedFrom("Screen") then
		-- A screen is being renamed (just need to update our lookup table, no other dependencies on screen names currently exist)
		self.screensByName[oldName] = nil
		self.screensByName[newName] = obj
	elseif obj:IsDerivedFrom("Panel") then
		-- A panel is being renamed. Update Our Name=>Control LUT
		self.controlsByName[oldName] = nil
		self.controlsByName[newName] = obj

		-- Update this panel in any screens
		for _,screen in ipairs(self.screens) do
			screen:UpdatePanelName(obj, oldName, newName)
		end
		
		-- Any panels that are using us in their "LayoutManager" property need to be updated with our new name.
		-- NOTE: SetLayoutManager uses Panel_FindByName, so this must be called AFTER the new name is applied.
		if #obj.layoutPanels > 0 then
			local layouts = obj.layoutPanels
			obj.layoutPanels = {}
			for _,layout in ipairs(layouts) do
				layout:SetLayoutManager(newName)
			end
		end
	end
end

---- OnStyleRenamed -----------------------------------------------------------
function Interface:OnStyleRenamed(style, oldName, newName)
	-- A style is being renamed (presumably if it's not a screen or panel it must be a style). Update any panels using the style.
	for _,ctrl in ipairs(self.controls) do
		if ctrl:GetStyle() == oldName then
			ctrl:SetStyle(newName)
		end
	end
	
	self.stylesByName[oldName] = nil
	self.stylesByName[newName] = style
end

---- AddControl ---------------------------------------------------------------
-- * obj - Panel - The control to add to this Interface, this should be a Panel or Panel derived class object.
-- Adds this control to this Interface. RTP applications that need to dynamically create controls should call
-- this function on their Interface so that dynamic controls behave properly with regards to screen changes.
-- Panel object will contain an ''Interface'' field that references this Interface it belongs to after this.
function Interface:AddControl(obj)
	if obj then
		-- Add to tables
		table.insert(self.controls, obj)

		local ctrlName = obj:GetName()

		if self.ForceUniqueNames then
			ctrlName = ctrlName.."_"..self:GetName()
		end

		self.controlsByName[ctrlName] = obj

		if not self.controlsByType[obj.ClassName] then
			self.controlsByType[obj.ClassName] = {}
		end

		table.insert(self.controlsByType[obj.ClassName], obj)

		obj.Interface = self
		
		-- Messy, callback function doesn't get called when loading XML otherwise.
		if LE_OnControlCreated then
			LE_OnControlCreated(self, obj)
		end
	end
end

---- DeleteControl ------------------------------------------------------------
-- * obj - Panel - The control (Panel derived class) object to remove from the Interface.
-- * bSkipChildren - boolean - Optional - Pass true for this parameter to delete only the given control. Default behavior is to also delete all child controls.
-- Removes reference to this control from this Interface and also calls Release() on the control.
function Interface:DeleteControl(obj, bSkipChildren)
	if obj then
		local ctrlName = obj:GetName()
		local childCount = obj:GetChildCount()
		
		if LE_OnControlDeleted then
			LE_OnControlDeleted(self, obj)
		end
		
		while childCount > 0 do
			if bSkipChildren then
				-- Unparent the child control from this control we're deleting if we aren't deleting the children. This is used to
				-- allow child panels to persist so they're info can be queried when importing a PSD in psd_parser.lua script.
				obj:GetChild(1):SetContainer(obj:GetContainer())
			else
				self:DeleteControl( obj:GetChild(1) )
			end
			
			childCount = childCount - 1
		end
		
		-- Remove from tables
		self.controlsByName[ctrlName] = nil

		-- NOTE: A crash here may indicate that you've created a panel manually but did not call Interface:AddControl() for it.
		for i,ctrl in ipairs(self.controlsByType[obj.ClassName]) do
			if ctrl == obj then
				table.remove(self.controlsByType[obj.ClassName], i)
				break
			end
		end

		for i,ctrl in ipairs(self.controls) do
			if ctrl == obj then
				table.remove(self.controls, i)
				break
			end
		end

		-- Remove entry for this control from any screens it may have been in
		for _,screen in pairs(self.screens) do
			for j,ctrl in ipairs(screen) do
				if ctrl == ctrlName then
					table.remove(screen, j)
					break
				end
			end
		end

		-- Remove the object's reference to this interface
		obj.Interface = nil

		-- Free memory and everything associated with this control
		obj:Release()
	end
end

---- AddStyle -----------------------------------------------------------------
-- * style - Table - Key/value pairs of properties to assign to controls that use this style.
-- Call this function to dynamically add a new style to the Interface. The ''style'' parameter uses the property
-- name as keys (so, strings) and the value for that property as the value. All properties in the style will get
-- assigned to controls that use this style. It must contain a ''Name'' and ''ClassName'' field, the former being
-- a unique name for this style, the latter being the name of the class (control type) this style can apply to.
-- Controls can use a style by setting the ''Style'' property to the ''Name'' of the style to use.
function Interface:AddStyle(style)
	-- For now just using a basic keyed table, which must have Name field as the unique name used to identify
	-- this style. Eventually we can include a specific class name to which this style can apply and instances
	-- to which it is applied so changing the style can update the instances immediately.  Should probably wrap
	-- Style in a Lua class to implement such functionality...
	if not style.Name then
		printf("AddStyle: ERROR - Style table does not include a Name field.")
		return
	end

	if self.stylesByName[style.Name] then
		-- TODO: Should we merge in this case instead of erroring out?
		printf("AddStyle: ERROR - Style with name '%s' already exists.", style.Name)
		return
	end

	printf("Adding Style: "..style.Name.." ["..style.ClassName.."]")
	
	table.insert(self.styles, style)
	self.stylesByName[style.Name] = style

	if StylesDialog then
		StylesDialog.OnStyleCreated(style.Name)
	end

	return true
end

---- DeleteStyle --------------------------------------------------------------
-- style - Table - The table that was passed to AddStyle of the style to delete.
-- This function removes the given style from this Interface. If you do not retain a reference to style tables after
-- calling AddStyle you can always look them up by name using the GetStyleByName method of this class.
function Interface:DeleteStyle(style)
	if style and style.Name then
		if StylesDialog then
			StylesDialog.OnStyleDestroyed(style.Name)
		end
		
		for i,entry in ipairs(self.styles) do
			if entry == style then
				table.remove(self.styles, i)
				break
			end
		end

		self.stylesByName[style.Name] = nil

		-- TODO: If changing Style into a class call Style:Release() here for any class-specific cleanup.
	else
		printf("DeleteStyle: ERROR - Invalid parameter.")
	end
end

---- UpdateStyle --------------------------------------------------------------
-- Updates the given style and for any fields that have changed also updates any controls using this style that
-- did not have a different control-specific value for that field.
function Interface:UpdateStyle(styleName, info)
	if not gIsLayoutEditor then
		printf("Interface.UpdateStyle: Only supported in LayoutEditor tool.")
		return
	end
	
	local style = self:GetStyleByName(styleName)
	local ctrls = {}
	local cmodified = false
	
	-- Collect controls using this style
	for _,ctrl in pairs(self.controlsByName) do
		if ctrl:GetStyle() == styleName then
			table.insert(ctrls, ctrl)
		end
	end
	
	-- "info" table should only contain entries for modified values in the style that need to be updated.
	for k,v in pairs(info) do
		-- If any controls using this style were using the styles old value then update the property in that control too.
		for _,ctrl in ipairs(ctrls) do
			if (not style[k]) or table.compare(ctrl["Get"..k](ctrl), style[k]) then
				ctrl["Set"..k](ctrl, v)
				cmodified = true
			end
		end
		
		style[k] = v
	end
	
	if cmodified then
		-- This is a function in the Layout Editor, we update the "<default>" screen here which refreshes the state of the properties in the <Controls>
		-- section after a style change that effected at least one of the controls was made. This is required for the style change to effect controls properly.
		UpdateScreen( "UpdateScreen", "<default>" )
	end
end

---- GetStyleByName -----------------------------------------------------------
-- * styleName - string - The name of the style to return.
-- This function returns the table containing key/value properties for the style with the given name, or nil if
-- no style with this name exists in this interface.
function Interface:GetStyleByName(styleName)
	return self.stylesByName[styleName]
end

---- AddScreen ----------------------------------------------------------------
-- * screen - Screen - The Screen class object to add to this interface.
-- This function can be used to dynamically add new screens to this interface. Upon return the screen
-- object will contain an ''Interface'' field that references this interface which it belongs to.
function Interface:AddScreen(screen)
	if not screen then
		printf("AddScreen: ERROR - screen parameter is nil.")
		return
	end

	local screen_name = screen:GetName()

	if not screen_name then
		printf("AddScreen: ERROR - Screen table does not include a Name field.")
		return
	end

	if self.screensByName[screen_name] then
		printf("AddScreen: ERROR - Screen with name '%s' already exists.", screen_name)
		return
	end

	table.insert(self.screens, screen)
	self.screensByName[screen_name] = screen

	screen.Interface = self
	
	-- Messy, but needs to be done here to get called when XML is loaded
	if LE_OnScreenCreated then
		LE_OnScreenCreated(self, screen)
	end
	
	return true
end

---- DeleteScreen -------------------------------------------------------------
-- * screen - Screen - The screen object to delete.
-- This function can be called to remove a screen from this interface. It removes all references to
-- the screen and also calls the ''Release'' function to destroy the screen object.
function Interface:DeleteScreen(screen)
	if screen then
		if LE_OnScreenDeleted then
			LE_OnScreenDeleted(self, screen)
		end
		
		local screen_name = screen:GetName()

		if self.current_screen == screen_name then
			self.current_screen = nil
		end

		for i,entry in ipairs(self.screens) do
			if entry == screen then
				table.remove(self.screens, i)
				break
			end
		end

		self.screensByName[screen_name] = nil

		screen:Release()
	else
		printf("DeleteScreen: ERROR - Invalid parameter.")
	end
end

---- GetSceenByName -----------------------------------------------------------
-- * name - string - The name of the screen to retrieve.
-- This function returns the Screen object for the screen with the given name, or nil if no screen
-- with this name exists in this interface.
function Interface:GetScreenByName(name)
	return self.screensByName[name]
end

---- DeleteAll ----------------------------------------------------------------
-- Deletes everything in this interface, wiping it clean for a new interface file to be loaded into it.
function Interface:DeleteAll()
	while (self.controls[1]) do
		self:DeleteControl(self.controls[1])
	end

	while (self.styles[1]) do
		self:DeleteStyle(self.styles[1])
	end

	while (self.screens[1]) do
		self:DeleteScreen(self.screens[1])
	end
end

---- Animate -----------------------------------------------------------------
-- * elap - Number - Elapsed time (in seconds) since the previous frame.
-- This is called by the Application class each frame, it calls the Animate function of all visible
-- controls, or the ''UpdateVisible'' function of non visible controls. The latter function is used
-- by the HotSpot class I think so they position correctly when visibility is toggled off and back on.
function Interface:Animate( elap)
	if self.controls then
		for _,ctrl in ipairs(self.controls) do

-- TODO !!!
-- turns out GetInfo() && TransferToLua is too expensive for intensive calls like this
--			if (ctrl:GetVisible()) then
			if (ctrl.cppinst:GetVisible()) then

				ctrl:Animate( elap)

			elseif (ctrl.UpdateVisible) then

				ctrl:UpdateVisible()
			end
		end
	end
end

---- DumpScreens -------------------------------------------------------------
-- This is a debugging function that dumps the entries for all screens in the Interface to the
-- debug log file.
function Interface:DumpScreens()
	for _,screen in ipairs(self.screens) do
		screen:DumpEntries()
	end
end

