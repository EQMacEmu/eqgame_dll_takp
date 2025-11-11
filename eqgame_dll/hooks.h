#include "eqgame.h"
class Eqmachooks;

#define MethodAddressToVariable(v, method) intptr_t  v; { auto fp = &##method; memcpy(&##v, &fp, 4); }

typedef unsigned char (__thiscall *_CEverQuest__HandleWorldMessage)(Eqmachooks *,DWORD *,unsigned __int32,char *,unsigned __int32);
_CEverQuest__HandleWorldMessage CEverQuest__HandleWorldMessage_Trampoline;

typedef int (__thiscall *_CDisplay__Render_World)(Eqmachooks *);
_CDisplay__Render_World CDisplay__Render_World_Trampoline;

typedef DWORD (WINAPI *_GetModuleFileNameA)(HMODULE,LPTSTR,DWORD);
_GetModuleFileNameA GetModuleFileNameA_tramp;

typedef LRESULT(WINAPI* _DefWindowProcA)(HWND, UINT, WPARAM, LPARAM);
_DefWindowProcA DefWindowProcA_Trampoline;

typedef int (__cdecl *_ProcessKeyDown)(int);
_ProcessKeyDown ProcessKeyDown_Trampoline;

typedef int (WINAPI *_sub_4B8231)(int, signed int);
_sub_4B8231 sub_4B8231_Trampoline;

typedef int (__thiscall *_CDisplay__StartWorldDisplay)(Eqmachooks *, int zoneindex, int x);
_CDisplay__StartWorldDisplay CDisplay__StartWorldDisplay_Trampoline;

typedef int (__cdecl *_sub_4F35E5)();
_sub_4F35E5 sub_4F35E5_Trampoline;

typedef HMODULE (WINAPI *_LoadLibraryA)(LPCSTR lpLibFileName);
_LoadLibraryA LoadLibraryA_Trampoline;

EQ_FUNCTION_TYPE_CEverQuest__InterpretCmd EQMACMQ_REAL_CEverQuest__InterpretCmd = NULL;

typedef int(__cdecl* _t3dInitializeDevice)(DWORD* a1, HWND hWnd, int a3, int* a4, int a5);
_t3dInitializeDevice t3dInitializeDevice_Trampoline;
