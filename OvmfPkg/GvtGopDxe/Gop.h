/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef __GOP_H_
#define __GOP_H_

#include <PiDxe.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PciIo.h>
#include <IndustryStandard/Pci.h>
#include <Library/FrameBufferBltLib.h>

#define VGT_CAPS_GOP_SUPPORT    (1 << 5)
#define VGT_MAGIC               0x4776544776544776ULL /* 'vGTvGTvG' */
#define VGT_IF_BASE             0x78000
#define VGT_G2V_OFFSET          0x818
#define VGT_GOP_OFFSET          0x86C
#define VGT_G2V_GOP_SETUP       0xd

typedef struct {
  UINT64 magic;                 /* VGT_MAGIC */
  UINT16 version_major;
  UINT16 version_minor;
  UINT32 vgt_id;
  UINT32 vgt_caps;              /* VGT capabilities */
} GVT_IF_HDR;

typedef struct {
  UINT32        fb_base;
  UINT32        width;
  UINT32        height;
  UINT32        pitch;
  UINT32        bpp;
  UINT32        size;
} GVT_GOP_INFO;

typedef struct {
  EFI_HANDLE                    Handle;
  EFI_PCI_IO_PROTOCOL           *PciIo;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  Gop;
  FRAME_BUFFER_CONFIGURE        *FrameBufferBltConfigure;
  UINTN                         FrameBufferBltConfigureSize;
  GVT_GOP_INFO                  Info;
} GVT_GOP_PRIVATE_DATA;

extern GVT_GOP_PRIVATE_DATA *mPrivate;

#define GVT_GOP_MAX_MODE 1
#define INVALIDE_MODE_NUMBER  0xffff

EFI_STATUS
SetupGvtGop (
  IN GVT_GOP_PRIVATE_DATA *Private
  );

EFI_STATUS
CleanUpGvtGop (
  IN GVT_GOP_PRIVATE_DATA *Private
  );

EFI_STATUS
UpdateGvtGop (
  IN GVT_GOP_PRIVATE_DATA *Private
  );

#endif
