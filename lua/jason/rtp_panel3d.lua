-- Panel3d
-- Class for a panel that can render 3D content.
-------------------------------------------------------------------------------
-- $Id: //depot/bin/RTPShell4/scripts/class/rtp_panel3d.lua#77 $
-- $Date: 2014/05/19 $
-- $Change: 47468 $
-- $Author: jlmaynard $
-------------------------------------------------------------------------------

require "rtp_panel_2"

gPanel3dMeshFlags = "b"
gPanel3dAnimate = true

---- Globals ------------------------------------------------------------------

local Class_Data =
{
	name = "Panel3d",
	super = "Panel",
	description = "Panel that can show 3D content in it.",
	script = "rtp_panel3d.lua",

	properties =
	{
		{ name="XmaxFiles", datatype = "stringlist", default = {}, description = "List of xmax filenames this panel can display.", edit = { group = "Panel3d", fileload="xmax" } },
		{ name="XmaxIndex", datatype = "integer", default = 1, description = "Index in the Xmax list of the xmax this panel is displaying.", edit = { group = "Panel3d", indexFor="XmaxFiles" } },
		{ name="LoadCallback", datatype = "string", default = "none", description = "Name of script function to call when an xmax is added.", edit = { group = "Callbacks", } },
		{ name="MouseInstFlag", datatype="boolean", default=false, description="Mouse drags will rotate the model if true or the camera if false.", edit={ group="Panel3d", column=-1 } },
		{ name="FocusInst", datatype="string", default="<root>", description="The object that mouse drags will affect.", edit={ group="Panel3d", column=1, nolabel=true } },
		{ name="AnimateFlag",  datatype="boolean", default=true, description="Should it animate or not.", edit={ group="Panel3d", column=-1, } },
		{ name="FrameRate", datatype="number", default=30, description="Number of frame per second to animate at.", edit={ group="Panel3d", column=1, nolabel=true, } },

		{ name="Camera", datatype="string", default="", description="Name of the camera node to view this scene through.", edit={ group="Camera", } },
		{ name="LiveCam", datatype="boolean", default=true, description="Flag indicating if camera is live rotatable.", edit={ group="Camera", } },
		{ name="CameraOverride", datatype="boolean", default=false, description="If true then this panels camera settings override camera settings in the XMAX file, defaults to false so panel camera settings are only used for the default camera.", edit={ group="Camera", column=1 }, },
		{ name="LockAlpha", datatype="boolean", default=false, description="Enable this to lock the alpha angle (preventing horizontal rotation).", edit={ group="Camera", } },
		{ name="LockBeta", datatype="boolean", default=false, description="Enable this to lock the beta angle (preventing vetical rotation).", edit={ group="Camera", column=1, } },
		{ name="OpeningAngle", datatype="number", default=60, description="The vertical opening angle of the camera.", edit={ group="Camera", slider = true, min = 10, max = 120 } },
		{ name="Zoom", datatype="number", default=1, description="The camera zoom level.", edit={ group="Camera", } },
		{ name="Hither", datatype="number", default=-1, description="The nearplane distance of the camera.", edit={ group="Camera", } },
		{ name="Yon", datatype="number", default=-1, description="The farplane distance of the camera.", edit={ group="Camera", } },
		{ name="CameraOffset", datatype="vector3", default={x=0,y=0,z=0}, description="Offset to apply to the camera to move the target away from the center of the panel.", edit={ group="Camera", } },
		{ name="WheelZoomFactor", datatype="number", default=20, description="How many world units the camera zooms in or out with one click of the mouse wheel.", edit={ group="Camera", } },
	},

	cpp_name = "LuaPanel3d",
	cpp_super = "LuaPanel",
	cpp_properties =
	{
		{ name = "ClearBackground", datatype = "boolean", default = true, description = "Whether or not this panel clears to the background.", edit = { group = "Panel3d", } },
		{ name = "ForceSubView", datatype = "boolean", default = false, description = "Whether or not this panel has its own viewport.", edit = { group = "Panel3d", } },

	},
	cpp_funcs =
	{
		{ name="Release", description="Releases all of the resources associated with this panel including meshes and cameras.", },

		{ name="ReleaseAllInstances", description="Releases all instances in this panel's scene, does not release loaded resources though.", },

		{ name="AddInstance", params={	{name="instance", data_type="(string/Instance)", description="Name of XMAX file to load or direct Instance object to add."},
						{name="flags", data_type="string", isOptional=true, description="Flags for Mesh constructor when instance is a string."},
						{name="priority", data_type="number", isOptional=true, description="Load priority for Mesh when instance is a string."}, },
					returnval="Instance",
					description="Adds a new 3D Instance to the scene this panel is rendering.", },

		{ name="SetCamera", params={	{name="camera", data_type="(string/Instance)", description="Name of camera to activate or direct Instance object of the camera."}, },
					returnval="boolean",
					description="Sets the active camera used to view this panel's scene through.", },

		{ name="GetProjection", params={{name="x", data_type="number", description="X coordinate in panel to map into 3D space."},
						{name="y", data_type="number", description="Y coordinate in panel to map into 3D space."},
						{name="z", data_type="number", description="Depth at which to project the given screen coordinate to in the world."}, },
					retrunval="boolean vis, number x, number y, number z",
					description="Returns the position in this panel that a 3D coordinate maps to and whether or not it's visible.", },

		{ name="GetPosAtDepth", params={{name="x", data_type="number", description="X coordinate in world space to map to this panel."},
						{name="y", data_type="number", description="Y coordinate in world space to map to this panel."},
						{name="z", data_type="number", description="Z coordinate in world space to map to this panel."}, },
					returnval="number x, number y, number z",
					description="Returns the world space position that the given point in this panel maps to at the given depth.", },

		{ name="FindInstance", params={	{name="name", data_type="string", description="Name of the instance to find in this panel."},
						{name="multiple_flag", data_type="boolean", isOptional=true, description="Whether to return multiple instances (true) or just the first one found (false)."}, },
					returnval="(Instance/table)",
					description="Returns the instance with the given name. If ''multiple_flag'' is true a table of all instances matching the given name is returned, key=InstanceName, value=Instance", },

		{ name="Pick", params={	{name="x", data_type="number", description="X coordinate in the panel to hit test."},
					{name="y", data_type="number", description="Y coordinate in the panel to hit test."},
					{name="collgroups", data_type="integer", isOptional=true, description="Number containing bit mask of collision groups to test for."},
					{name="debug", data_type="boolean", isOptional=true, description="If true verbose debug output will be enabled on this specific collision test."}, },
					returnval="Instance inst, number collgroup, vector3 worldPos",
					description="Performs a hit test for a 3D instance at the given position in this panel, returns instance if found or nil if none.", },
	},
}

local Private_Data =
{
	defaultCamera = nil,		-- Default camera created for this panel (based on the radius of the scene and always using the camera params from this panel)
	viewCamera = nil,		-- Camera used to view the scene in this panel.
	dragTid = nil,
	onLoadCallback=nil,
	fileToIndex = false,
}

PreRegisterClass(Class_Data, Private_Data)

local SuperClass = _G[Class_Data.super] or {}
_G[Class_Data.super] = SuperClass

gDragMode3d = "dragRotate"


---- PostInit -----------------------------------------------------------------
-- Called after all constructor properties have been set, this sets up our default camera view.
function Panel3d:PostInit()
	SuperClass.PostInit(self)

	-- Create default camera for this panel
	if (self.defaultCamera == nil) then
		-- Create the default camera (eventually we need to support saving and loading all the nodes in a scene,
		-- then the serialized "Camera" property would need to be applied after the scene has been loaded).
		local camera = CreateInstance("camera", self:GetName().."_camera")

		if (camera) then
			self:AddInstance(camera)
			local cam_info = { openingAngle = self.OpeningAngle, zoom = self.Zoom, hither = self.Hither, yon = self.Yon, offset = self.CameraOffset }
			camera:SetInfo(cam_info)

			self.defaultCamera = camera
		end
	end

	-- If a camera name was given (not the default camera name) but the camera node wasn't found, then maybe the xmax model was loaded after
	-- the camera was set instead of before, so try to find it again now.
	if (self.viewCamera == nil) then
		local camname = self:GetCamera()
		local cdata = FindClassData(self.ClassName)

		if (camname ~= cdata.defaultprops.Camera.default) then
			local camera = self:FindInstance(camname)

			if camera then
				-- Set to default name then back to this camera to ensure that the strings don't match
				self:SetCamera(cdata.defaultprops.Camera.default)
				self:SetCamera(camera)
			end
		end
	end

	-- If no camera has been set by this point then use the default one
	if not self.viewCamera then
		self:SetCamera(self.defaultCamera)
	end

	-- DEFAULT CAMERA DATA
	self.centerPos = { x=0, y=0, z=0 }
	self.spheriPos = { rot = { alpha=140, beta=-22, theta=0 }, radius=10, clampmin = { alpha=0, beta=0, theta=0, radius=1 }, clampmax = { alpha=0, beta=0, theta=0, radius=1000 } }

	self:SetFocusInst(self.FocusInst)
end

---- Set XmaxFiles ------------------------------------------------------------
-- * files - stringlist - List of XMAX files to load into this panel.
-- This function loads the given XMAX files, if the list changes the old ones get unloaded first and
-- the new list gets loaded in its place.
function Panel3d:SetXmaxFiles(files)
	-- TODO: We should handle this better, we want to merge the new file list with what's already existing to avoid reloading any models that
	-- are already loaded, but to also add new ones and free old ones. This compare prevents reloading identical lists but won't catch when a
	-- model gets removed from the list (and will reload everything if one gets added, but it's better than reloading everything every time!).
	debug_printf( "gPanel3dMeshFlags = %s",tostring(gPanel3dMeshFlags) )
	if not table.compare(files, self.XmaxFiles) then
		self:ReleaseAllInstances()

		self.XmaxFiles = files
		self.fileToIndex = {}

		local inst = nil
		for i,filename in ipairs(files) do
			inst = self.cppinst:AddInstance(filename, gPanel3dMeshFlags, gLoadPriority)		-- todo add flags & priority properties
			if (self.onLoadCallback) then
				self.onLoadCallback( self, filename, inst)
			end

			inst:SetActive(i == self.XmaxIndex)

			self.fileToIndex[filename] = i
			self:ExtractCamerasInfo( inst,filename )
		end
	end
end

---- Get Xmax File Count ------------------------------------------------------
-- Returns the number of XMAX files loaded into this panel.
function Panel3d:GetXmaxFileCount()
	return #self.XmaxFiles
end

---- Extract Cameras Info -----------------------------------------------------
-- * top_inst - Instance - The instance whose child nodes to search through for cameras.
-- * filename - string - The name of the XMAX file that ''top_inst'' was craeted from.
-- This goes through all child nodes of the given instance looking for names containing "Camera" and
-- extracts important data about them (careful to avoid nulls that are used as targets for the camera).
function Panel3d:ExtractCamerasInfo( top_inst,filename )
	self.cameras = self.cameras or {}
	local camera_list = top_inst:FindChildInstanceList("Camera")

	-- If there are no cameras defined here, we are done.
	if( not camera_list ) then
		debug_printf( "Panel3d:ExtractCamerasInfo found no cameras in %q",tostring(filename) )
		return
	end

	-- Store essential data about each camera defined in this 'top_inst'.
	for _,v in pairs( camera_list ) do
		local inst = v.instance
		local camName = inst:GetName()

		-- The camera name must lead with "Camera" and must not contain ".Target" (targets are handled separately)
		if( (string.find(camName,"Camera",1,true) == 1) and not string.find(camName,".Target",1,true) and not self.cameras[camName] ) then
			local camEntry = { inst = inst }
			camEntry.inst_pos = camEntry.inst:GetPos( "local" )
			camEntry.target = self:FindInstance( camName..".Target" )
			if( camEntry.target ) then
				camEntry.target_pos = camEntry.target:GetPos( "local" )
				camEntry.radius = pos_mag( pos_sub(camEntry.inst_pos,camEntry.target_pos) )
			else
				warningf( "Panel3d:ExtractCamerasInfo WARNING: Xmax camera %q is not accompanied by a target.",camName )
			end
			camEntry.filename = filename

			self.cameras[camName] = camEntry
			debug_printf( "Panel3d:ExtractCamerasInfo storing camera %q from %s",tostring(camName),tostring(filename) )
		else
			debug_printf( "Panel3d:ExtractCamerasInfo ignoring %q",tostring(camName) )
		end
	end
end

---- Set XmaxIndex ------------------------------------------------------------
-- * id - number - The index of the XMAX model to show.
-- This function sets which XMAX model is being viewed. This class is primarily written to view one of the XMAX files it contains
-- at a time, use this function to switch between them. It also handles kicking off the asset loading for the new XMAX file if
-- background loading is enabled.
function Panel3d:SetXmaxIndex( id)
	local inst = nil
	local retinst

	for i,filename in ipairs(self.XmaxFiles) do
		inst = self.cppinst:FindInstance(filename)

		local visible = i == id
		inst:SetActive(visible)

		-- If background loading enabled then set load priority to 0 for active model to load it, and -1 for previously active model to unload it.
		if gLoadPriority == -1 then
			if visible then
				inst:SetLoadPriority( 0 )
			elseif i == self.XmaxIndex then
				inst:SetLoadPriority( -1 )
			end
		end

		if (visible) then
			retinst = inst
			self:SetupCameraForInst(inst)
		end
	end

	self.XmaxIndex = id

	return retinst
end


---- ShowXmaxFile -------------------------------------------------------------
-- * filename - string - Name of the XMAX file to show.
-- This is similar to SetXmaxIndex except that it shows the XMAX file by name instead of by index.
function Panel3d:ShowXmaxFile(filename)
	if not self.fileToIndex then
		printf("ERROR: ShowXmaxInstance() called before SetXmaxFiles")
	end

	local index = self.fileToIndex[filename]
	local inst

	if index then
		inst = self:SetXmaxIndex(index)
	else
		printf("ERROR: Filename not in fileToIndex table: %s", tostring(filename))
	end

	return inst
end


---- SetLoadCallback ---------------------------------------------------------------
-- * funcName - string - Name of the function to call when an XMAX file is loaded.
-- If a valid function name is given, it will be called after loading an XMAX file.
-- The function used as the callback must take these parameters (in order):
-- * panel - Panel3d - The panel into which the XMAX file was loaded (this panel).
-- * filename - string - Name of the XMAX file that was just loaded.
-- * inst - Instance - The C++ Instance object of the root instance for the loaded XMAX file.
function Panel3d:SetLoadCallback( funcName)

	if funcName == "" or funcName == "none" then
		self.onLoadCallback = nil
	else
		self.onLoadCallback = LookupGlobalValue(funcName)
	end

	self.LoadCallback = funcName
end


---- SetCamera ----------------------------------------------------------------
-- * camera - The camera to use for viewing, this can be the following data types:
-- ** string - The name of the camera node.
-- ** Camera - The C++ Instance handle of the camera to use.
-- * updateCamera - boolean - If true the starting position of the camera will be saved so it can later be restored.
-- This activates the given camera, making it the one that the scene will be viewed through. If you will be allowing the user to
-- rotate the camera you should pass ''true'' in for the second parameter to restore the camera back to it's starting position
-- whenever switching away from the camera.
function Panel3d:SetCamera( camera,updateCamera )

	local camname

	if (type(camera) == "string") then
		-- Save camera name and look up the instance
		camname = camera
		camera = self:FindInstance(camera)
	elseif camera then
		-- If not a string then camera is assumed to be the camera object (userdata)
		camname = camera:GetName()
	end

	printf("Panel3d - SetCamera - %s", tostring(camname))

	if (camname ~= self.Camera) or (camera and not self.viewCamera) then
		self.Camera = camname

		if camera then
--[[	-- don't, its either the custom camera for the panel or an xmax camera
			-- Free up the old camera (if any)
			if self.viewCamera then
				self.viewCamera:Release()
			end
]]
			if self.viewCamera then
				-- If prior position was saved then restore it before deactivating the camera
				if self.camPosSaved then
					self.viewCamera:SetPos(self.camPosSaved)
					self.camPosSaved = nil
				end

				self.viewCamera:SetActive(false)
			end

			self.viewCamera = camera
			self.viewCamera:SetActive(true)
			self.cppinst:SetCamera( self.viewCamera)

			if updateCamera then
				-- Save the starting position of this camera and update our spheriPos so we can rotate it from it's current position
				self.camPosSaved = self.viewCamera:GetPos()
				self:MoveCameraTarget(self.viewCamera, self.focusObj, "local", "immediate")
			end

			-- Ensure the new camera is setup with the current panel settings if panel overrides xmax
			local cam_info = { offset = self.CameraOffset }
			if self.viewCamera then
				if self.CameraOverride then
					MergeTable(cam_info, { openingAngle = self.OpeningAngle, zoom = self.Zoom, hither = self.Hither, yon = self.Yon })
				end

				self.viewCamera:SetInfo(cam_info)
			end
		else
			printf("Panel3d - Camera not found!! ")
		end
	end
end


---- ResetCamera --------------------------------------------------------------
-- camera - string or Camera - The camera to set.
-- updateCamera - boolean - True to save the camera position so it can later be restored.
-- Change the current camera, and reset it (if possible) to its initial position, rotation and radius.
-- These initial values are defined by XMAX data.
function Panel3d:ResetCamera( camera,updateCamera )
	self:SetCamera( camera,updateCamera )

	local camEntry = self.cameras[self.Camera]
	if( not camEntry ) then
		-- This camera was not defined in Xmax.
		return
	end

	-- Set the camera target and its position and rotation.
	self.centerPos = table.deepcopy( camEntry.target_pos )
	self.spheriPos.rot = { alpha = camEntry.inst_pos.alpha, beta = camEntry.inst_pos.beta, theta = camEntry.inst_pos.theta }
	-- Set the camera radius
	self:_clampRadius( camEntry.radius )

	-- Place the camera
	self.viewCamera:SetPos( camEntry.inst_pos )
	-- self.viewCamera:MoveTo( camEntry.inst_pos, "immediate", 1, true )
	self.viewCamera:MoveToSpherical( self.centerPos, self.spheriPos.rot, self.spheriPos.radius, "immediate", 1, false, self.spheriPos.clampmin, self.spheriPos.clampmax  )	-- immediate
end


---- SetupCameraForInst -------------------------------------------------------
-- * inst - Instance - The C++ instance object that the camera should be looking at.
-- This function attempts to position the camera a decent distance back from the given instance so that it is looking at it.
function Panel3d:SetupCameraForInst( inst )
	-- If not the default created camera for this panel then don't modify it (this was messing up all of our cameras that are positioned by the artists)
	if (self.Camera ~= self:GetName().."_camera") then
		if (self.viewCamera and self.LiveCam) then
			-- TODO: Figure out proper radius and clamp values for artist positioned cameras somehow...
		end

		return
	end

	local name = inst:GetName()

	--printf("Panel3d:SetupCameraForInst: %s", name )

	if (name == "<root>") then
		-- root is special case

		name = self.XmaxFiles[self.XmaxIndex]
		if (name) then
			inst = self.cppinst:FindInstance(name)
		end
	end

	if (self.viewCamera and self.LiveCam) then

		--self.centerPos = { x=0, y=0, z=0 }
		self.dragScale = gDragScale or 1

		if (self.MouseInstFlag) then
			-- reset
			self.spheriPos.rot = { alpha=140, beta=-22, theta=0 }
			self.viewCamera:MoveToSpherical( self.centerPos, self.spheriPos.rot, self.spheriPos.radius, "immediate", 1, false, self.spheriPos.clampmin, self.spheriPos.clampmax  )	-- immediate

			self.spheriPos.rot = { alpha=0, beta=0, theta=0 }
			self.focusObj:MoveTo( self.spheriPos.rot, "immediate")

			if (not arg) then
				self.spheriPos.rot = { alpha=140, beta=-22, theta=0 }
			end
		else
			-- reset
			local radius = inst:GetRadius()
			if( radius and radius > 0 ) then
				radius = radius * 1.7
			else
				radius = self.spheriPos.radius
			end
			self:_clampRadius( radius )

			self.spheriPos.rot = { alpha=140, beta=-22, theta=0 }
			self.viewCamera:MoveToSpherical( self.centerPos, self.spheriPos.rot, self.spheriPos.radius, "immediate", 1, false, self.spheriPos.clampmin, self.spheriPos.clampmax  )	-- immediate
		end
	end
end


---- SetFocusInst -------------------------------------------------------------
-- inst - string or Instance - Name, or direct C++ Instance object, of the instance the camera is to focus on.
-- This attempts to adjust the camera to be looking at the given 3D instance.
function Panel3d:SetFocusInst( inst)

	if (type(inst) == "string") then
		inst = self.cppinst:FindInstance(inst)
	end

	if (inst) then
		self.FocusInst = inst:GetName()
		self.focusObj = inst

		self:SetupCameraForInst(inst)
	end
end


---- SetMouseInstFlag ---------------------------------------------------------
-- arg - boolean - Whether or not the mouse should rotate the instance instead of the camera.
-- This function controls whether the focus instance (arg=true), or the camera (arg=false) rotates with
-- mouse/touch input on the panel.
function Panel3d:SetMouseInstFlag( arg)

	self.MouseInstFlag = arg

	if (self.viewCamera and self.LiveCam) then
		-- reset
		self.spheriPos.rot = { alpha=140, beta=-22, theta=0 }
		self.viewCamera:MoveToSpherical( self.centerPos, self.spheriPos.rot, self.spheriPos.radius, "immediate", 1, false, self.spheriPos.clampmin, self.spheriPos.clampmax  )	-- immediate

		if (self.focusObj) then
			self.spheriPos.rot = { alpha=0, beta=0, theta=0 }
			self.focusObj:MoveTo( self.spheriPos.rot, "immediate")
		end

		if (not arg) then
			self.spheriPos.rot = { alpha=140, beta=-22, theta=0 }
		end
	end
end


---- SetOpeningAngle ----------------------------------------------------------
-- arg - number - The opening (FoV) angle of the camera.
-- This manually sets the opening angle of the camera, it can only apply to the default camera, but not cameras
-- from an XMAX file, unless the ''CameraOverride'' property is enabled.
function Panel3d:SetOpeningAngle( arg)
	self.OpeningAngle = arg

	local cam_info = { openingAngle = self.OpeningAngle }

	if self.defaultCamera then
		self.defaultCamera:SetInfo(cam_info)
	end

	if self.viewCamera and (self.viewCamera ~= self.defaultCamera) and self.CameraOverride then
		self.viewCamera:SetInfo(cam_info)
	end
end


---- SetZoom ------------------------------------------------------------------
-- arg - number - Zoom amount.
-- This sets the camera zoom setting. It only effects the default camera, but not cameras from an
-- XMAX file, unless the ''CameraOverride'' property is enabled.
function Panel3d:SetZoom( arg)
	self.Zoom = arg

	local cam_info = { zoom = self.Zoom }

	if self.defaultCamera then
		self.defaultCamera:SetInfo(cam_info)
	end

	if self.viewCamera and (self.viewCamera ~= self.defaultCamera) and self.CameraOverride then
		self.viewCamera:SetInfo(cam_info)
	end
end


---- SetHither ----------------------------------------------------------------
-- arg - number - Distance to near clipping plane (hither) in world units.
-- This sets the distance to the near clipping plane of the camera. It only effects the default camera,
-- but not cameras from an XMAX file, unless the ''CameraOverride'' property is enabled.
function Panel3d:SetHither( arg)
	self.Hither = arg

	local cam_info = { hither = self.Hither }

	if self.defaultCamera then
		self.defaultCamera:SetInfo(cam_info)
	end

	if self.viewCamera and (self.viewCamera ~= self.defaultCamera) and self.CameraOverride then
		self.viewCamera:SetInfo(cam_info)
	end
end


---- SetYon -------------------------------------------------------------------
-- arg - number - Distance to far clipping plane (yon) in world units.
-- This sets the distance to the far clipping plane of the camera. It only effects the default camera,
-- but not cameras from an XMAX file, unless the ''CameraOverride'' property is enabled.
function Panel3d:SetYon( arg)
	self.Yon = arg

	local cam_info = { yon = self.Yon }

	if self.defaultCamera then
		self.defaultCamera:SetInfo(cam_info)
	end

	if self.viewCamera and (self.viewCamera ~= self.defaultCamera) and self.CameraOverride then
		self.viewCamera:SetInfo(cam_info)
	end
end


---- SetCameraOffset ----------------------------------------------------------
-- arg - vector3 - Offsets to use for the camera.
-- This sets up a camera offset so that the object being rotated around can be moved off-center in the
-- viewport being rendered to. Each of the ''x'', ''y'', and ''z'' fields in the arg table are optional
-- and will default to zero if not specified. Camera offsets can not be specified by an XMAX file, thus
-- scripts can set them WITHOUT the ''CameraOverride'' property being enabled.
function Panel3d:SetCameraOffset( arg)
	self.CameraOffset.x = arg.x or 0
	self.CameraOffset.y = arg.y or 0
	self.CameraOffset.z = arg.z or 0

	local cam_info = { offset = self.CameraOffset }

	if self.defaultCamera then
		self.defaultCamera:SetInfo(cam_info)
	end

	-- NOTE: CameraOverride not required for this because offset is not set in the xmax file anyway.
	if self.viewCamera and (self.viewCamera ~= self.defaultCamera) then --and self.CameraOverride then
		self.viewCamera:SetInfo(cam_info)
	end
end


---- SetCameraOverride -----------------------------------------------------
-- arg - boolean - Whether the script can change XMAX camera settings or not.
-- Passing true to this function allows scripts to change properties of a camera from an XMAX file that are
-- usually controlled by the 3D artist, such as opening angle or zoom.
function Panel3d:SetCameraOverride(arg)
	self.CameraOverride = arg

	if self.viewCamera and (self.viewCamera ~= self.defaultCamera) then
		if arg then
			local cam_info = { openingAngle=self.OpeningAngle, zoom=self.Zoom, hither=self.Hither, yon=self.Yon }
			self.viewCamera:SetInfo(cam_info)
		else
			-- TODO: Restore default info from the XMAX file here (technically should be done for all cameras, or maybe if SetActive(false) on a camera will restore it?)
		end
	end
end


-- These drag* functions are the values the gDragMode3d string should be set to.
-- This is temporary until mouse buttons can be distinguished between to allow to select
-- between movement and rotation.

---- dragMoveXY ---------------------------------------------------------------
-- Helper function to move the camera in the XY plane in response to mouse/touch input on the panel.
-- This should only be called internally.
function Panel3d:dragMoveXY(dx, dy)
	self.centerPos.x = self.centerPos.x + dx * self.dragScale
	self.centerPos.y = self.centerPos.y + dy * self.dragScale

	-- live = true means that the move doesn't automaticially cancel on a tolerance, for when dragging
	self.viewCamera:MoveToSpherical( self.centerPos, self.spheriPos.rot, self.spheriPos.radius, "factorto", .1, true, self.spheriPos.clampmin, self.spheriPos.clampmax )
end


---- dragMoveXZ ---------------------------------------------------------------
-- Helper function to move the camera in the XZ plane in response to mouse/touch input on the panel.
-- This should only be called internally.
function Panel3d:dragMoveXZ(dx, dy)

	self.centerPos.x = self.centerPos.x + dx * self.dragScale
	self.centerPos.z = self.centerPos.z + dy * self.dragScale
	self.viewCamera:MoveToSpherical( self.centerPos, self.spheriPos.rot, self.spheriPos.radius, "factorto", .1, true, self.spheriPos.clampmin, self.spheriPos.clampmax  )
end


---- dragRotate ---------------------------------------------------------------
-- Helper function to rotate the camera (or focus instance) in response to mouse/touch input on the panel.
-- This should only be called internally.
function Panel3d:dragRotate(dx, dy)

	if (self.MouseInstFlag and self.focusObj) then
		if not self:GetLockAlpha() then
			self.spheriPos.rot.alpha = self.spheriPos.rot.alpha + dx * self.dragScale
		end
		if not self:GetLockBeta() then
			self.spheriPos.rot.beta = self.spheriPos.rot.beta - dy * self.dragScale
		end

		self.focusObj:MoveTo( self.spheriPos.rot, "factorto", .1, true )
	else
		if (not self.viewCamera or not self.LiveCam) then
			return
		end

		if not self:GetLockAlpha() then
			self.spheriPos.rot.alpha = self.spheriPos.rot.alpha - dx * self.dragScale
		end
		if not self:GetLockBeta() then
			self.spheriPos.rot.beta = self.spheriPos.rot.beta - dy * self.dragScale
		end

		self.viewCamera:MoveToSpherical( self.centerPos, self.spheriPos.rot, self.spheriPos.radius, "factorto", .1, true, self.spheriPos.clampmin, self.spheriPos.clampmax  )
	end

--	printf("a,b = %d,%d", self.spheriPos.rot.alpha, self.spheriPos.rot.beta)

end


---- dragZoom -----------------------------------------------------------------
-- Helper function to change the distance the camera is from it's focus object in response to mouse/touch
-- input in the panel. This should only be called internally.
function Panel3d:dragZoom(dx, dy)

	if (not self.viewCamera or not self.LiveCam) then
		return
	end
	self:_clampRadius( self.spheriPos.radius + dy * self.dragScale )

	self.viewCamera:MoveToSpherical( self.centerPos, self.spheriPos.rot, self.spheriPos.radius, "factorto", .1, true, self.spheriPos.clampmin, self.spheriPos.clampmax  )
end


---- SetRadius ----------------------------------------------------------------
-- * radius - number - How far back to position the camera from its focus instance.
-- This function changes how close the camera is to the instance it is looking at.
function Panel3d:SetRadius(radius)
	local startRadius = self.spheriPos.radius

	self:_clampRadius( radius )

	-- No need to move the camera if the radius did not change (because it was clamped).
	if( self.viewCamera and self.LiveCam and (startRadius ~= self.spheriPos.radius) ) then
		self.viewCamera:MoveToSpherical( self.centerPos, self.spheriPos.rot, self.spheriPos.radius, "immediate", 1, false, self.spheriPos.clampmin, self.spheriPos.clampmax  )	-- immediate
	end
end


---- GetRadius ----------------------------------------------------------------
-- Returns the current distance of the camera from its focus instance.
function Panel3d:GetRadius()
	return self.spheriPos.radius
end


---- ClampRadius --------------------------------------------------------------
-- Clamps the radius between the given min/max values.
function Panel3d:ClampRadius( rMin,rMax )
	-- Swap the params if they are out of order.
	if( rMin and rMax and rMin > rMax ) then
		rMin,rMax = rMax,rMin
	end
	self.spheriPos.clampmin.radius = rMin or self.spheriPos.clampmin.radius
	self.spheriPos.clampmax.radius = rMax or self.spheriPos.clampmax.radius

	self:_clampRadius()
end


---- _clampRadius -------------------------------------------------------------
-- Helper function both to set the camera radius and to constrain it to be within specified limits.
function Panel3d:_clampRadius( r )
	r = r or self.spheriPos.radius	-- optional param

	-- A clamp value of -1 meand "no clamp" (negative distance is otherwise meaningless)
	local rMin = self.spheriPos.clampmin.radius or -1
	local rMax = self.spheriPos.clampmax.radius or -1

	if( rMin < 0 ) then
		if( rMax >= 0 ) then
			self.spheriPos.radius = math.min( r,rMax )
		end
	elseif( rMax < 0 ) then
		-- assumed: rMin >= 0
		self.spheriPos.radius = math.max( r,rMin )
	else
		-- assumed: rMin >= 0 and rMax >= 0
		self.spheriPos.radius = math.min( rMax,math.max(r,rMin) )
	end

	debug_printf( "Panel3d:_clampRadius input r = %s, set to %s",tostring(r),tostring(self.spheriPos.radius) )
end


---- MoveCameraTarget ---------------------------------------------------------
-- * camera - Camera - The C++ Instance object for the camera to move.
-- * target - Instance - The C++ Instance object for the camera to look at.
-- * coordspace - string - "global" or "local" specifying which coordinate space to use for positioning the camera.
-- * factorstr - string - "immediate" or "factorto", the former animates the camera to look at the target instead of snapping it.
-- This function adjusts the camera look angles so that it is facing the given target instance. This could be called for an animating
-- target to cause the camera to look at the target instance as it moves.
function Panel3d:MoveCameraTarget(camera,target,coordspace,factorstr)
	coordspace = coordspace or "local"	-- default to local (this is what C code does if argument is omitted)
	factorstr = factorstr or "factorto"
	local factoramt = (factorstr == "immediate") and 1 or .1

	--assuming camera and target are instances for now
	self.centerPos = target:GetPos(coordspace)
	local camPos = camera:GetPos(coordspace)
	self.spheriPos.rot.alpha,self.spheriPos.rot.beta = GetLookAt(camPos.x, camPos.y, camPos.z, self.centerPos.x, self.centerPos.y, self.centerPos.z)
	self:_clampRadius( NormalizeVect(camPos.x - self.centerPos.x,  camPos.y - self.centerPos.y,  camPos.z - self.centerPos.z) )

	self.viewCamera:MoveToSpherical( self.centerPos, self.spheriPos.rot, self.spheriPos.radius, factorstr, factoramt, true, self.spheriPos.clampmin, self.spheriPos.clampmax  )
end


---- HandleTouch --------------------------------------------------------------
-- * args - table - Touch arguments.
-- This handles touches on the Panel3d causing it to rotate the camera by default. You could implement a custom
-- handler that accepts the delta X and Y movement as parameters (two numbers) and set the ''gDragMode3d'' to the name
-- of your function, however your function would have to be defined as an extension to this class (ie: function Panel3d:myDragFunc(dx, dy)).
function Panel3d:HandleTouch(args)
	if SuperClass.HandleTouch(self, args) then
		return true
	end

	-- Touch events move/rotate the camera
	if args.target == self then
		local msg = args.type
		local tid = args.tid
		if msg == "down" then

--			print("Panel3d:OnTouch : down")

			if not self.dragTid then
				self.dragTid = tid
				self.dragX = args.local_x
				self.dragY = args.local_y
				local InterfaceWidth
				if gMainApp then
					InterfaceWidth=gMainApp:GetInterfaceSize().x
				elseif (gInterfaceWid) then
					InterfaceWidth=gInterfaceWid
				else
					errorf("unable to get Interface width!")
				end
--				self.dragScale = (self:GetWidth() /InterfaceWidth )
				self.dragScale = gDragScale or 1
				self:CaptureTouch(tid)
				return true
			end
		elseif msg == "up" then

			if (tid == self.dragTid) then

				if (self.viewCamera and self.LiveCam) then

					-- cancel live move mode
					if (self.MouseInstFlag and self.focusObj) then
						self.focusObj:MoveTo( self.spheriPos.rot, "factorto", .1, false )
					else
						self.viewCamera:MoveToSpherical( self.centerPos, self.spheriPos.rot, self.spheriPos.radius, "factorto", .1, false, self.spheriPos.clampmin, self.spheriPos.clampmax  )
					end
				end

				self.dragTid = nil
				self:ReleaseTouch(tid)
				return true
			end
		elseif msg == "move" then

			if (tid == self.dragTid) then
				local dx = args.local_x - self.dragX
				local dy = args.local_y - self.dragY

				if dx == 0 and dy == 0 then
					return true
				end

				self.dragX = args.local_x
				self.dragY = args.local_y

				Panel3d[gDragMode3d](self, dx, dy)

				-- this shouldn't be necessary
				-- all it does is force all instances with dirty pos's to update immediately
				-- which will get done on the next Tick() call
				-- this makes it happen twice
				--UpdateOnly()
			end

			return true
		end
	end

	return false
end


---- DropFiles ----------------------------------------------------------------
-- * files - array(?) - List of files dropped onto this panel from the OS file browser.
-- * pos - vector2 - Position in this panel at which the files were dropped.
-- This is an override from the Panel class that allows a Panel3d to accept XMAX files
-- drag-n-dropped on it to add them to the XMAX list.
function Panel3d:DropFiles(files, pos)

--	DumpTable("files", files)

	local xmaxfiles = table.deepcopy(self.XmaxFiles)
	local hitxmax = false
	local hitindex = -1
	for _,p in pairs(files) do

		local path, name, ext = SplitPathNameExt(p)

		if (ext == "xmax") then

			local ne = name.."."..ext

			-- check existing
			for i,n in pairs(xmaxfiles) do

				if (n == ne) then
                    -- found an xmax
                    print("dropped xmax file "..ne)

                    hitxmax = true
                    table.remove( xmaxfiles, i)
                    hitindex = i
					break
				end
			end
		end
	end

	if (hitxmax) then
		self:SetXmaxFiles(xmaxfiles)
	else
		SuperClass.DropFiles(self, files, pos)
	end
	if (hitindex ~= -1) then
		self:SetXmaxIndex(#xmaxfiles)
	end
end


---- Animate ------------------------------------------------------------------
-- * elap - number - Elapsed time in seconds since last frame.
-- This is an override from Panel class to update the keyframe animation on the
-- focus object, if there is one, and if our AnimateFlag property is enabled.
function Panel3d:Animate( elap)

	local frames = self.FrameRate * elap

	if (gPanel3dAnimate and self.AnimateFlag and self.focusObj) then

		self.focusObj:IncFrame( frames)
	end
	SuperClass.Animate(self, elap)

end

---- OnWheelEvent -------------------------------------------------------------
-- * args - table - Table of touch event arguments for this wheel event.
-- Set the OnWheel property to this function to support zooming the camera.
function Panel3d:OnWheelEvent(args)
	local delta = -args.wheel * self:GetWheelZoomFactor()

	self:dragZoom(0, delta)
end
