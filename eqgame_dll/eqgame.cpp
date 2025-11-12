#include "eqgame.h"
#include <string>
#include <iomanip>
#include "eqmac_functions.h"
#include "hooks.h"

#define INI_FILE "./eqclient.ini"

// these settings are controlled through the INI file
bool enableDeviceGammaRamp = false;
bool enableCPUTimebaseFix = true;
bool enableFPSLimiter = false;

// util.cpp
//void Log(const char* msg);
void Log(const char* msg, ...);
void Patch(void* dst, const void* src, size_t len);
bool Detour(void* address, void* dst);
void* MakeTrampoline(void* target, int len);
void* DetourWithTrampoline(void* address, void* dst, int len);
void* FindIATPointer(HMODULE module, const char* functionName);
void* DetourIAT(HMODULE module, const char* functionName, void* detour);

// FPSLimit.cpp
void LoadFPSLimiterIniSettings();
void Pulse();

HMODULE hEQGameEXE;
HMODULE hEQGFXMod;
static HWND* EQhWnd = reinterpret_cast<HWND*>(0x00807e04);
std::string new_title("");

void GetINIString(LPCSTR appName, LPCSTR keyName, LPCSTR defaultStr, LPSTR result, DWORD result_size, bool writeDefault)
{
	DWORD ret = GetPrivateProfileStringA(appName, keyName, "*", result, result_size, INI_FILE);
	if (result[0] == '*')
	{
		WritePrivateProfileStringA(appName, keyName, defaultStr, INI_FILE);
		strcpy_s(result, result_size, defaultStr);
	}
}

bool ParseINIBool(const char* str)
{
	if (str && (*str == 'T' || *str == 't' || *str == '1'))
		return true;
	return false;
}

LARGE_INTEGER g_ProcessorSpeed;
LARGE_INTEGER g_ProcessorTicks;

unsigned __int64 GetCpuTicks_Detour()
{
	LARGE_INTEGER qpcResult;
	QueryPerformanceCounter(&qpcResult);
	return (qpcResult.QuadPart - g_ProcessorTicks.QuadPart);
}

unsigned __int64 GetCpuSpeed2_Detour()
{
	LARGE_INTEGER Frequency;

	if (!QueryPerformanceFrequency(&Frequency))
	{
		MessageBoxW(0, L"This OS is not supported.", L"Error", 0);
		exit(-1);
	}

	g_ProcessorSpeed.QuadPart = Frequency.QuadPart / 1000;
	QueryPerformanceCounter(&g_ProcessorTicks);
	return g_ProcessorSpeed.QuadPart;
}

void InstallCPUTimebaseFixHooks()
{
	// CPU high clock speed overflow fix
	if (enableCPUTimebaseFix)
	{
		// this is in eqgame.exe
		Detour((PBYTE)0x00559BF4, (PBYTE)GetCpuTicks_Detour);

		// these two are in eqgfx_dx8.dll
		if (hEQGFXMod)
		{
			// there are two functions: GetCpuSpeed2 and GetCpuSpeed3
			// the game calls both and compares the results usually to see which worked better, so we'll just override both to go to our new function
			intptr_t cpuSpeed2 = (intptr_t)GetProcAddress(hEQGFXMod, "GetCpuSpeed2");
			if (cpuSpeed2)
			{
				Detour((PBYTE)cpuSpeed2, (PBYTE)GetCpuSpeed2_Detour);
			}
			intptr_t cpuSpeed3 = (intptr_t)GetProcAddress(hEQGFXMod, "GetCpuSpeed3");
			if (cpuSpeed3)
			{
				Detour((PBYTE)cpuSpeed3, (PBYTE)GetCpuSpeed2_Detour);
			}
		}
	}
}

bool CtrlPressed()
{
	return *(DWORD*)0x00809320 > 0;
}
bool AltPressed()
{
	return *(DWORD*)0x0080932C > 0;
}
bool ShiftPressed()
{
	return *(DWORD*)0x0080931C > 0;
}

int(__cdecl* msg_send_corpse_equip)(class EQ_Equipment*) = (int(__cdecl*)(class EQ_Equipment*))0x4DF03D;

//int base_val = 362;
class Eqmachooks {
public:

	//unsigned char CEverQuest__HandleWorldMessage_Trampoline(DWORD *,unsigned __int32,char *,unsigned __int32);
	unsigned char CEverQuest__HandleWorldMessage_Detour(DWORD* con, unsigned __int32 Opcode, char* Buffer, unsigned __int32 len)
	{
		//std::cout << "Opcode: 0x" << std::hex << Opcode << std::endl;
		if (Opcode == 0x4052) {//OP_LootItemPacket
			return msg_send_corpse_equip((EQ_Equipment*)Buffer);
		}
		if (Opcode == 0x4038) { // OP_ShopDelItem=0x3840
			if (!*(BYTE*)0x8092D8) {
				return NULL;
				// stone skin UI doesn't like this
				/*
				Merchant_DelItem_Struct* mds = (Merchant_DelItem_Struct*)(Buffer + 2);
				std::string outstring;
				outstring = "MDS npcid = ";
				outstring += std::to_string(mds->npcid);
				outstring += " slot = ";
				outstring += std::to_string(mds->itemslot);
				WriteLog(outstring);
				if (!mds->itemslot || mds->itemslot > 29)
					return NULL;
				*/
			}

		}
		/*if (Opcode == 0x400C) {
			// not using new UI
			if (!*(BYTE*)0x8092D8) {

				if (len > 2) {
					unsigned char *buff = new unsigned char[28960];
					memcpy(buff, Buffer, 2);
					int newsize = InflatePacket((unsigned char*)(Buffer + 2), len - 2, buff, 28960);
					std::string outstring;
					outstring = "Merchant inventory uncompressed size = ";
					outstring += " newsize = ";
					outstring += std::to_string(newsize);
					WriteLog(outstring);
					if (newsize > 0) {
						unsigned char* newbuff = new unsigned char[28960];
						memcpy(newbuff, buff, base_val);
						unsigned char* outbuff = new unsigned char[28960];
						int outsize = DeflatePacket((const unsigned char*)newbuff, base_val, outbuff, 28960);
						if (outsize > 0) {
							outstring = "Merchant inventory uncompressed size = ";
							outstring += "outsize = ";
							outstring += std::to_string(outsize);
							WriteLog(outstring);
							memcpy((unsigned char*)(buff + 2), outbuff, outsize);
							base_val += 362;
							return CEverQuest__HandleWorldMessage_Trampoline(con, Opcode, (char *)buff, outsize + 2);
						}
					}
					outstring = "Merchant inventory uncompressed size = ";
					outstring += std::to_string(newsize);
					outstring += " input sizeof Buffer = ";
					outstring += std::to_string(sizeof(Buffer));
					outstring += " input len = ";
					outstring += std::to_string(len);
					WriteLog(outstring);

				}
			}
		}*/

		return CEverQuest__HandleWorldMessage_Trampoline(this, con, Opcode, Buffer, len);
	}

	// for FPSLimit.cpp
	int CDisplay__Render_World_Detour()
	{
		Pulse();
		return CDisplay__Render_World_Trampoline(this);
	}

	// Level of Detail preference bug fix
	int CDisplay__StartWorldDisplay_Detour(int zoneindex, int x)
	{
		// this function always sets LoD to on, regardless of the user's preference
		int ret = CDisplay__StartWorldDisplay_Trampoline(this, zoneindex, x);

		int lod = *(char*)0x798AE8; // this is the preference the user has selected, loaded from eqOptions1.opt
		float(__cdecl * s3dSetDynamicLOD)(DWORD, float, float) = *(float(__cdecl**)(DWORD, float, float))0x007F986C; // this is a variable holding the pointer to the gfx dll function
		s3dSetDynamicLOD(lod, 1.0f, 100.0f); // apply the user's setting for real

		return ret;
	}
};

// MGB for BST
int WINAPI sub_4B8231_Detour(int a1, signed int a2) {
	if (a1 == 15 && a2 == 35)
		return 1;
	return sub_4B8231_Trampoline(a1, a2);
}

// command line parsing
int sub_4F35E5_Detour() {

	const char* v3;
	int v22;
	char exename[256];
	v3 = GetCommandLineA();
	*(int*)0x00809464 = sscanf(
		v3,
		"%s %s %s %s %s %s %s %s %s",
		&exename,
		(char*)0x806410,
		(char*)0x806510,
		(char*)0x806610,
		(char*)0x806710,
		(char*)0x806810,
		(char*)0x806910,
		(char*)0x806A10,
		(char*)0x806B10);

	DWORD v59 = 0;
	int v20 = 0x00806410;

	if (*(DWORD*)0x00809464 > 1)
	{
		while (1)
		{
			v22 = strlen((const char*)v20);
			if (v22 + strlen((char*)0x00807B08) + 2 < 0x1C0)
			{
				strcat((char*)0x00807B08, (const char*)v20);
				strcat((char*)0x00807B08, " ");
			}
			if (!_strnicmp((const char*)v20, "nosound.txt", 5u))
				break;

			if (!_strnicmp((const char*)v20, "/ticket:", 8u))
			{
				char ticket_[63];
				strncpy(ticket_, (const char*)(v20 + 8), 0x3Fu);
				if (strlen(ticket_) > 1 && ticket_[strlen(ticket_) - 1] == 34)
					ticket_[strlen(ticket_) - 1] = 0;

				// original code
				// strncpy((char *)0x00807D48, ticket_, 0x3Fu);
				// MessageBox(NULL, ticket_, NULL, MB_OK);

				// replacement code
				std::string userpass = ticket_;

				std::string username = userpass.substr(0, userpass.find("/"));
				std::string password = userpass.substr(userpass.find("/") + 1);

				if (username.length() > 3 && password.length() > 3) {
					strcpy((char*)0x807AC8, username.c_str()); // username
					strcpy((char*)0x807924, password.c_str()); // password = > likely needs encrypted pass?
				}
			}
			else if (!_strnicmp((const char*)v20, "/title:", 7u))
			{
				char my_title[128] = { 0 };
				strncpy(my_title, (const char*)(v20 + 7), 0x7Fu);
				if (strlen(my_title) > 1)
					new_title = my_title;
			}

			++v59;
			v20 += 256;
			if (v59 >= *(DWORD*)0x00809464)
				break;
		}

	}

	return sub_4F35E5_Trampoline();
}

// for chat /command parsing
int __fastcall EQMACMQ_DETOUR_CEverQuest__InterpretCmd(void* this_ptr, void* not_used, class EQPlayer* a1, char* a2)
{
	if (a1 == NULL || a2 == NULL)
	{
		return EQMACMQ_REAL_CEverQuest__InterpretCmd(this_ptr, a1, a2);
	}

	if (strlen(a2) == 0)
	{
		return EQMACMQ_REAL_CEverQuest__InterpretCmd(this_ptr, NULL, NULL);
	}

	// double slashes not needed, convert "//" to "/" by removing first character
	if (strncmp(a2, "//", 2) == 0)
	{
		memmove(a2, a2 + 1, strlen(a2));
	}
	if (strcmp(a2, "/fps") == 0) {
		// enable fps indicator
		if (hEQGFXMod) {
			if (*(BYTE*)((int)hEQGFXMod + 0x00A4F770) == 0)
				*(BYTE*)((int)hEQGFXMod + 0x00A4F770) = 1;
			else
				*(BYTE*)((int)hEQGFXMod + 0x00A4F770) = 0;
		}
		return EQMACMQ_REAL_CEverQuest__InterpretCmd(this_ptr, NULL, NULL);
	}
	else if ((strcmp(a2, "/raiddump") == 0) || (strcmp(a2, "/outputfile raid") == 0)) {
		// beginning of raid structure
		DWORD raid_ptr = 0x007914D0;
		DWORD name_ptr = raid_ptr + 72;
		DWORD level_ptr = raid_ptr + 136;
		DWORD class_ptr = raid_ptr + 144;
		DWORD is_leader_ptr = raid_ptr + 275;
		DWORD group_num_ptr = raid_ptr + 276;

		CHAR RaidLeader[64];
		CHAR CharName[64];
		CHAR Class[64];
		CHAR Level[8];
		int i = 0;
		if (*(BYTE*)(raid_ptr) == 1) {
			memcpy(RaidLeader, (char*)(0x794FA0), 64);
			char v50[64];
			char v51[256];
			time_t a2;
			a2 = time(0);
			struct tm* v4;
			v4 = localtime(&a2);
			sprintf(
				v50,
				"%04d%02d%02d-%02d%02d%02d",
				v4->tm_year + 1900,
				v4->tm_mon + 1,
				v4->tm_mday,
				v4->tm_hour,
				v4->tm_min,
				v4->tm_sec);
			sprintf(v51, "RaidRoster-%s.txt", v50);
			FILE* result;
			result = fopen(v51, "w");
			if (result != NULL) {

				while (*(BYTE*)(raid_ptr) == 1) {
					memcpy(CharName, (char*)(name_ptr), 64);
					memcpy(Level, (char*)(level_ptr), 8);
					memcpy(Class, (char*)(class_ptr), 64);
					bool group_leader = *(CHAR*)(is_leader_ptr) != 0;
					int group_num = (int)*(CHAR*)(group_num_ptr);
					group_num++;
					std::string type = "";
					if (group_leader)
						type = "Group Leader";
					if (strcmp(CharName, RaidLeader) == 0)
						type = "Raid Leader";
					raid_ptr++;
					name_ptr += 208;
					level_ptr += 208;
					class_ptr += 208;
					is_leader_ptr += 208;
					group_num_ptr += 208;

					fprintf(result, "%d\t%s\t%s\t%s\t%s\t%s\n", group_num, CharName, Level, Class, type.c_str(), "");
				}
				fclose(result);
			}
		}
		return EQMACMQ_REAL_CEverQuest__InterpretCmd(this_ptr, NULL, NULL);
	}

	return EQMACMQ_REAL_CEverQuest__InterpretCmd(this_ptr, a1, a2);
}

// fixes the skin loading UI in options
int sprintf_Detour_loadskin(char* const Buffer, const char* const format, ...)
{
	va_list ap;

	va_start(ap, format);
	char* cxstr = va_arg(ap, char*);
	cxstr += 20; // get the buffer inside the CXStr
	char useini = va_arg(ap, int);
	va_end(ap);

	return sprintf(Buffer, format, cxstr, useini);
}

// for alt-enter full screen stretch mode in eqw
int __cdecl ProcessKeyDown_Detour(int a1)
{
	if (EQ_OBJECT_CEverQuest != NULL && EQ_OBJECT_CEverQuest->GameState == 5 && a1 == 0x1c && AltPressed() && !ShiftPressed() && !CtrlPressed()) {
		auto eqw = GetModuleHandle("eqw.dll");
		FARPROC fn_get = eqw ? GetProcAddress(eqw, "GetEnableFullScreen") : nullptr;
		FARPROC fn_set = eqw ? GetProcAddress(eqw, "SetEnableFullScreen") : nullptr;
		if (fn_get && fn_set) {
			int full_screen_mode = reinterpret_cast<int(__stdcall*)()>(fn_get)();
			reinterpret_cast<void(__stdcall*)(int enable)>(fn_set)(full_screen_mode == 0);
		}
		else {
			//print_chat("Error attempting to toggle full screen mode");
		}
		return ProcessKeyDown_Trampoline(0x00); // null
	}

	return ProcessKeyDown_Trampoline(a1);
}

// faster startup
void SkipLicense(HMODULE eqmain_dll)
{
	DWORD offset = (DWORD)eqmain_dll + 0x267A0;
	const unsigned char test1[] = { 0xB0, 0x01, 0xC3 }; // return 1
	Patch((DWORD*)offset, &test1, sizeof(test1));
}

// faster startup
void SkipSplash(HMODULE eqmain_dll)
{
	DWORD offset = (DWORD)eqmain_dll + 0x2562C;
	const unsigned char test1[] = { 0xE9, 45, 0, 0, 0 }; // jump over the next 45 bytes
	Patch((DWORD*)offset, &test1, sizeof(test1));
}

// don't send character save data, server doesn't use it
void PatchSaveBypass()
{
	//const char test1[] =  { 0xEB, 0x21 };
	//PatchA((DWORD*)0x0052B70A, &test1, sizeof(test1));
	const char test1[] = { 0x00, 0x00 };
	Patch((DWORD*)0x0052B716, &test1, sizeof(test1));
	// OP_Save
	// this stops sending OP_SAVE
	//const char test2[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
	//PatchA((DWORD*)0x00536797, &test2, sizeof(test2));
	// this forces sending OP_SAVE with size of 0.
	const char test2[] = { 0x00, 0x00 };
	Patch((DWORD*)0x0053678C, &test2, sizeof(test2));
}

// for exe checksum
DWORD WINAPI GetModuleFileNameA_detour(HMODULE hMod, LPTSTR outstring, DWORD nSize)
{
	DWORD allocsize = nSize;
	DWORD ret = GetModuleFileNameA(hMod, outstring, nSize);
	if (hMod == nullptr)
	{
		PCHAR szProcessName = 0;
		szProcessName = strrchr(outstring, '\\');
		szProcessName[0] = '\0';
		sprintf_s(outstring, allocsize, "%s\\eqmac.exe", outstring);
	}
	return ret;
}

// set window title
bool window_title_set = false;
LRESULT WINAPI eqw_DefWindowProcA_Detour(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static bool in_eqw_DefWindowProcA_Detour = false;

	// already set title
	if (hWnd == *EQhWnd && Msg == WM_SETTEXT && window_title_set)
		return 0;

	if (!in_eqw_DefWindowProcA_Detour)
	{
		in_eqw_DefWindowProcA_Detour = true;

		if (hWnd == *EQhWnd && !window_title_set)
		{
			if (new_title.length() == 0)
			{
				HWND cur = NULL;
				char str[255];
				int i = 0;
				do {
					i++;
					sprintf(str, "Client%d", i);
					cur = FindWindowA(NULL, str);
				} while (cur != NULL && i < 100);
				new_title = str;
			}
			SetWindowTextA(hWnd, new_title.c_str());
			window_title_set = true;
		}

		in_eqw_DefWindowProcA_Detour = false;
	}

	return DefWindowProcA_Trampoline(hWnd, Msg, wParam, lParam);
}

WORD savedDeviceGammaRamp[3][256];
bool gamma_saved = false;
void DeviceGammaSave()
{
	HDC DC = GetDC(0);
	GetDeviceGammaRamp(DC, savedDeviceGammaRamp);
	ReleaseDC(0, DC);
	gamma_saved = true;
}
void DeviceGammaRestore()
{
	if (gamma_saved)
	{
		HDC DC = GetDC(0);
		SetDeviceGammaRamp(DC, savedDeviceGammaRamp);
		ReleaseDC(0, DC);
	}
}
// this is a mod that was present in the original eqw, replicated here to reproduce the same effect with eqw_takp
void __stdcall D3D8_SetGammaRamp_Detour(DWORD pDevice, DWORD Flags, DWORD pRamp)
{
	if (!gamma_saved) DeviceGammaSave();
	HDC DC = GetDC(0);
	SetDeviceGammaRamp(DC, (LPVOID)pRamp);
	ReleaseDC(0, DC);
}
// this restores the saved gamma.  this isn't perfect but it replicates the behavior from original eqw for parity
typedef void(__stdcall* _D3D8_Reset)(DWORD pDevice, DWORD pPresentationParameters);
_D3D8_Reset D3D8_Reset_Trampoline;
void __stdcall D3D8_Reset_Detour(DWORD pDevice, DWORD pPresentationParameters)
{
	DeviceGammaRestore();
	D3D8_Reset_Trampoline(pDevice, pPresentationParameters);
}
// patch vtable of Direct3D8 object to redirect SetGammaRamp and Reset
void Hook_D3D8()
{
	int* Direct3D8_obj = *(int**)((int)hEQGFXMod + 0xA4F92C);
	int ix_SetGammaRamp = 18;
	int ix_Reset = 14;

	if (Direct3D8_obj)
	{
		// SetGammaRamp
		int addr = (int)D3D8_SetGammaRamp_Detour;
		int vtable_SetGammaRamp = *Direct3D8_obj + ix_SetGammaRamp * 4;
		Patch((void*)vtable_SetGammaRamp, &addr, 4);

		// Reset
		addr = (int)D3D8_Reset_Detour;
		int vtable_Reset = *Direct3D8_obj + ix_Reset * 4;
		D3D8_Reset_Trampoline = (_D3D8_Reset) * (int*)vtable_Reset;
		Patch((void*)vtable_Reset, &addr, 4);
	}
}
int __cdecl t3dInitializeDevice_Detour(DWORD* a1, HWND hWnd, int a3, int* a4, int a5)
{
	int ret = t3dInitializeDevice_Trampoline(a1, hWnd, a3, a4, a5);
	if (ret == 0)
	{
		Hook_D3D8();
	}
	return ret;
}
void Hook_t3dInitializeDevice(HMODULE gfx_dll)
{
	t3dInitializeDevice_Trampoline = (_t3dInitializeDevice)DetourWithTrampoline((void*)((uintptr_t)gfx_dll + 0x6DD80), t3dInitializeDevice_Detour, 7);
}

void PatchEqMain()
{
	HMODULE hMod = GetModuleHandleA("eqmain.dll");
	if (hMod)
	{
		SkipLicense(hMod);
		SkipSplash(hMod);
	}
}

void PatchEqGfx()
{
	HMODULE hMod = GetModuleHandleA("EQGfx_Dx8.DLL");
	hEQGFXMod = hMod;
	if (hMod)
	{
		if (enableDeviceGammaRamp)
		{
			InstallCPUTimebaseFixHooks();
			Hook_t3dInitializeDevice(hEQGFXMod);
		}
	}
}

void InstallEQWCallbacks()
{
	auto eqw = GetModuleHandle("eqw.dll");
	FARPROC set_eqmain = eqw ? GetProcAddress(eqw, "SetEqMainInitFn") : nullptr;
	FARPROC set_eqgfx = eqw ? GetProcAddress(eqw, "SetEqGfxInitFn") : nullptr;
	if (!set_eqmain || !set_eqgfx) {
		MessageBoxA(NULL, "Installation error", "eqw.dll is not compatible", MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}
	// Install the patch callbacks for eqmain.dll and eqgfx_dx8.dll.
	reinterpret_cast<void(__stdcall*)(void(*)())>(set_eqmain)(PatchEqMain);
	reinterpret_cast<void(__stdcall*)(void(*)())>(set_eqgfx)(PatchEqGfx);
}

void InitHooks()
{
	char buf[2048];

	GetINIString("eqgame_dll", "EnableDeviceGammaRamp", "FALSE", buf, sizeof(buf), true);
	enableDeviceGammaRamp = ParseINIBool(buf);

	GetINIString("eqgame_dll", "EnableFPSLimiter", "FALSE", buf, sizeof(buf), true);
	enableFPSLimiter = ParseINIBool(buf);

	GetINIString("eqgame_dll", "DisableCpuTimebaseFix", "FALSE", buf, sizeof(buf), true);
	enableCPUTimebaseFix = !ParseINIBool(buf);

	// to patch eqmain.dll and eqgfx_dx8.dll when they're loaded
	InstallEQWCallbacks();

	// for alt-enter full screen stretch mode in eqw
	ProcessKeyDown_Trampoline = (_ProcessKeyDown)DetourWithTrampoline((void*)EQ_FUNCTION_ProcessKeyDown, (void*)ProcessKeyDown_Detour, 6);

	// for changing title to Client1, Client2, etc or /title:value
	HMODULE eqw_dll = GetModuleHandleA("eqw.dll");
	if (eqw_dll)
	{
		DefWindowProcA_Trampoline = (_DefWindowProcA)DetourIAT(eqw_dll, "DefWindowProcA", eqw_DefWindowProcA_Detour);
	}

	//bypass filename req
	const unsigned char test3[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,0x90, 0xEB, 0x1B, 0x90, 0x90, 0x90, 0x90 };
	Patch((DWORD*)0x005595A7, &test3, sizeof(test3));

	// don't send character save data, server doesn't use it
	PatchSaveBypass();

	//here to fix the no items on corpse bug - eqmule
	MethodAddressToVariable(CEverQuest__HandleWorldMessage_Detour, Eqmachooks::CEverQuest__HandleWorldMessage_Detour);
	CEverQuest__HandleWorldMessage_Trampoline = (_CEverQuest__HandleWorldMessage)DetourWithTrampoline((void*)0x004E829F, (void*)CEverQuest__HandleWorldMessage_Detour, 5);

	// exe checksum
	{
		//.text:004F2F02 C40 FF 15 48 31 5E 00                             call    ds:GetModuleFileNameA
		unsigned char NOP = 0x90;
		unsigned char E8 = 0xE8;
		Patch((void*)0x004F2F02, &NOP, 1);
		Patch((void*)(0x004F2F02 + 1), &E8, 1);
		intptr_t addr = (intptr_t)GetModuleFileNameA_detour - ((intptr_t)0x004F2F02 + 1) - 5;
		Patch((void*)(0x004F2F02 + 2), &addr, 4);
	}

	// Add MGB for Beastlords
	sub_4B8231_Trampoline = (_sub_4B8231)DetourWithTrampoline((void*)0x004B8231, (void*)sub_4B8231_Detour, 5);

	// Fix bug with Level of Detail setting always being turned on ignoring user's preference
	MethodAddressToVariable(CDisplay__StartWorldDisplay_Detour, Eqmachooks::CDisplay__StartWorldDisplay_Detour);
	CDisplay__StartWorldDisplay_Trampoline = (_CDisplay__StartWorldDisplay)DetourWithTrampoline((void*)0x004A849E, (void*)CDisplay__StartWorldDisplay_Detour, 9);

	// Fix bug with option window UI skin load dialog always loading default instead of selected skin
	uintptr_t addr = (intptr_t)sprintf_Detour_loadskin - (intptr_t)0x00426115;
	Patch((void*)0x00426111, &addr, 4);

	// Fix bug with spell casting bar not showing at high spell haste values
	unsigned char jge = 0x7D;
	Patch((void*)0x004c55b7, &jge, 1);

	// for command line parsing
	sub_4F35E5_Trampoline = (_sub_4F35E5)DetourWithTrampoline((void*)0x004F35E5, sub_4F35E5_Detour, 5);

	// for chat /command parsing
	EQMACMQ_REAL_CEverQuest__InterpretCmd = (EQ_FUNCTION_TYPE_CEverQuest__InterpretCmd)DetourWithTrampoline((PBYTE)EQ_FUNCTION_CEverQuest__InterpretCmd, (PBYTE)EQMACMQ_DETOUR_CEverQuest__InterpretCmd, 9);

	// for FPSLimit.cpp
	if (enableFPSLimiter)
	{
		MethodAddressToVariable(CDisplay__Render_World_Detour, Eqmachooks::CDisplay__Render_World_Detour);
		CDisplay__Render_World_Trampoline = (_CDisplay__Render_World)DetourWithTrampoline((void*)0x004AA8BC, (void*)CDisplay__Render_World_Detour, 5);
		LoadFPSLimiterIniSettings();
	}

	// eqclient.ini file settings
	{
		char szResult[255];
		char szDefault[255];
		DWORD error;

		sprintf(szDefault, "%d", 32);
		error = GetPrivateProfileStringA("Defaults", "VideoModeBitsPerPixel", szDefault, szResult, 255, INI_FILE);
		if (!GetLastError())
		{
			// if set to 16 bit, change to 32
			if (!strcmp(szResult, "16"))
				WritePrivateProfileStringA("Defaults", "VideoModeBitsPerPixel", szDefault, INI_FILE);
		}

		sprintf(szDefault, "%d", 32);
		error = GetPrivateProfileStringA("VideoMode", "BitsPerPixel", szDefault, szResult, 255, INI_FILE);
		if (!GetLastError())
		{
			// if set to 16 bit, change to 32
			if (!strcmp(szResult, "16"))
				WritePrivateProfileStringA("VideoMode", "BitsPerPixel", szDefault, INI_FILE);
		}
		else
		{
			// we do not have one set
			DEVMODE dm;
			// initialize the DEVMODE structure
			ZeroMemory(&dm, sizeof(dm));
			dm.dmSize = sizeof(dm);
			DWORD bits = 32;
			//DWORD freq = 40;
			if (0 != EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm))
			{
				// get default display settings
				bits = dm.dmBitsPerPel;
				//freq = dm.dmDisplayFrequency;
			}
			//sprintf(szDefault, "%d", freq);
			//WritePrivateProfileStringA("VideoMode", "RefreshRate", szDefault, INI_FILE);
			sprintf(szDefault, "%d", bits);
			WritePrivateProfileStringA("VideoMode", "BitsPerPixel", szDefault, INI_FILE);
		}

		// turn on chat keepalive
		sprintf(szDefault, "%d", 1);
		WritePrivateProfileStringA("Defaults", "ChatKeepAlive", szDefault, INI_FILE);
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		HMODULE callerModule = GetModuleHandleA(0);
		char lpFilename[260];
		GetModuleFileNameA(callerModule, lpFilename, 260);
		hEQGameEXE = callerModule;

		InitHooks();

		return TRUE;
	}
	else if (ul_reason_for_call == DLL_PROCESS_DETACH)
	{
		return TRUE;
	}
}
