/** @file
  Copyright (c) 2019, Intel Corporation. All rights reserved.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.   The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef __ACRN_PLATFORM_H__
#define __ACRN_PLATFORM_H__

#include <IndustryStandard/E820.h>
//
// Host Bridge Device ID (DID) value for ACRN
//
#ifdef OVMF_AS_ACRN_SOS
#define ACRN_HOSTBRIDGE_DEVICE_ID 0x5af0U
#define ACRN_E820_PHYSICAL_ADDRESS 0x00001000U 
#else
#define ACRN_HOSTBRIDGE_DEVICE_ID 0x1275U
#define ACRN_E820_PHYSICAL_ADDRESS  0x000EF000U
#endif
//
// Values we program into the PM base address registers
//
#define ACRN_PMBASE_VALUE 0x400

#pragma pack(1)
  typedef struct {
    CHAR8             Signature[4];
    UINT32            E820EntriesCount;
    EFI_E820_ENTRY64  E820Map[];
  } ACRN_E820_INFO;
#pragma pack()

#endif
