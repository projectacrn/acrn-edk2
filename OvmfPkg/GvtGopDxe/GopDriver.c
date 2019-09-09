/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Gop.h"

GVT_GOP_PRIVATE_DATA  *mPrivate = NULL;


STATIC
EFI_STATUS
EFIAPI
DetectGvtDevice (
  IN EFI_PCI_IO_PROTOCOL *PciIo
  )
{
  EFI_STATUS                    Status;
  PCI_DEVICE_INDEPENDENT_REGION Hdr;
  UINT64                        Magic, OrigAttr;
  UINT32                        GvtCaps;

  DEBUG((EFI_D_VERBOSE, "%a:%d\n", __FUNCTION__, __LINE__));
  //
  // Read the PCI Configuration Header from the PCI Device
  //
  Status = PciIo->Pci.Read (
        PciIo,
        EfiPciIoWidthUint32,
        0,
        sizeof (Hdr) / sizeof (UINT32),
        &Hdr
        );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = EFI_UNSUPPORTED;
  // Here we check if it's an Intel's VGA controller
  if (Hdr.ClassCode[2] != PCI_CLASS_DISPLAY ||
      Hdr.VendorId != 0x8086) {
    DEBUG((EFI_D_VERBOSE, "%a: [%x:%x] is not GVT device(class:%x)\n",
          __FUNCTION__, Hdr.VendorId, Hdr.DeviceId, Hdr.ClassCode[2]));
    goto Done;
  }

  //
  // Save original PCI attributes, and enable IO space access, memory space
  // access, and Bus Master (DMA).
  //
  Status = PciIo->Attributes (PciIo, EfiPciIoAttributeOperationGet, 0,
                    &OrigAttr);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = PciIo->Attributes (PciIo, EfiPciIoAttributeOperationEnable,
                    EFI_PCI_DEVICE_ENABLE, NULL);
  if (EFI_ERROR (Status)) {
    goto RestoreAttrib;
  }

  //Check if the GVT Magic presents
  Status = PciIo->Mem.Read (
        PciIo,
        EfiPciIoWidthUint32,
        PCI_BAR_IDX0,
        VGT_IF_BASE,
        sizeof(Magic)/sizeof(UINT32),
        &Magic
        );
  if (EFI_ERROR (Status)) {
    goto RestoreAttrib;
  }

  if (Magic != VGT_MAGIC) {
    DEBUG((EFI_D_VERBOSE,
        "Wrong Magic %x for [%x:%x]\n",
        Magic,
        Hdr.VendorId, Hdr.DeviceId));
    Status = EFI_UNSUPPORTED;
    goto RestoreAttrib;
  }

  //Check if the GVT caps matches
  Status = PciIo->Mem.Read (
        PciIo,
        EfiPciIoWidthUint32,
        PCI_BAR_IDX0,
        VGT_IF_BASE + OFFSET_OF(GVT_IF_HDR, vgt_caps),
        sizeof(GvtCaps)/sizeof(UINT32),
        &GvtCaps
        );
  if (EFI_ERROR (Status)) {
    goto RestoreAttrib;
  }

  if (!(GvtCaps & VGT_CAPS_GOP_SUPPORT)) {
    DEBUG((EFI_D_WARN,
        "Wrong cap %x for [%x:%x]\n",
        GvtCaps,
        Hdr.VendorId, Hdr.DeviceId));
    Status = EFI_UNSUPPORTED;
    goto RestoreAttrib;
  }

  //Now we are all set :)
  DEBUG((EFI_D_VERBOSE,
      "Found GVT device on [%x:%x] %x\n",
      Hdr.VendorId, Hdr.DeviceId, GvtCaps));
  Status = EFI_SUCCESS;

RestoreAttrib:
  PciIo->Attributes (PciIo, EfiPciIoAttributeOperationEnable,
                    OrigAttr, NULL);
Done:
  return Status;
}


STATIC
EFI_STATUS
EFIAPI
GvtGopBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL *This,
  IN EFI_HANDLE                  ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL    *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS          Status;
  EFI_PCI_IO_PROTOCOL *PciIo;

  //
  // Open the PCI I/O Protocol
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **) &PciIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DetectGvtDevice(PciIo);

  DEBUG((EFI_D_INFO, "supported? %c\n", Status?'N':'Y'));

  //
  // Close the PCI I/O Protocol
  //
  gBS->CloseProtocol (
        ControllerHandle,
        &gEfiPciIoProtocolGuid,
        This->DriverBindingHandle,
        ControllerHandle
        );

  return Status;
}


STATIC
EFI_STATUS
EFIAPI
GvtGopBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL *This,
  IN EFI_HANDLE                  ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL    *RemainingDevicePath OPTIONAL
  )
{
  EFI_TPL               OldTpl;
  EFI_STATUS            Status;
  if (mPrivate)
     return EFI_SUCCESS;

  OldTpl = gBS->RaiseTPL(TPL_CALLBACK);

  DEBUG((EFI_D_VERBOSE, "%a:%d\n", __FUNCTION__, __LINE__));

  mPrivate = AllocateZeroPool(sizeof (GVT_GOP_PRIVATE_DATA));
  if (mPrivate == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto RestoreTPL;
  }

  // Initialize the gop private data
  Status = SetupGvtGop(mPrivate);
  if (EFI_ERROR (Status)) {
    goto FreePrivate;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **) &mPrivate->PciIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto FreePrivate;
  }

  // Install the gop protocol
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ControllerHandle,
                  &gEfiGraphicsOutputProtocolGuid,
                  &mPrivate->Gop,
                  NULL
                  );

  if (EFI_ERROR (Status)) {
    goto FreePrivate;
  }

  // Notify kernel to set up display for Gop
  Status = UpdateGvtGop(mPrivate);
  if (EFI_ERROR (Status)) {
    goto FreeProtocol;
  }

  Status = mPrivate->Gop.SetMode(&mPrivate->Gop, 0);
  if (EFI_ERROR (Status)) {
    goto FreeProtocol;
  }

  goto RestoreTPL;

FreeProtocol:
  gBS->UninstallMultipleProtocolInterfaces(
         mPrivate->Handle,
         &gEfiGraphicsOutputProtocolGuid,
         &mPrivate->Gop,
         NULL
      );

FreePrivate:
  if (mPrivate->PciIo) {
    gBS->CloseProtocol (
	ControllerHandle,
	&gEfiPciIoProtocolGuid,
	This->DriverBindingHandle,
	ControllerHandle
	);
    mPrivate->PciIo = NULL;
  }

  CleanUpGvtGop(mPrivate);

  FreePool(mPrivate);
  mPrivate = NULL;

RestoreTPL:
  gBS->RestoreTPL(OldTpl);
  return Status;
}


STATIC
EFI_STATUS
EFIAPI
GvtGopBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL *This,
  IN  EFI_HANDLE                  ControllerHandle,
  IN  UINTN                       NumberOfChildren,
  IN  EFI_HANDLE                  *ChildHandleBuffer OPTIONAL
  )
{
  if (!mPrivate)
    return EFI_SUCCESS;

  gBS->UninstallMultipleProtocolInterfaces(
         mPrivate->Handle,
         &gEfiGraphicsOutputProtocolGuid,
         &mPrivate->Gop,
         NULL
  );

  if (mPrivate->PciIo) {
    gBS->CloseProtocol (
	 ControllerHandle,
	 &gEfiPciIoProtocolGuid,
	 This->DriverBindingHandle,
	 ControllerHandle
    );
    mPrivate->PciIo = NULL;
  }

  CleanUpGvtGop(mPrivate);

  FreePool(mPrivate);
  mPrivate = NULL;

  return EFI_SUCCESS;
}

STATIC EFI_DRIVER_BINDING_PROTOCOL mDriverBinding = {
  GvtGopBindingSupported,
  GvtGopBindingStart,
  GvtGopBindingStop,
  0x10,
  NULL,
  NULL
};

//
// Entry point of the driver.
//
EFI_STATUS
EFIAPI
GvtGopEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  DEBUG((EFI_D_VERBOSE, "GopEntry\n"));

  mPrivate = NULL;

  return EfiLibInstallDriverBindingComponentName2 (ImageHandle, SystemTable,
           &mDriverBinding, ImageHandle, NULL, NULL);
}
