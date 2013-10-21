#include <ntddk.h>
#include <Wdm.h>

#define IOCTL_EEYE_INITFB CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_IN_DIRECT, FILE_ANY_ACCESS)

NTSTATUS Read(PDEVICE_OBJECT, PIRP);
NTSTATUS Create(PDEVICE_OBJECT, PIRP);
NTSTATUS Close(PDEVICE_OBJECT, PIRP);
NTSTATUS HandleIOCTL(PDEVICE_OBJECT, PIRP);
NTSTATUS NotImplemented(PDEVICE_OBJECT, PIRP);
void Dtor(PDRIVER_OBJECT );
int FBPhysAddr = 0, FBSz = 0;
PVOID vaddr = 0;
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
  pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = &HandleIOCTL;
  pDriverObject->DriverUnload = &Dtor;
  pDeviceObject->Flags |= DO_DIRECT_IO;
  pDeviceObject->Flags &= (~DO_DEVICE_INITIALIZING);
  return NtStatus;
}

NTSTATUS Read(PDEVICE_OBJECT  DriverObject, PIRP Irp){
  int  size;
  PIO_STACK_LOCATION pIoStackIrp = NULL;
  PCHAR pBuffer;
  int i;
  char *c;
  NTSTATUS NtStatus = STATUS_UNSUCCESSFUL;

  DbgPrint("Read Called \r\n");
  Irp->IoStatus.Information = 0;
  pIoStackIrp = IoGetCurrentIrpStackLocation(Irp);
  if(pIoStackIrp){
    size = pIoStackIrp->Parameters.Read.Length;
    size = size  < FBSz ? size : FBSz;
    pBuffer =
      MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    DbgPrint("User buffer @ virt addr %p\n", pBuffer);
    if( pBuffer && vaddr ){
      DbgPrint("Moving %i bytes from %p to %p\n", size, vaddr, pBuffer);
      _asm cli;
      RtlMoveMemory(pBuffer, vaddr, size);
      _asm sti;
      Irp->IoStatus.Information = size;
    }
    else DbgPrint("vaddr or PBuffer = NULL\r\n");
  }
  else DbgPrint("can't get stack location\r\n");
  Irp->IoStatus.Status = NtStatus;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return NtStatus;
}

NTSTATUS HandleIOCTL(PDEVICE_OBJECT  DriverObject, PIRP Irp){
  NTSTATUS status = STATUS_UNSUCCESSFUL;
  PIO_STACK_LOCATION pIoStackIrp = IoGetCurrentIrpStackLocation(Irp);
  int *payload;
  PHYSICAL_ADDRESS paddr;

	DbgPrint("IOCTL handler called\r\n");
  if( (pIoStackIrp->
       Parameters.DeviceIoControl.IoControlCode == IOCTL_EEYE_INITFB)
      && (pIoStackIrp->
	  Parameters.DeviceIoControl.InputBufferLength == 2*sizeof(int))
      && (Irp->AssociatedIrp.SystemBuffer) ){
    payload = (int *)Irp->AssociatedIrp.SystemBuffer;
    FBPhysAddr = payload[0];
    FBSz = payload[1];
    if( FBPhysAddr && (FBSz > 0) ){
      paddr.QuadPart = FBPhysAddr;
      vaddr = MmMapIoSpace(paddr, FBSz, MmNonCached);
      DbgPrint("FBPhysAddr: %p,\nPBSz: 0x%x,\nFB mapped @ virt addr %p\n", 
	       FBPhysAddr, FBSz, vaddr);
      status = STATUS_SUCCESS;
    }
  }
  else status = STATUS_INVALID_DEVICE_REQUEST;
  Irp->IoStatus.Status = status;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return status;
}

NTSTATUS Create(PDEVICE_OBJECT  DriverObject, PIRP Irp){
  DbgPrint("Create called\r\n");
  return STATUS_SUCCESS; 
}

NTSTATUS Close(PDEVICE_OBJECT  DriverObject, PIRP Irp){
  if(vaddr){ 
    MmUnmapIoSpace(vaddr, FBSz);
    vaddr = 0;
  }
  DbgPrint("FB unmapped\n");
  DbgPrint("Close Called\r\n");
  return STATUS_SUCCESS;
}

NTSTATUS NotImplemented(PDEVICE_OBJECT  DriverObject, PIRP Irp){
  DbgPrint("NotImplemented called\r\n");
  return STATUS_SUCCESS; //STATUS_NOT_IMPLEMENTED?
}

void Dtor(PDRIVER_OBJECT  DriverObject){
  UNICODE_STRING usDosDeviceName;
  DbgPrint("Dtor Called \n");
  if(vaddr) MmUnmapIoSpace(vaddr, FBSz);
  DbgPrint("FB unmapped\n");
  RtlInitUnicodeString(&usDosDeviceName, L"\\DosDevices\\Example");
  IoDeleteSymbolicLink(&usDosDeviceName);
  IoDeleteDevice(DriverObject->DeviceObject);
	return STATUS_SUCCESS;
}
