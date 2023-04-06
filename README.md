# vscript_lua51
This extension enables Lua VScript from CS2 to load external lua module dll. Tested on Mar 22nd version. 

# Usage
1. Prepare build environment: VS2022 + CMake
2. Build lua51.dll and vscript.dll
3. Rename vscript.dll to vscript2.dll in CAGO\game\bin\win64
4. Copy lua51.dll and vscript.dll into CAGO\game\bin\win64
5. Put any other external lua module dll (like luasocket.dll) into CAGO\game\bin\win64
6. Load module by `local luasocket = require("luasocket")`
Warning: This is a client-side hack, NEVER install it under VAC, or you may get banned. 

# Support
PRs and issues are welcomed. 