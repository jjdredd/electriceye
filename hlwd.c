#include <ntddk.h>
#include <Wdm.h>

NTSTATUS Read(PDEVICE_OBJECT, PIRP);
NTSTATUS Create(PDEVICE_OBJECT, PIRP);
NTSTATUS Close(PDEVICE_OBJECT, PIRP);
NTSTATUS NotImplemented(PDEVICE_OBJECT, PIRP);
void Dtor(PDRIVER_OBJECT );

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath){
  NTSTATUS NtStatus = STATUS_SUCCESS;
  PDEVICE_OBJECT pDeviceObject = NULL;
  UNICODE_STRING usDriverName, usDosDeviceName;
  int i;

  RtlInitUnicodeString(&usDriverName, L"\\Device\\ElectricEye");
  RtlInitUnicodeString(&usDosDeviceName, L"\\DosDevices\\ElectricEye");
  DbgPrint("Driver Entry \n");
  NtStatus = IoCreateDevice(pDriverObject, 0,
			    &usDriverName,
			    FILE_DEVICE_UNKNOWN,
			    FILE_DEVICE_SECURE_OPEN,
			    FALSE, &pDeviceObject);
  IoCreateSymbolicLink(&usDosDeviceName, &usDriverName);
  for(i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
    pDriverObject->MajorFunction[i] = &NotImplemented;
  pDriverObject->MajorFunction[IRP_MJ_CLOSE] = &Close;
  pDriverObject->MajorFunction[IRP_MJ_CREATE] = &Create;
  pDriverObject->MajorFunction[IRP_MJ_READ] = &Read;
  pDriverObject->DriverUnload = &Dtor;
  pDeviceObject->Flags |= DO_DIRECT_IO;
  pDeviceObject->Flags &= (~DO_DEVICE_INITIALIZING);
  return NtStatus;
}

NTSTATUS Read(PDEVICE_OBJECT  DriverObject, PIRP Irp){
  PVOID FBPhysAddr = 0xd0000000, vaddr = 0;
  int FBSz = 0x7ffffff, size;
  PHYSICAL_ADDRESS paddr;
  PIO_STACK_LOCATION pIoStackIrp = NULL;
  PCHAR pBuffer;
  int i;
  char *c;

  DbgPrint("Read Called \r\n");
  pIoStackIrp = IoGetCurrentIrpStackLocation(Irp);
  if(pIoStackIrp){
    size = pIoStackIrp->Parameters.Read.Length;
    size = size  < FBSz ? size : FBSz;
    pBuffer =
      MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    DbgPrint("User buffer @ virt addr %p\n", pBuffer);
    if(pBuffer){
      paddr.QuadPart = FBPhysAddr;
      vaddr = MmMapIoSpace(paddr, size, MmNonCached);
      DbgPrint("FB mapped @ virt addr %p", vaddr);
      if(vaddr){
	DbgPrint("Moving %i bytes from %p to %p\n", size, vaddr, pBuffer);
	_asm cli;
	RtlMoveMemory(pBuffer, vaddr, size);
	_asm sti;
	MmUnmapIoSpace(vaddr, size);
	DbgPrint("FB unmapped\n");
      }
      else{
	DbgPrint("vaddr = NULL\r\n");
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_UNSUCCESSFUL;
      }
    }
  }
  DbgPrint("Move successful!\n");
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}

NTSTATUS Create(PDEVICE_OBJECT  DriverObject, PIRP Irp){
  DbgPrint("Create called\r\n");
  return STATUS_SUCCESS;
}

NTSTATUS Close(PDEVICE_OBJECT  DriverObject, PIRP Irp){
  DbgPrint("Close Called\r\n");
  return STATUS_SUCCESS;
}

NTSTATUS NotImplemented(PDEVICE_OBJECT  DriverObject, PIRP Irp){
  DbgPrint("NotImplemented called\r\n");
  return STATUS_SUCCESS;
}

void Dtor(PDRIVER_OBJECT  DriverObject){
  UNICODE_STRING usDosDeviceName;
  DbgPrint("Dtor Called \n");
  RtlInitUnicodeString(&usDosDeviceName, L"\\DosDevices\\Example");
  IoDeleteSymbolicLink(&usDosDeviceName);
  IoDeleteDevice(DriverObject->DeviceObject);
}
