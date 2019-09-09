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
  EFI_STATUS                    Status = EFI_SUCCESS;

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
