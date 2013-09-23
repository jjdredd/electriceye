#include <ntddk.h>
#include <Wdm.h>

NTSTATUS Read(PDEVICE_OBJECT, PIRP);
NTSTATUS Create(PDEVICE_OBJECT, PIRP);
NTSTATUS Close(PDEVICE_OBJECT, PIRP);
NTSTATUS NotImplemented(PDEVICE_OBJECT, PIRP);
void Dtor(PDRIVER_OBJECT );

PVOID vaddr = 0;
int FBSz = 0;

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath){
  NTSTATUS NtStatus = STATUS_SUCCESS;
  PDEVICE_OBJECT pDeviceObject = NULL, pVideoDeviceObject = NULL;
  UNICODE_STRING usDriverName, usDosDeviceName, usVideoDeviceName;
  PCM_RESOURCE_LIST pVideoDevResList;
  PCM_FULL_RESOURCE_DESCRIPTOR pFullResourceDescriptor;
  CM_PARTIAL_RESOURCE_LIST PartialResourceList;
  PCM_PARTIAL_RESOURCE_DESCRIPTOR pPartialResourceDescriptor;
  FILE_OBJECT FileObject;
  int i, j, ResourceListSz = 3*sizeof(CM_RESOURCE_LIST), ResourceListNeededSz;
  PHYSICAL_ADDRESS paddr;

  DbgPrint("Driver Entry \n");
  RtlInitUnicodeString(&usDriverName, L"\\Device\\ElectricEye");
  RtlInitUnicodeString(&usDosDeviceName, L"\\DosDevices\\ElectricEye");
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

  /* Guess number one: video card device name */
  RtlInitUnicodeString(&usVideoDeviceName, L"\\Device\\Video0");
  if( !NT_SUCCESS(IoGetDeviceObjectPointer(&usVideoDeviceName, FILE_READ_DATA,
				&FileObject, pVideoDeviceObject))){
    DbgPrint("couldn't get device object pointer \n");
    return  STATUS_UNSUCCESSFUL;
  }
  
  if( (pVideoDevResList = ExAllocatePool(NonPagedPool, ResourceListSz)) 
      == NULL ){
    DbgPrint("couldn't allocate pool\n");
    ObDereferenceObject(&FileObject);
    return  STATUS_UNSUCCESSFUL;
  }

  while( (NtStatus = IoGetDeviceProperty(pVideoDeviceObject, 
			      DevicePropertyBootConfigurationTranslated,
			      ResourceListSz, (PVOID)pVideoDevResList, 
			      &ResourceListNeededSz))
	 == STATUS_BUFFER_TOO_SMALL ){
    ExFreePool(pVideoDevResList);
    if( (pVideoDevResList = ExAllocatePool(NonPagedPool, 
					  ResourceListNeededSz)) 
	== NULL ){
      DbgPrint("couldn't reallocate pool\n");
      ObDereferenceObject(&FileObject);
      return  STATUS_UNSUCCESSFUL;
    }
  }
  if(!NT_SUCCESS(NtStatus)){
	DbgPrint("couldn't get device property\n");
ObDereferenceObject(&FileObject);
	return STATUS_UNSUCCESSFUL;
  }
  /* have you seen the movie called Inception ? here it goes */
  DbgPrint("full list starting\n");
  pFullResourceDescriptor = (PCM_FULL_RESOURCE_DESCRIPTOR) pVideoDevResList->List;
  for(i = 0; i <  pVideoDevResList->Count; i++,
	pFullResourceDescriptor = 
	(PCM_FULL_RESOURCE_DESCRIPTOR)
	(pFullResourceDescriptor->PartialResourceList.PartialDescriptors +
	 pFullResourceDescriptor->PartialResourceList.Count)){
    /* we need to go deeper (c) */
    PartialResourceList = pFullResourceDescriptor->PartialResourceList;
    pPartialResourceDescriptor = PartialResourceList.PartialDescriptors;
    for(j = 0; j < PartialResourceList.Count; j++,
	  pPartialResourceDescriptor = 
	  (PCM_PARTIAL_RESOURCE_DESCRIPTOR)
	  (&PartialResourceList + sizeof(CM_PARTIAL_RESOURCE_LIST)
	   + j*sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR))){
      /* Guess number two: framebuffer is the biggest of smallest */
      if( (pPartialResourceDescriptor->Type == CmResourceTypeMemory)
	  &&(FBSz < pPartialResourceDescriptor->u.Memory.Length)){
	paddr = pPartialResourceDescriptor->u.Memory.Start;
	FBSz = pPartialResourceDescriptor->u.Memory.Length;
      }
    }
  }
  DbgPrint("Guessed framebuffer address %p\r\n size %i", paddr.QuadPart
	   , FBSz);
  vaddr = MmMapIoSpace(paddr, FBSz, MmNonCached);
  DbgPrint("FB mapped @ virt addr %p", vaddr);
  if(!vaddr) NtStatus = STATUS_UNSUCCESSFUL;
  ExFreePool(pVideoDevResList);
ObDereferenceObject(&FileObject);
  return NtStatus;
}

NTSTATUS Read(PDEVICE_OBJECT  DriverObject, PIRP Irp){
  
  int  size;
  PIO_STACK_LOCATION pIoStackIrp = NULL;
  PCHAR pBuffer;
  NTSTATUS NtStatus = STATUS_SUCCESS;
 
  DbgPrint("Read Called \r\n");
  Irp->IoStatus.Information = 0;
  pIoStackIrp = IoGetCurrentIrpStackLocation(Irp);
  if(pIoStackIrp){
    size = pIoStackIrp->Parameters.Read.Length;
    size = size  < FBSz ? size : FBSz;
    pBuffer =
      MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    DbgPrint("User buffer @ virt addr %p\n", pBuffer);
    if(pBuffer){
      if(vaddr){
	DbgPrint("Moving %i bytes from %p to %p\n", size, vaddr, pBuffer);
	RtlCopyMemory(pBuffer, vaddr, size);
	Irp->IoStatus.Information = size;
      }
      else{
	DbgPrint("vaddr = NULL\r\n");
	NtStatus = STATUS_UNSUCCESSFUL;
      }
    }
    else{
	DbgPrint("vaddr = NULL\r\n");
	NtStatus = STATUS_UNSUCCESSFUL;
    }
  }
  else{
	DbgPrint("vaddr = NULL\r\n");
	NtStatus = STATUS_UNSUCCESSFUL;
  }

  DbgPrint("Move successful!\n");
  Irp->IoStatus.Status = NtStatus;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return NtStatus;
}

NTSTATUS Create(PDEVICE_OBJECT  DriverObject, PIRP Irp){
  DbgPrint("Create called\r\n");
  return STATUS_SUCCESS; //STATUS_NOT_IMPLEMENTED ???
}

NTSTATUS Close(PDEVICE_OBJECT  DriverObject, PIRP Irp){
  DbgPrint("Close Called\r\n");
  return STATUS_SUCCESS; //STATUS_NOT_IMPLEMENTED ???
}

NTSTATUS NotImplemented(PDEVICE_OBJECT  DriverObject, PIRP Irp){
  DbgPrint("NotImplemented called\r\n");
  return STATUS_SUCCESS; //STATUS_NOT_IMPLEMENTED ???
}

void Dtor(PDRIVER_OBJECT  DriverObject){
  UNICODE_STRING usDosDeviceName;
  DbgPrint("Dtor Called \n");
  RtlInitUnicodeString(&usDosDeviceName, L"\\DosDevices\\Example");
  IoDeleteSymbolicLink(&usDosDeviceName);
  IoDeleteDevice(DriverObject->DeviceObject);
  MmUnmapIoSpace(vaddr, FBSz);
  DbgPrint("FB unmapped\n");
}
