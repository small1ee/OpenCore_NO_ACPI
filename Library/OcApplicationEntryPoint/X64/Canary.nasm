; @file
; Copyright (C) 2021, ISP RAS. All rights reserved.
;
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
BITS 64

#include <AutoGen.h>
extern ASM_PFX(_ModuleEntryPointReal)

section .data
align 8
global ASM_PFX(__security_cookie)
ASM_PFX(__security_cookie):
global ASM_PFX(__stack_chk_guard)
ASM_PFX(__stack_chk_guard):
  dq 0

section .text
;  BIT4 - Enable BreakPoint as ASSERT. (MdePkg.dec)
%define DEBUG_PROPERTY_ASSERT_BREAKPOINT_ENABLED ((FixedPcdGet8(PcdDebugPropertyMask) & 0x10) != 0)

; #######################################################################
; VOID __stack_chk_fail (VOID)
; #######################################################################
align 8
global ASM_PFX(__stack_chk_fail)
ASM_PFX(__stack_chk_fail):
%if DEBUG_PROPERTY_ASSERT_BREAKPOINT_ENABLED
  int 3
  ret
%else
.back:
  cli
  hlt
  jmp short .back
%endif

; #######################################################################
; VOID
; __security_check_cookie (
;   IN UINTN  Value
;   )
; #######################################################################
align 8
global ASM_PFX(__security_check_cookie)
ASM_PFX(__security_check_cookie):
  cmp  qword [rel ASM_PFX(__security_cookie)], rcx
  jnz  ASM_PFX(__stack_chk_fail)
  ret

; #######################################################################
; EFI_STATUS
; EFIAPI
; _ModuleEntryPoint (
;   IN EFI_HANDLE        ImageHandle,
;   IN EFI_SYSTEM_TABLE  *SystemTable
;   )
; #######################################################################
align 8
global ASM_PFX(_ModuleEntryPoint)
ASM_PFX(_ModuleEntryPoint):
  ; Save ImageHandle and *SystemTable.
  mov r8, rcx
  mov r9, rdx

%if FixedPcdGet8(PcdCanaryAllowRdtscFallback)
  mov r10, rbx        ; RBX is nonvolatile
  mov eax, 1          ; Feature Information
  cpuid               ; result in EAX, EBX, ECX, EDX
  bt ecx, 30          ; check RDRAND feature flag
  jae .noRdRand       ; CF = 0
.retry:
  rdrand rdx
  jae .retry           ; RDRAND bad data (CF = 0), retry until (CF = 1).
  jmp short .done
.noRdRand:
  rdtsc               ; Read time-stamp counter into EDX:EAX.
  shld rdx, rdx, 32
  or rdx, rax
.done:
  mov rbx, r10
%else
.again:
  rdrand rdx
  jae .again           ; RDRAND bad data (CF = 0), retry until (CF = 1).
%endif

  mov [rel ASM_PFX(__security_cookie)], rdx
  ; Restore ImageHandle and *SystemTable.
  mov rcx, r8
  mov rdx, r9
  jmp ASM_PFX(_ModuleEntryPointReal)
