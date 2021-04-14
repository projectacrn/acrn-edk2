/**@file
  ACRN Platform PEI support

  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2011, Andrei Warkentin <andreiw@motorola.com>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

//
// The Library classes this module consumes
//
#include <Library/DebugLib.h>
#include <IndustryStandard/E820.h>
#include <Library/MtrrLib.h>

#include "Platform.h"

#define ACRN_E820_PHYSICAL_ADDRESS          0x000EF000

#pragma pack(1)
  typedef struct {
    CHAR8             Signature[4];
    UINT32            E820EntriesCount;
    EFI_E820_ENTRY64  E820Map[];
  } ACRN_E820_INFO;
#pragma pack()


STATIC
BOOLEAN
E820IsValid (
  IN   ACRN_E820_INFO       *E820
  )
{
  return (!AsciiStrnCmp (E820->Signature, "820", 3) &&
          E820->E820EntriesCount > 0);
}


RETURN_STATUS
AcrnGetSystemMemorySizeBelow4gb (
  OUT  UINT32               *MemSize
  )
{
  ACRN_E820_INFO    *E820;
  INT32             Loop;
  EFI_E820_ENTRY64  *Entry;

  //
  // Parse E820 map
  //
  E820 = (VOID *)ACRN_E820_PHYSICAL_ADDRESS;

  if (!E820IsValid (E820)) {
    return RETURN_UNSUPPORTED;
  }

  for (Loop = (INT32)(E820->E820EntriesCount - 1), Entry = &E820->E820Map[Loop];
       Loop >= 0; Entry = &E820->E820Map[--Loop]) {
    if (Entry->BaseAddr + Entry->Length < BASE_4GB &&
        Entry->Type == EfiAcpiAddressRangeMemory) {
      *MemSize = Entry->BaseAddr + Entry->Length;
      return RETURN_SUCCESS;
    }
  }

  return RETURN_NOT_FOUND;
}


RETURN_STATUS
AcrnGetSystemMemorySizeAbove4gb (
  OUT  UINT64               *MemSize
  )
{
  ACRN_E820_INFO    *E820;
  INT32             Loop;
  EFI_E820_ENTRY64  *Entry;
  UINT64            TotalMemSize = 0;

  //
  // Parse E820 map
  //
  E820 = (VOID *)ACRN_E820_PHYSICAL_ADDRESS;

  if (!E820IsValid (E820)) {
    return RETURN_UNSUPPORTED;
  }

  for (Loop = (INT32)(E820->E820EntriesCount - 1), Entry = &E820->E820Map[Loop];
       Loop >= 0 && Entry->BaseAddr + Entry->Length > BASE_4GB;
       Entry = &E820->E820Map[--Loop]) {
    ASSERT (Entry->BaseAddr >= BASE_4GB);
    if (Entry->Type == EfiAcpiAddressRangeMemory) {
      TotalMemSize += Entry->Length;
    }
  }

  *MemSize = TotalMemSize;
  return RETURN_SUCCESS;
}


RETURN_STATUS
AcrnGetFirstNonAddress (
  OUT  UINT64               *MaxAddress
  )
{
  ACRN_E820_INFO    *E820;
  EFI_E820_ENTRY64  *Entry;

  //
  // Parse E820 map
  //
  E820 = (VOID *)ACRN_E820_PHYSICAL_ADDRESS;

  if (!E820IsValid (E820)) {
    return RETURN_UNSUPPORTED;
  }

  Entry = &E820->E820Map[E820->E820EntriesCount - 1];

  if (Entry->BaseAddr + Entry->Length >= BASE_4GB) {
    *MaxAddress = Entry->BaseAddr + Entry->Length;
    return RETURN_SUCCESS;
  }

  return RETURN_NOT_FOUND;
}


RETURN_STATUS
AcrnFindPciMmio64Aperture (
  OUT  UINT64               *Pci64Base,
  OUT  UINT64               *Pci64Size
  )
{
  *Pci64Base = BASE_256GB;
  *Pci64Size = SIZE_256GB;

  return RETURN_SUCCESS;
}


RETURN_STATUS
AcrnPublishRamRegions (
  VOID
  )
{
  ACRN_E820_INFO    *E820;
  UINT64            PciExBarBase;
  BOOLEAN           MtrrSupported;
  UINT32            Loop;
  EFI_E820_ENTRY64  *Entry;

  //
  // Parse for RAM and 64-bit PCI host aperture in E820 map
  //
  E820 = (VOID *)ACRN_E820_PHYSICAL_ADDRESS;

  if (!E820IsValid (E820)) {
    return RETURN_UNSUPPORTED;
  }

  DEBUG ((EFI_D_INFO, "Using memory map provided by ACRN\n"));

  PciExBarBase = FixedPcdGet64 (PcdPciExpressBaseAddress);
  MtrrSupported = IsMtrrSupported ();

  for (Loop = 0, Entry = &E820->E820Map[Loop]; Loop < E820->E820EntriesCount;
       Entry = &E820->E820Map[++Loop]) {
    if (Entry->Type == EfiAcpiAddressRangeMemory) {
      AddMemoryBaseSizeHob (Entry->BaseAddr, Entry->Length);

      if (MtrrSupported) {
        MtrrSetMemoryAttribute (Entry->BaseAddr, Entry->Length, CacheWriteBack);
      }
    } else if (Entry->Type == EfiAcpiAddressRangeReserved &&
               (Entry->BaseAddr < PciExBarBase || Entry->BaseAddr >= BASE_4GB)) {
      AddReservedMemoryBaseSizeHob (Entry->BaseAddr, Entry->Length, FALSE);
    }
  }

  return RETURN_SUCCESS;
}
