#pragma once
#include <Psapi.h>

#define INRANGE(x,a,b) (x >= a && x <= b) 
#define SigGetBits( x ) (INRANGE((x&(~0x20)),'A','F') ? ((x&(~0x20)) - 'A' + 0xa) : (INRANGE(x,'0','9') ? x - '0' : 0))
#define SigGetByte( x ) (SigGetBits(x[0]) << 4 | SigGetBits(x[1]))

using CreateInterfaceFn = void* (__cdecl*)(const char*, int*);

class ModuleData
{
public:
	ModuleData(const char* moduleName, bool grabFactory = true)
	{
		bValid = false;
		factory = nullptr;

		hMod = GetModuleHandleA(moduleName);
		if (!hMod)
			return;

		if (grabFactory)
		{
			factory = proc_address<CreateInterfaceFn>("CreateInterface");
			if (!factory)
				return;
		}

		MODULEINFO modInfo;
		if (!K32GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(MODULEINFO)))
			return;

		scanStartAddr = (DWORD)modInfo.lpBaseOfDll;
		scanEndAddr = scanStartAddr + modInfo.SizeOfImage;

		if (!scanStartAddr)
			return;

		bValid = true;
	}

	template <typename T>
	inline T proc_address(const char* name)
	{
		return reinterpret_cast<T>(GetProcAddress(hMod, name));
	}

	template <typename T>
	inline T* capture_interface(const char* name)
	{
		return reinterpret_cast<T*>(factory(name, nullptr));
	}

	template <typename T>
	inline T pattern_scan(const char* sig, int deref = 0, int offset = 0)
	{
		return reinterpret_cast<T>(_pattern_scan(sig,deref, offset));
	}

	inline bool valid() const { return bValid; }
private:
	void* _pattern_scan(const char* sig, int deref, int offset)
	{
		//credits for the scanning code belongt to learn_more
		const char* pat = sig;
		PVOID match = 0;
		bool found = false;
		for (DWORD pCur = scanStartAddr; pCur < scanEndAddr; pCur++)
		{
			if (!*pat)
			{
				found = true;
				break;
			}

			if (*(PBYTE)pat == '\?' || *(BYTE*)pCur == SigGetByte(pat))
			{

				if (!match)
					match = reinterpret_cast<PVOID>(pCur);

				if (!pat[2])
				{
					found = true;
					break;
				}

				pat += (*(PWORD)pat == '\?\?' || *(PBYTE)pat != '\?') ? 3 : 2;
			}
			else
			{
				pat = sig;
				match = 0;
			}
		}

		if (!found)
			return nullptr;

		if (offset != 0)
			match = reinterpret_cast<PVOID>((DWORD)match + offset);

		if (deref > 0)
		{
			for (int i = 0; i < deref; i++)
			{
				if (!match)
					return nullptr;

				match = *reinterpret_cast<PVOID*>(match);
			}
		}

		return match;
	}

	HMODULE hMod;
	CreateInterfaceFn factory;
	DWORD scanStartAddr;
	DWORD scanEndAddr;
	bool bValid;
};

