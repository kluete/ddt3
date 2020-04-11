#!/usr/bin/env luajit

-- package.path = "../?.lua;../?/init.lua;" .. package.path

package.path = package.path .. ";" .. os.getenv("P4_WORKSPACE") .. "/DebLua/?.lua"
require "lua_shell"

local wd = pshell.pwd()

local git_dir = os.getenv("LXGIT")
package.path = git_dir.."/?.lua;".. package.path
-- package.path = git_dir.."/?.lua;".. git_dir .."/glua/init.lua;"..package.path

local gl  = require 'glua.init'
local gui = require 'glua.gui'

local CWD = ""

---- Main ----------------------------------------------------------------------

function main()

	Log.Init(CWD .. "gl_simple.log")
	Log.SetTimestamp("%H:%M:%S > ")
	
	Log.f("initialize display")
	-- gl.utInitContextVersion(2, 1)
	gl.utInitDisplayString('rgba double depth>=32 samples~8')
	gl.utInitWindowSize(400, 300)
	gl.utInitWindowPosition(100, 100)

	Log.f("glutCreateWindow")
	local window = gl.utCreateWindow("gl_simple")

	gl.ClearColor(.2, .2, .2, 1)
	gl.Enable(gl.BLEND)
	gl.BlendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA)

	local font = gui.font{size = 48}
	local guiProgram = gl.program{
		[gl.VERTEX_SHADER]   = gl.path('glua.shaders.gui', 'vert'),
		[gl.FRAGMENT_SHADER] = gl.path('glua.shaders.gui', 'frag')
	}()
	
	guiProgram.color = {1, 1, .5, 1}
	guiProgram.alphaMask = true

	local text = font:array(guiProgram, 'Hello world!')

	gl.utDisplayFunc(function ()
		gl.Clear(gl.COLOR_BUFFER_BIT + gl.DEPTH_BUFFER_BIT)
		text()
		gl.utSwapBuffers()
		end)

	gl.utReshapeFunc(function (w, h)
		guiProgram.modelViewProjectionMatrix = gl.ortho(0, w, h, 0, -1, 0)
		gl.Viewport(0, 0, w, h)
		end)

	Log.f("glutMainLoop() start")
	gl.utMainLoop()
	Log.f("glutMainLoop() end")
end



