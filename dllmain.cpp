﻿// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"

//TODO: Let old krkr2 game run under the Win8 OS and upper.
PVOID pGetProcAddress = NULL;
typedef FARPROC(WINAPI* PGetProcAddress)(HMODULE hModule, LPCSTR lpProcName);
FARPROC WINAPI MyGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	if (!strcmp(lpProcName, "GetSystemWow64DirectoryA"))
	{
		return NULL;
	}
	return ((PGetProcAddress)pGetProcAddress)(hModule, lpProcName);
}

void APIHook()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	pGetProcAddress = DetourFindFunction("Kernel32.dll", "GetProcAddress");
	DetourAttach(&pGetProcAddress, MyGetProcAddress);
	DetourTransactionCommit();
}

//////////////////////////////////////////////////////////////////////

//TODO: Change the file name into SJIS in krkr2.
//For krkrz, you can hook mb2wc.
//In krkr2, hook mb2wc is unsafely.
//If use SQLite in krkr2. This func isn't work ( But you may need it to change filename in TVP VM ).
bool IsGBK(char* Text)
{
	for (int i = 0; i < strlen(Text); i++)
	{
		if ((unsigned int)Text[i] == '?')
			return true;
		//cout << hex << (unsigned int)Text[i] << endl;
	}
	return false;
}

char sc[MAX_PATH];
void __stdcall AyamiShowTextFuncW(wchar_t* Text)
{
	if (!strcmp(sc, wtoc(Text)))
		return;
	wchar_t* cpystr = ctowJIS(wtoc(Text));
	if (!IsGBK(wtoc(cpystr)))
	{
		wcscpy(Text, cpystr);
		strcpy(sc, wtoc(Text));
	}
	//cout << wtoc(Text) << endl;
}
//You need change addr by yourself.
PVOID pGetFileText = (PVOID)(BaseAddr + 0x1FDB76);
__declspec(naked)void mGetFileText()
{
	__asm
	{
		pushad
		pushfd
		push esi
		call AyamiShowTextFuncW
		popfd
		popad
		jmp pGetFileText
	}
}

void TVPHook()
{
	DetourTransactionBegin();
	DetourAttach((void**)&pGetFileText, mGetFileText);
	DetourTransactionCommit();
}

//////////////////////////////////////////////////////////////////////

//It works for krkr2 only.
//for krkrz, you must find addr of TVPGetFunctionExporter by yourself.
bool IsOnce = false;

typedef iTVPFunctionExporter* (WINAPI* TVPGetFunctionExporterFunc)();
TVPGetFunctionExporterFunc pfTVPGetFunctionExporter = nullptr;

typedef IStream* (WINAPI* PfunTVPCreateIStream)(const ttstr&, tjs_uint32);
PVOID pfTVPCreateIStream = nullptr;


IStream* WINAPI HookTVPCreateIStream(const ttstr& _name, tjs_uint32 flags)
{
	cout << wtoc(_name.c_str()) << endl;
	return ((PfunTVPCreateIStream)pfTVPCreateIStream)(_name, flags);
}

PVOID VMHook(char* funcname, PVOID addr, PVOID hookfunc)
{
	addr = TVPGetImportFuncPtr(funcname);
	if (addr)
	{

		cout << hex << "0x" << addr << endl;
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&addr, hookfunc);
		if (DetourTransactionCommit() != NOERROR)
		{
		error:
			MessageBox(0, L"ERROR TO HOOK TVP INLINE FUNCTION", 0, 0);
			return nullptr;
		}
	}
	else
		goto error;
	return addr;
}

iTVPFunctionExporter* WINAPI HookTVPGetFunctionExporter()
{
	iTVPFunctionExporter* result = pfTVPGetFunctionExporter();
	TVPInitImportStub(result);
	if (!IsOnce) 
	{
		pfTVPCreateIStream = VMHook((char*)"IStream * ::TVPCreateIStream(const ttstr &,tjs_uint32)", pfTVPCreateIStream, HookTVPCreateIStream);
		if (pfTVPCreateIStream != nullptr)
			IsOnce = true;
		else if (!IsOnce)
		{
			MessageBoxW(NULL, L"Failed to Query Function Ptr", 0, MB_OK);
		}
	}
	return result;
}

void TJSHook()
{
	HMODULE hModule = GetModuleHandle(NULL);
	*(FARPROC*)&pfTVPGetFunctionExporter = GetProcAddress(hModule, "TVPGetFunctionExporter");
	if( pfTVPGetFunctionExporter!=nullptr )
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach((PVOID*)&pfTVPGetFunctionExporter, HookTVPGetFunctionExporter);
		DetourTransactionCommit();
	}

}

//TODO: Help get the func name of tvp.
int WINAPI HookMultiByteToWideChar(
	_In_      UINT   CodePage,
	_In_      DWORD  dwFlags,
	_In_      LPCSTR lpMultiByteStr,
	_In_      int    cbMultiByte,
	_Out_opt_ LPWSTR lpWideCharStr,
	_In_      int    cchWideChar
)
{
	cout << lpMultiByteStr << endl;
	txt << lpMultiByteStr << endl;
	return MultiByteToWideChar(CodePage, dwFlags,
		lpMultiByteStr, cbMultiByte, lpWideCharStr, cchWideChar);
}

void HelpHook()
{
	IATPatch("Kernel32.dll", GetProcAddress(GetModuleHandleW(L"Kernel32.dll"), "MultiByteToWideChar"), (PROC)HookMultiByteToWideChar);
}
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		make_console();
		//APIHook();
		TVPHook();
		TJSHook();
		//HelpHook();
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

