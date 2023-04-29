#include <Windows.h>
#include <mutex>

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

#define F_DEF(FUNCNAME, ...) ptrdiff_t g_addr_##FUNCNAME;
#include "signatures.h"
#undef F_DEF

bool SearchSignatures()
{
	bool failed = false;
#define F_DEF(FUNCNAME, ...) \
	if(g_addr_##FUNCNAME == 0) \
	{ \
		g_addr_##FUNCNAME = FindFunction(#FUNCNAME, __VA_ARGS__); \
		failed = failed || (g_addr_##FUNCNAME == 0); \
	}

#include "signatures.h"
#undef F_DEF
	return !failed;
}
std::once_flag g_bSearchSignaturesCalled;

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

// take back luajit functions
extern "C" {
	LUALIB_API int luaopen_bit(lua_State* L);
	//LUALIB_API int luaopen_jit(lua_State* L);
	LUALIB_API int luaopen_ffi(lua_State* L);
	LUALIB_API int luaopen_string_buffer(lua_State* L);

	LUA_API int luaJIT_setmode(lua_State* L, int idx, int mode);
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

#define LJ_TFUNC		(~8u)
#define itype(o)	((uint32_t)(*(int64_t *)(o) >> 47))
#define tvisfunc(o)	(itype(o) == LJ_TFUNC)
#define LJ_GCVMASK 0x7FFFFFFFFFFFULL

LUA_API lua_CFunction(lua_tocfunction) (lua_State* L, int idx)
{
	TValue* o = index2adr(L, idx);
	uintptr_t gcptr = (*(int64_t *)o & LJ_GCVMASK);

	if (tvisfunc(o)) {
		BYTE op = **(BYTE**)(gcptr + 32);
		if (op == 95 || op == 96)
			return (lua_CFunction)(gcptr + 40);
	}
	return nullptr;
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

LUALIB_API int (luaopen_jit)(lua_State* L) // not found
{
	return luaL_error(L, "luaopen_jit : not yet implemented");
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
	std::call_once(g_bSearchSignaturesCalled, SearchSignatures); \
	static auto pfn = reinterpret_cast<decltype(FUNCNAME)*>(g_addr_##FUNCNAME); \
	return pfn; \
} \
template<class DependentFunctionType, int ArgNum> struct PP_CAT(FUNCNAME,_FuncDefHelper); \
PP_REPEAT_N(FUNCNAME, F_DEF_HELPER_N, PP_EMPTY, PP_REPEAT_MAX) \
template struct PP_CAT(FUNCNAME,_FuncDefHelper)<decltype(FUNCNAME), F_ArgNum<decltype(FUNCNAME)>::value>;

#include "signatures.h"
#undef F_DEF