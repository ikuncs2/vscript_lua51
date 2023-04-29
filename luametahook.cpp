

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include "Windows.h"
#include "metahook.h"

// function SearchPattern(string ModuleName, string Pattern)
int LuaMH_SearchPattern(lua_State* L)
{
	const char* name = lua_tostring(L, 1);
	size_t pattern_len = 0;
	const char* pattern = lua_tolstring(L, 2, &pattern_len);
	HMODULE hModule = GetModuleHandleA(name);
	if (!hModule)
		return luaL_error(L, "invalid module");

	BYTE* pBase = (BYTE*)MH_GetModuleBase(hModule);
	auto dwModuleSize = MH_GetModuleSize(hModule);
	VirtualProtect(pBase, dwModuleSize, PAGE_EXECUTE_READWRITE, NULL);
	auto addr = MH_SearchPattern(pBase, dwModuleSize, pattern, pattern_len);
	if (!addr)
	{
		return luaL_error(L, "signature not found");
	}

	lua_pushlightuserdata(L, addr);
	return 1;
}

// function GetModuleRange(string ModuleName) => Base, Size
int LuaMH_GetModuleRange(lua_State* L)
{
	const char* name = lua_tostring(L, 1);
	HMODULE hModule = GetModuleHandleA(name);
	if (!hModule)
		return luaL_error(L, "invalid module");
	BYTE* pBase = (BYTE*)MH_GetModuleBase(hModule);
	auto dwModuleSize = MH_GetModuleSize(hModule);

	lua_pushlightuserdata(L, pBase);
	lua_pushinteger(L, (lua_Integer)dwModuleSize);
	return 2;
}

const luaL_reg lib[] = {
	{ "SearchPattern", LuaMH_SearchPattern},
	{ "GetModuleRange", LuaMH_GetModuleRange},
	{ nullptr, nullptr }
};

extern "C" __declspec(dllexport) int luaopen_luametahook(lua_State* L)
{
	luaL_register(L, "luametahook", lib);
	return 1;
}