#include <windows.h>

#include "vfuncs.h"
#include "interfaces.h"
#include "ModuleData.h"

// interface and function pointers
ICVar* g_pCVars = nullptr;
CEngineClient* g_pEngineClient = nullptr;
CEntityList* g_pEntityList = nullptr;
void* g_pClientMode = nullptr;

// our cvars to let the user toggle activation of both features
CVar speed_factor;
CVar auto_bunnyhop;

// create move hook for bunnyhopping
VMTHook* g_pClientHook = nullptr;
using CreateMoveFn = bool(__thiscall*)(PVOID, float, CUserCmd*);
CreateMoveFn oCreateMove = nullptr;
bool __stdcall hkdCreateMove(float input_sample_frametime, CUserCmd* pCmd)
{
	auto ret = oCreateMove(g_pClientMode, input_sample_frametime, pCmd);

	auto pLocalPlayer = g_pEntityList->GetClientEntity(g_pEngineClient->GetLocalPlayer());
	
	if (pLocalPlayer)
	{
		int flags = *(int*)((DWORD)pLocalPlayer + 0xF8);
		bool onground = (flags & FL_ONGROUND) != 0;

		// if bunnyhopping is enabled
		// and the player is holding the jump key
		// and the player is not on ground
		// remove the IN_JUMP flag to only send it when we hit the ground again
		if (auto_bunnyhop.nValue == 1 && pCmd->buttons & IN_JUMP && !onground)
			pCmd->buttons &= ~IN_JUMP;
	}

	return ret;
}

// cl_move hook for speedhake
void* pCL_Move = 0;
void* CL_Move_trampoline = nullptr;
using CL_MoveFn = void(__cdecl*)(float, bool);
void __cdecl CL_Move_hook(float extraSamples, bool finalTick)
{
#define call_original reinterpret_cast<CL_MoveFn>(CL_Move_trampoline)

	if (!(GetAsyncKeyState(VK_LSHIFT) & 0x8000))
		return call_original(extraSamples, finalTick);

	// this will let us process as many ticks of movement per actual tick as we want
	// since the player isnt updated until after this function returns,
	// this may interfere with bunnyhopping
	for (int i = 0; i < speed_factor.nValue; i++) 
	{
		call_original(extraSamples, finalTick);
	}

#undef call_original
}

// create a simple jmp hook (aka detour, aka trampoline hook) for CL_Move
bool create_trampoline_hook()
{
	// allocate trampoline
	CL_Move_trampoline = VirtualAlloc(0, 11, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (CL_Move_trampoline == nullptr)
		return false;

	// copy the first few instructions of the target to the trampoline
	memcpy(CL_Move_trampoline, pCL_Move, 6);
	
	// change the next instruction to a jump
	*(char*)((uint)(CL_Move_trampoline) + 6) = '\xE9';

	// copy in the relative address of what comes after the instructions we copied
	auto relative_return = (uint)(pCL_Move)-(uint)(CL_Move_trampoline)-5;
	*(uint*)((uint)(CL_Move_trampoline) + 7) = relative_return;

	// make the target functions first 5 bytes writeable
	DWORD protection_buf;
	VirtualProtect(pCL_Move, 5, PAGE_EXECUTE_READWRITE, &protection_buf);

	// place our jump to our hook function
	*(char*)pCL_Move = '\xE9';
	auto relative_hook = (uint)(&CL_Move_hook)-(uint)(pCL_Move)-5;
	*(uint*)((uint)(pCL_Move) + 1) = relative_hook;

	// change the permissions back to what they were
	VirtualProtect(pCL_Move, 5, protection_buf, &protection_buf);

	return true;
}

// the setup thread to init everything else
DWORD __stdcall setup_thread()
{
#define errormsg(x) MessageBoxA(NULL, x, "error", MB_OK)

	// getting cvar interface
	ModuleData vstd("vstdlib.dll");
	if (!vstd.valid())
	{
		errormsg("vstdlib.dll not found!");
		return 0;
	}

	g_pCVars = vstd.capture_interface<ICVar>("VEngineCvar007");
	if (!g_pCVars)
	{
		errormsg("g_pCVars not found!");
		return 0;
	}

	// getting client.dll stuff
	ModuleData client("client.dll");
	if (!client.valid())
	{
		errormsg("client.dll not found!");
		return 0;
	}

	g_pEntityList = client.capture_interface<CEntityList>("VClientEntityList003");
	if (!g_pEntityList)
	{
		errormsg("g_pEntityList not found!");
		return 0;
	}

	auto rel_call_address = client.pattern_scan<DWORD>("E8 ? ? ? ? 83 3E 01");
	// we need to get the absolute address of this relative E8 call instruction
	// so we add the return address to the dereferenced relative address to get it
	auto get_client_mode = reinterpret_cast<void*(__cdecl*)(void)>(rel_call_address + 5 + (*(DWORD*)(rel_call_address + 1)));
	g_pClientMode = get_client_mode();

	if (!g_pClientMode)
	{
		errormsg("g_pClientMode not found!");
		return 0;
	}

	// getting engine.dll stuff
	ModuleData engine("engine.dll");
	if (!engine.valid())
	{
		errormsg("engine.dll not found!");
		return 0;
	}

	g_pEngineClient = engine.capture_interface<CEngineClient>("VEngineClient015");
	
	if (!g_pEngineClient)
	{
		errormsg("g_pEngineClient not found!");
		return 0;
	}

	pCL_Move = engine.pattern_scan<void*>("55 8B EC 83 EC 24 56 E8");

	if (!pCL_Move)
	{
		errormsg("pCL_Move not found!");
		return 0;
	}

	// setting up and registering our console vars
	auto CVarConstructor = engine.pattern_scan<CvarConstructorFn>("55 8B EC F3 0F 10 45 ? 8B 55 14");
	if (!CVarConstructor)
	{
		errormsg("CVarConstructor not found!");
		return 0;
	}

	CVarConstructor(&speed_factor, "speed_factor", "1", 0, "The amount of additional ticks per original tick (ghetto speedhack factor)", true, 1, true, 256);
	g_pCVars->RegisterConCommand(&speed_factor);

	CVarConstructor(&auto_bunnyhop, "auto_bunnyhop", "1", 0, "Indicates that the player should bunnyhop automatically if the jump key is being held", true, 0, true, 1);
	g_pCVars->RegisterConCommand(&auto_bunnyhop);

	// setting up our hooks
	g_pClientHook = new VMTHook(g_pClientMode);
	
	oCreateMove = g_pClientHook->hook<CreateMoveFn>(&hkdCreateMove, 24);

	if (!create_trampoline_hook())
		errormsg("failed to create trampoline");

	return 0;

#undef errormsg
}

BOOL WINAPI DllMain(HMODULE hDll, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)setup_thread, NULL, NULL, NULL);
		
	return 1;
}