
--[[
	LuaJIT FFI GLUT demo
	by Adam Strzelecki
	http://www.nanoant.com


	left mouse button to rotate cube
	right mouse button to rotate lights
]]

--[[
local posix = require "posix"
local wd2 = posix.getcwd()
]]

package.path = package.path .. ";" .. os.getenv("P4_WORKSPACE") .. "/DebLua/?.lua"
require "lua_shell"

local wd = pshell.pwd()

package.path = os.getenv("LXGIT").."/?.lua;".. package.path

-- local gl        = require 'glua'
local gl        = require 'glua.init'
local gui       = require 'glua.gui'
local primitive = require 'glua.primitive'
local time      = require 'glua.time'
local ffi       = require 'ffi'

local lights =
{
  {
    position = {  1.3, 1.3, 1.3  },
    ambient  = {  0,  0,  0  },
    diffuse  = {  1,  0,  0  },
    specular = {  1,  1,  1  }
  },
  {
    position = {  0,  0, -1.3  },
    ambient  = {  0,  0,  0  },
    diffuse  = {  1,  1,  0  },
    specular = {  1,  1,  1  }
  },
  {
    position = { -1.3, 0,  1.3  },
    ambient  = {  0,  0,  0  },
    diffuse  = {  0,  0,  1  },
    specular = {  0,  0,  1  }
  }
}

local core = false

--[===[
for i = 1, #arg do
  if     arg[i]:match('^--compat$') then core = false
  elseif arg[i]:match('^--help$')   then
    print [[
    --help   Shows help
    --compat Use compatibility profile]]
  end
end
]===]

local CWD = ""

---- Main ----------------------------------------------------------------------

function main()
	
	Log.Init(CWD .. "solidCube.log")
	Log.SetTimestamp("%H:%M:%S > ")
	
	Log.f("initialize display (glut module calls glutInit)")
	if core then
		-- gl.utInitContextVersion(3, 2)
		gl.utInitContextVersion(2, 1)
		gl.utInitContextFlags(gl.UT_FORWARD_COMPATIBLE)
		gl.utInitContextProfile(gl.UT_CORE_PROFILE)
	end
	-- gl.utInitDisplayString('rgba double depth>=16 samples~8')
	gl.utInitDisplayString('rgba double depth>=32 samples~8')
	gl.utInitWindowSize(500, 500)
	gl.utInitWindowPosition(100, 100)

	Log.f("create window & local mouse state variables")
	local window = gl.utCreateWindow("SolidCube")
	local width, height
	local buttons = {}
	-- gl.utFullScreen()

	Log.f("set up background color")
	gl.ClearColor(.2, .2, .2, 0)
	gl.Enable(gl.CULL_FACE)

	Log.f("enable depth test")
	gl.Enable(gl.DEPTH_TEST)
	
-- textures
	Log.f("set up textures")
	local tex_t = {		gl.path('glua.textures.GraniteWall-ColorMap',  'png'),
				gl.path('glua.textures.GraniteWall-NormalMap', 'png'),
			}

	local textures = gl.textures{ [0] = tex_t[1], [1] = tex_t[2] }
	
-- shaders
	Log.f("load shaders")
	local shad_t = {	gl.path('glua.shaders.normal', 'vert'),
				gl.path('glua.shaders.normal', 'frag')
			}
				
	local normalProgram = gl.program{
				[gl.VERTEX_SHADER] = shad_t[1],
				[gl.FRAGMENT_SHADER] = shad_t[2]
	}()

-- lights
	Log.f("set up lights")
	gl.Uniform1i(normalProgram.numLights, #lights)
	local lightPosition = gl.vvec3(#lights)
	local lightAmbient  = gl.vvec3(#lights)
	local lightDiffuse  = gl.vvec3(#lights)
	local lightSpecular = gl.vvec3(#lights)
	for l = 1, #lights do
		if lights[l].position then lightPosition[l-1] = gl.vec3(unpack(lights[l].position)) end
		if lights[l].ambient  then lightAmbient[l-1]  = gl.vec3(unpack(lights[l].ambient))  end
		if lights[l].diffuse  then lightDiffuse[l-1]  = gl.vec3(unpack(lights[l].diffuse))  end
		if lights[l].specular then lightSpecular[l-1] = gl.vec3(unpack(lights[l].specular)) end
	end
	gl.Uniform3fv(normalProgram.lightPosition, #lights, lightPosition[0].gl)
	gl.Uniform3fv(normalProgram.lightAmbient,  #lights, lightAmbient[0].gl)
	gl.Uniform3fv(normalProgram.lightDiffuse,  #lights, lightDiffuse[0].gl)
	gl.Uniform3fv(normalProgram.lightSpecular, #lights, lightSpecular[0].gl)

	Log.f("set up texture mapping")
	gl.Uniform1i(normalProgram.colorTex,  0)
	gl.Uniform1i(normalProgram.normalTex, 1)

	Log.f("set up material")
	normalProgram.materialAmbient   = {.1, .1, .1}
	normalProgram.materialDiffuse   = {.5, .5, .5}
	normalProgram.materialSpecular  = {1,  1,  1 }
	normalProgram.materialShininess = .2

-- cube
	Log.f("load solid cube")
	local cube = primitive.cube(normalProgram)

	Log.f("setup matrices")
	local projection = gl.identity
	-- local view       = gl.translate(0,0,-4)
	local view       = gl.translate(0,0,-24)
	local model      = gl.identity
	local light      = gl.identity

	normalProgram.lightMatrix = light

	local modelViewMatrix = {}
	local modelViewProjectionMatrix = {}

	-- called upon window resize & creation
	gl.utReshapeFunc(function(w, h)
				Log.f("glutReshapeFunc(%d, %d) START", w, h)
				width, height = w, h
				projection = gl.perspective(60, w / h, .1, 1000)
				gl.Viewport(0, 0, w, h)

				for y = -40, 40 do
					for x = -40, 40 do
						local modelView = view * model * gl.translate(x * 3, y * 3, 0)
						modelViewMatrix[x*1000+y] = modelView.gl
						modelViewProjectionMatrix[x*1000+y] = (projection * modelView).gl
					end
				end
				Log.f("glutReshapeFunc() DONE")	
			end)

	Log.f("setup idle callback")
	local frames, start
			
	local rotateCallback = gl.utIdleCallback(function()
							Log.f("glutIdleCallback()")
							light = gl.rotatey(.002) * light
							normalProgram.lightMatrix = light
							frames = frames + 1
							gl.utPostRedisplay()
						end)

	-- called when mouse moves
	gl.utMotionFunc(function(x, y)
				Log.f("glutMotionFunc(%d, %d) START", x, y)
				
				local left = buttons[gl.UT_LEFT_BUTTON]
				  if left and left.state == gl.UT_DOWN then
				    model = gl.rotate((y - left.y) * .007, (x - left.x) * .007, 0) * model
				    left.x, left.y = x, y
				    gl.utPostRedisplay()
				  end
				Log.f("glutMotionFunc() DONE")
				end)

	-- called when mouse button is clicked
	gl.utMouseFunc(function(button, state, x, y)
			  buttons[button] = {state = state, x = x, y = y}
			  -- rotate teapot
			  if button == gl.UT_RIGHT_BUTTON then
			    if state == gl.UT_DOWN then
			      frames = 0
			      start  = time()
			      gl.utIdleFunc(rotateCallback)
			    else
			      local sec = start.since
			      -- print(string.format('%.2f FPS %.2f sec', frames / sec, sec)); io.stdout:flush()
			      gl.utIdleFunc(nil)
			    end
			  end
			end)

	Log.f("setup main drawing function")
	gl.utDisplayFunc(function ()
			Log.f("glutDisplayFunc() START");
			gl.Clear(gl.COLOR_BUFFER_BIT + gl.DEPTH_BUFFER_BIT)
			Log.f("af glClear()");
			  for y = -40, 40 do
			    for x = -40, 40 do
				local modelView = view * model * gl.translate(x * 3, y * 3, 0)
				normalProgram.modelViewMatrix           = modelView
				normalProgram.modelViewProjectionMatrix = projection * modelView
				normalProgram.modelViewMatrix           = modelViewMatrix[x*1000+y]
				normalProgram.modelViewProjectionMatrix = modelViewProjectionMatrix[x*1000+y]
				Log.f("bf cube()");
				cube()
				Log.f("af cube()");
			    end
			  end
			Log.f("bf glutSwapBuffers()");
			gl.utSwapBuffers()
			Log.f("glutDisplayFunc() END");
			end)

	Log.f("enter main loop")
	gl.utMainLoop()

	Log.f("EXITED main loop")
end







