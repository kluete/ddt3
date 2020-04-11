-- RTP: Panel Class
--
-- This class along with the LuaPanel C++ class represent a panel on the screen,
-- which is a simple 2D container that can contain other panels.  The panel class
-- serves as the core element of all 2D GUI controls.
-------------------------------------------------------------------------------
-- $Id: //depot/bin/RTPShell4/scripts/class/rtp_panel_2.lua#107 $
-- $Date: 2014/07/16 $
-- $Change: 48861 $
-- $Author: jlmaynard $
-------------------------------------------------------------------------------

require "rtp_object_2"
require "pinch_gesture"

gLoadPriority = 0 -- immediate

-- IMPORTANT: These must be in alphabetical order, and C++ code enums must be alphabetized as well, because property pane always sorts lists)
-- Image modes (these are the strings that appear in Layout Editor property pane)
local imageModes = { "aspect", "fit", "normal", "stretch", }

-- Image Horizontal Alignment
local hAligns = { "center", "left", "right", }

-- Image Vertical Alignment
local vAligns = { "bottom", "center", "top", }


---- Panel Class Definition ---------------------------------------------------

local Class_Data =
{
	name = "Panel",
	super = "Object",
	description = "Root object type for a GUI (2D) element",
	script = "rtp_panel_2.lua",

	properties =
	{
		{ name = "Images", datatype = "stringlist", default = {}, description = "List of image filenames this panel uses, typically only one at a time is shown.", edit = { group = "Panel", fileload={"png","jpg","wmv"}, } },
		{ name = "LayoutManager", datatype = "string", default = "none", description = "Name of the panel to call upon to redo the layout when the size changes.", edit = { group = "Panel", } },
		{ name = "Manual", datatype = "boolean", default = false, description = "Whether this panel was hand-edited, and therefore ineligible for deletion during a PSD import.", edit = { group = "Panel", } },
		-- TODO: Remove the Manual flag, Source property being set to Manual (the default) can be the equivalent of having Manual set to true.
		{ name = "Source", datatype = "string", default = "Manual", description = "Where this panel came from, either the name of the PSD file, defaults to Manual indicating not from a PSD.", edit = { group = "Panel", visible = false } },

		-- Instance specific preview handlers
		{ name = "OnPreviewTouch", datatype = "string", default = "none", description = "Name of the script function to call when a touch event preview occurs on this panel.", edit = { group = "Callbacks", } },
		{ name = "OnPreviewClick", datatype = "string", default = "none", description = "Name of the script function to call when a click event preview occurs on this panel.", edit = { group = "Callbacks", } },

		-- Instance specific event handlers
		{ name = "OnTouch", datatype = "string", default = "none", description = "Name of the script function to call when a touch event occurs on this panel.", edit = { group = "Callbacks", } },
		{ name = "OnClick", datatype = "string", default = "none", description = "Name of the script function to call when a click event occurs on this panel.", edit = { group = "Callbacks", } },
		{ name = "ClickParam1", datatype = "string", default = "", description = "First parameter used by click handler.", edit = { group = "Callbacks", } },
		{ name = "ClickParam2", datatype = "string", default = "", description = "Second parameter used by click handler.", edit = { group = "Callbacks", column = 1, } },
		{ name = "OnPinch", datatype = "string", default = "none", description = "Name of the script function to call when a pinch event occurs on this panel.", edit = { group = "Callbacks", } },
		{ name = "OnLayout", datatype = "string", default = "none", description = "Function to call when the size of this panel changes, to handle laying out is child panels.", edit = { group = "Callbacks", }, },
		{ name = "OnKey", datatype="string", default = "none", description = "Name of the script function to call when a key event occurs while this panel has keyboard focus.", edit = { group = "Callbacks", }, },
		{ name = "OnWheel", datatype="string", default = "none", description = "Name of the script function to call when the mouse wheel is moved over this panel.", edit = { group = "Callbacks", }, },
	},

	-- C++ class association
	cpp_name = "LuaPanel",
	cpp_super = "LuaObject",

	cpp_properties =
	{
		{ name = "Visible", datatype = "boolean", default = false, description = "Whether or not this panel is visible.", edit = { group = "Panel" } },
		{ name = "Enabled", datatype = "boolean", default = true, description = "Whether or not this panel is enabled.", edit = { group = "Panel", column=1 } },
		{ name = "ClipChildren", datatype = "boolean", default = false, description = "Whether or not child panels get clipped to the area of this panel.", edit = { group = "Panel" } },
		{ name = "ClipSelf", datatype = "boolean", default = false, description = "Whether or not this panel needs to clip its own content to its bounds.", edit = { group = "Panel", column=1 } },
		{ name = "SizeToContent", datatype = "boolean", default = false, description = "Whether or not this panel automatically adjusts it's size to fit all of it's children.", edit = { group = "Panel" } },
		{ name = "Collidable", datatype = "boolean", default = true, description = "Whether or not this panel is collidable, meaning that it can respond to clicks from the user.", edit = { group = "Panel", column=1 } },
		{ name = "AlwaysTouch", datatype = "boolean", default = false, description = "Whether or not this panel should always have it's touch handlers called even for events that were already handled.", edit = { group = "Panel" } },
		{ name = "RenderToTex", datatype = "boolean", default = false, description = "Otherwise use render-to-texture for rotated panels.", edit = { group = "Panel", column=1} },
		{ name = "LoopMovie", datatype = "boolean", default = true, description = "Whether or not the movie shown in this panel is played in looping mode.", edit = { group = "Panel" } },
		{ name = "AutoPlayMovie", datatype = "boolean", default = true, description = "Whether or not a movie plays automaticaly when this panel is shown and uses a movie for its image.", edit = { group = "Panel", column=1 } },
		{ name = "Preload", datatype = "boolean", default = true, description = "Whether or not this panel loads all of its images when first created, if false images are only loaded when visible.", edit = { group = "Panel" } },

		-- Defaults are stupid instead of zero because the editing controls always alphabetize lists (it's either this or use straight-up numbers for these fields, which would be confusing to end user)
		{ name = "ImageMode", datatype = "number", default = 4, description = "How this panel fits an image into it.", edit = { group = "Panel", list=imageModes, }, },
		{ name = "ImageHAlign", datatype = "number", default = 2, description = "Horizontal alignment of the image in this panel.", edit = { group = "Panel", list=hAligns, }, },
		{ name = "ImageVAlign", datatype = "number", default = 3, description = "Vertical alignment of the image in this panel.", edit = { group = "Panel", list=vAligns, }, },
		
		{ name = "Position", datatype = "vector2", default = { x=0, y=0, }, description = "Position of this panel (top-left coordinate), relative to it's parent or screen if no parent.", edit = { group = "Position", } },
		{ name = "Depth", datatype = "number", default = 0, description = "Sorting depth of this panel, relative to it's parent. Zero is the maximum value (back-most), more negative numbers sort in front.", edit = { group = "Position", } },
		{ name = "SortDepth", datatype = "number", default = 0, description = "Depth value used to allow this panel to sort at the given Z value within the 3D scene.", edit = { group = "Position", column = 1 } },
		{ name = "Width", datatype = "number", default = 0, description = "Width of this panel in pixels, may not be respected by all panel types currently.", edit = { group = "Position" } },
		{ name = "Height", datatype = "number", default = 0, description = "Height of this panel in pixels, may not be respected by all panel types currently.", edit = { group = "Position", column = 1 } },
		{ name = "MinScale", datatype = "number", default = 0.1, description = "Minimum amount of scale this panel can be zoomed down to by pinch zoom gestures, defaults to 0.1 (10%)", edit = { group = "Position" } },
		{ name = "MaxScale", datatype = "number", default = 10, description = "Maximum amount of scale this panel can be zoomed up to by pinch zoom gestures, defaults to 10.0 (1000%)", edit = { group = "Position", column = 1 } },
		{ name = "Scale", datatype = "number", default = 1, description = "Scale of this panel, [0..1) decreases the size, above 1 increases the size.", edit = { group = "Position", } },
		{ name = "Rotation", datatype = "number", default = 0, description = "Rotation angle of this panel, in degrees. Positive values rotate counter-clockwise.", edit = { group = "Position", } },

		{ name = "Color", datatype = "color", default = { a=1,r=1,g=1,b=1 }, description = "Color to tint this panel, defaults to white for no tinting. Alpha value ignored, use Opacity property for transparency.", edit = { group = "Appearance", } },
		{ name = "Opacity", datatype = "number", default = 1, description = "Opacity of this panel, 0 is totally transparent, 1 is fully opaque.", edit = { group = "Appearance", slider = true, min = 0, max = 1, } },
		{ name = "ShowBorder", datatype = "boolean", default = false, description = "Whether or not to draw a border around the bounding rect of this panel.", edit = { group = "Appearance", }, },
		{ name = "BorderColor", datatype = "color", default = { a=1,r=1,g=0,b=0 }, description = "Color of the lines drawn as the border for the panel when visible.", edit = { group="Appearance", }, },
		{ name = "BorderOpacity", datatype = "number", default = 1, description = "Opacity of the lines drawn as the border for the panel when visible.", edit = { group="Appearance", slider = true, min = 0, max = 1, }, },
		{ name = "BorderThickness", datatype = "number", default = 2, description = "Thickness of the lines of the panel border when visible (in pixels).", edit = { group="Appearance", }, },
		{ name = "ImageIndex", datatype = "integer", default = 0, description = "Index in the Images list of the image this panel is displaying (0 is always used for untextured).", edit = { group = "Panel", --[[indexFor="Images", --]]} },

		{ name = "Container", datatype = "string", default = "none", description = "Name of the panel that contains this panel, or none if this is a top-level panel with no parent.", edit = { visible=false } },

		{ name = "PostFx", datatype = "string", default = "none", description = "Name of post effect applied to the viewport.", edit = { visible=false } },
	},

	cpp_funcs =
	{
		{ name = "ResetImageSources", description="Clears out the entire list of images used by this panel.", },
		
		{ name = "IsImageIndexValid", params={ {name="idx", data_type="integer", description="Index of the image to test for validity."}, },
				returnval="boolean",
				description="Return true if the given image index is valid, false if it's out of range.", },

		{ name = "CaptureTouch", params={ {name="tid", data_type="integer", description="ID of the touch event to capture."}, },
				description="Forces events for the given touch id to continue being sent to the current target, even if the touch leaves the target's bounds." },

		{ name = "ReleaseTouch", params={ {name="tid", data_type="integer", description="ID of the touch event to uncapture."}, },
				description="Release the given touch id after a prior CaptureTouch was called, so other panels can once again receive events for the given touch.", },

		{ name = "GetChildCount", returnval="integer", description="Returns the total number of child panels this panel contains. This count includes only direct children of this panel (not children of children).", },

		{ name = "ScreenToLocal", params={ {name="pos", data_type="vector2", description="A screen space position to transform into this panel."}, },
				returnval="vector2",
				description="Transforms a 2D screen coordinate into this panels local space.", },

		{ name = "LocalToScreen", params={ {name="pos", data_type="vector2", description="A local space position in this panel to transform into screen coordinates."}, },
				returnval="vector2",
				description="Transforms a 2D position from this panels local space to screen space.", },

		{ name = "ParentToLocal", params={ {name="pos", data_type="vector2", description="Position in the parents coordinate space to transform into our local space."}, },
				returnval="vector2",
				description="Transforms a 2D position from the parent panels coordinate space to this panels local space.", },

		{ name = "LocalToParent", params={ {name="pos", data_type="vector2", description="A local space position in this panel to transform into our parent panel's coordinate space."}, },
				returnval="vector2",
				description="Transforms a 2D position from this panels local space to this panels parent coordinate space.", },

		{ name = "GetImageSize", params={ {name="idx", data_type="integer", isOptional=true, description="Index of the image whose size to retrieve. If not given, the size of the currently active image is returned."}, },
				returnval="number width, number height",
				description="Returns the width and height of the indicated image.", },
	},

	-- Static class variables
	IdToPanel = {},
	KBFocusOnTouch = false,		-- Whether or not to automatically assign keyboard focus to a panel when it's the target of a "down" touch event (defaults to false only for backward compatibility)
	PanelLibVersion = 4.0,		-- Version number for this panel class (don't bother updating, it's really not used other than one time from RTP3)
}

local Private_Data =
{
	Interface = nil,		-- reference to the interface this panel belongs to (if any, set by Interface class)
	movies = {},
	movie_names = {},
	pinch_gesture = nil,		-- pinch gesture tracker (only gets used if OnPinch function is supplied for this panel)
	radioButtonGroups = {},		-- key=group-name (string), value=array of buttons in that group (uses SetToggleState() to control which one is down)
	layoutPanels = {},		-- array of panels that this panel acts as the layout manager for (so if we get deleted we can clear the reference to them)
}

PreRegisterClass(Class_Data, Private_Data)

local SuperClass = _G[Class_Data.super] or {}
_G[Class_Data.super] = SuperClass


---- Panel_FindByName ---------------------------------------------------------
-- * name - string - Name of the panel to retrieve.
-- Returns the panel object with the given name (names should all be unique)
function Panel_FindByName(name)
	local cpanel = PanelFromName(name)

	if cpanel then
		return Panel_FindById(cpanel:GetID())
	end
end

---- Panel_FindById -----------------------------------------------------------
-- * id - number - Unique id of the panel to retrieve.
-- Returns the panel from a unique id.
function Panel_FindById(id)
	return Class_Data.IdToPanel[id]
end

---- RefreshPanelInfo ---------------------------------------------------------
-- Callable from C++ code to easily refresh given info table for the given panel (by id)
function RefreshPanelInfo(id, info)
	local panel = Panel_FindById(id)

	if panel then
		if not info then
			info = { ClassName=panel.ClassName }
		end

		panel:GetInfo( info, true )
	end

	return info
end

---- Init ---------------------------------------------------------------------
-- First thing called after a new panel is constructed.
function Panel:PostInit()
	-- Add this panel to the LUT to map id to panel.
	local id = self:GetID()
	Class_Data.IdToPanel[id] = self

	-- Object class init
	SuperClass.PostInit(self)
end

---- PostLoad -----------------------------------------------------------------
-- Called once immediately after the interface from which this panel was created has finished loading. This does not
-- get called for manually created panels, only when loading from an Interface.
function Panel:PostLoad()
	-- If a LayoutManager is set but the panel wasn't found then look it up now (this can happen if a panel uses one of it's children as it's layout manager,
	-- because panels get created in parent=>child order the initial lookup when parent is constructed will not find the child because it hasn't been created yet).
	local lmname = self:GetLayoutManager()
	
	if (lmname ~= "none") and not self.layoutManagerObj then
		self.layoutManagerObj = Panel_FindByName(lmname)
		
		if self.layoutManagerObj then
			-- Add ourself to the list of panels that our layout manager is managing
			table.insert(self.layoutManagerObj.layoutPanels, self)
		end
	end
end

---- Release ------------------------------------------------------------------
-- Override of the C++ function to also clear out entry in the id-to-panel LUT.
function Panel:Release()
	-- TODO: Look into why isn't LuaMovie destructor getting called when manually releasing a panel? (MovieViewer tool can be used to test this)
	-- For now just stop any movie that is playing when the panel is released so you won't continue to hear its audio.
	local activeMovie = self:GetMovie()
	
	if activeMovie then
		activeMovie:Stop()
		activeMovie = nil
	end
	
	self.movies = nil
	self.movie_names = nil
	self.radioButtonGroups = nil
	
	-- If we were acting as layout manager for any panels then clear that reference to us since we're being deleted now.
	for _,panel in ipairs(self.layoutPanels) do
		panel:SetLayoutManager("none")
	end
	
	-- Clear out the id to panel LUT
	local id = self:GetID()
	Class_Data.IdToPanel[id] = nil

	-- Release C++ call
	SuperClass.Release(self)
end

---- Get Parent ---------------------------------------------------------------
-- Returns the parent panel of this panel. This is an override of the C++ function to return the panel object instead of just the id.
function Panel:GetParent()
	local parentid = self.cppinst:GetParent()

	if parentid then
		return Panel_FindById(parentid)
	end
end

---- Get Child ----------------------------------------------------------------
-- * idx - number - Number of which child to retrieve (starting at 1 for the first child).
-- Returns the idx'th child panel of this panel. This is an override of the C++ function to return the panel object instead of just the id.
function Panel:GetChild(idx)
	local childid = self.cppinst:GetChild(idx)

	if childid then
		return Panel_FindById(childid)
	end
end

---- Get Sibling --------------------------------------------------------------
-- Returns the next sibling panel of this panel. This is an override of the C++ function to return the panel object instead of just the id.
function Panel:GetSibling()
	local siblingid = self.cppinst:GetSibling()

	if siblingid then
		return Panel_FindById(siblingid)
	end
end

---- Set Name -----------------------------------------------------------------
-- * name - string - New name to use for this panel.
-- Changes the name of this panel. Panels are frequently referred to by name under the assumption that all panels have unique names,
-- this should generally not be called by applications (other than the Layout Editor tool).
function Panel:SetName(name)
	local oldName = self:GetName()

	SuperClass.SetName(self, name)
	
	-- If we belong to an interface it needs to know when our name changes so it can update entries in the screen lists
	if self.Interface then
		self.Interface:OnNameChange(self, oldName, name)
	end
end

---- Set Size -----------------------------------------------------------------
-- * w - number - Width of this panel, in pixels.
-- * h - number - Height of this panel, in pixels.
-- ** Alternatively this function can be called with a single table containing ''w'' and ''h'' fields.
-- Sets the size of the panel. This also invokes the Layout Manager or ''OnLayout'' function to refresh the contents
-- of this panel. Therefore applications should always change the size of a panel by calling this function instead of
-- setting the Width and Height properties directly.
function Panel:SetSize(w, h)
	if type(w) == "table" then
		local t = w
		w, h = t.w, t.h
	end

	self:SetWidth(w)
	self:SetHeight(h)

	-- SetSize calls upon the layout manager (if any) to adjust the child panels of this panel.
	-- Thus most of the time you should call this SetSize function instead of making direct calls
	-- to individual Width and Height functions. Kind of messy but my other ideas for refreshing the
	-- layout involved cPanel::UpdatePos() to trigger a layout update, which would end up in inifinite
	-- recursion however since layout update moves/resizes children which dirties the entire hierarchy.
	if self.layoutManagerObj then
		self.layoutManagerObj:LayoutPanel(self)
	elseif self.fOnLayout then
		self.fOnLayout(self)
	end
end

---- Get Size -----------------------------------------------------------------
-- Returns the width and heigth respectively, as two numbers.
function Panel:GetSize()
	return self:GetWidth(), self:GetHeight()
end

---- Set Layout Manager -------------------------------------------------------
-- * name - string - Name of the panel that is responsible for laying out this panel's child controls.
-- Use this function to assign another panel as the layout manager for this panel, which means it is going to manage
-- laying out our child controls whenever our size changes. This is currently only used by the ''Splitter'' class to
-- resize the two panels the splitter sits between when it is dragged. For general usage though, the panel given here
-- will have its ''LayoutPanel'' method called with this panel as the parameter when its size changes.
function Panel:SetLayoutManager(name)
	if name ~= self.LayoutManager then
		self.LayoutManager = name

		if name == "none" then
			self.layoutManagerObj = nil
		else
			self.layoutManagerObj = Panel_FindByName(name)
			
			if self.layoutManagerObj then
				-- Add ourself to the list of panels that our layout manager is managing
				table.insert(self.layoutManagerObj.layoutPanels, self)
			end
		end
	end
end

---- Get Image Count ---------------------------------------------------------------
-- Return the number of images this panel contains in its image list.
function Panel:GetImageCount()
	return #(self.Images)
end

---- Set Images ---------------------------------------------------------------
-- * images - array - An array of image filenames to include in this panel.
-- Sets the list of images that this panel uses. Note that this is a replacement function, not an appending
-- function, so all the old images are unloaded then the images listed in the given array are loaded in their
-- place. You can cause a movie to play in a panel by simply including a movie filename in the image list here.
function Panel:SetImages(images)
	-- Duplicate check, when SetInfo() is called but images don't change we get called with the same table our current Images field references so just comparing tables refs here is fine.
	--if (images == self.Images) then
	if (table.compare(images, self.Images) and table.compare(self.Images, images)) then
		return
	end

	local idxSave = self:GetImageIndex()

	if (#self.Images > 0) then
		self:ResetImageSources()
		self:SetImageIndex(0)			-- must reset now
	end

	self.Images = images

	for i,img in ipairs(images) do
		local sourceId, movie = nil

		-- Support for image sequences
		local dir = gCommonPath..img
		if (gLocalizedCommonPath) then
			dir = gLocalizedCommonPath..img		-- support for localization path
		end

		if (DirectoryExists( dir)) then

			if (dir ~= self.movie_names[i]) then
				movie = Movie( dir, "rq" )		-- load into ram (preload all images), quickloop
				if (movie) then
					movie:SetRate(24)
				end
				sourceId = self.cppinst:AddImageSource(movie, dir, "rq")	-- NOTE: C++ class holds a reference to movie so we can let ours go out of scope here
				self.movie_names[i] = dir
			end
		-- Support for movie files
		elseif AssetLib.GetAssetType(img) == "movie" then
			local movie_flags = "l"

			if not dir then
				-- capture movie
				dir = img
			else
				dir = AssetLib.GetFilePath(img, "movie")

				if OnGetMovieFlags then
					-- Global function can be implemented to return flags for a movie before it gets constructed (useful for setting up movies as stereoscopic)
					movie_flags = OnGetMovieFlags( img )
				end
			end

			-- Empty movie constructor just creates the container for it but doesn't load the movie yet
			movie = Movie()
			sourceId = self.cppinst:AddImageSource(movie, dir, movie_flags)
			self.movie_names[i] = img
		else
			-- Call C++ directly, bypassing the Lua call prevents attempt to re-add to the Images array.
			sourceId = self.cppinst:AddImageSource(img)
			self.movie_names[i] = nil
		end
		self.movies[i] = movie

		-- Shouldn't need this, by using 0 as untextured in C++ the images that get added here are 1-based (just like Lua)
		assert( sourceId == i )
	end

--	DumpTable("SetImages", images)

	-- Restore the image index that was used before
	if (#self.Images >= idxSave) then
		self:SetImageIndex(idxSave)
	end
end

---- Add Image ----------------------------------------------------------------
-- * image - string - Filename of an image to add.
-- This function loads the given image and adds it to the end of this panels image list.
-- This function does NOT currently support loading movie files instead of images.
function Panel:AddImage(image)
	local sourceId = self.cppinst:AddImageSource(image)

	if sourceId <= 0 then
		printf("Error loading source image: %s", image)
	end

	table.insert(self.Images, image)

	assert( sourceId == #self.Images )
end

---- Set Movie ----------------------------------------------------------------
-- * movie - userdata - C++ Movie object (create with the global Movie() constructor function).
-- This function adds the given movie to this panels image list and sets the panel up to show it.
function Panel:SetMovie( movie)
	local sourceId = self.cppinst:AddImageSource(movie, movie:GetName(), "")
	self.movie_names[sourceId] = movie:GetName()
	self.movies[sourceId] = movie

	self:SetImageIndex(sourceId)

	return sourceId
end

---- Get Movie -----------------------------------------------------------
-- * index - number - Optional - Index of the movie to return (1 based, defaults to current image index if omitted).
-- The function returns the movie at the given index, or nil if either the index is invalid or the
-- image at the given index is not a movie.
function Panel:GetMovie( index)
	if self.movies then
		index = index or self:GetImageIndex()
		return self.movies[index]
	end
end

---- Get Num Images -----------------------------------------------------------
-- Returns the number of images in this panels image list. This does not include image index 0,
-- which is the id used for untextured (solid color).
function Panel:GetNumImages()
	return #self.Images
end

---- Get Max Image Size -------------------------------------------------------
-- Returns the dimensions of the largest image this panel contains in its image list, as two
-- numbers: width, height. Returns 0,0 if the panel contains no images.
function Panel:GetMaxImageSize()
	local count = self:GetNumImages()
	local maxW, maxH = 0, 0
	
	for i=1,count do
		local w, h = self:GetImageSize(i)
		maxW = math.max(w, maxW)
		maxH = math.max(h, maxH)
	end
	
	return maxW, maxH
end

---- Get Min Image Size -------------------------------------------------------
-- Returns the dimensions of the smallest image this panel contains in its image list, as two
-- numbers: width, height. Returns 0,0 if the panel contains no images.
function Panel:GetMinImageSize()
	local count = self:GetNumImages()
	local minW, minH = 99999999, 99999999
	
	if count == 0 then
		return 0, 0	-- this indicates there are no images in the panel
	end
	
	for i=1,count do
		local w, h = self:GetImageSize(i)
		minW = math.min(w, minW)
		minH = math.min(h, minH)
	end
	
	return minW, minH
end

---- Set On Touch -------------------------------------------------------------
-- * funcName - string - Name of the function to call when a touch event occurs on this panel.
-- This can be used to specify a custom touch event handler for this panel.
function Panel:SetOnTouch(funcName)
	if funcName == "" or funcName == "none" then
		self.onTouchFunc = nil
	else
		self.onTouchFunc = LookupGlobalValue(funcName)
		
		-- Verify valid type (some projects are trying to use the "OnTouch" string property for this, which is invalid)
		local ftype = type(self.onTouchFunc)
		
		if ftype ~= "function" then
			printf("Panel[%s].SetOnTouch: Touch handler '%s' is not a function (type=%s)", self:GetName(), funcName, ftype)
			self.onTouchFunc = nil
		end
	end

	self.OnTouch = funcName
end

---- Set On Wheel -------------------------------------------------------------
function Panel:SetOnWheel(funcName)
	if funcName == "" or funcName == "none" then
		self.onWheelFunc = nil
	else
		self.onWheelFunc = LookupGlobalValue(funcName)
		
		-- Verify valid type (some projects are trying to use the "OnTouch" string property for this, which is invalid)
		local ftype = type(self.onWheelFunc)
		
		if ftype ~= "function" then
			printf("Panel[%s].SetOnTouch: Touch handler '%s' is not a function (type=%s)", self:GetName(), funcName, ftype)
			self.onWheelFunc = nil
		end
	end

	self.OnWheel = funcName
end

---- Set On Pinch -------------------------------------------------------------
-- * funcName - string - Name of the function to call when the user performs a pinch gesture on this panel.
-- Calling this with a valid function name enables pinch recognition on this panel and will call the given function
-- when a pinch event occurs.
-- 
-- The function that handles the pinches must take three parameters:
-- * panel - Panel - The panel handling the pinch gesture (typically the same panel that is the target for the pinch).
-- * args - table - Table of key/value arguments containing the following fields:
-- ** perc - number - Percentage that the current pinch length is of it's starting length (0..1 involves a squeeze, > 1 is an expand)
-- ** bStarting - boolean - This is true if this pinch gesture is just started, false if this is an update.
-- ** pinchFocus - vector2 - The midpoint of the line between the two end points of the pinch gesture when it started, in local space of the panel being pinched.
function Panel:SetOnPinch(funcName)
	if funcName == "" or funcName == "none" then
		self.HandlePinch = nil
		self.pinch_gesture = nil
	else
		self.HandlePinch = LookupGlobalValue(funcName)

		if not self.pinch_gesture then
			self.pinch_gesture = PinchGesture:New(self)
		end
	end

	self.OnPinch = funcName
end

---- Set On Key ---------------------------------------------------------------
-- * funcName - string - Name of the function to call when the user presses a key on the keyboard while this panel has keyboard focus.
-- Use this key handler function to handle keyboard input for a specific panel. The function takes the panel as the first parameter and
-- an argument table as the second parameter containing the following fields:
-- * keyState - string - State of the key, one of:
-- ** "up" - The key was released.
-- ** "down" - The key was pressed down.
-- ** "focus" - The keyboard focus state changed, in this case the ''focus'' argument is used instead of the ''key'' argument.
-- * key - string - Name of the key that was pressed or released if ''keyState'' is "up" or "down"
-- * focus - boolean - Whether we gained (true) or lost (false) keyboard focus, if ''keyState'' is "focus".
function Panel:SetOnKey(funcName)
	if funcName == "" or funcName == "none" then
		self.onKeyFunc = nil
	else
		self.onKeyFunc = LookupGlobalValue(funcName)
	end

	self.OnKey = funcName
end

---- Set On Click -------------------------------------------------------------
-- * funcName - string - Name of the function to call when a Click event occurs on this panel.
-- This is used to handle Click events after a panel is clicked on. Note that only ''Button'' panels (or panel types
-- derived from Button) will generate Click events, however this property is in the root ''Panel'' class because it's
-- not uncommon for a parent Panel to be the one to handle clicks on the buttons it contains. The click handler has two
-- extra parameters preceding the standard event args in it, so the function signature should look like:
--  function MyOnClick(panel, param1, param2, args)
-- Where ''param1'' and ''param2'' are the ''ClickParam1'' and ''ClickParam2'' fields of the panel respectively. The
-- ''param1'' and ''param2'' parameters will likely be removed in a future revision so that all event handlers have
-- a consistent function signature. There's no reason you can't just call ''panel:GetClickParam1()'' instead.
function Panel:SetOnClick(funcName)
	if funcName == "" or funcName == "none" then
		self.onClickFunc = nil
	else
		self.onClickFunc = LookupGlobalValue(funcName)
	end

	self.OnClick = funcName
end

---- Set On Layout ------------------------------------------------------------
-- * funcName - string - Name of the function to call when the layout for this panel needs to be redone.
-- The given function will be called when this panel is resized and should update positions and/or sizes of
-- the child panels it contains to fit properly at the new panel size. The ''LayoutManager'' property has priority
-- over the ''OnLayout'' function, but both are optional.
function Panel:SetOnLayout(funcName)
	if funcName == "" or funcName == "none" then
		self.fOnLayout = nil
	else
		self.fOnLayout = LookupGlobalValue(funcName)
	end
	
	self.OnLayout = funcName
end

---- Set On Preview Touch -----------------------------------------------------
-- * funcName - string - Name of the function to call to preview a touch event on this panel.
-- This function gets called with touch arguments during the preview pass of a touch event.
function Panel:SetOnPreviewTouch(funcName)
	if funcName == "" or funcName == "none" then
		self.onPreviewTouchFunc = nil
	else
		self.onPreviewTouchFunc = LookupGlobalValue(funcName)
	end

	self.OnPreviewTouch = funcName
end

---- Set On Preview Click -----------------------------------------------------
-- * funcName - string - Name of the function to call to preview a click event on this panel.
-- This function gets called during the preview pass of a click event.
function Panel:SetOnPreviewClick(funcName)
	if funcName == "" or funcName == "none" then
		self.onPreviewClickFunc = nil
	else
		self.onPreviewClickFunc = LookupGlobalValue(funcName)
	end

	self.OnPreviewClick = funcName
end

---- Dump ---------------------------------------------------------------------
-- * indent - string - Optional - String that gets prepended to lines logged by this function.
-- This function recursively dumps information about this panel and the panels it contains, the indent
-- string uses an increasing number of spaces so child panels are further indented than their parents.
-- The indent string will default to a blank string if not given.
function Panel:Dump(indent)
	if not indent then
		indent = ""
	end

	local pos = self:GetPosition()
	local imgidx = self:GetImageIndex()

	printf("%s%s[%s]: (%i,%i,%i) [%ix%i] v:%s/%s i:%i(%s)", indent, self:GetName(), self.ClassName, pos.x, pos.y, self:GetDepth(), self:GetWidth(), self:GetHeight(), self:GetVisible(), self:GetVisible(true), imgidx, tostring(self.Images[imgidx]))

	local count = self:GetChildCount()

	indent = indent.."  "

	for i=1,count do
		local child = self:GetChild(i)

		if child then
			child:Dump(indent)
		end
	end
end

---- PreviewTouch -------------------------------------------------------------
-- Called during the preview phase of a touch event, calls the ''OnPreviewTouch'' function if any.
function Panel:PreviewTouch(args)
	if self.onPreviewTouchFunc then
		return self.onPreviewTouchFunc(self, args)
	end
end

---- HandleTouch --------------------------------------------------------------
-- Does some default behavior to handle a touch on a panel, this involves running the pinch gesture recognizer
-- if necessary, updating keyboard focus, and calling the ''OnTouch'' function if one is given. Derived classes
-- should always call the SuperClass version first in order to get this default behavior and process it only if
-- this function returns false. A return value of true indicates the touch was already handled by this function.
function Panel:HandleTouch(args)
	local ret = false

	-- Pass touch events to pinch gesture tracker (when a pinch gesture returns true we stop calling the OnTouch handler so it won't process drags while pinching)
	if self.pinch_gesture then
		if self.pinch_gesture:HandleTouch(args) then
			return true
		end
	end

	if not ret then
		-- This is a global flag that when true sets keyboard focus to a panel as soon as it starts being clicked on
		--
		-- TODO: Maybe panel could have a flag that says "never give me keyboard focus" or keyboard focus could be locked to the current panel that has it... so
		-- we might need to put more conditions in here before changing the focus, we can add that stuff when a need for it comes up though.
		if Panel.KBFocusOnTouch and args.type == "down" then
			SetKeyboardFocus(args.target)
		end
		
		if args.type == "wheel" then
			if self.onWheelFunc then
				if self.onWheelFunc(self, args) then
					return true
				end
			end
		else
			if self.onTouchFunc then
				if self.onTouchFunc(self, args) then
					return true
				end
			end
		end
	end
	
	return false
end

---- HandleKey ----------------------------------------------------------------
-- Called when a keyboard event occurs while this panel has keyboard focus, calls the ''OnKey'' function if any.
function Panel:HandleKey(args)
	if self.onKeyFunc then
		return self.onKeyFunc(self, args)
	end
end

---- PreviewClick -------------------------------------------------------------
-- Called during the preview phase of a click event on this panel, calls the ''OnPreviewClick'' function if any.
function Panel:PreviewClick(args)
	if self.onPreviewClickFunc then
		return self.onPreviewClickFunc(self, args)
	end
end

---- HandleClick --------------------------------------------------------------
-- Called when a click event occurs on this panel, calls the ''OnClick'' function if any.
function Panel:HandleClick(args)
	if self.onClickFunc then

--		return self.onClickFunc(self, args)
		return self.onClickFunc(self, self.ClickParam1, self.ClickParam2, args)
	end
end

---- WriteXML -----------------------------------------------------------------
-- This is an override from the ''Object'' class for custom Panel XML support. We always set the
-- ShowBorder property to false before writing, and the ''bAutoClose'' flag is ignored for panels
-- since whether the XML node contains child nodes depends on whether or not there are child panels.
function Panel:WriteXML(f, indent, bAutoClose)
	local nchildren = self:GetChildCount()

	local saveShowBorder = self:GetShowBorder()
	
	self:SetShowBorder(false)

	if nchildren == 0 then
		-- Close node
		SuperClass.WriteXML(self, f, indent, true)
	else
		-- Open node
		SuperClass.WriteXML(self, f, indent, false)

		-- Write out child panels
		for i=1,nchildren do
			local child = self:GetChild(i)
			child:WriteXML( f, indent.."\t" )
		end

		-- Close node
		f:write( string.format("%s</%s>\n", indent, self.ClassName) )
	end

	self:SetShowBorder(saveShowBorder)
end

---- Animate ------------------------------------------------------------------
-- * elap - number - Number of elapsed seconds since the previous frame.
-- This is called each frame to update any realtime animations on the panel.
function Panel:Animate( elap)
	local nchildren = self:GetChildCount()

	SuperClass.Animate(self, elap)

	for i=1,nchildren do
		local child = self:GetChild(i)
		child:Animate( elap )
	end
end

---- DropFiles ----------------------------------------------------------------
-- * files - array(?) - A table whose value are filenames of the file dropped (presumably an array with number keys).
-- * pos - vector2 - Screen position of where the file was dropped at.
-- This is used exclusively in the LayoutEditor tool as an attempt to support drag-n-drop of files into the tool to add
-- images to the panel. Will likely be removed and/or reworked in a future revision of the tool.
function Panel:DropFiles(files, pos)

	DumpTable("files", files)
	--DumpTable("Images", self.Images)

	local imagefiles = table.deepcopy(self.Images)
	local hitindex = -1
	for _,p in pairs(files) do

		local path, name, ext = SplitPathNameExt(p)

        --print("Dropping image file " .. name .. "." .. ext)

		if (ext == "jpg" or ext == "png" or ext == "tga") then

			local ne = name.."."..ext
			local found = false

			-- check existing
			for i,n in pairs(imagefiles) do

				if (n == ne) then
					found = true

                    print("dropped image file "..ne)

                    table.remove( imagefiles, i)
                    hitindex = i
					break
				end
			end
		end
	end
	--DumpTable("Images", imagefiles)
	if (hitindex ~= -1) then
		self:SetImages(imagefiles)
		self:SetImageIndex(hitindex)
	end
end

---- GetPositionCentered ------------------------------------------------------
-- Returns the center point of this panel in it's parent's coordinate space.
function Panel:GetPositionCentered()
	local pos = self:GetPosition()
	local wid = self:GetWidth()/2
	local hgt = self:GetHeight()/2

	return { x = pos.x + wid, y = pos.y + hgt }
end

---- SetPositionCentered  -----------------------------------------------------
-- * x - number - Horizontal position of this panel relative to its parent (in pixels)
-- * y - number - Vertical position of this panel relative to its parent (in pixels)
-- * clip - boolean - Optional - Pass true for this parameter to prevent this panel from going outside of it's parents bounds.
-- Sets the position of this panel by specifying where you want the center of the panel to be (instead of the top-left).
function Panel:SetPositionCentered( x, y, clip)

	local wid = self:GetWidth()
	local hgt = self:GetHeight()

	x = x - wid/2
	y = y - hgt/2

	if (clip) then

		if (x < 0) then
			x = 0
		end
		if (y < 0) then
			y = 0
		end

		local container = Panel_FindByName(self:GetContainer())
		if (container) then
			local cwid = container:GetWidth()
			local chgt = container:GetHeight()

			if (x > cwid-wid-1) then
				x = cwid-wid-1
			end
			if (y > chgt-hgt-1) then
				y = chgt-hgt-1
			end
		end
	end

	self:SetPosition( { x = x, y = y } )
end

---- SetPositionAlign ---------------------------------------------------------
-- * x - number - Horizontal position of this panel relative to its parent (in pixels).
-- * y - number - Vertical position of this panel relative to its parent (in pixels).
-- * align - string - Alignment flags, this string contains one or more of the following substrings:
-- ** "left" - X position defines where to put the left edge of this panel.
-- ** "right" - X position defines where to put the right edge of this panel.
-- ** "top" - Y position defines where to put the top edge of this panel.
-- ** "bottom" - Y position defines where to put the bottom edge of this panel.
-- * offsetx - number - Optional - Specific number of pixels to offset the panel horizontally.
-- * offsety - number - Optional - Specific number of pixels to offset the panel vertically.
-- * clip - boolean - Optional - Pass true for this parameter to prevent this panel from going outside of it's parents bounds.
-- This sets the position of this panel based on a specified edge of this panel. This is used by the HotSpot class to support
-- hotspots that are anchored to their 3D attachment differently instead of always having to be aligned at the top-left.
function Panel:SetPositionAlign( x, y, align, offsetx, offsety, clip)

	local wid = self:GetWidth()
	local hgt = self:GetHeight()

	if (string.find(align, "left")) then
		if (offsetx) then
			x = x - offsetx
		end
	elseif (string.find(align, "right")) then
		x = x - wid

		if (offsetx) then
			x = x + offsetx
		end
	else
		x = x - wid/2
		-- Offsets were not being used previously.
		if (offsetx) then
			x = x - offsetx
		end
	end

	if (string.find(align, "top")) then
		if (offsety) then
			y = y - offsety
		end
	elseif (string.find(align, "bottom")) then
		y = y - hgt

		if (offsety) then
			y = y + offsety
		end
	else
		y = y - hgt/2
		-- Offsets were not being used previously.
		if (offsety) then
			y = y - offsety
		end
	end

	if (clip) then

		if (x < 0) then
			x = 0
		end
		if (y < 0) then
			y = 0
		end

		local container = Panel_FindByName(self:GetContainer())
		if (container) then
			local cwid = container:GetWidth()
			local chgt = container:GetHeight()

			if (x > cwid-wid-1) then
				x = cwid-wid-1
			end
			if (y > chgt-hgt-1) then
				y = chgt-hgt-1
			end
		end
	end

	self:SetPosition( { x = x, y = y } )
end

---- OnCloseButton ------------------------------------------------------------
-- Function intended to be used as a generic click handler that hides the parent panel of the button. This is
-- an interesting idea, only used by the Panel3d test application currently.
function Panel:OnCloseButton( args)

	printf("OnCloseButton %s", self:GetName())

	local panel=Panel_FindByName(self:GetContainer())

	panel:SetVisible(false)
end

---- _DuplicatePanel ----------------------------------------------------------
-- Helper function to create a duplicate of the given panel and optionally duplicate all of its
-- children as well.
local function _DuplicatePanel( panel,suffixId,doChildren )
	local nameSuffix = "_"..tostring( suffixId )

	-- Copy the panel info
	local info = {}
	panel:GetInfo( info,true )
	assert( info.Name )

	-- Add suffix to any string value that is the name of a panel
	for k,v in pairs( info ) do
		-- If we're not duplicating children, we still need to ensure that self-references are kept intact
		if( doChildren or v == info.Name ) then
			if( type(v) == "string" and v ~= "true" and v ~= "false" ) then	-- don't bother looking up 'true' and 'false'
				if( Panel_FindByName(v) and v ~= "AppPanel" and v ~= "LayoutDoc" ) then	-- ignore special cases: AppPanel and LayoutDoc are special panels that won't ever get duplicated
					-- the value is a string panel name...we assume that this is a self-reference
					info[k] = v..nameSuffix
				end
			end
		end
	end

	-- Create the duplicate panel (has no children, yet)
	local dupe = CreateClassInstance( info.ClassName,info )

	if( doChildren ) then
		-- Each group (with the same suffixId) will reside at the same depth.
		-- This helps avoid odd depth changes when changing panels' visibility.
		local depth = panel:GetDepth()
		if( depth ~= 0 ) then
			depth = depth - suffixId
			dupe:SetDepth( depth )
		end

		-- Create and add the duplicate children to the duplicate panel
		for i = 1,panel:GetChildCount() do
			local child = panel:GetChild( i )
			local dupChild = _DuplicatePanel( child,suffixId,doChildren )
			dupChild:SetContainer( info.Name )
		end
	end

	return dupe
end

---- Duplicate ----------------------------------------------------------------
-- * copyCount - number - Optional - How many duplicates of this panel to create (1 if not given).
-- * doChildren - boolean - Optional - Pass true if you want to duplicate all of the child panels as well.
-- Creates copies of this panel. Names and panel references will be appended with "_<n>",
-- where '<n>' is a value in [1,copyCount]. Returns an array of newly created panels, or
-- just the new panel if 'copyCount' is 1.
function Panel:Duplicate( copyCount,doChildren )
	copyCount = copyCount or 1
	doChildren = ( doChildren == nil ) and true
	local panelList = {}
	for i = 1,copyCount do
		local dupPanel = _DuplicatePanel( self,i,doChildren )
		table.insert( panelList,dupPanel )
	end

	-- Don't return a table if it's just the one panel...just return the panel.
	if( copyCount == 1 ) then
		return panelList[1]
	end
	return panelList
end

---- AddRadioButton -----------------------------------------------------------
-- * button - Button - The Button object to add to the given radio group in this panel (should be a child of this panel).
-- * groupName - string - Name identifier of the radio group to add this button to (will be created if this is the first one in the group).
-- This function adds the given button to the radio group with the given name and enables logic to treat it as a radio button
-- instead of a standard push or toggle button.
function Panel:AddRadioButton(button, groupName)
	local list = self.radioButtonGroups[groupName]
	
	if not list then
		list = {}
		self.radioButtonGroups[groupName] = list
	end
	
	-- Double-check to make sure this button is not already in this list.
	for _,btn in ipairs(list) do
		if btn == button then
			printf("Button[%s]: Already in radio group '%s', ignoring duplicate add.", button:GetName(), groupName)
			return
		end
	end
	
	table.insert(list, button)
	
	-- Radio buttons are always toggle
	button:SetToggle(true)
	button:SetToggleState(false)
end

---- RemoveRadioButton --------------------------------------------------------
-- * button - Button - The Button object to remove from the radio group.
-- * groupName - string - The name of the radio group the button is being removed from.
-- This function removes the given button from the given radio group. The button will retain
-- toggle functionality after being removed, call SetToggle(false) if you want it to become
-- a push button instead.
function Panel:RemoveRadioButton(button, groupName)
	local list = self.radioButtonGroups[groupName]
	
	if not list then
		return
	end
	
	local btnindex
	
	for i,btn in ipairs(list) do
		if btn == button then
			btnindex = i
			break
		end
	end
	
	if btnindex then
		table.remove(list, btnindex)
	else
		printf("Button[%s]: Not in radio group '%s', ignoring remove.", button:GetName(), groupName)
	end
end

---- SelectRadioButton --------------------------------------------------------
-- * groupName - string - Name of the radio group to set the selected button in.
-- * button - The button to set as the selected button in the group, can be of types:
-- ** number - The index of the button in the group to select.
-- ** string - The name of the button to select.
-- ** Button - The button object to select.
-- * bNoState - boolean - Optional - Pass true for this to skip the SetToggleState() calls on the button (used internally)
-- Sets which button in the given radio group is selected, since all in the group must be radio buttons
-- the current selection (if any) will be deselected first then this button will be selected.
function Panel:SelectRadioButton(groupName, button, bNoState)
	local list = self.radioButtonGroups[groupName]
	
	if not list then
		printf("SetRadioButton: Invalid radio group '%s'!", groupName)
		return
	end
	
	if type(button) == "number" then
		-- select button by index
		button = list[button]
	elseif type(button) == "string" then
		-- select button by name
		for _,btn in ipairs(list) do
			if btn:GetName() == button then
				button = btn
				break
			end
		end
	end	-- assuming button object if not a number or string
	
	if list.curSel ~= button then
		if not bNoState then
			if list.curSel then
				list.curSel:SetToggleState(false)
			end
			
			button:SetToggleState(true)
		end
		
		list.curSel = button
	end
end

---- FakeClick ----------------------------------------------------------------
-- * localX - number - X coordinate in the panel to fake a click at.
-- * localY - number - Y coordinate in the panel to fake a click at.
-- * bFullPath - boolean - Optional - Pass true for this param to send fake touch events through entire hierarchy instead of only to this panel (not yet implemented)
-- Fakes a 'down' touch event immediately followed by an 'up' touch event on this panel to fake a click.
-- The fake touch events will always use a touch ID of 999, which should never collide with a real touch id.
-- Currently this just calls the panel's ''HandleTouch'' method directly, passing the fake events through the
-- entire hierarchy is not yet supported (mainly due to the need to compute per-panel local coordinates).
function Panel:FakeClick(localX, localY, bFullPath)
	local args = { type="down", tid=999, local_x=localX, local_y=localY, target=self, inside=true }
	self:HandleTouch( args )
	
	args.type = "up"
	self:HandleTouch( args )
end
