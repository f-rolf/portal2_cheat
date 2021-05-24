#pragma once

#define FL_ONGROUND (1 << 0)

#define IN_ATTACK		(1 << 0)
#define IN_JUMP			(1 << 1)
#define IN_DUCK			(1 << 2)
#define IN_FORWARD		(1 << 3)
#define IN_BACK			(1 << 4)
#define IN_USE			(1 << 5)

class CBaseEntity;

class QAngle
{
public:
	float x, y, z;
};

class CUserCmd
{//size = 96
public:
	virtual ~CUserCmd() { };	// 0
	int command_number;			// 4
	int tick_count;				// 8
	QAngle viewangles;			// 12 - 20
	float forwardmove;			// 24
	float sidemove;				// 28
	float upmove;				// 32
	int buttons;				// 36
	byte impulse;				// 40
	char pad0[10];				// 41 - 51
	int random_seed;			// 52
	short mousedx;				// 56
	short mousedy;				// 58
	bool hasbeenpredicted;		// 60
	int pad1[8];				// 64 - 96
};

struct UtlVector_t
{
	PVOID* m_pMemory;
	int m_nAllocationCount;
	int m_nGrowSize;
	int m_Size;
	PVOID* m_pElements;
};

class CVar
{
public:
	CVar()
	{
		memset(this, 0, sizeof(CVar));
	}
	void* vtable;					//0x0000
	CVar* pNext;					//0x0004 
	__int32 bRegistered;			//0x0008 
	const char* pszName;			//0x000C 
	const char* pszHelpString;		//0x0010 
	__int32 nFlags;					//0x0014 
	void* icvarVtable;				//0x0018
	CVar* pParent;					//0x001C 
	const char* cstrDefaultValue;	//0x0020 
	const char* cstrValue;			//0x0024 
	__int32 stringLength;			//0x0028
	float fValue;					//0x002C 
	__int32 nValue;					//0x0030 
	__int32 bHasMin;				//0x0034 
	float fMinVal;					//0x0038 
	__int32 bHasMax;				//0x003C 
	float fMaxVal;					//0x0040 
	UtlVector_t utlvecCallbacks;	//0x0044
};

class ICVar
{
public:
	void RegisterConCommand(CVar* pCVar)
	{
		typedef void(__thiscall* OriginalFn)(void*, CVar*);
		getvfunc<OriginalFn>(this, 12)(this, pCVar);
	}
};
using CvarConstructorFn = void(__thiscall*)(CVar* cvar, const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax);

class CEngineClient
{
public:
	int GetLocalPlayer(void)
	{
		typedef int(__thiscall* OriginalFn)(void*);
		return getvfunc<OriginalFn>(this, 12)(this);
	}
};

class CEntityList
{
public:
	CBaseEntity * GetClientEntity(int entnum)
	{
		typedef CBaseEntity* (__thiscall* OriginalFn)(PVOID, int);
		return getvfunc<OriginalFn>(this, 3)(this, entnum);
	}
};
