#include "Gop.h"

STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION mModeList[] = {
  {
    .Version = 0,
    .PixelFormat = PixelBlueGreenRedReserved8BitPerColor,
    .HorizontalResolution = 1024,
    .PixelsPerScanLine = 1024,
    .VerticalResolution = 768,
  },
};

//
// Graphics Output Protocol Member Functions
//

EFI_STATUS
EFIAPI
GvtGopQueryMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  UINT32                                ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  )
{
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *ModeInfo;

  if (!mPrivate) {
    return EFI_NOT_STARTED;
  }

  if (ModeNumber >= GVT_GOP_MAX_MODE) {
    return EFI_INVALID_PARAMETER;
  }

  ModeInfo = AllocateZeroPool (sizeof(*ModeInfo));
  if (ModeInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem(ModeInfo, &mModeList[0], sizeof(*ModeInfo));

  DEBUG((EFI_D_INFO, "%a:%d Get mode %dx%d\n", __FUNCTION__, __LINE__,
        ModeInfo->HorizontalResolution,
        ModeInfo->VerticalResolution));

  *SizeOfInfo = sizeof(*ModeInfo);
  *Info = ModeInfo;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GvtGopSetMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
  IN  UINT32                       ModeNumber
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black;
  EFI_STATUS                    Status;

  DEBUG((EFI_D_INFO, "%a:%d index:%d\n", __FUNCTION__, __LINE__, ModeNumber));

  if (ModeNumber >= GVT_GOP_MAX_MODE) {
    DEBUG((EFI_D_WARN, "ModeNumber is out of range\n"));
    return EFI_UNSUPPORTED;
  }

  if (!mPrivate) {
    DEBUG((EFI_D_WARN, "mPrivate is invalid\n"));
    return EFI_NOT_STARTED;
  }

  //
  // Re-initialize the frame buffer configure when mode changes.
  //
  Status = FrameBufferBltConfigure (
             (VOID*) (UINTN) This->Mode->FrameBufferBase,
             This->Mode->Info,
             mPrivate->FrameBufferBltConfigure,
             &mPrivate->FrameBufferBltConfigureSize
             );
  if (Status == RETURN_BUFFER_TOO_SMALL) {
    //
    // Frame buffer configure may be larger in new mode.
    //

    mPrivate->FrameBufferBltConfigure =
      AllocatePool (mPrivate->FrameBufferBltConfigureSize);
    ASSERT (mPrivate->FrameBufferBltConfigure != NULL);

    //
    // Create the configuration for FrameBufferBltLib
    //
    Status = FrameBufferBltConfigure (
                (VOID*) (UINTN) This->Mode->FrameBufferBase,
                This->Mode->Info,
                mPrivate->FrameBufferBltConfigure,
                &mPrivate->FrameBufferBltConfigureSize
                );
  }
  ASSERT (Status == RETURN_SUCCESS);

  //
  // Per UEFI Spec, need to clear the visible portions of the output display to black.
  //
  ZeroMem (&Black, sizeof (Black));
  Status = This->Blt(This,
           &Black,
             EfiBltVideoFill,
             0, 0,
             0, 0,
             This->Mode->Info->HorizontalResolution,
             This->Mode->Info->VerticalResolution,
             0);
  ASSERT_RETURN_ERROR (Status);

  return Status;
}

EFI_STATUS
EFIAPI
GvtGopBlt (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL         *BltBuffer, OPTIONAL
  IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION     BltOperation,
  IN  UINTN                                 SourceX,
  IN  UINTN                                 SourceY,
  IN  UINTN                                 DestinationX,
  IN  UINTN                                 DestinationY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height,
  IN  UINTN                                 Delta
  )
{
  EFI_STATUS                      Status;
  EFI_TPL                         OriginalTPL;

  if (!mPrivate || !mPrivate->FrameBufferBltConfigure)
    return EFI_NOT_STARTED;

  //
  // We have to raise to TPL Notify, so we make an atomic write the frame buffer.
  // We would not want a timer based event (Cursor, ...) to come in while we are
  // doing this operation.
  //
  OriginalTPL = gBS->RaiseTPL (TPL_NOTIFY);

  switch (BltOperation) {
  case EfiBltVideoToBltBuffer:
  case EfiBltBufferToVideo:
  case EfiBltVideoFill:
  case EfiBltVideoToVideo:
    Status = FrameBufferBlt (
      mPrivate->FrameBufferBltConfigure,
      BltBuffer,
      BltOperation,
      SourceX,
      SourceY,
      DestinationX,
      DestinationY,
      Width,
      Height,
      Delta
      );
    break;

  default:
    Status = EFI_INVALID_PARAMETER;
    break;
  }

  gBS->RestoreTPL (OriginalTPL);

  return Status;
}

//
//  functions to prepare/update/clear GOP
//

EFI_STATUS
SetupGvtGop (
  IN GVT_GOP_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                    Status = EFI_SUCCESS;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;

  DEBUG((EFI_D_VERBOSE, "%a:%d\n", __FUNCTION__, __LINE__));

  Private->FrameBufferBltConfigure = NULL;
  Private->FrameBufferBltConfigureSize = 0;

  Gop            = &Private->Gop;
  Gop->QueryMode = GvtGopQueryMode;
  Gop->SetMode   = GvtGopSetMode;
  Gop->Blt       = GvtGopBlt;

  //
  // Initialize the private data
  //
  Status = gBS->AllocatePool (
      EfiBootServicesData,
      sizeof (EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE),
      (VOID **) &Private->Gop.Mode
      );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
                  (VOID **) &Private->Gop.Mode->Info
                  );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  Private->Gop.Mode->MaxMode = GVT_GOP_MAX_MODE;
  Private->Gop.Mode->Mode    = 0;
  Private->Gop.Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  CopyMem(Private->Gop.Mode->Info,
          &mModeList[0],
          sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));

  return Status;
}

EFI_STATUS
CleanUpGvtGop (
  IN GVT_GOP_PRIVATE_DATA  *Private
  )
{
  if (Private->FrameBufferBltConfigure) {
    FreePool(Private->FrameBufferBltConfigure);
    Private->FrameBufferBltConfigure = NULL;
    Private->FrameBufferBltConfigureSize = 0;
  }

  if (!Private->Gop.Mode) {
    return EFI_SUCCESS;
  }

  if (!Private->Gop.Mode->Info) {
    goto FreeMode;
  }

  gBS->FreePool(Private->Gop.Mode->Info);
  Private->Gop.Mode->Info = NULL;

FreeMode:
  gBS->FreePool(Private->Gop.Mode);
  Private->Gop.Mode = NULL;

  return EFI_SUCCESS;
}

EFI_STATUS
UpdateGvtGop (
  IN GVT_GOP_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS            Status;
  UINT32                Notify = VGT_G2V_GOP_SETUP;
  GVT_GOP_INFO          gop_info;

  DEBUG((EFI_D_VERBOSE, "%a:%d\n", __FUNCTION__, __LINE__));
  Status = Private->PciIo->Mem.Write (
        Private->PciIo,
        EfiPciIoWidthUint32,
        PCI_BAR_IDX0,
        VGT_IF_BASE + VGT_G2V_OFFSET,
        sizeof(Notify)/sizeof(UINT32),
        &Notify
        );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  //Check GVT Gop settings
  Status = Private->PciIo->Mem.Read (
        Private->PciIo,
        EfiPciIoWidthUint32,
        PCI_BAR_IDX0,
        VGT_IF_BASE + VGT_GOP_OFFSET,
        sizeof(gop_info)/sizeof(UINT32),
        &gop_info
        );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  if (gop_info.fb_base == 0) {
    DEBUG((EFI_D_WARN, "Failed to get FbBase\n"));
    Status = EFI_UNSUPPORTED;
    goto Done;
  }

  DEBUG((EFI_D_INFO, "w:%d h:%d p:%d b:%d s:%d base:%lx\n",
        gop_info.width, gop_info.height, gop_info.pitch,
        gop_info.bpp, gop_info.size, gop_info.fb_base
        ));

  Private->Gop.Mode->FrameBufferBase = gop_info.fb_base;
  Private->Gop.Mode->FrameBufferSize = gop_info.size;

  mModeList[0].HorizontalResolution = gop_info.width;
  mModeList[0].VerticalResolution = gop_info.height;
  mModeList[0].PixelsPerScanLine = gop_info.pitch;

  CopyMem(&Private->Info, &gop_info, sizeof(GVT_GOP_INFO));
  CopyMem(Private->Gop.Mode->Info,
          &mModeList[0],
          sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));

Done:
  return Status;
}
