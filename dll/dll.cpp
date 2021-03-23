#include <Windows.h>


BOOL WriteFile(LPCSTR lpFileName, LPVOID lpBuffer, DWORD dwSize)
{
	BOOL bRet = FALSE;
	DWORD dwWritten = 0;
	HANDLE hFile = NULL;

	hFile = CreateFileA(lpFileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	bRet = ::WriteFile(hFile, lpBuffer, dwSize, &dwWritten, NULL);
	CloseHandle(hFile);
	return bRet;
}

extern "C" __declspec(dllexport) void whoami()
{
	char szUser[260] = { 0 };
	DWORD dwUserLen = sizeof(szUser);

	if (GetUserNameA(szUser, &dwUserLen))
	{
		WriteFile("C:\\Users\\user\\Desktop\\kinject\\whoami.log", szUser, sizeof(szUser));
	}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}