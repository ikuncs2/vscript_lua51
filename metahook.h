#pragma once

inline PVOID MH_GetModuleBase(HMODULE hModule)
{
	MEMORY_BASIC_INFORMATION mem;

	if (!VirtualQuery(hModule, &mem, sizeof(MEMORY_BASIC_INFORMATION)))
		return 0;

	return (PVOID)mem.AllocationBase;
}

inline LONG_PTR MH_GetModuleSize(HMODULE hModule)
{
	return ((IMAGE_NT_HEADERS*)((LONG_PTR)hModule + ((IMAGE_DOS_HEADER*)hModule)->e_lfanew))->OptionalHeader.SizeOfImage;
}

inline void* MH_SearchPattern(void* pStartSearch, LONG_PTR dwSearchLen, const char* pPattern, LONG_PTR dwPatternLen)
{
	BYTE *dwStartAddr = (BYTE *)pStartSearch;
	BYTE *dwEndAddr = dwStartAddr + dwSearchLen - dwPatternLen;

	while (dwStartAddr < dwEndAddr)
	{
		bool found = true;

		for (LONG_PTR i = 0; i < dwPatternLen; i++)
		{
			if ((BYTE)pPattern[i] != 0x2A && (BYTE)pPattern[i] != dwStartAddr[i])
			{
				found = false;
				break;
			}
		}

		if (found)
			return dwStartAddr;

		dwStartAddr++;
	}

	return 0;
}