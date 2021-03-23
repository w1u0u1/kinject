#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "getopt.h"

#define REQUEST_INJECTDLL	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x101, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

struct InjectParam
{
	DWORD ProcessId;
	WCHAR FileName[260];
};

void SendCode(DWORD dwCode, struct InjectParam* param)
{
	HANDLE          h;
	DWORD           bytesIO;
	DWORD			dwRet = 0;
	BOOL			bRet = FALSE;

	h = CreateFileA("\\\\.\\KInject", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (h != INVALID_HANDLE_VALUE)
	{
		bRet = DeviceIoControl(h, dwCode, param, sizeof(struct InjectParam), &dwRet, sizeof(dwRet), &bytesIO, NULL);
		if(bRet)
			printf("ok %d\n", dwRet);
		else
			printf("error %d\n", GetLastError());

		CloseHandle(h);
	}
	else
		printf("error %d\n", GetLastError());
}

int main(int argc, char* argv[])
{
	struct InjectParam param = { 0 };

	int ch;
	while ((ch = getopt(argc, argv, "f:p:")) != EOF)
	{
		switch (ch)
		{
		case 'f':
			wsprintfW(param.FileName, L"\\??\\%S", optarg);
			break;
		case 'p':
			param.ProcessId = atoi(optarg);
			break;
		default:
			return 1;
		}
	}

	SendCode(REQUEST_INJECTDLL, &param);
	
	return 0;
}