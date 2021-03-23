#include "inject.h"

#define DRIVER_NAME     L"\\Driver\\KInject"
#define DEVICE_NAME     L"\\Device\\KInject"
#define DOSDEVICE_NAME  L"\\DosDevices\\KInject"

NTSTATUS
IoCreateDriver(
    IN  PUNICODE_STRING DriverName    OPTIONAL,
    IN  PDRIVER_INITIALIZE InitializationFunction
);

DRIVER_INITIALIZE DriverEntry;
#pragma alloc_text(INIT, DriverEntry)

#define REQUEST_INJECTDLL           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x101, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)



NTSTATUS DevioctlDispatch(
    _In_ struct _DEVICE_OBJECT* DeviceObject,
    _Inout_ struct _IRP* Irp
)
{
    NTSTATUS				status = STATUS_SUCCESS;
    ULONG					bytesIO = 0;
    PIO_STACK_LOCATION		stack;
    BOOLEAN					condition = FALSE;
    char*                   rp;
    DWORD*                  wp;

    UNREFERENCED_PARAMETER(DeviceObject);

    stack = IoGetCurrentIrpStackLocation(Irp);

    do
    {
        if (stack == NULL)
        {
            status = STATUS_INTERNAL_ERROR;
            break;
        }

        rp = (char*)Irp->AssociatedIrp.SystemBuffer;
        wp = (DWORD*)Irp->AssociatedIrp.SystemBuffer;
        if (rp == NULL)
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        switch (stack->Parameters.DeviceIoControl.IoControlCode)
        {
        case REQUEST_INJECTDLL:
        {
            if (stack->Parameters.DeviceIoControl.InputBufferLength == 0 || stack->Parameters.DeviceIoControl.OutputBufferLength != sizeof(DWORD))
            {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (NT_SUCCESS(InjInject((struct InjectParam*)rp)))
                *wp = 1;
            else
                *wp = 0;

            bytesIO = sizeof(DWORD);
            status = STATUS_SUCCESS;
        }
        break;
        default:
            status = STATUS_INVALID_PARAMETER;
        };
    } while (condition);

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesIO;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS UnsupportedDispatch(
    _In_ struct _DEVICE_OBJECT* DeviceObject,
    _Inout_ struct _IRP* Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}

NTSTATUS CreateDispatch(
    _In_ struct _DEVICE_OBJECT* DeviceObject,
    _Inout_ struct _IRP* Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}

NTSTATUS CloseDispatch(
    _In_ struct _DEVICE_OBJECT* DeviceObject,
    _Inout_ struct _IRP* Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}

NTSTATUS DriverInitialize(
    _In_  struct _DRIVER_OBJECT* DriverObject,
    _In_  PUNICODE_STRING RegistryPath
)
{
    NTSTATUS        status;
    UNICODE_STRING  SymLink, DevName;
    PDEVICE_OBJECT  devobj;
    ULONG           t;

    //RegistryPath is NULL
    UNREFERENCED_PARAMETER(RegistryPath);

    RtlInitUnicodeString(&DevName, DEVICE_NAME);
    status = IoCreateDevice(DriverObject, 0, &DevName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, TRUE, &devobj);
    if (!NT_SUCCESS(status))
        return status;

    RtlInitUnicodeString(&SymLink, DOSDEVICE_NAME);
    status = IoCreateSymbolicLink(&SymLink, &DevName);

    devobj->Flags |= DO_BUFFERED_IO;

    for (t = 0; t <= IRP_MJ_MAXIMUM_FUNCTION; t++)
        DriverObject->MajorFunction[t] = &UnsupportedDispatch;

    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = &DevioctlDispatch;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = &CreateDispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = &CloseDispatch;
    DriverObject->DriverUnload = NULL;

    devobj->Flags &= ~DO_DEVICE_INITIALIZING;

    return status;
}

NTSTATUS DriverEntry(
    _In_  struct _DRIVER_OBJECT *DriverObject,
    _In_  PUNICODE_STRING RegistryPath
)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    UNICODE_STRING  drvName;

    RtlInitUnicodeString(&drvName, DRIVER_NAME);
    status = IoCreateDriver(&drvName, &DriverInitialize);

    return STATUS_SUCCESS;
}