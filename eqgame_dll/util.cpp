#include "eqgame.h"
#include <stdio.h>

// string functions using a static buffer
static char text[2048];

void Log(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vsnprintf_s(text, sizeof(text), _TRUNCATE, format, ap);
	va_end(ap);

	OutputDebugStringA(text);
}
void Log(const char *msg)
{
	return Log("%s", msg);
}

void Patch(void *dst, const void *src, size_t len)
{
	DWORD oldProtect;
	VirtualProtect(dst, len, PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy(dst, src, len);
	VirtualProtect(dst, len, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), dst, len);
}

bool Detour(void *address, void *dst)
{
	DWORD oldProtect;
	int len = 5; // 5 bytes for E9 + address

	// make memory writeable
	VirtualProtect(address, len, PAGE_EXECUTE_READWRITE, &oldProtect);

	// calculate relative jump target address
	intptr_t relativeAddress = ((intptr_t)dst - (intptr_t)address) - len;

	// write jmp instruction
	*(unsigned char *)address = 0xE9;	// jmp
	*(intptr_t *)((intptr_t)address + 1) = relativeAddress; // target

	// restore memory protection
	VirtualProtect(address, len, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), NULL, NULL);

	return true;
}

void *MakeTrampoline(void *target, int len)
{
	// allocate memory for trampoline function (len + 5 for the return jmp)
	void *trampoline = VirtualAlloc(0, len + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!trampoline) return NULL;

	// copy len bytes from the target into the trampoline
	// for Detour() it only needs 5 bytes saved but the entire function could be copied if needed
	memcpy(trampoline, target, len);

	// calculate relative jump target address
	intptr_t trampRelativeAddress = (intptr_t)target - (intptr_t)trampoline - 5;

	// write jmp instruction to the end of the trampoline
	*(unsigned char *)((intptr_t)trampoline + len) = 0xE9;
	*(intptr_t *)((intptr_t)trampoline + len + 1) = trampRelativeAddress;

	return trampoline;
}

void *DetourWithTrampoline(void *address, void *dst, int len)
{
	// Detour will overwrite 5 bytes at address, save it in a trampoline first
	void *trampoline = MakeTrampoline(address, len);
	Detour(address, dst);

	return trampoline;
}

void *FindIATPointer(HMODULE module, const char *functionName)
{
	uintptr_t baseAddr = (uintptr_t)module;
	void *ret = NULL;

	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)baseAddr;
	if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE)
	{
		PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)(dosHeader->e_lfanew + baseAddr);
		if (ntHeader->Signature == IMAGE_NT_SIGNATURE)
		{
			PIMAGE_OPTIONAL_HEADER oh = &ntHeader->OptionalHeader;
			if (oh->Magic == 0x020b /*64*/ || oh->Magic == 0x010b /*32*/)
			{
				// import table should be second entry
				PIMAGE_DATA_DIRECTORY importDataDirectory = &oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
				PIMAGE_IMPORT_DESCRIPTOR idt = (PIMAGE_IMPORT_DESCRIPTOR)(importDataDirectory->VirtualAddress + baseAddr);

				// loop through each imported dll
				for (DWORD i = 0; i < importDataDirectory->Size / sizeof(IMAGE_IMPORT_DESCRIPTOR); i++)
				{
					if (idt[i].Characteristics && idt[i].Name)
					{
						//Log("IAT %i %s\n", i, (char *)(idt[i].Name + baseAddr)); // dll name

						PIMAGE_THUNK_DATA ilt = (PIMAGE_THUNK_DATA)(idt[i].OriginalFirstThunk + baseAddr);
						PIMAGE_THUNK_DATA iat = (PIMAGE_THUNK_DATA)(idt[i].FirstThunk + baseAddr);

						// loop through each imported function
						for (int j = 0; ilt[j].u1.AddressOfData; j++)
						{
							if (!(ilt[j].u1.Ordinal & IMAGE_ORDINAL_FLAG)) // skip ordinal imports
							{
								PIMAGE_IMPORT_BY_NAME import = (PIMAGE_IMPORT_BY_NAME)(ilt[j].u1.AddressOfData + baseAddr);
								if (import->Name)
								{
									//Log("    %s %p %p\n", import->Name, iat[j].u1.Function, &iat[j].u1.Function);
									if (!strcmp((char *)import->Name, functionName))
									{
										ret = &iat[j].u1.Function;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	return ret;
}

void *DetourIAT(HMODULE module, const char* functionName, void *detour)
{
	uintptr_t* ptr = (uintptr_t*)FindIATPointer(module, functionName);
	void* ret = (void*)*ptr;
	if (ptr)
	{
		uintptr_t addr = (uintptr_t)detour;
		Patch((void*)ptr, &addr, 4);
	}

	return ret;
}
