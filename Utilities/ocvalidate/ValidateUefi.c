/** @file
  Copyright (C) 2018, vit9696. All rights reserved.
  Copyright (C) 2020, PMheart. All rights reserved.

  All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "NvramKeyInfo.h"
#include "ocvalidate.h"
#include "OcValidateLib.h"

#include <Library/BaseLib.h>
#include <Library/OcConsoleLib.h>

/**
  Callback function to verify whether one UEFI driver is duplicated in UEFI->Drivers.

  @param[in]  PrimaryDriver    Primary driver to be checked.
  @param[in]  SecondaryDriver  Secondary driver to be checked.

  @retval     TRUE             If PrimaryDriver and SecondaryDriver are duplicated.
**/
STATIC
BOOLEAN
UefiDriverHasDuplication (
  IN  CONST VOID  *PrimaryDriver,
  IN  CONST VOID  *SecondaryDriver
  )
{
  CONST OC_UEFI_DRIVER_ENTRY  *UefiPrimaryDriver;
  CONST OC_UEFI_DRIVER_ENTRY  *UefiSecondaryDriver;
  CONST CHAR8                 *UefiDriverPrimaryString;
  CONST CHAR8                 *UefiDriverSecondaryString;

  UefiPrimaryDriver         = *(CONST OC_UEFI_DRIVER_ENTRY **) PrimaryDriver;
  UefiSecondaryDriver       = *(CONST OC_UEFI_DRIVER_ENTRY **) SecondaryDriver;
  UefiDriverPrimaryString   = OC_BLOB_GET (&UefiPrimaryDriver->Path);
  UefiDriverSecondaryString = OC_BLOB_GET (&UefiSecondaryDriver->Path);

  return StringIsDuplicated ("UEFI->Drivers", UefiDriverPrimaryString, UefiDriverSecondaryString);
}

/**
  Callback function to verify whether one UEFI ReservedMemory entry overlaps the other,
  in terms of Address and Size.

  @param[in]  PrimaryEntry     Primary entry to be checked.
  @param[in]  SecondaryEntry   Secondary entry to be checked.

  @retval     TRUE             If PrimaryEntry and SecondaryEntry have overlapped Address and Size.
**/
STATIC
BOOLEAN
UefiReservedMemoryHasOverlap (
  IN  CONST VOID  *PrimaryEntry,
  IN  CONST VOID  *SecondaryEntry
  )
{
  CONST OC_UEFI_RSVD_ENTRY           *UefiReservedMemoryPrimaryEntry;
  CONST OC_UEFI_RSVD_ENTRY           *UefiReservedMemorySecondaryEntry;
  UINT64                             UefiReservedMemoryPrimaryAddress;
  UINT64                             UefiReservedMemoryPrimarySize;
  UINT64                             UefiReservedMemorySecondaryAddress;
  UINT64                             UefiReservedMemorySecondarySize;

  UefiReservedMemoryPrimaryEntry     = *(CONST OC_UEFI_RSVD_ENTRY **) PrimaryEntry;
  UefiReservedMemorySecondaryEntry   = *(CONST OC_UEFI_RSVD_ENTRY **) SecondaryEntry;
  UefiReservedMemoryPrimaryAddress   = UefiReservedMemoryPrimaryEntry->Address;
  UefiReservedMemoryPrimarySize      = UefiReservedMemoryPrimaryEntry->Size;
  UefiReservedMemorySecondaryAddress = UefiReservedMemorySecondaryEntry->Address;
  UefiReservedMemorySecondarySize    = UefiReservedMemorySecondaryEntry->Size;

  if (!UefiReservedMemoryPrimaryEntry->Enabled || !UefiReservedMemorySecondaryEntry->Enabled) {
    return FALSE;
  }

  if (UefiReservedMemoryPrimaryAddress < UefiReservedMemorySecondaryAddress + UefiReservedMemorySecondarySize
    && UefiReservedMemorySecondaryAddress < UefiReservedMemoryPrimaryAddress + UefiReservedMemoryPrimarySize) {
    DEBUG ((DEBUG_WARN, "UEFI->ReservedMemory: ???????????????????????????????????? "));
    return TRUE;
  }

  return FALSE;
}

STATIC
BOOLEAN
ValidateReservedMemoryType (
  IN  CONST CHAR8  *Type
  )
{
  UINTN               Index;
  STATIC CONST CHAR8  *AllowedType[] = {
    "Reserved",          "LoaderCode",    "LoaderData",     "BootServiceCode",         "BootServiceData",
    "RuntimeCode",       "RuntimeData",   "Available",      "Persistent",              "UnusableMemory",
    "ACPIReclaimMemory", "ACPIMemoryNVS", "MemoryMappedIO", "MemoryMappedIOPortSpace", "PalCode"
  };

  for (Index = 0; Index < ARRAY_SIZE (AllowedType); ++Index) {
    if (AsciiStrCmp (Type, AllowedType[Index]) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
UINT32
CheckUefiAPFS (
  IN  OC_GLOBAL_CONFIG  *Config
  )
{
  UINT32                   ErrorCount;
  OC_UEFI_CONFIG           *UserUefi;
  OC_MISC_CONFIG           *UserMisc;
  BOOLEAN                  IsEnableJumpstartEnabled;
  UINT32                   ScanPolicy;

  ErrorCount               = 0;
  UserUefi                 = &Config->Uefi;
  UserMisc                 = &Config->Misc;

  //
  // If FS restrictions is enabled but APFS FS scanning is disabled, it is an error.
  //
  IsEnableJumpstartEnabled = UserUefi->Apfs.EnableJumpstart;
  ScanPolicy               = UserMisc->Security.ScanPolicy;
  if (IsEnableJumpstartEnabled
    && (ScanPolicy & OC_SCAN_FILE_SYSTEM_LOCK) != 0
    && (ScanPolicy & OC_SCAN_ALLOW_FS_APFS) == 0) {
    DEBUG ((DEBUG_WARN, "UEFI->APFS->EnableJumpstart ?????????, ?????? Misc->Security->ScanPolicy ???????????????APFS??????!\n"));
    ++ErrorCount;
  }

  return ErrorCount;
}

STATIC
UINT32
CheckUefiAppleInput (
  IN  OC_GLOBAL_CONFIG  *Config
  )
{
  UINT32          ErrorCount;
  OC_UEFI_CONFIG  *UserUefi;
  CONST CHAR8     *AppleEvent;

  ErrorCount      = 0;
  UserUefi        = &Config->Uefi;

  ErrorCount      = 0;

  AppleEvent = OC_BLOB_GET (&UserUefi->AppleInput.AppleEvent);
  if (AsciiStrCmp (AppleEvent, "Auto") != 0
    && AsciiStrCmp (AppleEvent, "Builtin") != 0
    && AsciiStrCmp (AppleEvent, "OEM") != 0) {
    DEBUG ((DEBUG_WARN, "UEFI->AppleInput->AppleEvent ???????????? (????????? Auto, Builtin, OEM)!\n"));
    ++ErrorCount;
  }

  if (UserUefi->Input.KeySupport && UserUefi->AppleInput.CustomDelays) {
    if (UserUefi->AppleInput.KeyInitialDelay != 0
      && UserUefi->AppleInput.KeyInitialDelay < UserUefi->Input.KeyForgetThreshold) {
      DEBUG ((DEBUG_WARN, "???KeySupport???????????????KeyInitialDelay, ??????????????????KeyForgetThreshold??? (???????????????????????????????????????); ????????????0?????????!\n"));
      ++ErrorCount;
    }
    if (UserUefi->AppleInput.KeySubsequentDelay < UserUefi->Input.KeyForgetThreshold) {
      DEBUG ((DEBUG_WARN, "KeySubsequentDelay???KeySupport??????????????????????????????KeyForgetThreshold??? (???????????????????????????????????????); ??????KeyForgetThreshold????????????????????????!\n"));
      ++ErrorCount;
    }
  }

  return ErrorCount;
}

STATIC
UINT32
CheckUefiAudio (
  IN  OC_GLOBAL_CONFIG  *Config
  )
{
  UINT32                   ErrorCount;
  OC_UEFI_CONFIG           *UserUefi;
  BOOLEAN                  IsAudioSupportEnabled;
  CONST CHAR8              *AsciiAudioDevicePath;
  CONST CHAR8              *AsciiPlayChime;

  ErrorCount               = 0;
  UserUefi                 = &Config->Uefi;

  IsAudioSupportEnabled    = UserUefi->Audio.AudioSupport;
  AsciiAudioDevicePath     = OC_BLOB_GET (&UserUefi->Audio.AudioDevice);
  AsciiPlayChime           = OC_BLOB_GET (&UserUefi->Audio.PlayChime);
  if (IsAudioSupportEnabled) {
    if (!AsciiDevicePathIsLegal (AsciiAudioDevicePath)) {
      DEBUG ((DEBUG_WARN, "UEFI->Audio->AudioDevice ?????????! ?????????????????????!\n"));
      ++ErrorCount;
    }

    if (AsciiPlayChime[0] == '\0') {
      DEBUG ((DEBUG_WARN, "UEFI->Audio->PlayChime ??????AudioSupport???????????????!\n"));
      ++ErrorCount;
    } else if (AsciiStrCmp (AsciiPlayChime, "Auto") != 0
      && AsciiStrCmp (AsciiPlayChime, "Enabled") != 0
      && AsciiStrCmp (AsciiPlayChime, "Disabled") != 0) {
      DEBUG ((DEBUG_WARN, "UEFI->Audio->PlayChime ???????????? (????????? Auto, Enabled, ??? Disabled)!\n"));
      ++ErrorCount;
    }
  }

  return ErrorCount;
}

STATIC
UINT32
CheckUefiDrivers (
  IN  OC_GLOBAL_CONFIG  *Config
  )
{
  UINT32                       ErrorCount;
  OC_UEFI_CONFIG               *UserUefi;
  UINT32                       Index;
  OC_UEFI_DRIVER_ENTRY         *DriverEntry;
  CONST CHAR8                  *Comment;
  CONST CHAR8                  *Driver;
  BOOLEAN                      HasOpenRuntimeEfiDriver;
  BOOLEAN                      HasOpenUsbKbDxeEfiDriver;
  UINT32                       IndexOpenUsbKbDxeEfiDriver;
  BOOLEAN                      HasPs2KeyboardDxeEfiDriver;
  BOOLEAN                      IndexPs2KeyboardDxeEfiDriver;
  BOOLEAN                      HasHfsEfiDriver;
  UINT32                       IndexHfsEfiDriver;
  BOOLEAN                      HasAudioDxeEfiDriver;
  UINT32                       IndexAudioDxeEfiDriver;
  BOOLEAN                      IsRequestBootVarRoutingEnabled;
  BOOLEAN                      IsKeySupportEnabled;
  BOOLEAN                      IsConnectDriversEnabled;

  ErrorCount                   = 0;
  UserUefi                     = &Config->Uefi;

  HasOpenRuntimeEfiDriver      = FALSE;
  HasOpenUsbKbDxeEfiDriver     = FALSE;
  IndexOpenUsbKbDxeEfiDriver   = 0;
  HasPs2KeyboardDxeEfiDriver   = FALSE;
  IndexPs2KeyboardDxeEfiDriver = 0;
  HasHfsEfiDriver              = FALSE;
  IndexHfsEfiDriver            = 0;
  HasAudioDxeEfiDriver         = FALSE;
  IndexAudioDxeEfiDriver       = 0;
  for (Index = 0; Index < UserUefi->Drivers.Count; ++Index) {
    DriverEntry = UserUefi->Drivers.Values[Index];
    Comment     = OC_BLOB_GET (&DriverEntry->Comment);
    Driver      = OC_BLOB_GET (&DriverEntry->Path);

    //
    // Check the length of path relative to OC directory.
    //
    if (StrLen (OPEN_CORE_UEFI_DRIVER_PATH) + AsciiStrSize (Driver) > OC_STORAGE_SAFE_PATH_MAX) {
      DEBUG ((DEBUG_WARN, "UEFI->Drivers[%u] ?????? (????????????%u)!\n", Index, OC_STORAGE_SAFE_PATH_MAX));
      ++ErrorCount;
    }

    //
    // Sanitise strings.
    //
    if (!AsciiCommentIsLegal (Comment)) {
      DEBUG ((DEBUG_WARN, "UEFI->Drivers[%u]->Comment ??????????????????!\n", Index));
      ++ErrorCount;
    }
    if (!AsciiUefiDriverIsLegal (Driver, Index)) {
      ++ErrorCount;
      continue;
    }

    if (!DriverEntry->Enabled) {
      continue;
    }

    if (AsciiStrCmp (Driver, "OpenRuntime.efi") == 0) {
      HasOpenRuntimeEfiDriver = TRUE;
    }
    if (AsciiStrCmp (Driver, "OpenUsbKbDxe.efi") == 0) {
      HasOpenUsbKbDxeEfiDriver   = TRUE;
      IndexOpenUsbKbDxeEfiDriver = Index;
    }
    if (AsciiStrCmp (Driver, "Ps2KeyboardDxe.efi") == 0) {
      HasPs2KeyboardDxeEfiDriver   = TRUE;
      IndexPs2KeyboardDxeEfiDriver = Index;
    }
    //
    // There are several HFS Plus drivers, including HfsPlus, VboxHfs, etc.
    // Here only "hfs" (case-insensitive) is matched.
    //
    if (OcAsciiStriStr (Driver, "hfs") != NULL) {
      HasHfsEfiDriver   = TRUE;
      IndexHfsEfiDriver = Index;
    }
    if (AsciiStrCmp (Driver, "AudioDxe.efi") == 0) {
      HasAudioDxeEfiDriver   = TRUE;
      IndexAudioDxeEfiDriver = Index;
    }
  }

  //
  // Check duplicated Drivers.
  //
  ErrorCount += FindArrayDuplication (
    UserUefi->Drivers.Values,
    UserUefi->Drivers.Count,
    sizeof (UserUefi->Drivers.Values[0]),
    UefiDriverHasDuplication
    );

  IsRequestBootVarRoutingEnabled = UserUefi->Quirks.RequestBootVarRouting;
  if (IsRequestBootVarRoutingEnabled) {
    if (!HasOpenRuntimeEfiDriver) {
      DEBUG ((DEBUG_WARN, "UEFI->Quirks->RequestBootVarRouting?????????, ??????OpenRuntime.efi??????UEFI->Drivers?????????!\n"));
      ++ErrorCount;
    }
  }

  IsKeySupportEnabled = UserUefi->Input.KeySupport;
  if (IsKeySupportEnabled) {
    if (HasOpenUsbKbDxeEfiDriver) {
      DEBUG ((DEBUG_WARN, "???UEFI->Drivers[%u]?????????OpenUsbKbDxe.efi ????????????UEFI->Input->KeySupport????????????!\n", IndexOpenUsbKbDxeEfiDriver));
      ++ErrorCount;
    }
  } else {
    if (HasPs2KeyboardDxeEfiDriver) {
      DEBUG ((DEBUG_WARN, "UEFI->Input->KeySupport???Ps2KeyboardDxe.efi?????????????????????!\n"));
      ++ErrorCount;
    }
  }

  if (HasOpenUsbKbDxeEfiDriver && HasPs2KeyboardDxeEfiDriver) {
    DEBUG ((
      DEBUG_WARN,
      "???UEFI->Drivers[%u]??????OpenUsbKbDxe.efi ,???Ps2KeyboardDxe.efi, ?????????????????????!\n",
      IndexOpenUsbKbDxeEfiDriver,
      IndexPs2KeyboardDxeEfiDriver
      ));
    ++ErrorCount;
  }

  IsConnectDriversEnabled = UserUefi->ConnectDrivers;
  if (!IsConnectDriversEnabled) {
    if (HasHfsEfiDriver) {
      DEBUG ((DEBUG_WARN, "HFS???????????????????????????UEFI->Drivers[%u]?????????,??????????????????UEFI->ConnectDrivers!\n", IndexHfsEfiDriver));
      ++ErrorCount;
    }
    if (HasAudioDxeEfiDriver) {
      DEBUG ((DEBUG_WARN, "AudioDevice.efi ???UEFI->Drivers[%u]?????????,??????????????????UEFI->ConnectDrivers!\n", IndexAudioDxeEfiDriver));
      ++ErrorCount;
    }
  }

  return ErrorCount;
}

STATIC
UINT32
CheckUefiInput (
  IN  OC_GLOBAL_CONFIG  *Config
  )
{
  UINT32          ErrorCount;
  OC_UEFI_CONFIG  *UserUefi;
  BOOLEAN         IsPointerSupportEnabled;
  CONST CHAR8     *PointerSupportMode;
  CONST CHAR8     *KeySupportMode;

  ErrorCount      = 0;
  UserUefi        = &Config->Uefi;

  IsPointerSupportEnabled = UserUefi->Input.PointerSupport;
  PointerSupportMode      = OC_BLOB_GET (&UserUefi->Input.PointerSupportMode);
  if (IsPointerSupportEnabled && AsciiStrCmp (PointerSupportMode, "ASUS") != 0) {
    DEBUG ((DEBUG_WARN, "UEFI->Input->?????????PointerSupport??????PointerSupportMode??????ASUS!\n"));
    ++ErrorCount;
  }

  KeySupportMode = OC_BLOB_GET (&UserUefi->Input.KeySupportMode);
  if (AsciiStrCmp (KeySupportMode, "Auto") != 0
    && AsciiStrCmp (KeySupportMode, "V1") != 0
    && AsciiStrCmp (KeySupportMode, "V2") != 0
    && AsciiStrCmp (KeySupportMode, "AMI") != 0) {
    DEBUG ((DEBUG_WARN, "UEFI->Input->KeySupportMode????????? (????????? Auto, V1, V2, AMI)!\n"));
    ++ErrorCount;
  }

  return ErrorCount;
}

STATIC
UINT32
CheckUefiOutput (
  IN  OC_GLOBAL_CONFIG  *Config
  )
{
  UINT32               ErrorCount;
  OC_UEFI_CONFIG       *UserUefi;
  CONST CHAR8          *TextRenderer;
  CONST CHAR8          *GopPassThrough;
  BOOLEAN              IsTextRendererSystem;
  BOOLEAN              IsClearScreenOnModeSwitchEnabled;
  BOOLEAN              IsIgnoreTextInGraphicsEnabled;
  BOOLEAN              IsReplaceTabWithSpaceEnabled;
  BOOLEAN              IsSanitiseClearScreenEnabled;
  CONST CHAR8          *ConsoleMode;
  CONST CHAR8          *Resolution;
  UINT32               UserWidth;
  UINT32               UserHeight;
  UINT32               UserBpp;
  BOOLEAN              UserSetMax;
  INT8                 UIScale;
  BOOLEAN              HasUefiOutputUIScale;

  ErrorCount           = 0;
  UserUefi             = &Config->Uefi;
  IsTextRendererSystem = FALSE;
  HasUefiOutputUIScale = FALSE;

  //
  // Sanitise strings.
  //
  TextRenderer = OC_BLOB_GET (&UserUefi->Output.TextRenderer);
  if (AsciiStrCmp (TextRenderer, "BuiltinGraphics") != 0
    && AsciiStrCmp (TextRenderer, "BuiltinText") != 0
    && AsciiStrCmp (TextRenderer, "SystemGraphics") != 0
    && AsciiStrCmp (TextRenderer, "SystemText") != 0
    && AsciiStrCmp (TextRenderer, "SystemGeneric") != 0) {
    DEBUG ((DEBUG_WARN, "UEFI->Output->TextRenderer???????????? (?????????BuiltinGraphics, BuiltinText, SystemGraphics, SystemText, ??? SystemGeneric)!\n"));
    ++ErrorCount;
  } else if (AsciiStrnCmp (TextRenderer, "System", L_STR_LEN ("System")) == 0) {
    //
    // Check whether TextRenderer has System prefix.
    //
    IsTextRendererSystem = TRUE;
  }

  if (!IsTextRendererSystem) {
    IsClearScreenOnModeSwitchEnabled = UserUefi->Output.ClearScreenOnModeSwitch;
    if (IsClearScreenOnModeSwitchEnabled) {
      DEBUG ((DEBUG_WARN, "UEFI->Output->ClearScreenOnModeSwitch?????????System TextRenderer??????????????? (??????????????? %a)!\n", TextRenderer));
      ++ErrorCount;
    }

    IsIgnoreTextInGraphicsEnabled    = UserUefi->Output.IgnoreTextInGraphics;
    if (IsIgnoreTextInGraphicsEnabled) {
      DEBUG ((DEBUG_WARN, "UEFI->Output->IgnoreTextInGraphics?????????System TextRenderer??????????????? (??????????????? %a)!\n", TextRenderer));
      ++ErrorCount;
    }

    IsReplaceTabWithSpaceEnabled     = UserUefi->Output.ReplaceTabWithSpace;
    if (IsReplaceTabWithSpaceEnabled) {
      DEBUG ((DEBUG_WARN, "UEFI->Output->ReplaceTabWithSpace?????????System TextRenderer??????????????? (??????????????? %a)!\n", TextRenderer));
      ++ErrorCount;
    }

    IsSanitiseClearScreenEnabled     = UserUefi->Output.SanitiseClearScreen;
    if (IsSanitiseClearScreenEnabled) {
      DEBUG ((DEBUG_WARN, "UEFI->Output->SanitiseClearScreen?????????System TextRenderer??????????????? (??????????????? %a)!\n", TextRenderer));
      ++ErrorCount;
    }
  }

  GopPassThrough = OC_BLOB_GET (&UserUefi->Output.GopPassThrough);
  if (AsciiStrCmp (GopPassThrough, "Enabled") != 0
    && AsciiStrCmp (GopPassThrough, "Disabled") != 0
    && AsciiStrCmp (GopPassThrough, "Apple") != 0) {
    DEBUG ((DEBUG_WARN, "UEFI->Output->GopPassThrough ???????????? (????????? Enabled, Disabled, Apple)!\n"));
    ++ErrorCount;
  }

  //
  // Parse Output->ConsoleMode by calling OpenCore libraries.
  //
  ConsoleMode = OC_BLOB_GET (&UserUefi->Output.ConsoleMode);
  OcParseConsoleMode (
    ConsoleMode,
    &UserWidth,
    &UserHeight,
    &UserSetMax
    );
  if (ConsoleMode[0] != '\0'
    && !UserSetMax
    && (UserWidth == 0 || UserHeight == 0)) {
    DEBUG ((DEBUG_WARN, "UEFI->Output->ConsoleMode?????????, ?????????Configurations.pdf!\n"));
    ++ErrorCount;
  }

  //
  // Parse Output->Resolution by calling OpenCore libraries.
  //
  Resolution = OC_BLOB_GET (&UserUefi->Output.Resolution);
  OcParseScreenResolution (
    Resolution,
    &UserWidth,
    &UserHeight,
    &UserBpp,
    &UserSetMax
    );
  if (Resolution[0] != '\0'
    && !UserSetMax
    && (UserWidth == 0 || UserHeight == 0)) {
    DEBUG ((DEBUG_WARN, "UEFI->Output->Resolution?????????, ?????????Configurations.pdf!\n"));
    ++ErrorCount;
  }

  UIScale = UserUefi->Output.UIScale;
  if (UIScale < -1 || UIScale > 2) {
    DEBUG ((DEBUG_WARN, "UEFI->Output->UIScale ????????? (???????????? -1 ??? 2 ??????)!\n"));
    ++ErrorCount;
  } else if (UIScale != -1) {
    HasUefiOutputUIScale = TRUE;
  }

  if (HasUefiOutputUIScale && mHasNvramUIScale) {
    DEBUG ((DEBUG_WARN, "UIScale ??? NVRAM ??? UEFI->Output ?????????!\n"));
    ++ErrorCount;
  }

  return ErrorCount;
}

//
// FIXME: Just in case this can be of use...
//
STATIC
UINT32
CheckUefiQuirks (
  IN  OC_GLOBAL_CONFIG  *Config
  )
{
  UINT32          ErrorCount;
  OC_UEFI_CONFIG  *UserUefi;
  INT8            ResizeGpuBars;

  ErrorCount      = 0;
  UserUefi        = &Config->Uefi;
  ResizeGpuBars   = UserUefi->Quirks.ResizeGpuBars;

  if (ResizeGpuBars < -1 || ResizeGpuBars > 19) {
    DEBUG ((DEBUG_WARN, "UEFI->Quirks->ResizeGpuBars ????????? (????????? -1 ??? 19 ??????)!\n"));
    ++ErrorCount;
  }

  return ErrorCount;
}

STATIC
UINT32
CheckUefiReservedMemory (
  IN  OC_GLOBAL_CONFIG  *Config
  )
{
  UINT32          ErrorCount;
  UINT32          Index;
  OC_UEFI_CONFIG  *UserUefi;
  CONST CHAR8     *AsciiReservedMemoryType;
  UINT64          ReservedMemoryAddress;
  UINT64          ReservedMemorySize;

  ErrorCount      = 0;
  UserUefi        = &Config->Uefi;

  //
  // Validate ReservedMemory[N].
  //
  for (Index = 0; Index < UserUefi->ReservedMemory.Count; ++Index) {
    AsciiReservedMemoryType = OC_BLOB_GET (&UserUefi->ReservedMemory.Values[Index]->Type);
    ReservedMemoryAddress   = UserUefi->ReservedMemory.Values[Index]->Address;
    ReservedMemorySize      = UserUefi->ReservedMemory.Values[Index]->Size;

    if (!ValidateReservedMemoryType (AsciiReservedMemoryType)) {
      DEBUG ((DEBUG_WARN, "UEFI->ReservedMemory[%u]->????????????!\n", Index));
      ++ErrorCount;
    }

    if (ReservedMemoryAddress % EFI_PAGE_SIZE != 0) {
      DEBUG ((DEBUG_WARN, "UEFI->ReservedMemory[%u]->Address (%Lu) ????????????????????????!\n", Index, ReservedMemoryAddress));
      ++ErrorCount;
    }

    if (ReservedMemorySize == 0ULL) {
      DEBUG ((DEBUG_WARN, "UEFI->ReservedMemory[%u]->Size ?????????0!\n", Index));
      ++ErrorCount;
    } else if (ReservedMemorySize % EFI_PAGE_SIZE != 0) {
      DEBUG ((DEBUG_WARN, "UEFI->ReservedMemory[%u]->Size (%Lu) ????????????????????????!\n", Index, ReservedMemorySize));
      ++ErrorCount;
    }
  }
  //
  // Now overlapping check amongst Address and Size.
  //
  ErrorCount += FindArrayDuplication (
    UserUefi->ReservedMemory.Values,
    UserUefi->ReservedMemory.Count,
    sizeof (UserUefi->ReservedMemory.Values[0]),
    UefiReservedMemoryHasOverlap
    );

  return ErrorCount;
}

UINT32
CheckUefi (
  IN  OC_GLOBAL_CONFIG  *Config
  )
{
  UINT32               ErrorCount;
  UINTN                Index;
  STATIC CONFIG_CHECK  UefiCheckers[] = {
    &CheckUefiAPFS,
    &CheckUefiAppleInput,
    &CheckUefiAudio,
    &CheckUefiDrivers,
    &CheckUefiInput,
    &CheckUefiOutput,
    &CheckUefiQuirks,
    &CheckUefiReservedMemory
  };

  DEBUG ((DEBUG_VERBOSE, "config loaded into %a!\n", __func__));

  ErrorCount = 0;

  for (Index = 0; Index < ARRAY_SIZE (UefiCheckers); ++Index) {
    ErrorCount += UefiCheckers[Index] (Config);
  }

  return ReportError (__func__, ErrorCount);
}
