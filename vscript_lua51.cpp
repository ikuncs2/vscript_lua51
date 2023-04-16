#include <Windows.h>

extern "C" {
#define LUA_BUILD_AS_DLL
#define LUA_CORE
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include "metahook.h"

inline ptrdiff_t FindFunction(const char* funcname, ptrdiff_t offset)
{
	auto hModule = LoadLibrary("vscript2.dll");
	BYTE* pBase = (BYTE*)MH_GetModuleBase(hModule);
	auto dwModuleSize = MH_GetModuleSize(hModule);
	auto addr = pBase + offset;
	return (ptrdiff_t)addr;
}

template<size_t N>
inline ptrdiff_t FindFunction(const char *funcname, const char(&pattern)[N])
{
	auto hModule = LoadLibrary("vscript2.dll");
	BYTE* pBase = (BYTE*)MH_GetModuleBase(hModule);
	auto dwModuleSize = MH_GetModuleSize(hModule);
	VirtualProtect(pBase, dwModuleSize, PAGE_EXECUTE_READWRITE, NULL);
	auto addr = MH_SearchPattern(pBase, dwModuleSize, pattern, N - 1);
	if (!addr)
	{
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "Error: signature not found for %s", funcname);
		MessageBoxA(NULL, buffer, "vscript_lua51", MB_ICONINFORMATION);
	}
	return (ptrdiff_t)addr;
}

typedef union TValue TValue;
typedef const TValue cTValue;

// declearation for internal methods
static TValue* index2adr(lua_State* L, int idx);
int lj_obj_equal(cTValue* o1, cTValue* o2);
void lj_err_callermsg(lua_State* L, const char* msg);
int luaL_loadbufferx(lua_State* L, const char* buff, size_t sz, const char* name, const char* mode);
void lj_err_run(lua_State* L);
void lj_err_argtype(lua_State* L, int narg, const char* xname);
static void err_argmsg(lua_State* L, int narg, const char* msg);

// implementation for default functions

LUA_API void (lua_replace)(lua_State* L, int idx) 
{
	// TODO : is this right?
	lua_remove(L, idx);
	lua_insert(L, idx);
}

LUA_API int (lua_isuserdata)(lua_State* L, int idx)
{
	return lua_type(L, idx) == LUA_TLIGHTUSERDATA || lua_type(L, idx) == LUA_TUSERDATA;
}

LUA_API int (lua_rawequal)(lua_State* L, int idx1, int idx2)
{
	TValue* ptr1 = index2adr(L, idx1);
	TValue* ptr2 = index2adr(L, idx2);

	BYTE* gState = *(BYTE**)((BYTE*)L + 0x10);
	void* luaNull = (void*)(gState + 0xf8);

	if (ptr1 != luaNull && ptr2 != luaNull) {
		return lj_obj_equal(ptr1, ptr2);
	}
	return false;
}

LUA_API lua_CFunction(lua_tocfunction) (lua_State* L, int idx)
{
	// TODO : not yet implemented
	abort();
}

LUA_API lua_State* (lua_tothread)(lua_State* L, int idx)
{
	// TODO : not yet implemented
	abort();
}

LUA_API const void* (lua_topointer)(lua_State* L, int idx)
{
	// TODO : not yet implemented
	abort();
}

// implementation for other apis
LUA_API const char* (lua_pushfstring)(lua_State* L, const char* fmt, ...)
{
	const char* ret;
	va_list argp;
	va_start(argp, fmt);
	ret = lua_pushvfstring(L, fmt, argp);
	va_end(argp);
	return ret;
}

LUA_API int (lua_dump)(lua_State* L, lua_Writer writer, void* data)
{
	// TODO : not yet implemented
	return luaL_error(L, "lua_dump : not yet implemented");
}

LUA_API int  (lua_yield)(lua_State* L, int nresults)
{
	// TODO : not yet implemented
	abort();
}

LUA_API int  (lua_resume)(lua_State* L, int narg)
{
	// TODO : not yet implemented
	abort();
}

LUA_API int  (lua_status)(lua_State* L)
{
	// TODO : not yet implemented
	abort();
}

LUA_API lua_Alloc(lua_getallocf) (lua_State* L, void** ud)
{
  	BYTE* gState = *(BYTE**)((BYTE*)L + 0x10);
	if (ud != nullptr) {
    		*ud = *(void**)(gState + 0x18);
	}
  	return *(lua_Alloc*)(gState + 0x10);
}

LUA_API void lua_setallocf(lua_State* L, lua_Alloc f, void* ud)
{
	BYTE* gState = *(BYTE**)((BYTE*)L + 0x10);
	*(void**)(gState + 0x18) = ud;
	*(lua_Alloc*)(gState + 0x10) = f;
}

LUALIB_API int (luaL_getmetafield)(lua_State* L, int obj, const char* e)
{
	if (!lua_getmetatable(L, obj))  /* no metatable? */
		return 0;
	lua_pushstring(L, e);
	lua_rawget(L, -2);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 2);  /* remove metatable and metafield */
		return 0;
	}
	else {
		lua_remove(L, -2);  /* remove only metatable */
		return 1;
	}
}

LUALIB_API lua_Integer(luaL_checkinteger) (lua_State* L, int numArg)
{
	return luaL_optinteger(L, numArg, 0);
}

LUALIB_API void (luaL_checkany)(lua_State* L, int narg)
{
	if (lua_type(L, narg) == LUA_TNONE)
	{
		luaL_argerror(L, narg, "value expected");
	}
}

LUALIB_API int (luaL_error)(lua_State* L, const char* fmt, ...) // variadic
{
	const char* msg;
	va_list argp;
	va_start(argp, fmt);
	msg = lua_pushvfstring(L, fmt, argp);
	va_end(argp);
	lj_err_callermsg(L, msg);
	return 0;  /* unreachable */
}

LUALIB_API int (luaL_checkoption)(lua_State* L, int narg, const char* def, const char* const lst[])
{
	ptrdiff_t i;
	const char* s = lua_tolstring(L, narg, NULL);
	if (s == NULL && (s = def) == NULL)
		luaL_typerror(L, narg, lua_typename(L, LUA_TSTRING));
	for (i = 0; lst[i]; i++)
		if (strcmp(lst[i], s) == 0)
			return (int)i;
	return luaL_argerror(L, narg, lua_pushfstring(L, "invalid option %s", s));
}

LUALIB_API int (luaL_loadbuffer)(lua_State* L, const char* buff, size_t sz, const char* name) // not found
{
	return luaL_loadbufferx(L, buff, sz, name, NULL);
}

LUALIB_API int (luaopen_os)(lua_State* L) // not found
{
	return luaL_error(L, "luaopen_os : not yet implemented");
}

LUALIB_API void (luaL_openlibs)(lua_State* L) // inlined
{
	luaL_error(L, "luaopen_os : not yet implemented");
}

LUA_API int lua_error(lua_State* L)
{
	lj_err_run(L);
	return 0;  /* unreachable */
}

LUALIB_API void luaL_register(lua_State* L, const char* libname, const luaL_Reg* l)
{
	luaL_openlib(L, libname, l, 0);
}

LUALIB_API int luaL_argerror(lua_State* L, int narg, const char* msg)
{
	err_argmsg(L, narg, msg);
	return 0;  /* unreachable */
}

LUALIB_API int luaL_typerror(lua_State* L, int narg, const char* xname)
{
	lj_err_argtype(L, narg, xname);
	return 0;  /* unreachable */
}

// find signature

template<class T> struct TypeIdentity { using type = T; };
template<int N> struct IntWrapper { static constexpr int value = N; };
struct Invalid {};
template<size_t N, class...> struct Varadaic_GetN : TypeIdentity<Invalid> {};
template<class First, class...Rest> struct Varadaic_GetN<0, First, Rest...> : TypeIdentity<First> {};
template<size_t N, class First, class...Rest> struct Varadaic_GetN<N, First, Rest...> : Varadaic_GetN<N - 1, Rest...> {};
template<class F> struct F_ReturnType;
template<class Ret, class...Arg> struct F_ReturnType<Ret(Arg...)> : TypeIdentity<Ret> {};
template<class F, size_t N> struct F_ArgType;
template<class Ret, size_t N, class...Arg> struct F_ArgType<Ret(Arg...), N> : Varadaic_GetN<N, Arg...> {};
template<class F> struct F_ArgNum;
template<class Ret, class...Arg> struct F_ArgNum<Ret(Arg...)> : IntWrapper<sizeof...(Arg)> {};

#define PP_TUPLE_FIRST(X, ...) X
#define PP_TUPLE_REST(X, ...) __VA_ARGS__
#define PP_TUPLE_INVOKE(X, TUPLE) X TUPLE

#define PP_CAT(a, b) PP_CAT_I(a, b)
#define PP_CAT_I(a, b) PP_CAT_II(~, a ## b)
#define PP_CAT_II(p, res) res
#define PP_COMMA() ,
#define PP_EMPTY() 
#define PP_REPEAT_0(CTX, PATTERN, JOINER)
#define PP_REPEAT_1(CTX, PATTERN, JOINER) PATTERN(CTX, 0)
#define PP_REPEAT_2(CTX, PATTERN, JOINER) PP_REPEAT_1(CTX, PATTERN, JOINER) JOINER() PATTERN(CTX, 1)
#define PP_REPEAT_3(CTX, PATTERN, JOINER) PP_REPEAT_2(CTX, PATTERN, JOINER) JOINER() PATTERN(CTX, 2)
#define PP_REPEAT_4(CTX, PATTERN, JOINER) PP_REPEAT_3(CTX, PATTERN, JOINER) JOINER() PATTERN(CTX, 3)
#define PP_REPEAT_5(CTX, PATTERN, JOINER) PP_REPEAT_4(CTX, PATTERN, JOINER) JOINER() PATTERN(CTX, 4)
#define PP_REPEAT_6(CTX, PATTERN, JOINER) PP_REPEAT_5(CTX, PATTERN, JOINER) JOINER() PATTERN(CTX, 5)
#define PP_REPEAT_N(CTX, PATTERN, JOINER, N) PP_TUPLE_INVOKE(PP_CAT(PP_REPEAT_,N),(CTX, PATTERN, JOINER))
#define PP_REPEAT_MAX 6

#define ARG_N(FUNCTYPE, N) _##N
#define ARG_DEF_N(FUNCTYPE, N) typename F_ArgType<FUNCTYPE, N>::type ARG_N(FUNCTYPE, N)
#define F_N(FUNCNAME, N, ...) \
extern "C" LUA_API typename F_ReturnType<decltype(FUNCNAME)>::type (FUNCNAME)(PP_REPEAT_N(FUNCNAME, ARG_DEF_N, PP_COMMA, N)) \
{ \
	static const auto pfn = (decltype(FUNCNAME)*)FindFunction(#FUNCNAME, __VA_ARGS__); \
	return pfn(PP_REPEAT_N(FUNCNAME, ARG_N, PP_COMMA, N)); \
}

#define F_DEF_HELPER_N(FUNCNAME, N) \
template<class DependentFunctionType> struct PP_CAT(FUNCNAME,_FuncDefHelper)<DependentFunctionType, N> \
{ \
	friend typename F_ReturnType<DependentFunctionType>::type \
		(FUNCNAME)(PP_REPEAT_N(DependentFunctionType, ARG_DEF_N, PP_COMMA, N)) \
	{ \
		return PP_CAT(FUNCNAME,_FindFunctionHelper)()(PP_REPEAT_N(DependentFunctionType, ARG_N, PP_COMMA, N)); \
	} \
};

#define F_DEF(FUNCNAME, ...) \
static decltype(FUNCNAME) *PP_CAT(FUNCNAME,_FindFunctionHelper) () \
{ \
	static auto pfn = reinterpret_cast<decltype(FUNCNAME)*>(FindFunction(#FUNCNAME, __VA_ARGS__)); \
	return pfn; \
} \
template<class DependentFunctionType, int ArgNum> struct PP_CAT(FUNCNAME,_FuncDefHelper); \
PP_REPEAT_N(FUNCNAME, F_DEF_HELPER_N, PP_EMPTY, PP_REPEAT_MAX) \
template struct PP_CAT(FUNCNAME,_FuncDefHelper)<decltype(FUNCNAME), F_ArgNum<decltype(FUNCNAME)>::value>;

F_DEF(index2adr, "\x4C\x8B\xC1\x85\xD2\x7E\x2A\x48\x8B\x41\x20\x48\x63\xD2")
F_DEF(lj_obj_equal, "\x4C\x8B\x09\x4C\x8B\x12")
F_DEF(lj_err_callermsg, "\x40\x53\x48\x83\xEC\x20\x48\x8B\x41\x10\x45\x33\xC9")
F_DEF(luaL_loadbufferx, "\x4C\x8B\xDC\x49\x89\x5B\x08\x57\x48\x81\xEC\xF0\x00\x00\x00\x4D\x85\xC9\x48\x89\x54\x24\x20\x48\x8D\x05\x32\x06\x00\x00")
F_DEF(lj_err_run, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x4C\x8B\x51\x10")
F_DEF(lj_err_argtype, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x63\xDA")
F_DEF(err_argmsg, "\x48\x89\x5C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x30\x8B\xDA")

F_DEF(lua_newstate, "\x48\x89\x5C\x24\x20\x55\x56\x41\x56\x48\x83\xEC\x50")
F_DEF(lua_close, "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x20\x48\x8B\x79\x10\x48\x8B\x9F\xC0\x00\x00\x00")
F_DEF(lua_newthread, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\x51\x10")
F_DEF(lua_atpanic, "\x4C\x8B\x41\x10\x49\x8B\x80\x60\x01\x00\x00")

F_DEF(lua_gettop, "\x48\x8B\x41\x28\x48\x2B\x41\x20")
F_DEF(lua_settop, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x63\xFA")
F_DEF(lua_pushvalue, "\x48\x83\xEC\x28\x4C\x8B\xD1\xE8\x2A\x2A\x2A\x2A\x49\x8B\x52\x28")
F_DEF(lua_remove, "\x4C\x8B\xC1\x85\xD2\x7E\x2A\x48\x8B\x41\x20\x48\x8B\x49\x28")
F_DEF(lua_insert, "\x4C\x8B\xC9\x85\xD2")
//F_N(lua_replace, 2, ?)
F_DEF(lua_checkstack, "\x48\x83\xEC\x28\x4C\x8B\xC9")
F_DEF(lua_xmove, "\x48\x3B\xCA\x74\x2A\x48\x89\x5C\x24\x08")

F_DEF(lua_isnumber, "\x48\x83\xEC\x28\xE8\x2A\x2A\x2A\x2A\x48\x8B\x08\x48\x8B\xC1\x48\xC1\xF8\x2F\x83\xF8\xF2")
F_DEF(lua_isstring, "\x48\x83\xEC\x28\xE8\x2A\x2A\x2A\x2A\x48\x8B\x08\x48\xC1\xF9\x2F")
F_DEF(lua_iscfunction, "\x48\x83\xEC\x28\xE8\x2A\x2A\x2A\x2A\x48\x8B\x08\x48\x8B\xC1\x48\xC1\xF8\x2F\x83\xF8\xF7")
//F_N(lua_isuserdata, 2, ?)
F_DEF(lua_type, "\x48\x83\xEC\x28\x4C\x8B\xD1\xE8\x2A\x2A\x2A\x2A\x4C\x8B\xD8")
F_DEF(lua_typename, "\x48\x63\xC2\x48\x8D\x0D\x3E\x43\x0B\x00")

F_DEF(lua_equal, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x45\x8B\xD0\x48\x8B\xD9")
//F_N(lua_rawequal, 3, ?)
F_DEF(lua_lessthan, "\x40\x53\x48\x83\xEC\x20\x45\x8B\xD0")

F_DEF(lua_tonumber, "\x48\x83\xEC\x28\xE8\x2A\x2A\x2A\x2A\x48\x8B\x08\x48\x8B\xD1\x48\xC1\xFA\x2F\x83\xFA\xF2\x77\x2A")
F_DEF(lua_tointeger, "\x48\x83\xEC\x28\xE8\x2A\x2A\x2A\x2A\x48\x8B\x08\x48\x8B\xD1\x48\xC1\xFA\x2F\x83\xFA\xF2\x73\x2A")
F_DEF(lua_toboolean, "\x48\x83\xEC\x28\xE8\x2A\x2A\x2A\x2A\x48\x8B\x08\x33\xC0")
F_DEF(lua_tolstring, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x49\x8B\xF8\x8B\xDA\x48\x8B\xF1\xE8\x2A\x2A\x2A\x2A")
F_DEF(lua_objlen, "\x40\x53\x48\x83\xEC\x20\x4C\x8B\xD1\xE8\x2A\x2A\x2A\x2A")
//F_N(lua_tocfunction, 2, ?)
F_DEF(lua_touserdata, "\x48\x83\xEC\x28\x4C\x8B\xD1\xE8\x2A\x2A\x2A\x2A\x48\x8B\x10")
//F_N(lua_tothread, 2, ?)
//F_N(lua_topointer, 2, ?)

F_DEF(lua_pushnil, "\x48\x8B\x41\x28\x48\xC7\x00\xFF\xFF\xFF\xFF\x48\x83\x41\x28\x08")
F_DEF(lua_pushnumber, "\x48\x8B\x41\x28\xF2\x0F\x11\x08")
F_DEF(lua_pushinteger, "\x48\x8B\x41\x28\x0F\x57\xC0")
F_DEF(lua_pushlstring, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x4C\x8B\x49\x10\x49\x8B\xF8")
F_DEF(lua_pushstring, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\xFA\x48\x8B\xD9\x48\x85\xD2")
F_DEF(lua_pushvfstring, "\x40\x53\x55\x48\x83\xEC\x48\x48\x8B\x59\x10")
// F_N(lua_pushfstring, 2, "\x48\x89\x54\x24\x10\x4C\x89\x44\x24\x18\x4C\x89\x4C\x24\x20\x53\x48\x83\xEC\x20\x4C\x8B\x41\x10")
F_DEF(lua_pushcclosure, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x48\x8B\xD9\x49\x63\xF8")
F_DEF(lua_pushboolean, "\x48\x8B\x41\x28\x45\x33\xC0")
F_DEF(lua_pushlightuserdata, "\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\xE8\x2A\x2A\x2A\x2A\x48\x8B\x53\x28")
F_DEF(lua_pushthread, "\x40\x53\x48\x83\xEC\x20\x48\xB8\x00\x00\x00\x00\x00\x80\xFC\xFF")

F_DEF(lua_gettable, "\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\xE8\x2A\x2A\x2A\x2A\x4C\x8B\x43\x28\x48\x8B\xD0\x49\x83\xE8\x08")
F_DEF(lua_getfield, "\x48\x89\x5C\x24\x10\x57\x48\x83\xEC\x20\x4D\x8B\xD0")
F_DEF(lua_rawget, "\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\xE8\x2A\x2A\x2A\x2A\x4C\x8B\x43\x28\x48\x8B\xCB")
F_DEF(lua_rawgeti, "\x40\x53\x48\x83\xEC\x20\x4D\x63\xD0")
F_DEF(lua_createtable, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x4C\x8B\x49\x10\x41\x8B\xF8")
F_DEF(lua_newuserdata, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x4C\x8B\x41\x10")
F_DEF(lua_getmetatable, "\x48\x83\xEC\x28\x4C\x8B\xD1\xE8\x2A\x2A\x2A\x2A\x48\x8B\x08\x48\x8B\xC1\x48\xC1\xF8\x2F\x83\xF8\xF4")
F_DEF(lua_getfenv, "\x48\x83\xEC\x28\x4C\x8B\xD1\xE8\x2A\x2A\x2A\x2A\x48\x8B\x08\x48\x8B\xC1\x48\xC1\xF8\x2F\x83\xF8\xF7")

F_DEF(lua_settable, "\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\xE8\x2A\x2A\x2A\x2A\x4C\x8B\x43\x28\x48\x8B\xD0\x49\x83\xE8\x10")
F_DEF(lua_setfield, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x4D\x8B\xD0\x48\x8B\xD9")
F_DEF(lua_rawset, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x48\x8B\xD9\xE8\x2A\x2A\x2A\x2A")
F_DEF(lua_rawseti, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x4D\x63\xD0")
F_DEF(lua_setmetatable, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x48\x89\x7C\x24\x18\x41\x56\x48\x83\xEC\x20\x48\x8B\xF9\xE8\x2A\x2A\x2A\x2A")
F_DEF(lua_setfenv, "\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\xE8\x2A\x2A\x2A\x2A\x4C\x8B\x4B\x28")

F_DEF(lua_call, "\x48\x63\xC2\x4C\x8B\xD1")
F_DEF(lua_pcall, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x48\x8B\x59\x10\x41\x8B\xF0")
F_DEF(lua_cpcall, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\x59\x10\x4C\x8D\x0D\xDB\xF4\xFF\xFF")
F_DEF(lua_load, "\x4C\x8B\xDC\x49\x89\x5B\x08\x57\x48\x81\xEC\xF0\x00\x00\x00\x4D\x85\xC9\x48\x89\x54\x24\x20\x48\x8D\x05\xE2\x06\x00\x00")
//F_N(lua_dump, 3, ?)

//F_N(lua_yield, 2, ?)
//F_N(lua_resume, 2, ?)
//F_N(lua_status, 1, ?)

F_DEF(lua_gc, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x48\x8B\x59\x10\x33\xFF")
//F_DEF(lua_error, 0x58170) // "\x48\x83\xEC\x28\xE8\x2A\x2A\x2A\x2A\xCC" function is too short...
F_DEF(lua_next, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\xD9\xE8\x2A\x2A\x2A\x2A\x48\x8B\x53\x28")
F_DEF(lua_concat, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x8B\xFA\x48\x8B\xD9\x83\xFA\x02")
//F_N(lua_getallocf, 2, ?)
//F_N(lua_setallocf, 3, ? )

F_DEF(luaL_openlib, "\x48\x89\x5C\x24\x20\x55\x56\x41\x56\x48\x83\xEC\x20")
//F_DEF(luaL_register, 0x5BDB0) // "\x45\x33\xC9\xE9\x2A\x2A\x2A\x2A\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC" function is too short...
//F_N(luaL_getmetafield, 3, ?)
F_DEF(luaL_callmeta, "\x48\x89\x5C\x24\x10\x48\x89\x6C\x24\x18\x56\x57\x41\x56\x48\x83\xEC\x20\x48\x8B\x59\x10\x48\x8B\xF1")
//F_DEF(luaL_typerror, 0x58100) // "\x48\x83\x23\xEC\x28\xE8\x2A\x2A\x2A\x2A\xCC" function is too short...
//F_DEF(luaL_argerror, 0x580C0) // "\x48\x83\xEC\x28\xE8\x2A\x2A\x2A\x2A\xCC" function is too short...
F_DEF(luaL_checklstring, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x49\x8B\xF0")
F_DEF(luaL_optlstring, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x49\x8B\xF1")
F_DEF(luaL_checknumber, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x8B\xDA\x48\x8B\xF9\xE8\x2A\x2A\x2A\x2A\x48\x8B\x08\x4C\x8B\xC1\x49\xC1\xF8\x2F\x41\x83\xF8\xF2\x77\x2A\xF2\x0F\x10\x00\x48\x8B\x5C\x24\x30\x48\x83\xC4\x20\x5F\xC3\x41\x83\xF8\xFB")
F_DEF(luaL_optnumber, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x8B\xDA\x48\x8B\xF9\xE8\x2A\x2A\x2A\x2A\x48\x8B\x08\x4C\x8B\xC1\x49\xC1\xF8\x2F\x41\x83\xF8\xF2\x77\x2A\xF2\x0F\x10\x00\x48\x8B\x5C\x24\x30\x48\x83\xC4\x20\x5F\xC3\x48\x83\xF9\xFF")
//F_N(luaL_checkinteger, 2, ?)
F_DEF(luaL_optinteger, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x4D\x8B\xD0\x8B\xDA")

F_DEF(luaL_checkstack, "\x48\x83\xEC\x28\x4C\x8B\xD1\x81\xFA\x40\x1F\x00\x00")
F_DEF(luaL_checktype, "\x40\x53\x48\x83\xEC\x20\x45\x8B\xD8")
//F_N(luaL_checkany, 3, ?)
F_DEF(luaL_newmetatable, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x48\x8B\x41\x10\x48\x8B\xD9")
F_DEF(luaL_checkudata, "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x48\x89\x7C\x24\x20\x41\x56\x48\x83\xEC\x20\x49\x8B\xD8")
F_DEF(luaL_where, "\x40\x53\x48\x83\xEC\x20\x4C\x8D\x44\x24\x40")
//F_N(luaL_error, 2, "\x40\x53\x48\x83\xEC\x20\x4C\x8D\x44\x24\x40")
//F_N(luaL_checkoption, 4, ?)
F_DEF(luaL_ref, "\x48\x89\x5C\x24\x10\x57\x48\x83\xEC\x20\x8D\x82\x0F\x27\x00\x00")
F_DEF(luaL_unref, "\x45\x85\xC0\x78\x2A\x48\x89\x5C\x24\x08")
F_DEF(luaL_loadfile, "\x40\x53\x56\x57\x48\x81\xEC\x00\x03\x00\x00")
//F_N(luaL_loadbuffer, 4, ?)
F_DEF(luaL_loadstring, "\x48\x89\x5C\x24\x08\x57\x48\x81\xEC\xF0\x00\x00\x00")
F_DEF(luaL_newstate, "\x48\x83\xEC\x28\x33\xD2\xB9\x50\x4D\x00\x00")
F_DEF(luaL_gsub, "\x40\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\xA8\xFE\xFF\xFF")
F_DEF(luaL_findtable, "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x41\x56\x41\x57\x48\x83\xEC\x20\x45\x8B\xF1")

F_DEF(luaL_buffinit, "\x48\x8D\x42\x18\x48\x89\x4A\x10")
F_DEF(luaL_prepbuffer, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x4C\x8B\x01")
F_DEF(luaL_addlstring, "\x40\x55\x57\x41\x57\x48\x83\xEC\x20\x48\x8B\xF9")
//F_N(luaL_addstring, 2, ?)
F_DEF(luaL_addvalue, "\x48\x89\x6C\x24\x20\x57\x48\x83\xEC\x20\x48\x8B\x69\x10")
F_DEF(luaL_pushresult, "\x48\x89\x74\x24\x10\x48\x89\x7C\x24\x18\x41\x56\x48\x83\xEC\x20\x4C\x8B\x01")

F_DEF(luaopen_base, "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x20\x48\x8B\x71\x48")
F_DEF(luaopen_io, "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x20\x4C\x8D\x0D\xD5\x3D\x0A\x00")
//F_N(luaopen_os, 1, ?)
F_DEF(luaopen_string, "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x20\x4C\x8D\x0D\x65\x67\x0A\x00")
F_DEF(luaopen_math, "\x40\x53\x48\x83\xEC\x20\xBA\x20\x00\x00\x00")
F_DEF(luaopen_debug, "\x48\x83\xEC\x28\x4C\x8D\x0D\xE5\x0E\x0A\x00")
F_DEF(luaopen_package, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x48\x89\x7C\x24\x18\x41\x56\x48\x83\xEC\x30\x48\x8D\x15\x34\x98\x0A\x00")
//F_N(luaL_openlibs, 1, ?)
