//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Linux launcher for Hammer
//
//===========================================================================//

#include "appframework/AppFramework.h"
#include "ihammer.h"
#include "tier0/dbg.h"
#include "vstdlib/cvar.h"
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "filesystem_init.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "vgui/ivgui.h"
#include "vgui/ISurface.h"
#include "inputsystem/iinputsystem.h"
#include "tier0/icommandline.h"

#if defined( _WIN32 )
#include <windows.h>
#include "p4lib/ip4.h"
#endif

#if defined( POSIX )
#include <limits.h>
#endif

//-----------------------------------------------------------------------------
// Global systems
//-----------------------------------------------------------------------------

IHammer *g_pHammer = NULL;
IMaterialSystem *g_pMaterialSystem = NULL;
IFileSystem *g_pFileSystem = NULL;
IDataCache *g_pDataCache = NULL;
IInputSystem *g_pInputSystem = NULL;

//-----------------------------------------------------------------------------
// Application
//-----------------------------------------------------------------------------

class CHammerApp : public CAppSystemGroup
{
public:
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown();
	virtual void Destroy();
};

CHammerApp g_ApplicationObject;

//-----------------------------------------------------------------------------
// Spew
//-----------------------------------------------------------------------------

SpewRetval_t HammerSpewFunc(
	SpewType_t type,
	tchar const *pMsg )
{
	if ( type == SPEW_ASSERT )
	{
		return SPEW_DEBUGGER;
	}

	if ( type == SPEW_ERROR )
	{
#if defined( _WIN32 )
		MessageBoxA(
			NULL,
			pMsg,
			"Hammer Error",
			MB_OK | MB_ICONSTOP );
#else
		fprintf( stderr, "\nHAMMER ERROR:\n%s\n", pMsg );
#endif

		return SPEW_ABORT;
	}

	return SPEW_CONTINUE;
}

//-----------------------------------------------------------------------------
// Create systems
//-----------------------------------------------------------------------------

bool CHammerApp::Create()
{
	// Save memory.
	CommandLine()->AppendParm( "-disallowhwmorph", NULL );

	// ------------------------------------------------------------------------
	// CVAR
	// ------------------------------------------------------------------------

	IAppSystem *pSystem;

	AppModule_t cvarModule = LoadModule(
		VStdLib_GetICVarFactory() );

	pSystem = AddSystem(
		cvarModule,
		CVAR_INTERFACE_VERSION );

	if ( !pSystem )
	{
		Error( "Unable to create CVAR system\n" );
		return false;
	}

	// ------------------------------------------------------------------------
	// FileSystem
	// ------------------------------------------------------------------------

	bool bSteam = false;

	char pFileSystemDLL[MAX_PATH];

	if ( FileSystem_GetFileSystemDLLName(
		pFileSystemDLL,
		sizeof( pFileSystemDLL ),
		bSteam ) != FS_OK )
	{
		Error( "Unable to determine filesystem module\n" );
		return false;
	}

	AppModule_t fileSystemModule =
		LoadModule( pFileSystemDLL );

	g_pFileSystem =
		(IFileSystem *)AddSystem(
			fileSystemModule,
			FILESYSTEM_INTERFACE_VERSION );

	if ( !g_pFileSystem )
	{
		Error( "Unable to create filesystem system\n" );
		return false;
	}

	FileSystem_SetBasePaths( g_pFileSystem );

	// ------------------------------------------------------------------------
	// Application systems
	//
	// DLL_EXT_STRING becomes ".so" on Linux in the Waf configuration.
	// ------------------------------------------------------------------------

	AppSystemInfo_t appSystems[] =
	{
		{ "libmaterialsystem" DLL_EXT_STRING,
			MATERIAL_SYSTEM_INTERFACE_VERSION },

		{ "libinputsystem" DLL_EXT_STRING,
			INPUTSYSTEM_INTERFACE_VERSION },

		{ "libstudiorender" DLL_EXT_STRING,
			STUDIO_RENDER_INTERFACE_VERSION },

		{ "libvphysics" DLL_EXT_STRING,
			VPHYSICS_INTERFACE_VERSION },

		{ "libdatacache" DLL_EXT_STRING,
			DATACACHE_INTERFACE_VERSION },

		{ "libdatacache" DLL_EXT_STRING,
			MDLCACHE_INTERFACE_VERSION },

		{ "libdatacache" DLL_EXT_STRING,
			STUDIO_DATA_CACHE_INTERFACE_VERSION },

		{ "libvguimatsurface" DLL_EXT_STRING,
			VGUI_SURFACE_INTERFACE_VERSION },

		{ "libvgui2" DLL_EXT_STRING,
			VGUI_IVGUI_INTERFACE_VERSION },

		{ "libhammer_dll" DLL_EXT_STRING,
			INTERFACEVERSION_HAMMER },

		{ "", "" }
	};

	if ( !AddSystems( appSystems ) )
	{
		Error( "Unable to load Hammer systems\n" );
		return false;
	}

	// ------------------------------------------------------------------------
	// Optional Perforce
	// ------------------------------------------------------------------------

#if defined( _WIN32 )
	if ( !CommandLine()->CheckParm( "-nop4" ) )
	{
		AppModule_t p4Module =
			LoadModule( "p4lib.dll" );

		AddSystem(
			p4Module,
			P4_INTERFACE_VERSION );
	}
#endif

	// ------------------------------------------------------------------------
	// Get interfaces
	// ------------------------------------------------------------------------

	g_pMaterialSystem =
		(IMaterialSystem *)FindSystem(
			MATERIAL_SYSTEM_INTERFACE_VERSION );

	g_pHammer =
		(IHammer *)FindSystem(
			INTERFACEVERSION_HAMMER );

	g_pDataCache =
		(IDataCache *)FindSystem(
			DATACACHE_INTERFACE_VERSION );

	g_pInputSystem =
		(IInputSystem *)FindSystem(
			INPUTSYSTEM_INTERFACE_VERSION );

	if ( !g_pHammer )
	{
		Error( "IHammer interface was not found\n" );
		return false;
	}

	if ( !g_pMaterialSystem )
	{
		Error( "IMaterialSystem interface was not found\n" );
		return false;
	}

	// ------------------------------------------------------------------------
	// Shader API
	// ------------------------------------------------------------------------

#if defined( LINUX )
	// Source fork contains a Linux OpenGL shader API.
	g_pMaterialSystem->SetShaderAPI( "shaderapigl.so" );
#else
	g_pMaterialSystem->SetShaderAPI( "shaderapidx9.dll" );
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Destroy
//-----------------------------------------------------------------------------

void CHammerApp::Destroy()
{
	g_pFileSystem = NULL;
	g_pMaterialSystem = NULL;
	g_pDataCache = NULL;
	g_pHammer = NULL;
	g_pInputSystem = NULL;
}

//-----------------------------------------------------------------------------
// PreInit
//-----------------------------------------------------------------------------

bool CHammerApp::PreInit()
{
	SpewOutputFunc( HammerSpewFunc );

	if ( !g_pHammer->InitSessionGameConfig(
		GetVProjectCmdLineValue() ) )
	{
		return false;
	}

	CFSSearchPathsInit initInfo;

	initInfo.m_pFileSystem =
		g_pFileSystem;

	initInfo.m_pDirectoryName =
		g_pHammer->GetDefaultModFullPath();

	if ( FileSystem_LoadSearchPaths(
		initInfo ) != FS_OK )
	{
		Error(
			"Unable to load search paths!\n" );

		return false;
	}

	g_pMaterialSystem->EnableEditorMaterials();

	g_pMaterialSystem->SetAdapter(
		0,
		MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE );

	return true;
}

//-----------------------------------------------------------------------------
// PostShutdown
//-----------------------------------------------------------------------------

void CHammerApp::PostShutdown()
{
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int CHammerApp::Main()
{
	return g_pHammer->MainLoop();
}

//-----------------------------------------------------------------------------
// Linux entry point
//-----------------------------------------------------------------------------

#if defined( LINUX )

int main( int argc, char **argv )
{
	return AppMain(
		argc,
		argv,
		&g_ApplicationObject );
}

#else

DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR(
	g_ApplicationObject );

#endif