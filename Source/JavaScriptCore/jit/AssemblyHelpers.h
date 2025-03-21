/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#if ENABLE(JIT)

#include "CodeBlock.h"
#include "EntryFrame.h"
#include "FPRInfo.h"
#include "GPRInfo.h"
#include "Heap.h"
#include "InlineCallFrame.h"
#include "JITAllocator.h"
#include "JITCode.h"
#include "MacroAssembler.h"
#include "MarkedSpace.h"
#include "RegisterAtOffsetList.h"
#include "RegisterSet.h"
#include "StackAlignment.h"
#include "TagRegistersMode.h"
#include "TypeofType.h"
#include "VM.h"

namespace JSC {

typedef void (*V_DebugOperation_EPP)(ExecState*, void*, void*);

class AssemblyHelpers : public MacroAssembler {
public:
    AssemblyHelpers(CodeBlock* codeBlock)
        : m_codeBlock(codeBlock)
        , m_baselineCodeBlock(codeBlock ? codeBlock->baselineAlternative() : 0)
    {
        if (m_codeBlock) {
            ASSERT(m_baselineCodeBlock);
            ASSERT(!m_baselineCodeBlock->alternative());
            ASSERT(m_baselineCodeBlock->jitType() == JITType::None || JITCode::isBaselineCode(m_baselineCodeBlock->jitType()));
        }
    }

    CodeBlock* codeBlock() { return m_codeBlock; }
    VM& vm() { return *m_codeBlock->vm(); }
    AssemblerType_T& assembler() { return m_assembler; }

    void checkStackPointerAlignment()
    {
        // This check is both unneeded and harder to write correctly for ARM64
#if !defined(NDEBUG) && !CPU(ARM64)
        Jump stackPointerAligned = branchTestPtr(Zero, stackPointerRegister, TrustedImm32(0xf));
        abortWithReason(AHStackPointerMisaligned);
        stackPointerAligned.link(this);
#endif
    }
    
    template<typename T>
    void storeCell(T cell, Address address)
    {
#if USE(JSVALUE64)
        store64(cell, address);
#else
        store32(cell, address.withOffset(PayloadOffset));
        store32(TrustedImm32(JSValue::CellTag), address.withOffset(TagOffset));
#endif
    }
    
    void loadCell(Address address, GPRReg gpr)
    {
#if USE(JSVALUE64)
        load64(address, gpr);
#else
        load32(address.withOffset(PayloadOffset), gpr);
#endif
    }
    
    void storeValue(JSValueRegs regs, Address address)
    {
#if USE(JSVALUE64)
        store64(regs.gpr(), address);
#else
        store32(regs.payloadGPR(), address.withOffset(PayloadOffset));
        store32(regs.tagGPR(), address.withOffset(TagOffset));
#endif
    }
    
    void storeValue(JSValueRegs regs, BaseIndex address)
    {
#if USE(JSVALUE64)
        store64(regs.gpr(), address);
#else
        store32(regs.payloadGPR(), address.withOffset(PayloadOffset));
        store32(regs.tagGPR(), address.withOffset(TagOffset));
#endif
    }
    
    void storeValue(JSValueRegs regs, void* address)
    {
#if USE(JSVALUE64)
        store64(regs.gpr(), address);
#else
        store32(regs.payloadGPR(), bitwise_cast<void*>(bitwise_cast<uintptr_t>(address) + PayloadOffset));
        store32(regs.tagGPR(), bitwise_cast<void*>(bitwise_cast<uintptr_t>(address) + TagOffset));
#endif
    }
    
    void loadValue(Address address, JSValueRegs regs)
    {
#if USE(JSVALUE64)
        load64(address, regs.gpr());
#else
        if (address.base == regs.payloadGPR()) {
            load32(address.withOffset(TagOffset), regs.tagGPR());
            load32(address.withOffset(PayloadOffset), regs.payloadGPR());
        } else {
            load32(address.withOffset(PayloadOffset), regs.payloadGPR());
            load32(address.withOffset(TagOffset), regs.tagGPR());
        }
#endif
    }
    
    void loadValue(BaseIndex address, JSValueRegs regs)
    {
#if USE(JSVALUE64)
        load64(address, regs.gpr());
#else
        if (address.base == regs.payloadGPR() || address.index == regs.payloadGPR()) {
            // We actually could handle the case where the registers are aliased to both
            // tag and payload, but we don't for now.
            RELEASE_ASSERT(address.base != regs.tagGPR());
            RELEASE_ASSERT(address.index != regs.tagGPR());
            
            load32(address.withOffset(TagOffset), regs.tagGPR());
            load32(address.withOffset(PayloadOffset), regs.payloadGPR());
        } else {
            load32(address.withOffset(PayloadOffset), regs.payloadGPR());
            load32(address.withOffset(TagOffset), regs.tagGPR());
        }
#endif
    }

    void loadValue(void* address, JSValueRegs regs)
    {
#if USE(JSVALUE64)
        load64(address, regs.gpr());
#else
        move(TrustedImmPtr(address), regs.payloadGPR());
        loadValue(Address(regs.payloadGPR()), regs);
#endif
    }
    
    // Note that this clobbers offset.
    void loadProperty(GPRReg object, GPRReg offset, JSValueRegs result);

    void moveValueRegs(JSValueRegs srcRegs, JSValueRegs destRegs)
    {
#if USE(JSVALUE32_64)
        if (destRegs.tagGPR() == srcRegs.payloadGPR()) {
            if (destRegs.payloadGPR() == srcRegs.tagGPR()) {
                swap(srcRegs.payloadGPR(), srcRegs.tagGPR());
                return;
            }
            move(srcRegs.payloadGPR(), destRegs.payloadGPR());
            move(srcRegs.tagGPR(), destRegs.tagGPR());
            return;
        }
        move(srcRegs.tagGPR(), destRegs.tagGPR());
        move(srcRegs.payloadGPR(), destRegs.payloadGPR());
#else
        move(srcRegs.gpr(), destRegs.gpr());
#endif
    }

    void moveValue(JSValue value, JSValueRegs regs)
    {
#if USE(JSVALUE64)
        move(Imm64(JSValue::encode(value)), regs.gpr());
#else
        move(Imm32(value.tag()), regs.tagGPR());
        move(Imm32(value.payload()), regs.payloadGPR());
#endif
    }

    void moveTrustedValue(JSValue value, JSValueRegs regs)
    {
#if USE(JSVALUE64)
        move(TrustedImm64(JSValue::encode(value)), regs.gpr());
#else
        move(TrustedImm32(value.tag()), regs.tagGPR());
        move(TrustedImm32(value.payload()), regs.payloadGPR());
#endif
    }
    
    void storeTrustedValue(JSValue value, Address address)
    {
#if USE(JSVALUE64)
        store64(TrustedImm64(JSValue::encode(value)), address);
#else
        store32(TrustedImm32(value.tag()), address.withOffset(TagOffset));
        store32(TrustedImm32(value.payload()), address.withOffset(PayloadOffset));
#endif
    }

    void storeTrustedValue(JSValue value, BaseIndex address)
    {
#if USE(JSVALUE64)
        store64(TrustedImm64(JSValue::encode(value)), address);
#else
        store32(TrustedImm32(value.tag()), address.withOffset(TagOffset));
        store32(TrustedImm32(value.payload()), address.withOffset(PayloadOffset));
#endif
    }
    
    Address addressFor(const RegisterAtOffset& entry)
    {
        return Address(GPRInfo::callFrameRegister, entry.offset());
    }
    
    void emitSave(const RegisterAtOffsetList& list)
    {
        for (const RegisterAtOffset& entry : list) {
            if (entry.reg().isGPR())
                storePtr(entry.reg().gpr(), addressFor(entry));
            else
                storeDouble(entry.reg().fpr(), addressFor(entry));
        }
    }
    
    void emitRestore(const RegisterAtOffsetList& list)
    {
        for (const RegisterAtOffset& entry : list) {
            if (entry.reg().isGPR())
                loadPtr(addressFor(entry), entry.reg().gpr());
            else
                loadDouble(addressFor(entry), entry.reg().fpr());
        }
    }

    void emitSaveCalleeSavesFor(CodeBlock* codeBlock)
    {
        ASSERT(codeBlock);

        const RegisterAtOffsetList* calleeSaves = codeBlock->calleeSaveRegisters();
        RegisterSet dontSaveRegisters = RegisterSet(RegisterSet::stackRegisters(), RegisterSet::allFPRs());
        unsigned registerCount = calleeSaves->size();

        for (unsigned i = 0; i < registerCount; i++) {
            RegisterAtOffset entry = calleeSaves->at(i);
            if (dontSaveRegisters.get(entry.reg()))
                continue;
            storePtr(entry.reg().gpr(), Address(framePointerRegister, entry.offset()));
        }
    }
    
    enum RestoreTagRegisterMode { UseExistingTagRegisterContents, CopyBaselineCalleeSavedRegistersFromBaseFrame };

    void emitSaveOrCopyCalleeSavesFor(CodeBlock* codeBlock, VirtualRegister offsetVirtualRegister, RestoreTagRegisterMode tagRegisterMode, GPRReg temp)
    {
        ASSERT(codeBlock);
        
        const RegisterAtOffsetList* calleeSaves = codeBlock->calleeSaveRegisters();
        RegisterSet dontSaveRegisters = RegisterSet(RegisterSet::stackRegisters(), RegisterSet::allFPRs());
        unsigned registerCount = calleeSaves->size();

#if USE(JSVALUE64)
        RegisterSet baselineCalleeSaves = RegisterSet::llintBaselineCalleeSaveRegisters();
#endif
        
        for (unsigned i = 0; i < registerCount; i++) {
            RegisterAtOffset entry = calleeSaves->at(i);
            if (dontSaveRegisters.get(entry.reg()))
                continue;
            
            GPRReg registerToWrite;
            
#if USE(JSVALUE32_64)
            UNUSED_PARAM(tagRegisterMode);
            UNUSED_PARAM(temp);
#else
            if (tagRegisterMode == CopyBaselineCalleeSavedRegistersFromBaseFrame && baselineCalleeSaves.get(entry.reg())) {
                registerToWrite = temp;
                loadPtr(AssemblyHelpers::Address(GPRInfo::callFrameRegister, entry.offset()), registerToWrite);
            } else
#endif
                registerToWrite = entry.reg().gpr();

            storePtr(registerToWrite, Address(framePointerRegister, offsetVirtualRegister.offsetInBytes() + entry.offset()));
        }
    }
    
    void emitRestoreCalleeSavesFor(CodeBlock* codeBlock)
    {
        ASSERT(codeBlock);

        const RegisterAtOffsetList* calleeSaves = codeBlock->calleeSaveRegisters();
        RegisterSet dontRestoreRegisters = RegisterSet(RegisterSet::stackRegisters(), RegisterSet::allFPRs());
        unsigned registerCount = calleeSaves->size();
        
        for (unsigned i = 0; i < registerCount; i++) {
            RegisterAtOffset entry = calleeSaves->at(i);
            if (dontRestoreRegisters.get(entry.reg()))
                continue;
            loadPtr(Address(framePointerRegister, entry.offset()), entry.reg().gpr());
        }
    }

    void emitSaveCalleeSaves()
    {
        emitSaveCalleeSavesFor(codeBlock());
    }

    void emitSaveThenMaterializeTagRegisters()
    {
#if USE(JSVALUE64)
#if CPU(ARM64)
        pushPair(GPRInfo::tagTypeNumberRegister, GPRInfo::tagMaskRegister);
#else
        push(GPRInfo::tagTypeNumberRegister);
        push(GPRInfo::tagMaskRegister);
#endif
        emitMaterializeTagCheckRegisters();
#endif
    }
    
    void emitRestoreCalleeSaves()
    {
        emitRestoreCalleeSavesFor(codeBlock());
    }

    void emitRestoreSavedTagRegisters()
    {
#if USE(JSVALUE64)
#if CPU(ARM64)
        popPair(GPRInfo::tagTypeNumberRegister, GPRInfo::tagMaskRegister);
#else
        pop(GPRInfo::tagMaskRegister);
        pop(GPRInfo::tagTypeNumberRegister);
#endif
#endif
    }

    // If you use this, be aware that vmGPR will get trashed.
    void copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(GPRReg vmGPR)
    {
#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
        loadPtr(Address(vmGPR, VM::topEntryFrameOffset()), vmGPR);
        copyCalleeSavesToEntryFrameCalleeSavesBufferImpl(vmGPR);
#else
        UNUSED_PARAM(vmGPR);
#endif
    }

    void copyCalleeSavesToEntryFrameCalleeSavesBuffer(EntryFrame*& topEntryFrame)
    {
#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
        const TempRegisterSet& usedRegisters = { RegisterSet::stubUnavailableRegisters() };
        GPRReg temp1 = usedRegisters.getFreeGPR(0);
        loadPtr(&topEntryFrame, temp1);
        copyCalleeSavesToEntryFrameCalleeSavesBufferImpl(temp1);
#else
        UNUSED_PARAM(topEntryFrame);
#endif
    }
    
    void copyCalleeSavesToEntryFrameCalleeSavesBuffer(GPRReg topEntryFrame)
    {
#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
        copyCalleeSavesToEntryFrameCalleeSavesBufferImpl(topEntryFrame);
#else
        UNUSED_PARAM(topEntryFrame);
#endif
    }

    void restoreCalleeSavesFromEntryFrameCalleeSavesBuffer(EntryFrame*&);

    void copyCalleeSavesFromFrameOrRegisterToEntryFrameCalleeSavesBuffer(EntryFrame*& topEntryFrame, const TempRegisterSet& usedRegisters = { RegisterSet::stubUnavailableRegisters() })
    {
#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
        GPRReg temp1 = usedRegisters.getFreeGPR(0);
        GPRReg temp2 = usedRegisters.getFreeGPR(1);
        FPRReg fpTemp = usedRegisters.getFreeFPR();
        ASSERT(temp2 != InvalidGPRReg);

        ASSERT(codeBlock());

        // Copy saved calleeSaves on stack or unsaved calleeSaves in register to vm calleeSave buffer
        loadPtr(&topEntryFrame, temp1);
        addPtr(TrustedImm32(EntryFrame::calleeSaveRegistersBufferOffset()), temp1);

        RegisterAtOffsetList* allCalleeSaves = RegisterSet::vmCalleeSaveRegisterOffsets();
        const RegisterAtOffsetList* currentCalleeSaves = codeBlock()->calleeSaveRegisters();
        RegisterSet dontCopyRegisters = RegisterSet::stackRegisters();
        unsigned registerCount = allCalleeSaves->size();

        for (unsigned i = 0; i < registerCount; i++) {
            RegisterAtOffset entry = allCalleeSaves->at(i);
            if (dontCopyRegisters.get(entry.reg()))
                continue;
            RegisterAtOffset* currentFrameEntry = currentCalleeSaves->find(entry.reg());

            if (entry.reg().isGPR()) {
                GPRReg regToStore;
                if (currentFrameEntry) {
                    // Load calleeSave from stack into temp register
                    regToStore = temp2;
                    loadPtr(Address(framePointerRegister, currentFrameEntry->offset()), regToStore);
                } else
                    // Just store callee save directly
                    regToStore = entry.reg().gpr();

                storePtr(regToStore, Address(temp1, entry.offset()));
            } else {
                FPRReg fpRegToStore;
                if (currentFrameEntry) {
                    // Load calleeSave from stack into temp register
                    fpRegToStore = fpTemp;
                    loadDouble(Address(framePointerRegister, currentFrameEntry->offset()), fpRegToStore);
                } else
                    // Just store callee save directly
                    fpRegToStore = entry.reg().fpr();

                storeDouble(fpRegToStore, Address(temp1, entry.offset()));
            }
        }
#else
        UNUSED_PARAM(topEntryFrame);
        UNUSED_PARAM(usedRegisters);
#endif
    }

    void emitMaterializeTagCheckRegisters()
    {
#if USE(JSVALUE64)
        move(MacroAssembler::TrustedImm64(TagTypeNumber), GPRInfo::tagTypeNumberRegister);
        orPtr(MacroAssembler::TrustedImm32(TagBitTypeOther), GPRInfo::tagTypeNumberRegister, GPRInfo::tagMaskRegister);
#endif
    }

    void clearStackFrame(GPRReg currentTop, GPRReg newTop, GPRReg temp, unsigned frameSize)
    {
        ASSERT(frameSize % stackAlignmentBytes() == 0);
        if (frameSize <= 128) {
            for (unsigned offset = 0; offset < frameSize; offset += sizeof(CPURegister))
                storePtr(TrustedImm32(0), Address(currentTop, -8 - offset));
        } else {
            constexpr unsigned storeBytesPerIteration = stackAlignmentBytes();
            constexpr unsigned storesPerIteration = storeBytesPerIteration / sizeof(CPURegister);

            move(currentTop, temp);
            Label zeroLoop = label();
            subPtr(TrustedImm32(storeBytesPerIteration), temp);
#if CPU(ARM64)
            static_assert(storesPerIteration == 2, "clearStackFrame() for ARM64 assumes stack is 16 byte aligned");
            storePair64(ARM64Registers::zr, ARM64Registers::zr, temp);
#else
            for (unsigned i = storesPerIteration; i-- != 0;)
                storePtr(TrustedImm32(0), Address(temp, sizeof(CPURegister) * i));
#endif
            branchPtr(NotEqual, temp, newTop).linkTo(zeroLoop, this);
        }
    }

#if CPU(X86_64) || CPU(X86)
    static constexpr size_t prologueStackPointerDelta()
    {
        // Prologue only saves the framePointerRegister
        return sizeof(void*);
    }

    void emitFunctionPrologue()
    {
        push(framePointerRegister);
        move(stackPointerRegister, framePointerRegister);
    }

    void emitFunctionEpilogueWithEmptyFrame()
    {
        pop(framePointerRegister);
    }

    void emitFunctionEpilogue()
    {
        move(framePointerRegister, stackPointerRegister);
        pop(framePointerRegister);
    }

    void preserveReturnAddressAfterCall(GPRReg reg)
    {
        pop(reg);
    }

    void restoreReturnAddressBeforeReturn(GPRReg reg)
    {
        push(reg);
    }

    void restoreReturnAddressBeforeReturn(Address address)
    {
        push(address);
    }
#endif // CPU(X86_64) || CPU(X86)

#if CPU(ARM_THUMB2) || CPU(ARM64)
    static constexpr size_t prologueStackPointerDelta()
    {
        // Prologue saves the framePointerRegister and linkRegister
        return 2 * sizeof(void*);
    }

    void emitFunctionPrologue()
    {
        tagReturnAddress();
        pushPair(framePointerRegister, linkRegister);
        move(stackPointerRegister, framePointerRegister);
    }

    void emitFunctionEpilogueWithEmptyFrame()
    {
        popPair(framePointerRegister, linkRegister);
    }

    void emitFunctionEpilogue()
    {
        move(framePointerRegister, stackPointerRegister);
        emitFunctionEpilogueWithEmptyFrame();
    }

    ALWAYS_INLINE void preserveReturnAddressAfterCall(RegisterID reg)
    {
        move(linkRegister, reg);
    }

    ALWAYS_INLINE void restoreReturnAddressBeforeReturn(RegisterID reg)
    {
        move(reg, linkRegister);
    }

    ALWAYS_INLINE void restoreReturnAddressBeforeReturn(Address address)
    {
        loadPtr(address, linkRegister);
    }
#endif

#if CPU(MIPS)
    static constexpr size_t prologueStackPointerDelta()
    {
        // Prologue saves the framePointerRegister and returnAddressRegister
        return 2 * sizeof(void*);
    }

    void emitFunctionPrologue()
    {
        pushPair(framePointerRegister, returnAddressRegister);
        move(stackPointerRegister, framePointerRegister);
    }

    void emitFunctionEpilogueWithEmptyFrame()
    {
        popPair(framePointerRegister, returnAddressRegister);
    }

    void emitFunctionEpilogue()
    {
        move(framePointerRegister, stackPointerRegister);
        emitFunctionEpilogueWithEmptyFrame();
    }

    ALWAYS_INLINE void preserveReturnAddressAfterCall(RegisterID reg)
    {
        move(returnAddressRegister, reg);
    }

    ALWAYS_INLINE void restoreReturnAddressBeforeReturn(RegisterID reg)
    {
        move(reg, returnAddressRegister);
    }

    ALWAYS_INLINE void restoreReturnAddressBeforeReturn(Address address)
    {
        loadPtr(address, returnAddressRegister);
    }
#endif

    void emitGetFromCallFrameHeaderPtr(int entry, GPRReg to, GPRReg from = GPRInfo::callFrameRegister)
    {
        loadPtr(Address(from, entry * sizeof(Register)), to);
    }
    void emitGetFromCallFrameHeader32(int entry, GPRReg to, GPRReg from = GPRInfo::callFrameRegister)
    {
        load32(Address(from, entry * sizeof(Register)), to);
    }
#if USE(JSVALUE64)
    void emitGetFromCallFrameHeader64(int entry, GPRReg to, GPRReg from = GPRInfo::callFrameRegister)
    {
        load64(Address(from, entry * sizeof(Register)), to);
    }
#endif // USE(JSVALUE64)
    void emitPutToCallFrameHeader(GPRReg from, int entry)
    {
        storePtr(from, Address(GPRInfo::callFrameRegister, entry * sizeof(Register)));
    }

    void emitPutToCallFrameHeader(void* value, int entry)
    {
        storePtr(TrustedImmPtr(value), Address(GPRInfo::callFrameRegister, entry * sizeof(Register)));
    }

    void emitGetCallerFrameFromCallFrameHeaderPtr(RegisterID to)
    {
        loadPtr(Address(GPRInfo::callFrameRegister, CallFrame::callerFrameOffset()), to);
    }
    void emitPutCallerFrameToCallFrameHeader(RegisterID from)
    {
        storePtr(from, Address(GPRInfo::callFrameRegister, CallFrame::callerFrameOffset()));
    }

    void emitPutReturnPCToCallFrameHeader(RegisterID from)
    {
        storePtr(from, Address(GPRInfo::callFrameRegister, CallFrame::returnPCOffset()));
    }
    void emitPutReturnPCToCallFrameHeader(TrustedImmPtr from)
    {
        storePtr(from, Address(GPRInfo::callFrameRegister, CallFrame::returnPCOffset()));
    }

    // emitPutToCallFrameHeaderBeforePrologue() and related are used to access callee frame header
    // fields before the code from emitFunctionPrologue() has executed.
    // First, the access is via the stack pointer. Second, the address calculation must also take
    // into account that the stack pointer may not have been adjusted down for the return PC and/or
    // caller's frame pointer. On some platforms, the callee is responsible for pushing the
    // "link register" containing the return address in the function prologue.
#if USE(JSVALUE64)
    void emitPutToCallFrameHeaderBeforePrologue(GPRReg from, int entry)
    {
        storePtr(from, Address(stackPointerRegister, entry * static_cast<ptrdiff_t>(sizeof(Register)) - prologueStackPointerDelta()));
    }
#else
    void emitPutPayloadToCallFrameHeaderBeforePrologue(GPRReg from, int entry)
    {
        storePtr(from, Address(stackPointerRegister, entry * static_cast<ptrdiff_t>(sizeof(Register)) - prologueStackPointerDelta() + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
    }

    void emitPutTagToCallFrameHeaderBeforePrologue(TrustedImm32 tag, int entry)
    {
        storePtr(tag, Address(stackPointerRegister, entry * static_cast<ptrdiff_t>(sizeof(Register)) - prologueStackPointerDelta() + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
    }
#endif
    
    JumpList branchIfNotEqual(JSValueRegs regs, JSValue value)
    {
#if USE(JSVALUE64)
        return branch64(NotEqual, regs.gpr(), TrustedImm64(JSValue::encode(value)));
#else
        JumpList result;
        result.append(branch32(NotEqual, regs.tagGPR(), TrustedImm32(value.tag())));
        if (value.isEmpty() || value.isUndefinedOrNull())
            return result; // These don't have anything interesting in the payload.
        result.append(branch32(NotEqual, regs.payloadGPR(), TrustedImm32(value.payload())));
        return result;
#endif
    }
    
    Jump branchIfEqual(JSValueRegs regs, JSValue value)
    {
#if USE(JSVALUE64)
        return branch64(Equal, regs.gpr(), TrustedImm64(JSValue::encode(value)));
#else
        Jump notEqual;
        // These don't have anything interesting in the payload.
        if (!value.isEmpty() && !value.isUndefinedOrNull())
            notEqual = branch32(NotEqual, regs.payloadGPR(), TrustedImm32(value.payload()));
        Jump result = branch32(Equal, regs.tagGPR(), TrustedImm32(value.tag()));
        if (notEqual.isSet())
            notEqual.link(this);
        return result;
#endif
    }

    Jump branchIfNotCell(GPRReg reg, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        if (mode == HaveTagRegisters)
            return branchTest64(NonZero, reg, GPRInfo::tagMaskRegister);
        return branchTest64(NonZero, reg, TrustedImm64(TagMask));
#else
        UNUSED_PARAM(mode);
        return branch32(MacroAssembler::NotEqual, reg, TrustedImm32(JSValue::CellTag));
#endif
    }

    Jump branchIfNotCell(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        return branchIfNotCell(regs.gpr(), mode);
#else
        return branchIfNotCell(regs.tagGPR(), mode);
#endif
    }
    
    Jump branchIfCell(GPRReg reg, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        if (mode == HaveTagRegisters)
            return branchTest64(Zero, reg, GPRInfo::tagMaskRegister);
        return branchTest64(Zero, reg, TrustedImm64(TagMask));
#else
        UNUSED_PARAM(mode);
        return branch32(MacroAssembler::Equal, reg, TrustedImm32(JSValue::CellTag));
#endif
    }
    Jump branchIfCell(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        return branchIfCell(regs.gpr(), mode);
#else
        return branchIfCell(regs.tagGPR(), mode);
#endif
    }
    
    Jump branchIfOther(JSValueRegs regs, GPRReg tempGPR)
    {
#if USE(JSVALUE64)
        move(regs.gpr(), tempGPR);
        and64(TrustedImm32(~TagBitUndefined), tempGPR);
        return branch64(Equal, tempGPR, TrustedImm64(ValueNull));
#else
        or32(TrustedImm32(1), regs.tagGPR(), tempGPR);
        return branch32(Equal, tempGPR, TrustedImm32(JSValue::NullTag));
#endif
    }
    
    Jump branchIfNotOther(JSValueRegs regs, GPRReg tempGPR)
    {
#if USE(JSVALUE64)
        move(regs.gpr(), tempGPR);
        and64(TrustedImm32(~TagBitUndefined), tempGPR);
        return branch64(NotEqual, tempGPR, TrustedImm64(ValueNull));
#else
        or32(TrustedImm32(1), regs.tagGPR(), tempGPR);
        return branch32(NotEqual, tempGPR, TrustedImm32(JSValue::NullTag));
#endif
    }
    
    Jump branchIfInt32(GPRReg gpr, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        if (mode == HaveTagRegisters)
            return branch64(AboveOrEqual, gpr, GPRInfo::tagTypeNumberRegister);
        return branch64(AboveOrEqual, gpr, TrustedImm64(TagTypeNumber));
#else
        UNUSED_PARAM(mode);
        return branch32(Equal, gpr, TrustedImm32(JSValue::Int32Tag));
#endif
    }

    Jump branchIfInt32(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        return branchIfInt32(regs.gpr(), mode);
#else
        return branchIfInt32(regs.tagGPR(), mode);
#endif
    }

    Jump branchIfNotInt32(GPRReg gpr, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        if (mode == HaveTagRegisters)
            return branch64(Below, gpr, GPRInfo::tagTypeNumberRegister);
        return branch64(Below, gpr, TrustedImm64(TagTypeNumber));
#else
        UNUSED_PARAM(mode);
        return branch32(NotEqual, gpr, TrustedImm32(JSValue::Int32Tag));
#endif
    }

    Jump branchIfNotInt32(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        return branchIfNotInt32(regs.gpr(), mode);
#else
        return branchIfNotInt32(regs.tagGPR(), mode);
#endif
    }

    // Note that the tempGPR is not used in 64-bit mode.
    Jump branchIfNumber(JSValueRegs regs, GPRReg tempGPR, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        UNUSED_PARAM(tempGPR);
        return branchIfNumber(regs.gpr(), mode);
#else
        UNUSED_PARAM(mode);
        ASSERT(tempGPR != InvalidGPRReg);
        add32(TrustedImm32(1), regs.tagGPR(), tempGPR);
        return branch32(Below, tempGPR, TrustedImm32(JSValue::LowestTag + 1));
#endif
    }

#if USE(JSVALUE64)
    Jump branchIfNumber(GPRReg gpr, TagRegistersMode mode = HaveTagRegisters)
    {
        if (mode == HaveTagRegisters)
            return branchTest64(NonZero, gpr, GPRInfo::tagTypeNumberRegister);
        return branchTest64(NonZero, gpr, TrustedImm64(TagTypeNumber));
    }
#endif
    
    // Note that the tempGPR is not used in 64-bit mode.
    Jump branchIfNotNumber(JSValueRegs regs, GPRReg tempGPR, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        UNUSED_PARAM(tempGPR);
        return branchIfNotNumber(regs.gpr(), mode);
#else
        UNUSED_PARAM(mode);
        add32(TrustedImm32(1), regs.tagGPR(), tempGPR);
        return branch32(AboveOrEqual, tempGPR, TrustedImm32(JSValue::LowestTag + 1));
#endif
    }

#if USE(JSVALUE64)
    Jump branchIfNotNumber(GPRReg gpr, TagRegistersMode mode = HaveTagRegisters)
    {
        if (mode == HaveTagRegisters)
            return branchTest64(Zero, gpr, GPRInfo::tagTypeNumberRegister);
        return branchTest64(Zero, gpr, TrustedImm64(TagTypeNumber));
    }
#endif

    Jump branchIfNotDoubleKnownNotInt32(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        if (mode == HaveTagRegisters)
            return branchTest64(Zero, regs.gpr(), GPRInfo::tagTypeNumberRegister);
        return branchTest64(Zero, regs.gpr(), TrustedImm64(TagTypeNumber));
#else
        UNUSED_PARAM(mode);
        return branch32(AboveOrEqual, regs.tagGPR(), TrustedImm32(JSValue::LowestTag));
#endif
    }

    // Note that the tempGPR is not used in 32-bit mode.
    Jump branchIfBoolean(GPRReg gpr, GPRReg tempGPR)
    {
#if USE(JSVALUE64)
        ASSERT(tempGPR != InvalidGPRReg);
        move(gpr, tempGPR);
        xor64(TrustedImm32(static_cast<int32_t>(ValueFalse)), tempGPR);
        return branchTest64(Zero, tempGPR, TrustedImm32(static_cast<int32_t>(~1)));
#else
        UNUSED_PARAM(tempGPR);
        return branch32(Equal, gpr, TrustedImm32(JSValue::BooleanTag));
#endif
    }

    // Note that the tempGPR is not used in 32-bit mode.
    Jump branchIfBoolean(JSValueRegs regs, GPRReg tempGPR)
    {
#if USE(JSVALUE64)
        return branchIfBoolean(regs.gpr(), tempGPR);
#else
        return branchIfBoolean(regs.tagGPR(), tempGPR);
#endif
    }
    
    // Note that the tempGPR is not used in 32-bit mode.
    Jump branchIfNotBoolean(GPRReg gpr, GPRReg tempGPR)
    {
#if USE(JSVALUE64)
        ASSERT(tempGPR != InvalidGPRReg);
        move(gpr, tempGPR);
        xor64(TrustedImm32(static_cast<int32_t>(ValueFalse)), tempGPR);
        return branchTest64(NonZero, tempGPR, TrustedImm32(static_cast<int32_t>(~1)));
#else
        UNUSED_PARAM(tempGPR);
        return branch32(NotEqual, gpr, TrustedImm32(JSValue::BooleanTag));
#endif
    }

    // Note that the tempGPR is not used in 32-bit mode.
    Jump branchIfNotBoolean(JSValueRegs regs, GPRReg tempGPR)
    {
#if USE(JSVALUE64)
        return branchIfNotBoolean(regs.gpr(), tempGPR);
#else
        return branchIfNotBoolean(regs.tagGPR(), tempGPR);
#endif
    }
    
    Jump branchIfObject(GPRReg cellGPR)
    {
        return branch8(
            AboveOrEqual, Address(cellGPR, JSCell::typeInfoTypeOffset()), TrustedImm32(ObjectType));
    }
    
    Jump branchIfNotObject(GPRReg cellGPR)
    {
        return branch8(
            Below, Address(cellGPR, JSCell::typeInfoTypeOffset()), TrustedImm32(ObjectType));
    }
    
    Jump branchIfType(GPRReg cellGPR, JSType type)
    {
        return branch8(Equal, Address(cellGPR, JSCell::typeInfoTypeOffset()), TrustedImm32(type));
    }
    
    Jump branchIfNotType(GPRReg cellGPR, JSType type)
    {
        return branch8(NotEqual, Address(cellGPR, JSCell::typeInfoTypeOffset()), TrustedImm32(type));
    }
    
    Jump branchIfString(GPRReg cellGPR) { return branchIfType(cellGPR, StringType); }
    Jump branchIfNotString(GPRReg cellGPR) { return branchIfNotType(cellGPR, StringType); }
    Jump branchIfSymbol(GPRReg cellGPR) { return branchIfType(cellGPR, SymbolType); }
    Jump branchIfNotSymbol(GPRReg cellGPR) { return branchIfNotType(cellGPR, SymbolType); }
    Jump branchIfBigInt(GPRReg cellGPR) { return branchIfType(cellGPR, BigIntType); }
    Jump branchIfNotBigInt(GPRReg cellGPR) { return branchIfNotType(cellGPR, BigIntType); }
    Jump branchIfFunction(GPRReg cellGPR) { return branchIfType(cellGPR, JSFunctionType); }
    Jump branchIfNotFunction(GPRReg cellGPR) { return branchIfNotType(cellGPR, JSFunctionType); }
    
    Jump branchIfEmpty(GPRReg gpr)
    {
#if USE(JSVALUE64)
        return branchTest64(Zero, gpr);
#else
        return branch32(Equal, gpr, TrustedImm32(JSValue::EmptyValueTag));
#endif
    }

    Jump branchIfEmpty(JSValueRegs regs)
    {
#if USE(JSVALUE64)
        return branchIfEmpty(regs.gpr());
#else
        return branchIfEmpty(regs.tagGPR());
#endif
    }

    Jump branchIfNotEmpty(GPRReg gpr)
    {
#if USE(JSVALUE64)
        return branchTest64(NonZero, gpr);
#else
        return branch32(NotEqual, gpr, TrustedImm32(JSValue::EmptyValueTag));
#endif
    }

    Jump branchIfNotEmpty(JSValueRegs regs)
    {
#if USE(JSVALUE64)
        return branchIfNotEmpty(regs.gpr());
#else
        return branchIfNotEmpty(regs.tagGPR());
#endif
    }

    // Note that this function does not respect MasqueradesAsUndefined.
    Jump branchIfUndefined(GPRReg gpr)
    {
#if USE(JSVALUE64)
        return branch64(Equal, gpr, TrustedImm64(JSValue::encode(jsUndefined())));
#else
        return branch32(Equal, gpr, TrustedImm32(JSValue::UndefinedTag));
#endif
    }

    // Note that this function does not respect MasqueradesAsUndefined.
    Jump branchIfUndefined(JSValueRegs regs)
    {
#if USE(JSVALUE64)
        return branchIfUndefined(regs.gpr());
#else
        return branchIfUndefined(regs.tagGPR());
#endif
    }

    // Note that this function does not respect MasqueradesAsUndefined.
    Jump branchIfNotUndefined(GPRReg gpr)
    {
#if USE(JSVALUE64)
        return branch64(NotEqual, gpr, TrustedImm64(JSValue::encode(jsUndefined())));
#else
        return branch32(NotEqual, gpr, TrustedImm32(JSValue::UndefinedTag));
#endif
    }

    // Note that this function does not respect MasqueradesAsUndefined.
    Jump branchIfNotUndefined(JSValueRegs regs)
    {
#if USE(JSVALUE64)
        return branchIfNotUndefined(regs.gpr());
#else
        return branchIfNotUndefined(regs.tagGPR());
#endif
    }

    Jump branchIfNull(GPRReg gpr)
    {
#if USE(JSVALUE64)
        return branch64(Equal, gpr, TrustedImm64(JSValue::encode(jsNull())));
#else
        return branch32(Equal, gpr, TrustedImm32(JSValue::NullTag));
#endif
    }

    Jump branchIfNull(JSValueRegs regs)
    {
#if USE(JSVALUE64)
        return branchIfNull(regs.gpr());
#else
        return branchIfNull(regs.tagGPR());
#endif
    }

    Jump branchIfNotNull(GPRReg gpr)
    {
#if USE(JSVALUE64)
        return branch64(NotEqual, gpr, TrustedImm64(JSValue::encode(jsNull())));
#else
        return branch32(NotEqual, gpr, TrustedImm32(JSValue::NullTag));
#endif
    }

    Jump branchIfNotNull(JSValueRegs regs)
    {
#if USE(JSVALUE64)
        return branchIfNotNull(regs.gpr());
#else
        return branchIfNotNull(regs.tagGPR());
#endif
    }

    template<typename T>
    Jump branchStructure(RelationalCondition condition, T leftHandSide, Structure* structure)
    {
#if USE(JSVALUE64)
        return branch32(condition, leftHandSide, TrustedImm32(structure->id()));
#else
        return branchPtr(condition, leftHandSide, TrustedImmPtr(structure));
#endif
    }

    Jump branchIfFastTypedArray(GPRReg baseGPR);
    Jump branchIfNotFastTypedArray(GPRReg baseGPR);

    Jump branchIfNaN(FPRReg fpr)
    {
        return branchDouble(DoubleNotEqualOrUnordered, fpr, fpr);
    }

    Jump branchIfNotNaN(FPRReg fpr)
    {
        return branchDouble(DoubleEqual, fpr, fpr);
    }

    Jump branchIfRopeStringImpl(GPRReg stringImplGPR)
    {
        return branchTestPtr(NonZero, stringImplGPR, TrustedImm32(JSString::isRopeInPointer));
    }

    Jump branchIfNotRopeStringImpl(GPRReg stringImplGPR)
    {
        return branchTestPtr(Zero, stringImplGPR, TrustedImm32(JSString::isRopeInPointer));
    }

    static Address addressForByteOffset(ptrdiff_t byteOffset)
    {
        return Address(GPRInfo::callFrameRegister, byteOffset);
    }
    static Address addressFor(VirtualRegister virtualRegister, GPRReg baseReg)
    {
        ASSERT(virtualRegister.isValid());
        return Address(baseReg, virtualRegister.offset() * sizeof(Register));
    }
    static Address addressFor(VirtualRegister virtualRegister)
    {
        // NB. It's tempting on some architectures to sometimes use an offset from the stack
        // register because for some offsets that will encode to a smaller instruction. But we
        // cannot do this. We use this in places where the stack pointer has been moved to some
        // unpredictable location.
        ASSERT(virtualRegister.isValid());
        return Address(GPRInfo::callFrameRegister, virtualRegister.offset() * sizeof(Register));
    }
    static Address addressFor(int operand)
    {
        return addressFor(static_cast<VirtualRegister>(operand));
    }

    static Address tagFor(VirtualRegister virtualRegister, GPRReg baseGPR)
    {
        ASSERT(virtualRegister.isValid());
        return Address(baseGPR, virtualRegister.offset() * sizeof(Register) + TagOffset);
    }
    static Address tagFor(VirtualRegister virtualRegister)
    {
        ASSERT(virtualRegister.isValid());
        return Address(GPRInfo::callFrameRegister, virtualRegister.offset() * sizeof(Register) + TagOffset);
    }
    static Address tagFor(int operand)
    {
        return tagFor(static_cast<VirtualRegister>(operand));
    }

    static Address payloadFor(VirtualRegister virtualRegister, GPRReg baseGPR)
    {
        ASSERT(virtualRegister.isValid());
        return Address(baseGPR, virtualRegister.offset() * sizeof(Register) + PayloadOffset);
    }
    static Address payloadFor(VirtualRegister virtualRegister)
    {
        ASSERT(virtualRegister.isValid());
        return Address(GPRInfo::callFrameRegister, virtualRegister.offset() * sizeof(Register) + PayloadOffset);
    }
    static Address payloadFor(int operand)
    {
        return payloadFor(static_cast<VirtualRegister>(operand));
    }

    // Access to our fixed callee CallFrame.
    static Address calleeFrameSlot(int slot)
    {
        ASSERT(slot >= CallerFrameAndPC::sizeInRegisters);
        return Address(stackPointerRegister, sizeof(Register) * (slot - CallerFrameAndPC::sizeInRegisters));
    }

    // Access to our fixed callee CallFrame.
    static Address calleeArgumentSlot(int argument)
    {
        return calleeFrameSlot(virtualRegisterForArgument(argument).offset());
    }

    static Address calleeFrameTagSlot(int slot)
    {
        return calleeFrameSlot(slot).withOffset(TagOffset);
    }

    static Address calleeFramePayloadSlot(int slot)
    {
        return calleeFrameSlot(slot).withOffset(PayloadOffset);
    }

    static Address calleeArgumentTagSlot(int argument)
    {
        return calleeArgumentSlot(argument).withOffset(TagOffset);
    }

    static Address calleeArgumentPayloadSlot(int argument)
    {
        return calleeArgumentSlot(argument).withOffset(PayloadOffset);
    }

    static Address calleeFrameCallerFrame()
    {
        return calleeFrameSlot(0).withOffset(CallFrame::callerFrameOffset());
    }

    static GPRReg selectScratchGPR(RegisterSet preserved)
    {
        GPRReg registers[] = {
            GPRInfo::regT0,
            GPRInfo::regT1,
            GPRInfo::regT2,
            GPRInfo::regT3,
            GPRInfo::regT4,
            GPRInfo::regT5,
        };

        for (GPRReg reg : registers) {
            if (!preserved.contains(reg))
                return reg;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }

    template<typename... Regs>
    static GPRReg selectScratchGPR(Regs... args)
    {
        RegisterSet set;
        constructRegisterSet(set, args...);
        return selectScratchGPR(set);
    }

    static void constructRegisterSet(RegisterSet&)
    {
    }

    template<typename... Regs>
    static void constructRegisterSet(RegisterSet& set, JSValueRegs regs, Regs... args)
    {
        if (regs.tagGPR() != InvalidGPRReg)
            set.set(regs.tagGPR());
        if (regs.payloadGPR() != InvalidGPRReg)
            set.set(regs.payloadGPR());
        constructRegisterSet(set, args...);
    }

    template<typename... Regs>
    static void constructRegisterSet(RegisterSet& set, GPRReg reg, Regs... args)
    {
        if (reg != InvalidGPRReg)
            set.set(reg);
        constructRegisterSet(set, args...);
    }

    // Add a debug call. This call has no effect on JIT code execution state.
    void debugCall(VM&, V_DebugOperation_EPP function, void* argument);

    // These methods JIT generate dynamic, debug-only checks - akin to ASSERTs.
#if !ASSERT_DISABLED
    void jitAssertIsInt32(GPRReg);
    void jitAssertIsJSInt32(GPRReg);
    void jitAssertIsJSNumber(GPRReg);
    void jitAssertIsJSDouble(GPRReg);
    void jitAssertIsCell(GPRReg);
    void jitAssertHasValidCallFrame();
    void jitAssertIsNull(GPRReg);
    void jitAssertTagsInPlace();
    void jitAssertArgumentCountSane();
#else
    void jitAssertIsInt32(GPRReg) { }
    void jitAssertIsJSInt32(GPRReg) { }
    void jitAssertIsJSNumber(GPRReg) { }
    void jitAssertIsJSDouble(GPRReg) { }
    void jitAssertIsCell(GPRReg) { }
    void jitAssertHasValidCallFrame() { }
    void jitAssertIsNull(GPRReg) { }
    void jitAssertTagsInPlace() { }
    void jitAssertArgumentCountSane() { }
#endif

    void jitReleaseAssertNoException(VM&);

    void incrementSuperSamplerCount();
    void decrementSuperSamplerCount();
    
    void purifyNaN(FPRReg);

    // These methods convert between doubles, and doubles boxed and JSValues.
#if USE(JSVALUE64)
    GPRReg boxDouble(FPRReg fpr, GPRReg gpr, TagRegistersMode mode = HaveTagRegisters)
    {
        moveDoubleTo64(fpr, gpr);
        if (mode == DoNotHaveTagRegisters)
            sub64(TrustedImm64(TagTypeNumber), gpr);
        else {
            sub64(GPRInfo::tagTypeNumberRegister, gpr);
            jitAssertIsJSDouble(gpr);
        }
        return gpr;
    }
    FPRReg unboxDoubleWithoutAssertions(GPRReg gpr, GPRReg resultGPR, FPRReg fpr)
    {
        add64(GPRInfo::tagTypeNumberRegister, gpr, resultGPR);
        move64ToDouble(resultGPR, fpr);
        return fpr;
    }
    FPRReg unboxDouble(GPRReg gpr, GPRReg resultGPR, FPRReg fpr)
    {
        jitAssertIsJSDouble(gpr);
        return unboxDoubleWithoutAssertions(gpr, resultGPR, fpr);
    }
    
    void boxDouble(FPRReg fpr, JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
        boxDouble(fpr, regs.gpr(), mode);
    }

    void unboxDoubleNonDestructive(JSValueRegs regs, FPRReg destFPR, GPRReg resultGPR, FPRReg)
    {
        unboxDouble(regs.payloadGPR(), resultGPR, destFPR);
    }

    // Here are possible arrangements of source, target, scratch:
    // - source, target, scratch can all be separate registers.
    // - source and target can be the same but scratch is separate.
    // - target and scratch can be the same but source is separate.
    void boxInt52(GPRReg source, GPRReg target, GPRReg scratch, FPRReg fpScratch)
    {
        // Is it an int32?
        signExtend32ToPtr(source, scratch);
        Jump isInt32 = branch64(Equal, source, scratch);
        
        // Nope, it's not, but regT0 contains the int64 value.
        convertInt64ToDouble(source, fpScratch);
        boxDouble(fpScratch, target);
        Jump done = jump();
        
        isInt32.link(this);
        zeroExtend32ToPtr(source, target);
        or64(GPRInfo::tagTypeNumberRegister, target);
        
        done.link(this);
    }
#endif

#if USE(JSVALUE32_64)
    void boxDouble(FPRReg fpr, GPRReg tagGPR, GPRReg payloadGPR)
    {
        moveDoubleToInts(fpr, payloadGPR, tagGPR);
    }
    void unboxDouble(GPRReg tagGPR, GPRReg payloadGPR, FPRReg fpr, FPRReg scratchFPR)
    {
        moveIntsToDouble(payloadGPR, tagGPR, fpr, scratchFPR);
    }
    
    void boxDouble(FPRReg fpr, JSValueRegs regs)
    {
        boxDouble(fpr, regs.tagGPR(), regs.payloadGPR());
    }
    void unboxDouble(JSValueRegs regs, FPRReg fpr, FPRReg scratchFPR)
    {
        unboxDouble(regs.tagGPR(), regs.payloadGPR(), fpr, scratchFPR);
    }

    void unboxDoubleNonDestructive(const JSValueRegs regs, FPRReg destFPR, GPRReg, FPRReg scratchFPR)
    {
        unboxDouble(regs, destFPR, scratchFPR);
    }
#endif
    
    void boxBooleanPayload(GPRReg boolGPR, GPRReg payloadGPR)
    {
#if USE(JSVALUE64)
        add32(TrustedImm32(ValueFalse), boolGPR, payloadGPR);
#else
        move(boolGPR, payloadGPR);
#endif
    }

    void boxBooleanPayload(bool value, GPRReg payloadGPR)
    {
#if USE(JSVALUE64)
        move(TrustedImm32(ValueFalse + value), payloadGPR);
#else
        move(TrustedImm32(value), payloadGPR);
#endif
    }

    void boxBoolean(GPRReg boolGPR, JSValueRegs boxedRegs)
    {
        boxBooleanPayload(boolGPR, boxedRegs.payloadGPR());
#if USE(JSVALUE32_64)
        move(TrustedImm32(JSValue::BooleanTag), boxedRegs.tagGPR());
#endif
    }

    void boxBoolean(bool value, JSValueRegs boxedRegs)
    {
        boxBooleanPayload(value, boxedRegs.payloadGPR());
#if USE(JSVALUE32_64)
        move(TrustedImm32(JSValue::BooleanTag), boxedRegs.tagGPR());
#endif
    }

    void boxInt32(GPRReg intGPR, JSValueRegs boxedRegs, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        if (mode == DoNotHaveTagRegisters) {
            move(intGPR, boxedRegs.gpr());
            or64(TrustedImm64(TagTypeNumber), boxedRegs.gpr());
        } else
            or64(GPRInfo::tagTypeNumberRegister, intGPR, boxedRegs.gpr());
#else
        UNUSED_PARAM(mode);
        move(intGPR, boxedRegs.payloadGPR());
        move(TrustedImm32(JSValue::Int32Tag), boxedRegs.tagGPR());
#endif
    }

    void boxCell(GPRReg cellGPR, JSValueRegs boxedRegs)
    {
#if USE(JSVALUE64)
        move(cellGPR, boxedRegs.gpr());
#else
        move(cellGPR, boxedRegs.payloadGPR());
        move(TrustedImm32(JSValue::CellTag), boxedRegs.tagGPR());
#endif
    }
    
    void callExceptionFuzz(VM&);
    
    enum ExceptionCheckKind { NormalExceptionCheck, InvertedExceptionCheck };
    enum ExceptionJumpWidth { NormalJumpWidth, FarJumpWidth };
    JS_EXPORT_PRIVATE Jump emitExceptionCheck(
        VM&, ExceptionCheckKind = NormalExceptionCheck, ExceptionJumpWidth = NormalJumpWidth);
    JS_EXPORT_PRIVATE Jump emitNonPatchableExceptionCheck(VM&);
    Jump emitJumpIfException(VM&);

#if ENABLE(SAMPLING_COUNTERS)
    static void emitCount(MacroAssembler& jit, AbstractSamplingCounter& counter, int32_t increment = 1)
    {
        jit.add64(TrustedImm32(increment), AbsoluteAddress(counter.addressOfCounter()));
    }
    void emitCount(AbstractSamplingCounter& counter, int32_t increment = 1)
    {
        add64(TrustedImm32(increment), AbsoluteAddress(counter.addressOfCounter()));
    }
#endif

#if ENABLE(SAMPLING_FLAGS)
    void setSamplingFlag(int32_t);
    void clearSamplingFlag(int32_t flag);
#endif

    JSGlobalObject* globalObjectFor(CodeOrigin codeOrigin)
    {
        return codeBlock()->globalObjectFor(codeOrigin);
    }
    
    bool isStrictModeFor(CodeOrigin codeOrigin)
    {
        auto* inlineCallFrame = codeOrigin.inlineCallFrame();
        if (!inlineCallFrame)
            return codeBlock()->isStrictMode();
        return inlineCallFrame->isStrictMode();
    }
    
    ECMAMode ecmaModeFor(CodeOrigin codeOrigin)
    {
        return isStrictModeFor(codeOrigin) ? StrictMode : NotStrictMode;
    }
    
    ExecutableBase* executableFor(const CodeOrigin& codeOrigin);
    
    CodeBlock* baselineCodeBlockFor(const CodeOrigin& codeOrigin)
    {
        return baselineCodeBlockForOriginAndBaselineCodeBlock(codeOrigin, baselineCodeBlock());
    }
    
    CodeBlock* baselineCodeBlockFor(InlineCallFrame* inlineCallFrame)
    {
        if (!inlineCallFrame)
            return baselineCodeBlock();
        return baselineCodeBlockForInlineCallFrame(inlineCallFrame);
    }
    
    CodeBlock* baselineCodeBlock()
    {
        return m_baselineCodeBlock;
    }
    
    static VirtualRegister argumentsStart(InlineCallFrame* inlineCallFrame)
    {
        if (!inlineCallFrame)
            return VirtualRegister(CallFrame::argumentOffset(0));
        if (inlineCallFrame->argumentsWithFixup.size() <= 1)
            return virtualRegisterForLocal(0);
        ValueRecovery recovery = inlineCallFrame->argumentsWithFixup[1];
        RELEASE_ASSERT(recovery.technique() == DisplacedInJSStack);
        return recovery.virtualRegister();
    }
    
    static VirtualRegister argumentsStart(const CodeOrigin& codeOrigin)
    {
        return argumentsStart(codeOrigin.inlineCallFrame());
    }

    static VirtualRegister argumentCount(InlineCallFrame* inlineCallFrame)
    {
        ASSERT(!inlineCallFrame || inlineCallFrame->isVarargs());
        if (!inlineCallFrame)
            return VirtualRegister(CallFrameSlot::argumentCount);
        return inlineCallFrame->argumentCountRegister;
    }

    static VirtualRegister argumentCount(const CodeOrigin& codeOrigin)
    {
        return argumentCount(codeOrigin.inlineCallFrame());
    }
    
    void emitLoadStructure(VM&, RegisterID source, RegisterID dest, RegisterID scratch);

    void emitStoreStructureWithTypeInfo(TrustedImmPtr structure, RegisterID dest, RegisterID)
    {
        emitStoreStructureWithTypeInfo(*this, structure, dest);
    }

    void emitStoreStructureWithTypeInfo(RegisterID structure, RegisterID dest, RegisterID scratch)
    {
#if USE(JSVALUE64)
        load64(MacroAssembler::Address(structure, Structure::structureIDOffset()), scratch);
        store64(scratch, MacroAssembler::Address(dest, JSCell::structureIDOffset()));
#else
        // Store all the info flags using a single 32-bit wide load and store.
        load32(MacroAssembler::Address(structure, Structure::indexingModeIncludingHistoryOffset()), scratch);
        store32(scratch, MacroAssembler::Address(dest, JSCell::indexingTypeAndMiscOffset()));

        // Store the StructureID
        storePtr(structure, MacroAssembler::Address(dest, JSCell::structureIDOffset()));
#endif
    }

    static void emitStoreStructureWithTypeInfo(AssemblyHelpers& jit, TrustedImmPtr structure, RegisterID dest);

    Jump barrierBranchWithoutFence(GPRReg cell)
    {
        return branch8(Above, Address(cell, JSCell::cellStateOffset()), TrustedImm32(blackThreshold));
    }

    Jump barrierBranchWithoutFence(JSCell* cell)
    {
        uint8_t* address = reinterpret_cast<uint8_t*>(cell) + JSCell::cellStateOffset();
        return branch8(Above, AbsoluteAddress(address), TrustedImm32(blackThreshold));
    }
    
    Jump barrierBranch(VM& vm, GPRReg cell, GPRReg scratchGPR)
    {
        load8(Address(cell, JSCell::cellStateOffset()), scratchGPR);
        return branch32(Above, scratchGPR, AbsoluteAddress(vm.heap.addressOfBarrierThreshold()));
    }

    Jump barrierBranch(VM& vm, JSCell* cell, GPRReg scratchGPR)
    {
        uint8_t* address = reinterpret_cast<uint8_t*>(cell) + JSCell::cellStateOffset();
        load8(address, scratchGPR);
        return branch32(Above, scratchGPR, AbsoluteAddress(vm.heap.addressOfBarrierThreshold()));
    }
    
    void barrierStoreLoadFence(VM& vm)
    {
        Jump ok = jumpIfMutatorFenceNotNeeded(vm);
        memoryFence();
        ok.link(this);
    }
    
    void mutatorFence(VM& vm)
    {
        if (isX86())
            return;
        Jump ok = jumpIfMutatorFenceNotNeeded(vm);
        storeFence();
        ok.link(this);
    }

    void cageWithoutUntagging(Gigacage::Kind kind, GPRReg storage)
    {
#if GIGACAGE_ENABLED
        if (!Gigacage::isEnabled(kind))
            return;

#if CPU(ARM64E)
        RegisterID tempReg = InvalidGPRReg;
        if (kind == Gigacage::Primitive) {
            tempReg = getCachedMemoryTempRegisterIDAndInvalidate();
            move(storage, tempReg);
            // Flip the registers since bitFieldInsert only inserts into the low bits.
            std::swap(storage, tempReg);
        }
#endif
        andPtr(TrustedImmPtr(Gigacage::mask(kind)), storage);
        addPtr(TrustedImmPtr(Gigacage::basePtr(kind)), storage);
#if CPU(ARM64E)
        if (kind == Gigacage::Primitive)
            bitFieldInsert64(storage, 0, 64 - numberOfPACBits, tempReg);
#endif

#else
        UNUSED_PARAM(kind);
        UNUSED_PARAM(storage);
#endif
    }

    // length may be the same register as scratch.
    void cageConditionally(Gigacage::Kind kind, GPRReg storage, GPRReg length, GPRReg scratch)
    {
#if GIGACAGE_ENABLED
        if (Gigacage::isEnabled(kind)) {
            if (kind != Gigacage::Primitive || Gigacage::isDisablingPrimitiveGigacageDisabled())
                cageWithoutUntagging(kind, storage);
            else {
#if CPU(ARM64E)
                if (length == scratch)
                    scratch = getCachedMemoryTempRegisterIDAndInvalidate();
#endif
                loadPtr(&Gigacage::basePtr(kind), scratch);
                Jump done = branchTest64(Zero, scratch);
#if CPU(ARM64E)
                GPRReg tempReg = getCachedDataTempRegisterIDAndInvalidate();
                move(storage, tempReg);
                ASSERT(LogicalImmediate::create64(Gigacage::mask(kind)).isValid());
                andPtr(TrustedImmPtr(Gigacage::mask(kind)), tempReg);
                addPtr(scratch, tempReg);
                bitFieldInsert64(tempReg, 0, 64 - numberOfPACBits, storage);
#else
                andPtr(TrustedImmPtr(Gigacage::mask(kind)), storage);
                addPtr(scratch, storage);
#endif // CPU(ARM64E)
                done.link(this);
            }
        }
#endif

#if CPU(ARM64E)
        if (kind == Gigacage::Primitive)
            untagArrayPtr(length, storage);
#endif
        UNUSED_PARAM(kind);
        UNUSED_PARAM(storage);
        UNUSED_PARAM(length);
        UNUSED_PARAM(scratch);
    }

    void emitComputeButterflyIndexingMask(GPRReg vectorLengthGPR, GPRReg scratchGPR, GPRReg resultGPR)
    {
        ASSERT(scratchGPR != resultGPR);
        Jump done;
        if (isX86() && !isX86_64()) {
            Jump nonZero = branchTest32(NonZero, vectorLengthGPR);
            move(TrustedImm32(0), resultGPR);
            done = jump();
            nonZero.link(this);
        }
        // If vectorLength == 0 then clz will return 32 on both ARM and x86. On 64-bit systems, we can then do a 64-bit right shift on a 32-bit -1 to get a 0 mask for zero vectorLength. On 32-bit ARM, shift masks with 0xff, which means it will still create a 0 mask.
        countLeadingZeros32(vectorLengthGPR, scratchGPR);
        move(TrustedImm32(-1), resultGPR);
        urshiftPtr(scratchGPR, resultGPR);
        if (done.isSet())
            done.link(this);
    }

    // If for whatever reason the butterfly is going to change vector length this function does NOT
    // update the indexing mask.
    void nukeStructureAndStoreButterfly(VM& vm, GPRReg butterfly, GPRReg object)
    {
        if (isX86()) {
            or32(TrustedImm32(bitwise_cast<int32_t>(nukedStructureIDBit())), Address(object, JSCell::structureIDOffset()));
            storePtr(butterfly, Address(object, JSObject::butterflyOffset()));
            return;
        }

        Jump ok = jumpIfMutatorFenceNotNeeded(vm);
        or32(TrustedImm32(bitwise_cast<int32_t>(nukedStructureIDBit())), Address(object, JSCell::structureIDOffset()));
        storeFence();
        storePtr(butterfly, Address(object, JSObject::butterflyOffset()));
        storeFence();
        Jump done = jump();
        ok.link(this);
        storePtr(butterfly, Address(object, JSObject::butterflyOffset()));
        done.link(this);
    }
    
    Jump jumpIfMutatorFenceNotNeeded(VM& vm)
    {
        return branchTest8(Zero, AbsoluteAddress(vm.heap.addressOfMutatorShouldBeFenced()));
    }
    
    void sanitizeStackInline(VM&, GPRReg scratch);
    
    // Emits the branch structure for typeof. The code emitted by this doesn't fall through. The
    // functor is called at those points where we have pinpointed a type. One way to use this is to
    // have the functor emit the code to put the type string into an appropriate register and then
    // jump out. A secondary functor is used for the call trap and masquerades-as-undefined slow
    // case. It is passed the unlinked jump to the slow case.
    template<typename Functor, typename SlowPathFunctor>
    void emitTypeOf(
        JSValueRegs regs, GPRReg tempGPR, const Functor& functor,
        const SlowPathFunctor& slowPathFunctor)
    {
        // Implements the following branching structure:
        //
        // if (is cell) {
        //     if (is object) {
        //         if (is function) {
        //             return function;
        //         } else if (doesn't have call trap and doesn't masquerade as undefined) {
        //             return object
        //         } else {
        //             return slowPath();
        //         }
        //     } else if (is string) {
        //         return string
        //     } else if (is bigint) {
        //         return bigint
        //     } else {
        //         return symbol
        //     }
        // } else if (is number) {
        //     return number
        // } else if (is null) {
        //     return object
        // } else if (is boolean) {
        //     return boolean
        // } else {
        //     return undefined
        // }
        //
        // FIXME: typeof Symbol should be more frequently seen than BigInt.
        // We should change the order of type detection based on this frequency.
        // https://bugs.webkit.org/show_bug.cgi?id=192650
        
        Jump notCell = branchIfNotCell(regs);
        
        GPRReg cellGPR = regs.payloadGPR();
        Jump notObject = branchIfNotObject(cellGPR);
        
        Jump notFunction = branchIfNotFunction(cellGPR);
        functor(TypeofType::Function, false);
        
        notFunction.link(this);
        slowPathFunctor(
            branchTest8(
                NonZero,
                Address(cellGPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined | OverridesGetCallData)));
        functor(TypeofType::Object, false);
        
        notObject.link(this);
        
        Jump notString = branchIfNotString(cellGPR);
        functor(TypeofType::String, false);

        notString.link(this);

        Jump notBigInt = branchIfNotBigInt(cellGPR);
        functor(TypeofType::BigInt, false);

        notBigInt.link(this);
        functor(TypeofType::Symbol, false);
        
        notCell.link(this);

        Jump notNumber = branchIfNotNumber(regs, tempGPR);
        functor(TypeofType::Number, false);
        notNumber.link(this);
        
        JumpList notNull = branchIfNotEqual(regs, jsNull());
        functor(TypeofType::Object, false);
        notNull.link(this);
        
        Jump notBoolean = branchIfNotBoolean(regs, tempGPR);
        functor(TypeofType::Boolean, false);
        notBoolean.link(this);
        
        functor(TypeofType::Undefined, true);
    }
    
    void emitDumbVirtualCall(VM&, CallLinkInfo*);
    
    void makeSpaceOnStackForCCall();
    void reclaimSpaceOnStackForCCall();

#if USE(JSVALUE64)
    void emitRandomThunk(JSGlobalObject*, GPRReg scratch0, GPRReg scratch1, GPRReg scratch2, FPRReg result);
    void emitRandomThunk(VM&, GPRReg scratch0, GPRReg scratch1, GPRReg scratch2, GPRReg scratch3, FPRReg result);
#endif

    // Call this if you know that the value held in allocatorGPR is non-null. This DOES NOT mean
    // that allocator is non-null; allocator can be null as a signal that we don't know what the
    // value of allocatorGPR is. Additionally, if the allocator is not null, then there is no need
    // to populate allocatorGPR - this code will ignore the contents of allocatorGPR.
    void emitAllocateWithNonNullAllocator(GPRReg resultGPR, const JITAllocator& allocator, GPRReg allocatorGPR, GPRReg scratchGPR, JumpList& slowPath);
    
    void emitAllocate(GPRReg resultGPR, const JITAllocator& allocator, GPRReg allocatorGPR, GPRReg scratchGPR, JumpList& slowPath);
    
    template<typename StructureType>
    void emitAllocateJSCell(GPRReg resultGPR, const JITAllocator& allocator, GPRReg allocatorGPR, StructureType structure, GPRReg scratchGPR, JumpList& slowPath)
    {
        emitAllocate(resultGPR, allocator, allocatorGPR, scratchGPR, slowPath);
        emitStoreStructureWithTypeInfo(structure, resultGPR, scratchGPR);
    }
    
    template<typename StructureType, typename StorageType>
    void emitAllocateJSObject(GPRReg resultGPR, const JITAllocator& allocator, GPRReg allocatorGPR, StructureType structure, StorageType storage, GPRReg scratchGPR, JumpList& slowPath)
    {
        emitAllocateJSCell(resultGPR, allocator, allocatorGPR, structure, scratchGPR, slowPath);
        storePtr(storage, Address(resultGPR, JSObject::butterflyOffset()));
    }
    
    template<typename ClassType, typename StructureType, typename StorageType>
    void emitAllocateJSObjectWithKnownSize(
        VM& vm, GPRReg resultGPR, StructureType structure, StorageType storage, GPRReg scratchGPR1,
        GPRReg scratchGPR2, JumpList& slowPath, size_t size)
    {
        Allocator allocator = allocatorForNonVirtualConcurrently<ClassType>(vm, size, AllocatorForMode::AllocatorIfExists);
        emitAllocateJSObject(resultGPR, JITAllocator::constant(allocator), scratchGPR1, structure, storage, scratchGPR2, slowPath);
    }
    
    template<typename ClassType, typename StructureType, typename StorageType>
    void emitAllocateJSObject(VM& vm, GPRReg resultGPR, StructureType structure, StorageType storage, GPRReg scratchGPR1, GPRReg scratchGPR2, JumpList& slowPath)
    {
        emitAllocateJSObjectWithKnownSize<ClassType>(vm, resultGPR, structure, storage, scratchGPR1, scratchGPR2, slowPath, ClassType::allocationSize(0));
    }
    
    // allocationSize can be aliased with any of the other input GPRs. If it's not aliased then it
    // won't be clobbered.
    void emitAllocateVariableSized(GPRReg resultGPR, CompleteSubspace& subspace, GPRReg allocationSize, GPRReg scratchGPR1, GPRReg scratchGPR2, JumpList& slowPath);
    
    template<typename ClassType, typename StructureType>
    void emitAllocateVariableSizedCell(VM& vm, GPRReg resultGPR, StructureType structure, GPRReg allocationSize, GPRReg scratchGPR1, GPRReg scratchGPR2, JumpList& slowPath)
    {
        CompleteSubspace* subspace = subspaceForConcurrently<ClassType>(vm);
        RELEASE_ASSERT_WITH_MESSAGE(subspace, "CompleteSubspace is always allocated");
        emitAllocateVariableSized(resultGPR, *subspace, allocationSize, scratchGPR1, scratchGPR2, slowPath);
        emitStoreStructureWithTypeInfo(structure, resultGPR, scratchGPR2);
    }

    template<typename ClassType, typename StructureType>
    void emitAllocateVariableSizedJSObject(VM& vm, GPRReg resultGPR, StructureType structure, GPRReg allocationSize, GPRReg scratchGPR1, GPRReg scratchGPR2, JumpList& slowPath)
    {
        emitAllocateVariableSizedCell<ClassType>(vm, resultGPR, structure, allocationSize, scratchGPR1, scratchGPR2, slowPath);
        storePtr(TrustedImmPtr(nullptr), Address(resultGPR, JSObject::butterflyOffset()));
    }

    JumpList branchIfValue(VM&, JSValueRegs, GPRReg scratch, GPRReg scratchIfShouldCheckMasqueradesAsUndefined, FPRReg, FPRReg, bool shouldCheckMasqueradesAsUndefined, JSGlobalObject*, bool negateResult);
    JumpList branchIfTruthy(VM& vm, JSValueRegs value, GPRReg scratch, GPRReg scratchIfShouldCheckMasqueradesAsUndefined, FPRReg scratchFPR0, FPRReg scratchFPR1, bool shouldCheckMasqueradesAsUndefined, JSGlobalObject* globalObject)
    {
        return branchIfValue(vm, value, scratch, scratchIfShouldCheckMasqueradesAsUndefined, scratchFPR0, scratchFPR1, shouldCheckMasqueradesAsUndefined, globalObject, false);
    }
    JumpList branchIfFalsey(VM& vm, JSValueRegs value, GPRReg scratch, GPRReg scratchIfShouldCheckMasqueradesAsUndefined, FPRReg scratchFPR0, FPRReg scratchFPR1, bool shouldCheckMasqueradesAsUndefined, JSGlobalObject* globalObject)
    {
        return branchIfValue(vm, value, scratch, scratchIfShouldCheckMasqueradesAsUndefined, scratchFPR0, scratchFPR1, shouldCheckMasqueradesAsUndefined, globalObject, true);
    }
    void emitConvertValueToBoolean(VM&, JSValueRegs, GPRReg result, GPRReg scratchIfShouldCheckMasqueradesAsUndefined, FPRReg, FPRReg, bool shouldCheckMasqueradesAsUndefined, JSGlobalObject*, bool negateResult = false);
    
    template<typename ClassType>
    void emitAllocateDestructibleObject(VM& vm, GPRReg resultGPR, Structure* structure, GPRReg scratchGPR1, GPRReg scratchGPR2, JumpList& slowPath)
    {
        auto butterfly = TrustedImmPtr(nullptr);
        emitAllocateJSObject<ClassType>(vm, resultGPR, TrustedImmPtr(structure), butterfly, scratchGPR1, scratchGPR2, slowPath);
        storePtr(TrustedImmPtr(structure->classInfo()), Address(resultGPR, JSDestructibleObject::classInfoOffset()));
    }
    
    void emitInitializeInlineStorage(GPRReg baseGPR, unsigned inlineCapacity)
    {
        for (unsigned i = 0; i < inlineCapacity; ++i)
            storeTrustedValue(JSValue(), Address(baseGPR, JSObject::offsetOfInlineStorage() + i * sizeof(EncodedJSValue)));
    }

    void emitInitializeInlineStorage(GPRReg baseGPR, GPRReg inlineCapacity)
    {
        Jump empty = branchTest32(Zero, inlineCapacity);
        Label loop = label();
        sub32(TrustedImm32(1), inlineCapacity);
        storeTrustedValue(JSValue(), BaseIndex(baseGPR, inlineCapacity, TimesEight, JSObject::offsetOfInlineStorage()));
        branchTest32(NonZero, inlineCapacity).linkTo(loop, this);
        empty.link(this);
    }

    void emitInitializeOutOfLineStorage(GPRReg butterflyGPR, unsigned outOfLineCapacity)
    {
        for (unsigned i = 0; i < outOfLineCapacity; ++i)
            storeTrustedValue(JSValue(), Address(butterflyGPR, -sizeof(IndexingHeader) - (i + 1) * sizeof(EncodedJSValue)));
    }
    
#if USE(JSVALUE64)
    void wangsInt64Hash(GPRReg inputAndResult, GPRReg scratch);
#endif
    
    // This assumes that index and length are 32-bit. This also assumes that they are already
    // zero-extended. Also this does not clobber index, which is useful in the baseline JIT. This
    // permits length and result to be in the same register.
    void emitPreparePreciseIndexMask32(GPRReg index, GPRReg length, GPRReg result);

#if ENABLE(WEBASSEMBLY)
    void loadWasmContextInstance(GPRReg dst);
    void storeWasmContextInstance(GPRReg src);
    static bool loadWasmContextInstanceNeedsMacroScratchRegister();
    static bool storeWasmContextInstanceNeedsMacroScratchRegister();
#endif

protected:
    void copyCalleeSavesToEntryFrameCalleeSavesBufferImpl(GPRReg calleeSavesBuffer);

    CodeBlock* m_codeBlock;
    CodeBlock* m_baselineCodeBlock;
};

} // namespace JSC

#endif // ENABLE(JIT)
