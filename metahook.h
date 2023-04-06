#pragma once

inline PVOID MH_GetModuleBase(HMODULE hModule)
{
	MEMORY_BASIC_INFORMATION mem;

	if (!VirtualQuery(hModule, &mem, sizeof(MEMORY_BASIC_INFORMATION)))
		return 0;

	return (PVOID)mem.AllocationBase;
}

inline DWORD MH_GetModuleSize(HMODULE hModule)
{
	return ((IMAGE_NT_HEADERS*)((LONG_PTR)hModule + ((IMAGE_DOS_HEADER*)hModule)->e_lfanew))->OptionalHeader.SizeOfImage;
}

inline void* MH_SearchPattern(void* pStartSearch, DWORD dwSearchLen, const char* pPattern, DWORD dwPatternLen)
{
	LONG_PTR dwStartAddr = (LONG_PTR)pStartSearch;
	LONG_PTR dwEndAddr = dwStartAddr + dwSearchLen - dwPatternLen;

	while (dwStartAddr < dwEndAddr)
	{
		bool found = true;

		for (DWORD i = 0; i < dwPatternLen; i++)
		{
			char code = *(char*)(dwStartAddr + i);
			if (pPattern[i] != 0x2A && pPattern[i] != code)
			{
				found = false;
				break;
			}
		}

		if (found)
			return (void*)dwStartAddr;

		dwStartAddr++;
	}

	return 0;
}