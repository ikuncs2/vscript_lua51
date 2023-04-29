
#include <assert.h>
#include <Windows.h>

extern "C" {
#define LUA_BUILD_AS_DLL
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

extern "C" {
    LUALIB_API int luaopen_ffi(lua_State* L);
    LUA_API int luaJIT_setmode(lua_State* L, int idx, int mode);
}

typedef void* (*CreateInterfaceFn)(const char *pName, int *pReturnCode);

struct AppSystemInfo_t
{
    const char* m_pModuleName;
    const char* m_pInterfaceName;
};

enum InitReturnVal_t
{
    INIT_FAILED = 0,
    INIT_OK,

    INIT_LAST_VAL,
};

enum AppSystemTier_t
{
    APP_SYSTEM_TIER0 = 0,
    APP_SYSTEM_TIER1,
    APP_SYSTEM_TIER2,
    APP_SYSTEM_TIER3,

    APP_SYSTEM_TIER_OTHER,
};

enum BuildType_t
{
    kBuildTypeRelease = 2
};

class IAppSystem
{
public:
    // Here's where the app systems get to learn about each other
    virtual bool Connect( CreateInterfaceFn factory ) = 0;
    virtual void Disconnect() = 0;

    // Here's where systems can access other interfaces implemented by this object
    // Returns NULL if it doesn't implement the requested interface
    virtual void *QueryInterface( const char *pInterfaceName ) = 0;

    // Init, shutdown
    virtual InitReturnVal_t Init() = 0;
    virtual void Shutdown() = 0;
    virtual void PreShutdown() = 0;

    // Returns all dependent libraries
    virtual const AppSystemInfo_t* GetDependencies() = 0;

    // Returns the tier
    virtual AppSystemTier_t GetTier() = 0;

    // Reconnect to a particular interface
    virtual void Reconnect(CreateInterfaceFn factory, const char* pInterfaceName) = 0;

    // Returns whether or not the app system is a singleton
    virtual bool IsSingleton() = 0;

    // Source 2 Added
    virtual BuildType_t	GetBuildType() = 0;
};

enum ScriptLanguage_t
{
    SL_NONE,
    SL_LUA,

    SL_DEFAULT = SL_LUA
};

class IScriptVMPartA
{
public:
    virtual void DeleteThis() = 0; // ?
    virtual bool Init() = 0;
    virtual bool ConnectDebugger() = 0;
    virtual void DisconnectDebugger() = 0;

    virtual ScriptLanguage_t GetLanguage() = 0;
    virtual const char* GetLanguageName() = 0;

    virtual void* GetInternalVM() = 0;

    // TODO : more
};

class IScriptVM;
class IScriptDebugger;

class IScriptManager : public IAppSystem
{
public:
    virtual IScriptVM *CreateVM( ScriptLanguage_t language = SL_DEFAULT ) = 0;
    virtual void DestroyVM( IScriptVM * ) = 0;
    virtual IScriptDebugger* GetDebugger() = 0;
};
#define VSCRIPT_INTERFACE_VERSION		"VScriptManager010"

CreateInterfaceFn GetOrigFactory()
{
    auto dl = LoadLibrary("vscript2.dll");
    return (CreateInterfaceFn)GetProcAddress(dl, "CreateInterface");
}

IScriptManager *g_pScriptManager = nullptr;

class CScriptManagerExt : public IScriptManager
{
public:
    bool Connect( CreateInterfaceFn factory ) override
    {
        return g_pScriptManager->Connect(factory);
    }

    void Disconnect() override
    {
        return g_pScriptManager->Disconnect();
    }

    void *QueryInterface( const char *pInterfaceName ) override
    {
        return g_pScriptManager->QueryInterface(pInterfaceName);
    }

    InitReturnVal_t Init() override
    {
        return g_pScriptManager->Init();
    }

    void Shutdown() override
    {
        return g_pScriptManager->Shutdown();
    }

    void PreShutdown() override
    {
        return g_pScriptManager->PreShutdown();
    }

    const AppSystemInfo_t* GetDependencies() override
    {
        return g_pScriptManager->GetDependencies();
    }

    AppSystemTier_t GetTier() override
    {
        return g_pScriptManager->GetTier();
    }

    void Reconnect(CreateInterfaceFn factory, const char* pInterfaceName) override
    {
        return g_pScriptManager->Reconnect(factory, pInterfaceName);
    }

    bool IsSingleton() override
    {
        return g_pScriptManager->IsSingleton();
    }

    BuildType_t	GetBuildType() override
    {
        return g_pScriptManager->GetBuildType();
    }

    IScriptVM *CreateVM( ScriptLanguage_t language = SL_DEFAULT ) override
    {
        auto vm = g_pScriptManager->CreateVM(language);
        auto vm2 = (IScriptVMPartA*)vm;
        
        lua_State *L = (lua_State *)vm2->GetInternalVM();

        // moemod : turn on jit again
#define LUAJIT_MODE_OFF		0x0000	/* Turn feature off. */
#define LUAJIT_MODE_ON		0x0100	/* Turn feature on. */
#define LUAJIT_MODE_FLUSH	0x0200	/* Flush JIT-compiled code. */
        luaJIT_setmode(L, 0, LUAJIT_MODE_ON);

        lua_getglobal(L, "print"); // #1
        lua_pushstring(L, "vscript patch: hello, world !!!"); // #2
        lua_call(L, 1, 0); // #0
        
        // moemod hack : restore package
        // valve sets package.loaders[*] and package.loadlib to nil
        // here we are going to restore them

        lua_getglobal(L, "package"); // #1 = package
        lua_getfield(L, -1, "loaders"); // #2 = package.loaders
        lua_pushinteger(L, 2); // #3 = 2
        lua_gettable(L, -2); // #3 = package.loaders[2]

        int valve_loader_ref = luaL_ref(L, LUA_REGISTRYINDEX); // #2 = package.loaders
        lua_pop(L, 2); // #0

        lua_pushcfunction(L, luaopen_package); // #1
        lua_call(L, 0, 0); // #0

        lua_getglobal(L, "table"); // #1 = table
        lua_getfield(L, -1, "insert"); // #2 = table.insert
        lua_remove(L, -2); // #1 = table.insert

        lua_getglobal(L, "package"); // #2 = package
        lua_getfield(L, -1, "loaders"); // #3 = package.loaders
        lua_remove(L, -2); // #2 = package.loaders

        lua_pushinteger(L, 2); // #3 = 2
        lua_rawgeti(L, LUA_REGISTRYINDEX, valve_loader_ref); // #4 = valve_loader
        lua_call(L, 3, 0); // #0

        luaL_unref(L, LUA_REGISTRYINDEX, valve_loader_ref);

        // moemod hack : restore io
        // not working, use local io = require("cio")
        //lua_pushcfunction(L, luaopen_io);
        //lua_pushstring(L, "io");
        //lua_pcall(L, 1, 0, 0);
        
        // moemod hack : restore ffi
        // not working, use local io = require("cffi")
        //luaL_findtable(L, LUA_REGISTRYINDEX, "_PRELOAD", 1);
        //lua_pushcfunction(L, luaopen_ffi);
        //lua_setfield(L, -2, "ffi");
        //lua_pop(L, 1);
        
        return vm;
    }

    void DestroyVM( IScriptVM *vm ) override
    {
        return g_pScriptManager->DestroyVM(vm);
    }

    IScriptDebugger* GetDebugger() override
    {
        return g_pScriptManager->GetDebugger();
    }
} g_ScriptManagerExt;

#if defined( _WIN32 )

// Used for dll exporting and importing
#define  DLL_EXPORT   extern "C" __declspec( dllexport )
#define  DLL_IMPORT   extern "C" __declspec( dllimport )

// Can't use extern "C" when DLL exporting a class
#define  DLL_CLASS_EXPORT   __declspec( dllexport )
#define  DLL_CLASS_IMPORT   __declspec( dllimport )

// Can't use extern "C" when DLL exporting a global
#define  DLL_GLOBAL_EXPORT   extern __declspec( dllexport )
#define  DLL_GLOBAL_IMPORT   extern __declspec( dllimport )

#elif defined(_LINUX) || defined(__APPLE__)
// Used for dll exporting and importing
#define  DLL_EXPORT   extern "C" __attribute__ ((visibility("default")))
#define  DLL_IMPORT   extern "C"

// Can't use extern "C" when DLL exporting a class
#define  DLL_CLASS_EXPORT __attribute__ ((visibility("default")))
#define  DLL_CLASS_IMPORT

// Can't use extern "C" when DLL exporting a global
#define  DLL_GLOBAL_EXPORT   extern __attribute ((visibility("default")))
#define  DLL_GLOBAL_IMPORT   extern

#else
#error "Unsupported Platform."
#endif

// EXPORT
DLL_EXPORT void * CreateInterface(const char *pName, int *pReturnCode)
{
    static CreateInterfaceFn vscript_factory = GetOrigFactory();
    if(!strcmp(pName, VSCRIPT_INTERFACE_VERSION))
    {
        g_pScriptManager = (IScriptManager *)vscript_factory(pName, pReturnCode);
        return &g_ScriptManagerExt;
    }
    return vscript_factory(pName, pReturnCode);
}
