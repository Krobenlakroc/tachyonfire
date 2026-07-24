--EDIT THIS STUFF
--

--OK CARY ON
require("io")
require("os")
require("string")

function os.capture(cmd, raw)
  local f = assert(io.popen(cmd, 'r'))
  local s = assert(f:read('*a'))
  f:close()
  if raw then return s end
  s = string.gsub(s, '^%s+', '')
  s = string.gsub(s, '%s+$', '')
  s = string.gsub(s, '[\n\r]+', ' ')
  return s
end

function dirLookup(t)
  local p = io.popen("find -name \'*.cpp\' -o -name \'*.c\'")  --Open directory look for files, save data in p. By giving '-type f' as parameter, it returns all files.
  count = 1
  print("Scanning for source files")
  for file in p:lines() do                         --Loop through all files
  --  print("File Found \[" .. count .. "\] " .. file)
    t[count] = file
    count = count+1

  end
  t.length = count -1
end

function dirLookupDocs(t)
  local p = io.popen("find -name \'*.h\' -o -name \'*.cpp\' -o -name \'*.c\' -o -name \'*.glsl\' -o -name \'*.frag\' -o -name \'*.vert\' -o -name \'*.lua\'")  --Open directory look for files, save data in p. By giving '-type f' as parameter, it returns all files.
  count = 1
  print("Scanning for documentaion files")
  for file in p:lines() do                         --Loop through all files
  --  print("File Found \[" .. count .. "\] " .. file)
    t[count] = file
    count = count+1

  end
  t.length = count -1
end

local answer
lastCmd = "r"
esc = string.char(27, 91) --escape character
background = ""--esc .."44m"
foreground = ""--esc .."37m"
background2 = ""--esc .."104m"
rst = esc .. "0m"
motd = background2 .. "C/C++ Build Script LATEST." ..background
shell = "~:"
luashell = "~;"
cmdshell = "$"
docshell = "?:" ---Wno-sign-compare


--[[cflags = "-c -std=c11 -Wall -Wunreachable-code  -Wwrite-strings -Wpointer-arith -Wcast-align -Wcast-qual -Wextra -pedantic -Wno-unused-variable -Wno-unused-parameter  -Wno-unused-function -Werror=implicit-function-declaration -DSPNG_USE_MINIZ -DLUA_USE_LINUX -std=c11 -Iftinclude -DGLEW_STATIC -D_POSIX_C_SOURCE=199309L -Iinclude/AL  -Iinclude/SDL2 -Iinclude/GL -march=native -m64 -mfpmath=sse -msse2  -O3 "]] --compile flags
--""

--
sanmode = "-fsanitize=address,undefined "

cflags = "-c -std=c11 -Wall -Wunreachable-code  -Wwrite-strings -Wpointer-arith -Wcast-align -Wcast-qual -Wextra -pedantic -Wno-unused-variable -Wno-unused-parameter  -Wno-unused-function -Werror=implicit-function-declaration -DSPNG_USE_MINIZ -DLUA_USE_LINUX -std=c11 -Iftinclude -Iinclude -DGLEW_STATIC -D_POSIX_C_SOURCE=199309L -Iinclude/AL  -Iinclude/SDL2 -Iinclude/GL -march=x86-64-v3 -mavx2 -m64 -mfpmath=sse -msse2 -O2 -flto"
---DDISABLE_AUDIO -DNO_THREADS --fair-sched=yes
dcflags = "-c -std=c11 -Werror=vla -Wunreachable-code  -Wwrite-strings -Wpointer-arith -Wcast-align -Wcast-qual -Wextra -Wno-unused-variable -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Werror=implicit-function-declaration -DSPNG_USE_MINIZ -DLUA_USE_LINUX -std=c11 -DGLEW_STATIC -D_POSIX_C_SOURCE=199309L -Iftinclude -Iinclude -Iinclude/AL  -Iinclude/SDL2 -Iinclude/GL -march=x86-64-v3 -m64 -mfpmath=sse -msse2  -g -Og  " --debug compile flags
sanflags = "-c -std=c11 -Werror=vla -Wunreachable-code  -Wwrite-strings -Wpointer-arith -Wcast-align -Wcast-qual -Wextra -Wno-unused-variable -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Werror=implicit-function-declaration -DSPNG_USE_MINIZ -DLUA_USE_LINUX -DCULL_SCALAR -std=c11 -DGLEW_STATIC -D_POSIX_C_SOURCE=199309L -Iinclude -Iftinclude -Iinclude/AL  -Iinclude/SDL2 -Iinclude/GL -march=core2 -m64 -mfpmath=sse -msse2  -g -Og " .. sanmode --debug compile flags
-- ocflags = "-c -std=c11 -Wall -Wunreachable-code  -Wwrite-strings -Wpointer-arith -Wcast-align -Wcast-qual -Wextra -pedantic -Wno-unused-variable -Wno-unused-parameter  -Wno-unused-function -Werror=implicit-function-declaration -DSPNG_USE_MINIZ -DLUA_USE_LINUX -std=c11 -Iftinclude -DGLEW_STATIC -D_POSIX_C_SOURCE=199309L -Iinclude/AL  -Iinclude/SDL2 -Iinclude/GL -march=native -m64 -mfpmath=sse -msse2  -O3" --release compile flags

ocflags = cflags

flags = "-O2 -flto -l:libSDL2-2.0.so.0 -lGL -lm -L./bin/lib64 -Wl,-rpath=./bin/lib64 -l:libopenal.so.1.22.2 -lpthread -std=c11" -- flags
flags_original = flags--"-lSDL2 -lGL -lm -L./bin/lib64 -Wl,-rpath=./bin/lib64 -l:libopenal.so.1.21.1 -lpthread -std=c11 " -- flags
flags_san = "-lSDL2 -lGL -lm -L./bin/lib64 -Wl,-rpath=./bin/lib64 -l:libopenal.so.1.21.1 -lpthread -std=c11 -g -Og " .. sanmode -- flags
flags_debug= "-lSDL2 -lGL -lm -L./bin/lib64 -Wl,-rpath=./bin/lib64 -l:libopenal.so.1.21.1 -lpthread -std=c11 -g -Og " -- flags
lflags = flags --link flags ---fno-signaling-nans -ffinite-math-only -fno-math-errno

cc="gcc" -- compiler
makefile = {}
target = {}
gfiles = {}
files = {}
docs = {}
docexten = {}
doctree = {}
doctree.docs = {}
doctree.docindex = {}
filetree = {}
out = "Makefile"
debug = false
lastrm = ""
cfiles = {}
cfiles.size = 0

uparrow = string.char(27) .. [[[A]]

basedir = os.capture("readlink -f build.lua")
basedir = string.gsub(basedir,"build.lua","")

function getRelativeDirectory(filename)
  local rawfilename
  subdir = os.capture("readlink -f " .. filename)
  local val3 = nil
  local val4 = 1

  while (true) do
  val3,val4 = string.find(filename,"/",val4 + 1)
  if ( not val3) then
    break
  end
  rawfilename = string.sub(filename,val4)
  end

  subdir = string.gsub(subdir,rawfilename,"")

  subdir = string.gsub(subdir,basedir,"")

  return subdir .. "/"
end

function autoDeps(filename)
ret = ""
for line in io.lines("src/" .. filename .. ".c") do
val1,val2 = string.find(line,"#include \"")
if (val1) then
substr = line:sub(val2,-1)
ret = ret .. " " .. getRelativeDirectory("src/" .. filename .. ".c") .. substr
end
end

return ret
end

function genTarget(id)
  target = {}

  if (cfiles[id]  == false) then
    target[1] = files[id] .. ".o: src/" .. files[id] .. ".cpp"
    target[2] = "\t$(CC) $(CLFLAGS) src/" ..  files[id] .. ".cpp" .. " -o bin/o/" .. files[id] .. ".o"
  else
    target[1] = files[id] .. ".o: src/" .. files[id] .. ".c" --.. autoDeps(files[id])
    target[2] = "\t$(CC) $(CLFLAGS) src/" ..  files[id] .. ".c" .. " -o bin/o/" .. files[id] .. ".o"
  end
end

function popo()


end
--liblua3.a glew.a
function genMakeFile()
  dirLookup(files)
  for i= 1,files.length do -- get rid of extra fluff
    s = files[i]
    files[i] =string.gsub(files[i],'%.cpp','')
    files[i] =string.gsub(files[i],'%.c','')

    if (files[i] .. ".c") == s then

      cfiles[i] = true
    else
      cfiles[i] = false
    end
    -- files[i] = string.gsub(files[i],'%src/','')

    files[i] = string.gsub(files[i],'%./src/','')
  end
  print("Generating Makefile")
  print(basedir)
  sumfiles = ""
  for i=1,files.length do
    sumfiles = sumfiles .. "bin/o/" .. files[i] .. ".o" .. " "
  end
  makefile[1] = "#Autogenerated Makefile from build.lua"
  makefile[2] = "CC=" .. cc
  makefile[3] = "CLFLAGS=" .. cflags
  makefile[4] = "LFLAGS=" .. lflags
  makefile[5] = "all: build clean"
  makefile[6] = ""
  makefile[7] = "build: " .. sumfiles
  makefile[8] = "\t$(CC)  " .. sumfiles .."libfreetype.a -ldl -lm -Wl,-rpath=bin/lib64 -Lbin/lib64  $(LFLAGS)" .. " -o bin/tachyonfire"
  makefile[9] = ""
  last = 10
  --generate targets
  for i=1, files.length do
    genTarget(i)
    makefile[last] = "bin/o/" ..target[1]
    makefile[last+1] =  target[2]
    makefile[last+2] = ""
    last = last +3
  end

  makefile[last] = "clean:"

  length = last
  --output the file
--   os.execute("rm " .. out)
--   for i=1,length do
--     os.execute("echo " .. makefile[i] .. " >> " ..out )
--   end


  local f = io.open(out, "w")
  if not f then
    error("Failed to open output file: " .. out)
  end

  for i = 1, length do
    f:write(makefile[i] .. "\n")
  end
  f:close()
  print("Makefile written to " .. out)
end

print(background .. foreground )
os.execute("clear")
print(background .. foreground .. motd)

dirLookupDocs(filetree)
genMakeFile() -- create a new make file
popo() -- populate bin/o/
local lastcmd
docmd = 0
exec = false
while(true) do
  if (not exec) then
    io.write(shell)
    awnser = io.read()
  end
  if (exec) then
    awnser = lastCmd
    exec = false
  end

  print(background .. foreground )
  os.execute("clear")
  print(background .. foreground .. motd)
  if awnser == "r" then -- run the bianary
    print("Running.")
    output = os.execute("bin/bin")
    lastCmd = "r"
  elseif awnser == "b"  or awnser == "bb" then -- build the project and run th bianary
    print("Building and running.")
    output = os.execute("make && bin/bin")
    --os.execute("rm $(find -name *.o)")
    lastCmd = "b"
  elseif awnser == "c" then -- build the project
    print("Building.")
    output = os.execute("PATH=/opt/gcc-15/bin:$PATH make")
    --os.execute("rm $(find -name *.o)")
    lastCmd = "c"
  elseif awnser == "o" then --display compile output
    print(output)
  elseif awnser == uparrow then--up arrow
    exec = true
    print(lastCmd)
  elseif awnser == "Q" then -- exit the program
    print(rst)
    os.execute("clear")
    os.exit(0)
  elseif awnser == "clear" or awnser == "reset" or awnser == "clr"  then -- clean the slate
    print(background .. foreground )
    os.execute("clear")
    print(background .. foreground .. motd)
  elseif awnser == "f" then -- flush the .o files
--     os.execute("rm *o")
--     os.execute("rm $(find -name *.o)")

    local function deleteFiles(dir)
      local p = io.popen('find "' .. dir .. '" -name "*.o" 2>/dev/null || dir /s /b "' .. dir .. '\\*.o" 2>nul')

      if p then
        for file in p:lines() do
          --print(file)
          os.remove(file)
        end
        p:close()
      end
    end
    deleteFiles(".")
  elseif awnser == "help" or awnser == "h" or awnser == "?" then
    print(background .. foreground .. "r     - Run the program.")
    print(background .. foreground .. "f     - flush the o files")
    print(background .. foreground .. "c     - compile")
    print(background2 .. foreground .. "b     - Rebuild and run the program.     ")
    print(background .. foreground .. "o     - Print build output status.")
    print(background2 .. foreground .. "clear - Clear the screen.                ")
    print(background .. foreground .. "Q     - Exit the program.")
    print(background2 .. foreground .. "l     - Run lua code inside this program." )
    print(background .. foreground .. "g     - Generate a new makefile")
    print(background2 .. foreground .. "etc   - Execute through bash.            " )
    print(background .. foreground .. "d     - View Documentaion, type d and then h more info")
      print(background2 .. foreground .. "db    - Debug.                           ")
      print(background .. foreground .. "ds    - Enable debugging symbols ")
      print(background2 .. foreground .. "rm    - Remove .o file to rebuild.        ")
    elseif awnser == "l" or awnser == "lua" then
      io.write(luashell)
      sde = io.read()
      f = loadstring(sde)
      if f then
        f()
      end
    elseif awnser == "g" or awnser == "gen" or awnser == "generate" then
      genMakeFile()

    elseif awnser == "ds" then
      debug = not debug
      if (debug) then
        flags = flags_debug
        cflags = dcflags --"-c -Wall -DLUA_USE_LINUX -DGLEW_STATIC -std=c11 -I/usr/include/SDL2 -Iinclude/GL -g"
        lflags = flags --link flags
        genMakeFile();
      else
        flags = flags_original--string.sub(flags,1,string.len(flags)-7);
        cflags = ocflags--"-c -Wall -DLUA_USE_LINUX -DGLEW_STATIC -std=c11 -I/usr/include/SDL2 -Iinclude/GL -Ofast"  --compile flags
        lflags = flags --link flags
        genMakeFile();
      end
    elseif awnser == "dsan" then

      flags = flags_san
      cflags = sanflags --"-c -Wall -DLUA_USE_LINUX -DGLEW_STATIC -std=c11 -I/usr/include/SDL2 -Iinclude/GL -g"
      lflags = flags --link flags
      genMakeFile();


    elseif awnser == "db" then
      os.execute("echo -e \"run\\n bt \\n q\" >> cmds")
      os.execute("gdb bin/bin -x cmds")
      os.execute("rm cmds")
    elseif awnser == "rmlb" then
      aw = lastrm
      fle = os.capture("find -name " .. aw ..".o")

      print("Deleting Last File:" .. fle)
      os.execute("rm " .. fle)
      print("Building and running.")
      output = os.execute("make && bin/bin")
      --os.execute("rm $(find -name *.o)")
      lastCmd = "b"

    elseif awnser == "rm" then
      io.write("Filename:")
      aw = io.read()
      if aw == "l" then
        aw = lastrm
        fle = os.capture("find -name " .. aw ..".o")

        print("Deleting Last File:" .. fle)
        os.execute("rm " .. fle)
      else
        lastrm = aw
        fle = os.capture("find -name " .. aw ..".o")

        print("Deleting:" .. fle)
        os.execute("rm " .. fle)
      end
    elseif awnser == "lines" then
      fileconcat = ""
      for i=1,filetree.length do
        blacklisted = false
        blacklist = {[[./src/miniz.c]],[[./src/spng.h]],[[./src/miniz.h]],[[./src/spng.c]],[[./src/glew.c]],[[./src/gl.c]]}
        for t=1,#blacklist do
          if (filetree[i] == blacklist[t]) then
            blacklisted = true
          end
        end

        if not (blacklisted or string.find(filetree[i],"./ftinclude") or string.find(filetree[i],"./include") or (filetree[i] == [[./src/fn_lua/luaconf.h]])  or (filetree[i] == [[./src/fn_lua/lua.h]]) or (filetree[i] == [[./src/fn_lua/lualib.h]]) or (filetree[i] == [[./src/fn_lua/lauxlib.h]]) or (filetree[i] == [[./build.lua]]) or (filetree[i] == [[./src/watermark.h]]) or (filetree[i] == [[./fn1/maps/gi.lua]]) or (filetree[i] == [[./src/fn_engine/stb_image.h]])  or ( not (string.find(filetree[i],"_lightmaps") == nil))) then  --if  not ((filetree[i] == [[./include/GL/glew.h]]) or (filetree[i] == [[./include/GL/glxew.h]]) or (filetree[i] == [[./include/GL/eglew.h]]) or (filetree[i] == [[./include/GL/wglew.h]])) then
          fileconcat = fileconcat .. filetree[i] .. " "
        end
      end
      os.execute("wc -l " .. fileconcat)
    elseif awnser == "files" then
      os.execute("find src -type f | wc -l")
    elseif awnser == "d" then
      exit = false
      while not exit do
        io.write(docshell)
        sde = io.read()
        if sde == "h" then
          print(background .. foreground .. "Filename     - Get info on that file")
          print(background2 .. foreground .. "h            - Show this prompt                     " ..background)
          print(background .. foreground .. "q            - quit documentaion mode" )
          print(background2 .. foreground .. "l            - list a quick description of each file" ..background)
          print(background .. foreground .. "lh           - list documentaion of header files" )
          print(background2 .. foreground .. "lcpp         - list documentaion of cpp files       " ..background)
          print(background .. foreground .. "g            - Generate the Documentaion" )
          print(background2 .. foreground .. "Q            - quit the program                     " ..background)

        elseif sde == "q"  then
          exit = true

        elseif sde == "Q" then
          print(rst)
          os.execute("clear")
          os.exit(0)

        elseif sde == "l" then
          for i = 1,doctree.docindex.length do
            print(doctree.docindex[i].small)

          end
        elseif sde == "lh" then
          for i = 1,doctree.docindex.length do
            if docexten[i] == ".h" then
              print(doctree.docindex[i].small)
            end
          end
        elseif sde == "lcpp" then
          for i = 1,doctree.docindex.length do
            if docexten[i] == ".cpp" then
              print(doctree.docindex[i].small)
            end
          end
        elseif sde == "g" then
          genDoc()
        else
          doc = doctree.docs[sde]
          if doc then
            if doc.long then
              print(doc.long)
            end
          end
        end

      end
    else

      print(background .. foreground .. cmdshell .. awnser)
      os.execute(awnser)

    end
    if not (awnser == uparrow) then
      lastCmd = awnser
    end
  end
