#include <Windows.h>

extern "C" {
#define LUA_BUILD_AS_DLL
#define LUA_CORE
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include "metahook.h"

inline ptrdiff_t FindFunction(ptrdiff_t offset)
{
	auto hModule = LoadLibrary("vscript2.dll");
	BYTE* pBase = (BYTE*)MH_GetModuleBase(hModule);
	auto dwModuleSize = MH_GetModuleSize(hModule);
	auto addr = pBase + offset;
	return (ptrdiff_t)addr;
}

template<size_t N>
inline ptrdiff_t FindFunction(const char(&pattern)[N])
{
	auto hModule = LoadLibrary("vscript2.dll");
	BYTE* pBase = (BYTE*)MH_GetModuleBase(hModule);
	auto dwModuleSize = MH_GetModuleSize(hModule);
	auto addr = (ptrdiff_t)MH_SearchPattern((void*)pBase, dwModuleSize, pattern, N);
	return addr;
}

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

void* index2adr(lua_State* L, int idx) {
	auto f = (void* (*)(lua_State* L, int idx))FindFunction(0x54f00);
	return f(L, idx);
}

LUA_API int (lua_rawequal)(lua_State* L, int idx1, int idx2)
{
	void* ptr1 = index2adr(L, idx1);
	void* ptr2 = index2adr(L, idx2);

	BYTE* gState = *(BYTE**)((BYTE*)L + 0x10);
	void* luaNull = (void*)(gState + 0xf8);

	if (ptr1 != luaNull && ptr2 != luaNull) {
		auto lj_obj_equal = (int(*)(void* a, void* b))FindFunction(0x6abd0);
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
	// TODO : not yet implemented
	abort();
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
	static const auto lj_err_callermsg = reinterpret_cast<void (*)(lua_State * L, const char* msg)>(FindFunction(0x578F0)); // TODO : signature
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
	static const auto luaL_loadbufferx = reinterpret_cast<int (*)(lua_State * L, const char* buf, size_t size, const char* name, const char* mode)>(FindFunction(0x6A560)); // TODO : signature
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
#define PP_REPEAT_N(CTX, PATTERN, JOINER, N) PP_TUPLE_INVOKE(PP_CAT(PP_REPEAT_,N),(CTX, PATTERN, JOINER))
#define PP_REPEAT_MAX 5

#define ARG_N(FUNCTYPE, N) _##N
#define ARG_DEF_N(FUNCTYPE, N) typename F_ArgType<FUNCTYPE, N>::type ARG_N(FUNCTYPE, N)
#define F_N(FUNCNAME, N, ...) \
extern "C" LUA_API typename F_ReturnType<decltype(FUNCNAME)>::type (FUNCNAME)(PP_REPEAT_N(FUNCNAME, ARG_DEF_N, PP_COMMA, N)) \
{ \
	static const auto pfn = (decltype(FUNCNAME)*)FindFunction(__VA_ARGS__); \
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
	static auto pfn = reinterpret_cast<decltype(FUNCNAME)*>(FindFunction(__VA_ARGS__)); \
	return pfn; \
} \
template<class DependentFunctionType, int ArgNum> struct PP_CAT(FUNCNAME,_FuncDefHelper); \
PP_REPEAT_N(FUNCNAME, F_DEF_HELPER_N, PP_EMPTY, PP_REPEAT_MAX) \
template struct PP_CAT(FUNCNAME,_FuncDefHelper)<decltype(FUNCNAME), F_ArgNum<decltype(FUNCNAME)>::value>;

F_DEF(lua_newstate, 0x65C00)
F_DEF(lua_close, 0x65A50)
F_DEF(lua_newthread, 0x561F0)
F_DEF(lua_atpanic, 0x58150)

F_DEF(lua_gettop, 0x55F90)
F_DEF(lua_settop, 0x56E20)
F_DEF(lua_pushvalue, 0x568A0)
F_DEF(lua_remove, 0x56AC0)
F_DEF(lua_insert, 0x56010)
//F_N(lua_replace, 2, ?)
F_DEF(lua_checkstack, 0x557D0)
F_DEF(lua_xmove, 0x571F0)

F_DEF(lua_isnumber, 0x560B0)
F_DEF(lua_isstring, 0x56100)
F_DEF(lua_iscfunction, 0x56070)
//F_N(lua_isuserdata, 2, ?)
F_DEF(lua_type, 0x57180)
F_DEF(lua_typename, 0x571E0)

F_DEF(lua_equal, 0x55A20)
//F_N(lua_rawequal, 3, ?)
F_DEF(lua_lessthan, 0x56130)

F_DEF(lua_tonumber, 0x570A0)
F_DEF(lua_tointeger, 0x56F80)
F_DEF(lua_toboolean, 0x56F60)
F_DEF(lua_tolstring, 0x56FE0)
F_DEF(lua_objlen, 0x56390)
//F_N(lua_tocfunction, 2, ?)
F_DEF(lua_touserdata, 0x57100)
//F_N(lua_tothread, 2, ?)
//F_N(lua_topointer, 2, ?)

F_DEF(lua_pushnil, 0x56760)
F_DEF(lua_pushnumber, 0x56780)
F_DEF(lua_pushinteger, 0x56660)
F_DEF(lua_pushlstring, 0x566E0)
F_DEF(lua_pushstring, 0x567C0)
F_DEF(lua_pushvfstring, 0x73CA0)
// F_N(lua_pushfstring, 2, 0x56610)
F_DEF(lua_pushcclosure, 0x56540)
F_DEF(lua_pushboolean, 0x56510)
F_DEF(lua_pushlightuserdata, 0x56690)
F_DEF(lua_pushthread, 0x56850)

F_DEF(lua_gettable, 0x55F30)
F_DEF(lua_getfield, 0x55DE0)
F_DEF(lua_rawget, 0x568E0)
F_DEF(lua_rawgeti, 0x56920)
F_DEF(lua_createtable, 0x559A0)
F_DEF(lua_newuserdata, 0x56260)
F_DEF(lua_getmetatable, 0x55E90)
F_DEF(lua_getfenv, 0x55D40)

F_DEF(lua_settable, 0x56DB0)
F_DEF(lua_setfield, 0x56BB0)
F_DEF(lua_rawset, 0x569A0)
F_DEF(lua_rawseti, 0x56A20)
F_DEF(lua_setmetatable, 0x56C60)
F_DEF(lua_setfenv, 0x56B20)

F_DEF(lua_call, 0x55780)
F_DEF(lua_pcall, 0x56440)
F_DEF(lua_cpcall, 0x55950)
F_DEF(lua_load, 0x6A4B0)
//F_N(lua_dump, 3, ?)

//F_N(lua_yield, 2, ?)
//F_N(lua_resume, 2, ?)
//F_N(lua_status, 1, ?)

F_DEF(lua_gc, 0x55B40)
F_DEF(lua_error, 0x58170)
F_DEF(lua_next, 0x56310)
F_DEF(lua_concat, 0x55830)
//F_N(lua_getallocf, 2, ?)
//F_N(lua_setallocf, 3, ? )

F_DEF(luaL_openlib, 0x5B990)
F_DEF(luaL_register, 0x5BDB0)
//F_N(luaL_getmetafield, 3, ?)
F_DEF(luaL_callmeta, 0x555B0)
F_DEF(luaL_typerror, 0x58100)
F_DEF(luaL_argerror, 0x580C0)
F_DEF(luaL_checklstring, 0x54FE0)
F_DEF(luaL_optlstring, 0x55430)
F_DEF(luaL_checknumber, 0x55090)
F_DEF(luaL_optnumber, 0x55520)
//F_N(luaL_checkinteger, 2, ?)
F_DEF(luaL_optinteger, 0x55390)

F_DEF(luaL_checkstack, 0x55110)
F_DEF(luaL_checktype, 0x55170)
//F_N(luaL_checkany, 3, ?)
F_DEF(luaL_newmetatable, 0x552B0)
F_DEF(luaL_checkudata, 0x551E0)
F_DEF(luaL_where, 0x58110)
//F_N(luaL_error, 2, 0x580D0)
//F_N(luaL_checkoption, 4, ?)
F_DEF(luaL_ref, 0x5BCE0)
F_DEF(luaL_unref, 0x5BDC0)
F_DEF(luaL_loadfile, 0x6A610)
//F_N(luaL_loadbuffer, 4, ?)
F_DEF(luaL_loadstring, 0x6A9D0)
F_DEF(luaL_newstate, 0x5B960)
F_DEF(luaL_gsub, 0x5B650)
F_DEF(luaL_findtable, 0x5B520)

F_DEF(luaL_buffinit, 0x5B440)
F_DEF(luaL_prepbuffer, 0x5BB60)
F_DEF(luaL_addlstring, 0x5B2D0)
//F_N(luaL_addstring, 2, ?)
F_DEF(luaL_addvalue, 0x5B370)
F_DEF(luaL_pushresult, 0x5BC70)

F_DEF(luaopen_base, 0x60200)
F_DEF(luaopen_io, 0x67210)
//F_N(luaopen_os, 1, ?)
F_DEF(luaopen_string, 0x64640)
F_DEF(luaopen_math, 0x655C0)
F_DEF(luaopen_debug, 0x6A3B0)
F_DEF(luaopen_package, 0x61290)
//F_N(luaL_openlibs, 1, ?)
