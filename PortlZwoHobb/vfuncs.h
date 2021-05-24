#pragma once

typedef unsigned int uint;

class VMTHook
{
public:
	VMTHook(void* ppdwClassBase)
	{
		m_ppdwClassBase = (PDWORD*)ppdwClassBase;
		m_pdwOldVMT = *m_ppdwClassBase;
		m_iVMTCount = get_vmt_count(m_pdwOldVMT);
		m_pdwNewVMT = new DWORD[m_iVMTCount];
		memcpy(m_pdwNewVMT, m_pdwOldVMT, 0x4 * m_iVMTCount);
		*m_ppdwClassBase = m_pdwNewVMT;
	}

	template<typename T>
	T hook(void* newFunc, int index)
	{
		if (index > m_iVMTCount)
			return nullptr;

		m_pdwNewVMT[index] = reinterpret_cast<DWORD>(newFunc);
		return reinterpret_cast<T>(m_pdwOldVMT[index]);
	}

private:
	int get_vmt_count(PDWORD pVMT)
	{
		DWORD dwIndex = 0;
		for (; pVMT[dwIndex]; dwIndex++)
		{
			if (IsBadCodePtr((FARPROC)pVMT[dwIndex]))
				break;
		}
		return dwIndex;
	}

	int m_iVMTCount;
	PDWORD* m_ppdwClassBase;
	PDWORD m_pdwOldVMT;
	PDWORD m_pdwNewVMT;
};

inline void** getvtable(void* inst)
{
	return *reinterpret_cast<void***>(inst);
}
template< typename Fn >
inline Fn getvfunc(void* inst, size_t index)
{
	return reinterpret_cast<Fn>(getvtable(inst)[index]);
}


