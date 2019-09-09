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
  DEBUG((EFI_D_INFO, "%a:%d index:%d\n", __FUNCTION__, __LINE__, ModeNumber));
  return EFI_SUCCESS;
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
  EFI_STATUS                      Status = EFI_SUCCESS;

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
  return EFI_SUCCESS;
}
