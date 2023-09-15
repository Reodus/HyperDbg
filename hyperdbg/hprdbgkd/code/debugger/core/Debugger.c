/**
 * @file Debugger.c
 * @author Sina Karvandi (sina@hyperdbg.org)
 * @brief Implementation of Debugger functions
 * @details
 *
 * @version 0.1
 * @date 2020-04-13
 *
 * @copyright This project is released under the GNU Public License v3.
 *
 */
#include "pch.h"

/**
 * @brief A wrapper for GetRegValue() in script-engine
 *
 * @return BOOLEAN Value of register
 */
UINT64
DebuggerGetRegValueWrapper(PGUEST_REGS GuestRegs, UINT32 /* REGS_ENUM */ RegId)
{
    return GetRegValue(GuestRegs, RegId);
}

/**
 * @brief Debugger get the last error
 *
 * @return UINT32 Error value
 */
UINT32
DebuggerGetLastError()
{
    return g_LastError;
}

/**
 * @brief Debugger set the last error
 * @param LastError The value of last error
 *
 * @return VOID
 */
VOID
DebuggerSetLastError(UINT32 LastError)
{
    g_LastError = LastError;
}

/**
 * @brief Initialize Debugger Structures and Routines
 *
 * @return BOOLEAN Shows whether the initialization process was successful
 * or not
 */
BOOLEAN
DebuggerInitialize()
{
    ULONG                       ProcessorCount       = KeQueryActiveProcessorCount(0);
    PROCESSOR_DEBUGGING_STATE * CurrentDebuggerState = NULL;

    //
    // Also allocate the debugging state
    //
    if (!GlobalDebuggingStateAllocateZeroedMemory())
    {
        return FALSE;
    }

    //
    // Allocate buffer for saving events
    //
    if (GlobalEventsAllocateZeroedMemory() == FALSE)
    {
        return FALSE;
    }

    //
    // Set the core's IDs
    //
    for (size_t i = 0; i < ProcessorCount; i++)
    {
        g_DbgState[i].CoreId = i;
    }

    //
    // Initialize lists relating to the debugger events store
    //
    InitializeListHead(&g_Events->EptHookExecCcEventsHead);
    InitializeListHead(&g_Events->HiddenHookReadAndWriteAndExecuteEventsHead);
    InitializeListHead(&g_Events->HiddenHookReadAndWriteEventsHead);
    InitializeListHead(&g_Events->HiddenHookReadAndExecuteEventsHead);
    InitializeListHead(&g_Events->HiddenHookWriteAndExecuteEventsHead);
    InitializeListHead(&g_Events->HiddenHookReadEventsHead);
    InitializeListHead(&g_Events->HiddenHookWriteEventsHead);
    InitializeListHead(&g_Events->HiddenHookExecuteEventsHead);
    InitializeListHead(&g_Events->EptHook2sExecDetourEventsHead);
    InitializeListHead(&g_Events->SyscallHooksEferSyscallEventsHead);
    InitializeListHead(&g_Events->SyscallHooksEferSysretEventsHead);
    InitializeListHead(&g_Events->CpuidInstructionExecutionEventsHead);
    InitializeListHead(&g_Events->RdmsrInstructionExecutionEventsHead);
    InitializeListHead(&g_Events->WrmsrInstructionExecutionEventsHead);
    InitializeListHead(&g_Events->ExceptionOccurredEventsHead);
    InitializeListHead(&g_Events->TscInstructionExecutionEventsHead);
    InitializeListHead(&g_Events->PmcInstructionExecutionEventsHead);
    InitializeListHead(&g_Events->InInstructionExecutionEventsHead);
    InitializeListHead(&g_Events->OutInstructionExecutionEventsHead);
    InitializeListHead(&g_Events->DebugRegistersAccessedEventsHead);
    InitializeListHead(&g_Events->ExternalInterruptOccurredEventsHead);
    InitializeListHead(&g_Events->VmcallInstructionExecutionEventsHead);
    InitializeListHead(&g_Events->TrapExecutionUserModeEventsHead);
    InitializeListHead(&g_Events->TrapExecutionKernelModeEventsHead);
    InitializeListHead(&g_Events->TrapExecutionMemoryEventsHead);
    InitializeListHead(&g_Events->ControlRegister3ModifiedEventsHead);
    InitializeListHead(&g_Events->ControlRegisterModifiedEventsHead);

    //
    // Enabled Debugger Events
    //
    g_EnableDebuggerEvents = TRUE;

    //
    // Set initial state of triggering events for VMCALLs
    //
    VmFuncSetTriggerEventForVmcalls(FALSE);

    //
    // Set initial state of triggering events for VMCALLs
    //
    VmFuncSetTriggerEventForCpuids(FALSE);

    //
    // Initialize script engines global variables holder
    //
    if (!g_ScriptGlobalVariables)
    {
        g_ScriptGlobalVariables = CrsAllocateNonPagedPool(MAX_VAR_COUNT * sizeof(UINT64));
    }

    if (!g_ScriptGlobalVariables)
    {
        //
        // Out of resource, initialization of script engine's global varialbe holders failed
        //
        return FALSE;
    }

    //
    // Zero the global variables memory
    //
    RtlZeroMemory(g_ScriptGlobalVariables, MAX_VAR_COUNT * sizeof(UINT64));

    //
    // Zero the TRAP FLAG state memory
    //
    RtlZeroMemory(&g_TrapFlagState, sizeof(DEBUGGER_TRAP_FLAG_STATE));

    //
    // Intialize the local and temp variables
    //
    for (size_t i = 0; i < ProcessorCount; i++)
    {
        CurrentDebuggerState = &g_DbgState[i];

        if (!CurrentDebuggerState->ScriptEngineCoreSpecificLocalVariable)
        {
            CurrentDebuggerState->ScriptEngineCoreSpecificLocalVariable = CrsAllocateNonPagedPool(MAX_VAR_COUNT * sizeof(UINT64));
        }

        if (!CurrentDebuggerState->ScriptEngineCoreSpecificLocalVariable)
        {
            //
            // Out of resource, initialization of script engine's local varialbe holders failed
            //
            return FALSE;
        }

        if (!CurrentDebuggerState->ScriptEngineCoreSpecificTempVariable)
        {
            CurrentDebuggerState->ScriptEngineCoreSpecificTempVariable = CrsAllocateNonPagedPool(MAX_TEMP_COUNT * sizeof(UINT64));
        }

        if (!CurrentDebuggerState->ScriptEngineCoreSpecificTempVariable)
        {
            //
            // Out of resource, initialization of script engine's local varialbe holders failed
            //
            return FALSE;
        }

        //
        // Zero the local and temp variables memory
        //
        RtlZeroMemory(CurrentDebuggerState->ScriptEngineCoreSpecificLocalVariable, MAX_VAR_COUNT * sizeof(UINT64));
        RtlZeroMemory(CurrentDebuggerState->ScriptEngineCoreSpecificTempVariable, MAX_TEMP_COUNT * sizeof(UINT64));
    }

    //
    // Initialize NMI broadcasting mechanism
    //
    VmFuncVmxBroadcastInitialize();

    //
    // Initialize attaching mechanism,
    // we'll use the functionalities of the attaching in reading modules
    // of user mode applications (other than attaching mechanism itself)
    //
    if (!AttachingInitialize())
    {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Uninitialize Debugger Structures and Routines
 *
 */
VOID
DebuggerUninitialize()
{
    ULONG                       ProcessorCount;
    PROCESSOR_DEBUGGING_STATE * CurrentDebuggerState = NULL;

    ProcessorCount = KeQueryActiveProcessorCount(0);

    //
    //  *** Disable, terminate and clear all the events ***
    //

    //
    // Because we want to delete all the objects and buffers (pools)
    // after we finished termination, the debugger might still use
    // the buffers for events and action, for solving this problem
    // we first disable the tag(s) and this way the debugger no longer
    // use that event and this way we can safely remove and deallocate
    // the buffers later after termination
    //

    //
    // Disable triggering events
    //
    g_EnableDebuggerEvents = FALSE;

    //
    // First, disable all events
    //
    DebuggerEnableOrDisableAllEvents(FALSE);

    //
    // Second, terminate all events
    //
    DebuggerTerminateAllEvents();

    //
    // Third, remove all events
    //
    DebuggerRemoveAllEvents();

    //
    // Uninitialize kernel debugger
    //
    KdUninitializeKernelDebugger();

    //
    // Uninitialize user debugger
    //
    UdUninitializeUserDebugger();

    //
    // Uninitialize NMI broadcasting mechanism
    //
    VmFuncVmxBroadcastUninitialize();

    //
    // Free g_Events
    //
    GlobalEventsFreeMemory();

    //
    // Free g_ScriptGlobalVariables
    //
    if (g_ScriptGlobalVariables != NULL)
    {
        CrsFreePool(g_ScriptGlobalVariables);
        g_ScriptGlobalVariables = NULL;
    }

    //
    // Free core specific local and temp variables
    //
    for (SIZE_T i = 0; i < ProcessorCount; i++)
    {
        CurrentDebuggerState = &g_DbgState[i];

        if (CurrentDebuggerState->ScriptEngineCoreSpecificLocalVariable != NULL)
        {
            CrsFreePool(CurrentDebuggerState->ScriptEngineCoreSpecificLocalVariable);
            CurrentDebuggerState->ScriptEngineCoreSpecificLocalVariable = NULL;
        }

        if (CurrentDebuggerState->ScriptEngineCoreSpecificTempVariable != NULL)
        {
            CrsFreePool(CurrentDebuggerState->ScriptEngineCoreSpecificTempVariable);
            CurrentDebuggerState->ScriptEngineCoreSpecificTempVariable = NULL;
        }
    }

    //
    // Free g_DbgState
    //
    GlobalDebuggingStateFreeMemory();
}

/**
 * @brief Create an Event Object
 *
 * @details should NOT be called in vmx-root
 *
 * @param Enabled Is the event enabled or disabled
 * @param CoreId The core id that this event is allowed to run
 * @param ProcessId The process id that this event is allowed to run
 * @param EventType The type of event
 * @param Tag User-mode generated unique tag (id) of the event
 * @param OptionalParam1 Optional parameter 1 for event
 * @param OptionalParam2 Optional parameter 2 for event
 * @param OptionalParam3 Optional parameter 3 for event
 * @param OptionalParam4 Optional parameter 4 for event
 * @param ConditionsBufferSize Size of condition code buffer (if any)
 * @param ConditionBuffer Address of condition code buffer (if any)
 * @return PDEBUGGER_EVENT Returns null in the case of error and event
 * object address when it's successful
 */
PDEBUGGER_EVENT
DebuggerCreateEvent(BOOLEAN             Enabled,
                    UINT32              CoreId,
                    UINT32              ProcessId,
                    VMM_EVENT_TYPE_ENUM EventType,
                    UINT64              Tag,
                    UINT64              OptionalParam1,
                    UINT64              OptionalParam2,
                    UINT64              OptionalParam3,
                    UINT64              OptionalParam4,
                    UINT32              ConditionsBufferSize,
                    PVOID               ConditionBuffer)
{
    //
    // As this function uses the memory allocation functions,
    // we have to make sure that it will not be called in vmx root
    //
    if (VmFuncVmxGetCurrentExecutionMode() == TRUE)
    {
        return NULL;
    }

    //
    // Initialize the event structure
    //
    PDEBUGGER_EVENT Event = CrsAllocateZeroedNonPagedPool(sizeof(DEBUGGER_EVENT) + ConditionsBufferSize);

    if (!Event)
    {
        //
        // There is a problem with allocating event
        //
        return NULL;
    }

    Event->CoreId         = CoreId;
    Event->ProcessId      = ProcessId;
    Event->Enabled        = Enabled;
    Event->EventType      = EventType;
    Event->Tag            = Tag;
    Event->CountOfActions = 0; // currently there is no action
    Event->OptionalParam1 = OptionalParam1;
    Event->OptionalParam2 = OptionalParam2;
    Event->OptionalParam3 = OptionalParam3;
    Event->OptionalParam4 = OptionalParam4;

    //
    // check if this event is conditional or not
    //
    if (ConditionBuffer != 0)
    {
        //
        // It's condtional
        //
        Event->ConditionsBufferSize   = ConditionsBufferSize;
        Event->ConditionBufferAddress = (UINT64)Event + sizeof(DEBUGGER_EVENT);

        //
        // copy the condtion buffer to the end of the buffer of the event
        //
        memcpy(Event->ConditionBufferAddress, ConditionBuffer, ConditionsBufferSize);
    }
    else
    {
        //
        // It's unconditioanl
        //
        Event->ConditionsBufferSize = 0;
    }

    //
    // Make the action lists ready
    //
    InitializeListHead(&Event->ActionsListHead);

    //
    // Return our event
    //
    return Event;
}

/**
 * @brief Create an action and add the action to an event
 *
 * @details should NOT be called in vmx-root
 *
 * @param Event Target event object
 * @param ActionType Type of action
 * @param SendTheResultsImmediately whether the results should be received
 * by the user-mode immediately
 * @param InTheCaseOfCustomCode Custom code structure (if any)
 * @param InTheCaseOfRunScript Run script structure (if any)
 * @return PDEBUGGER_EVENT_ACTION
 */
PDEBUGGER_EVENT_ACTION
DebuggerAddActionToEvent(PDEBUGGER_EVENT                                 Event,
                         DEBUGGER_EVENT_ACTION_TYPE_ENUM                 ActionType,
                         BOOLEAN                                         SendTheResultsImmediately,
                         PDEBUGGER_EVENT_REQUEST_CUSTOM_CODE             InTheCaseOfCustomCode,
                         PDEBUGGER_EVENT_ACTION_RUN_SCRIPT_CONFIGURATION InTheCaseOfRunScript)
{
    PDEBUGGER_EVENT_ACTION Action;
    SIZE_T                 Size;

    //
    // As this function uses the memory allocation functions,
    // we have to make sure that it will not be called in vmx root
    //
    if (VmFuncVmxGetCurrentExecutionMode() == TRUE)
    {
        return NULL;
    }

    //
    // Allocate action + allocate code for custom code
    //

    if (InTheCaseOfCustomCode != NULL)
    {
        //
        // We should allocate extra buffer for custom code
        //
        Size = sizeof(DEBUGGER_EVENT_ACTION) + InTheCaseOfCustomCode->CustomCodeBufferSize;
    }
    else if (InTheCaseOfRunScript != NULL)
    {
        //
        // We should allocate extra buffer for script
        //
        Size = sizeof(DEBUGGER_EVENT_ACTION) + InTheCaseOfRunScript->ScriptLength;
    }
    else
    {
        //
        // We shouldn't allocate extra buffer as there is no custom code
        //
        Size = sizeof(DEBUGGER_EVENT_ACTION);
    }

    Action = CrsAllocateZeroedNonPagedPool(Size);

    if (Action == NULL)
    {
        //
        // There was an error in allocation
        //
        return NULL;
    }

    //
    // If the user needs a buffer to be passed to the debugger then
    // we should allocate it here (Requested buffer is only available for custom code types)
    //
    if (ActionType == RUN_CUSTOM_CODE && InTheCaseOfCustomCode->OptionalRequestedBufferSize != 0)
    {
        //
        // Check if the optional buffer is not more that the size
        // we can send to usermode
        //
        if (InTheCaseOfCustomCode->OptionalRequestedBufferSize >= MaximumPacketsCapacity)
        {
            //
            // There was an error
            //
            CrsFreePool(Action);
            return NULL;
        }

        //
        // User needs a buffer to play with
        //
        PVOID RequestedBuffer = CrsAllocateZeroedNonPagedPool(InTheCaseOfCustomCode->OptionalRequestedBufferSize);

        if (!RequestedBuffer)
        {
            //
            // There was an error in allocation
            //
            CrsFreePool(Action);
            return NULL;
        }

        //
        // Add it to the action
        //
        Action->RequestedBuffer.EnabledRequestBuffer = TRUE;
        Action->RequestedBuffer.RequestBufferSize    = InTheCaseOfCustomCode->OptionalRequestedBufferSize;
        Action->RequestedBuffer.RequstBufferAddress  = RequestedBuffer;
    }

    //
    // If the user needs a buffer to be passed to the debugger script then
    // we should allocate it here (Requested buffer is only available for custom code types)
    //
    if (ActionType == RUN_SCRIPT && InTheCaseOfRunScript->OptionalRequestedBufferSize != 0)
    {
        //
        // Check if the optional buffer is not more that the size
        // we can send to usermode
        //
        if (InTheCaseOfRunScript->OptionalRequestedBufferSize >= MaximumPacketsCapacity)
        {
            //
            // There was an error
            //
            CrsFreePool(Action);
            return NULL;
        }

        //
        // User needs a buffer to play with
        //
        PVOID RequestedBuffer = CrsAllocateZeroedNonPagedPool(InTheCaseOfRunScript->OptionalRequestedBufferSize);

        if (!RequestedBuffer)
        {
            //
            // There was an error in allocation
            //
            CrsFreePool(Action);
            return NULL;
        }

        //
        // Add it to the action
        //
        Action->RequestedBuffer.EnabledRequestBuffer = TRUE;
        Action->RequestedBuffer.RequestBufferSize    = InTheCaseOfRunScript->OptionalRequestedBufferSize;
        Action->RequestedBuffer.RequstBufferAddress  = RequestedBuffer;
    }

    if (ActionType == RUN_CUSTOM_CODE)
    {
        //
        // Check if it's a Custom code without custom code buffer which is invalid
        //
        if (InTheCaseOfCustomCode->CustomCodeBufferSize == 0)
        {
            //
            // There was an error
            //
            CrsFreePool(Action);
            return NULL;
        }

        //
        // Move the custom code buffer to the end of the action
        //

        Action->CustomCodeBufferSize    = InTheCaseOfCustomCode->CustomCodeBufferSize;
        Action->CustomCodeBufferAddress = (UINT64)Action + sizeof(DEBUGGER_EVENT_ACTION);

        //
        // copy the custom code buffer to the end of the buffer of the action
        //
        memcpy(Action->CustomCodeBufferAddress, InTheCaseOfCustomCode->CustomCodeBufferAddress, InTheCaseOfCustomCode->CustomCodeBufferSize);
    }

    //
    // If it's run script action type
    //
    else if (ActionType == RUN_SCRIPT)
    {
        //
        // Check the buffers of run script
        //
        if (InTheCaseOfRunScript->ScriptBuffer == NULL || InTheCaseOfRunScript->ScriptLength == NULL)
        {
            //
            // Invalid configuration
            //
            CrsFreePool(Action);
            return NULL;
        }

        //
        // Allocate the buffer from a non-page pool on the script
        //
        Action->ScriptConfiguration.ScriptBuffer = (BYTE *)Action + sizeof(DEBUGGER_EVENT_ACTION);

        //
        // Copy the memory of script to our non-paged pool
        //
        RtlCopyMemory(Action->ScriptConfiguration.ScriptBuffer, InTheCaseOfRunScript->ScriptBuffer, InTheCaseOfRunScript->ScriptLength);

        //
        // Set other fields
        //
        Action->ScriptConfiguration.ScriptLength                = InTheCaseOfRunScript->ScriptLength;
        Action->ScriptConfiguration.ScriptPointer               = InTheCaseOfRunScript->ScriptPointer;
        Action->ScriptConfiguration.OptionalRequestedBufferSize = InTheCaseOfRunScript->OptionalRequestedBufferSize;
    }

    //
    // Create an order code for the current action
    // and also increase the Count of action in event
    //
    Event->CountOfActions++;
    Action->ActionOrderCode = Event->CountOfActions;

    //
    // Fill other parts of the action
    //
    Action->ImmediatelySendTheResults = SendTheResultsImmediately;
    Action->ActionType                = ActionType;
    Action->Tag                       = Event->Tag;

    //
    // Now we should add the action to the event's LIST_ENTRY of actions
    //
    InsertHeadList(&Event->ActionsListHead, &(Action->ActionsList));

    return Action;
}

/**
 * @brief Register an event to a list of active events
 *
 * @param Event Event structure
 * @return BOOLEAN TRUE if it successfully registered and FALSE if not registered
 */
BOOLEAN
DebuggerRegisterEvent(PDEBUGGER_EVENT Event)
{
    PLIST_ENTRY TargetEventList = NULL;

    //
    // Register the event
    //
    TargetEventList = DebuggerGetEventListByEventType(Event->EventType);

    if (TargetEventList != NULL)
    {
        InsertHeadList(TargetEventList, &(Event->EventsOfSameTypeList));

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/**
 * @brief Trigger events of a special type to be managed by debugger
 *
 * @param EventType Type of events
 * @param CallingStage Stage of calling (pre-event or post-event)
 * @param Context An optional parameter (different in each event)
 * @param PostEventRequired Whether the caller is requested to
 * trigger a post-event event
 * @param Regs Guest gp-registers
 *
 * @return VMM_CALLBACK_TRIGGERING_EVENT_STATUS_TYPE returns the staus
 * of handling events
 */
VMM_CALLBACK_TRIGGERING_EVENT_STATUS_TYPE
DebuggerTriggerEvents(VMM_EVENT_TYPE_ENUM                   EventType,
                      VMM_CALLBACK_EVENT_CALLING_STAGE_TYPE CallingStage,
                      PVOID                                 Context,
                      BOOLEAN *                             PostEventRequired,
                      GUEST_REGS *                          Regs)
{
    DebuggerCheckForCondition *      ConditionFunc;
    DEBUGGER_TRIGGERED_EVENT_DETAILS EventTriggerDetail;
    PLIST_ENTRY                      TempList  = 0;
    PLIST_ENTRY                      TempList2 = 0;
    PROCESSOR_DEBUGGING_STATE *      DbgState  = NULL;

    //
    // Check if triggering debugging actions are allowed or not
    //
    if (!g_EnableDebuggerEvents)
    {
        //
        // Debugger is not enabled
        //
        return VMM_CALLBACK_TRIGGERING_EVENT_STATUS_DEBUGGER_NOT_ENABLED;
    }

    //
    // Find the debugging state structure
    //
    DbgState = &g_DbgState[KeGetCurrentProcessorNumberEx(NULL)];

    //
    // Set the registers for debug state
    //
    DbgState->Regs = Regs;

    //
    // Find the debugger events list base on the type of the event
    //
    TempList  = DebuggerGetEventListByEventType(EventType);
    TempList2 = TempList;

    if (TempList == NULL)
    {
        return VMM_CALLBACK_TRIGGERING_EVENT_STATUS_INVALID_EVENT_TYPE;
    }

    while (TempList2 != TempList->Flink)
    {
        TempList                     = TempList->Flink;
        PDEBUGGER_EVENT CurrentEvent = CONTAINING_RECORD(TempList, DEBUGGER_EVENT, EventsOfSameTypeList);

        //
        // check if the event is enabled or not
        //
        if (!CurrentEvent->Enabled)
        {
            continue;
        }

        //
        // Check if this event is for this core or not
        //
        if (CurrentEvent->CoreId != DEBUGGER_EVENT_APPLY_TO_ALL_CORES && CurrentEvent->CoreId != DbgState->CoreId)
        {
            //
            // This event is not related to either or core or all cores
            //
            continue;
        }

        //
        // Check if this event is for this process or not
        //
        if (CurrentEvent->ProcessId != DEBUGGER_EVENT_APPLY_TO_ALL_PROCESSES && CurrentEvent->ProcessId != PsGetCurrentProcessId())
        {
            //
            // This event is not related to either our process or all processes
            //
            continue;
        }

        //
        // Check event type specific conditions
        //
        switch (CurrentEvent->EventType)
        {
        case EXTERNAL_INTERRUPT_OCCURRED:

            //
            // For external interrupt exiting events we check whether the
            // vector match the event's vector or not
            //
            // Context is the physical address
            //
            if (Context != CurrentEvent->OptionalParam1)
            {
                //
                // The interrupt is not for this event
                //
                continue;
            }

            break;

        case HIDDEN_HOOK_READ_AND_WRITE_AND_EXECUTE:
        case HIDDEN_HOOK_READ_AND_WRITE:
        case HIDDEN_HOOK_READ_AND_EXECUTE:
        case HIDDEN_HOOK_WRITE_AND_EXECUTE:
        case HIDDEN_HOOK_READ:
        case HIDDEN_HOOK_WRITE:
        case HIDDEN_HOOK_EXECUTE:

            //
            // For hidden hook read/write/execute we check whether the address
            // is in the range of what user specified or not, this is because
            // we get the events for all hidden hooks in a page granularity
            //

            //
            // Context should be checked in physical address
            //
            if (!(((PEPT_HOOKS_CONTEXT)(Context))->PhysicalAddress >= CurrentEvent->OptionalParam1 && ((PEPT_HOOKS_CONTEXT)(Context))->PhysicalAddress < CurrentEvent->OptionalParam2))
            {
                //
                // The value is not withing our expected range
                //
                continue;
            }
            else
            {
                //
                // Fix the context to virtual address
                //
                Context = ((PEPT_HOOKS_CONTEXT)(Context))->VirtualAddress;
            }

            break;

        case HIDDEN_HOOK_EXEC_CC:

            //
            // Here we check if it's HIDDEN_HOOK_EXEC_CC then it means
            // so we have to make sure to perform its actions only if
            // the hook is triggered for the address described in
            // event, note that address in event is a virtual address
            //
            if (Context != CurrentEvent->OptionalParam1)
            {
                //
                // Context is the virtual address
                //

                //
                // The hook is not for this (virtual) address
                //
                continue;
            }

            break;

        case HIDDEN_HOOK_EXEC_DETOURS:

            //
            // Here we check if it's HIDDEN_HOOK_EXEC_DETOURS
            // then it means that it's detours hidden hook exec so we have
            // to make sure to perform its actions, only if the hook is triggered
            // for the address described in event, note that address in event is
            // a physical address and the address that the function that triggers
            // these events and sent here as the context is also converted to its
            // physical form
            // This way we are sure that no one can bypass our hook by remapping
            // address to another virtual address as everything is physical
            //
            if (((PEPT_HOOKS_CONTEXT)Context)->PhysicalAddress != CurrentEvent->OptionalParam1)
            {
                //
                // Context is the physical address
                //

                //
                // The hook is not for this (physical) address
                //
                continue;
            }
            else
            {
                //
                // Convert it to virtual address
                //
                Context = ((PEPT_HOOKS_CONTEXT)Context)->VirtualAddress;
            }

            break;

        case RDMSR_INSTRUCTION_EXECUTION:
        case WRMSR_INSTRUCTION_EXECUTION:

            //
            // check if MSR exit is what we want or not
            //
            if (CurrentEvent->OptionalParam1 != DEBUGGER_EVENT_MSR_READ_OR_WRITE_ALL_MSRS && CurrentEvent->OptionalParam1 != Context)
            {
                //
                // The msr is not what we want
                //
                continue;
            }

            break;

        case EXCEPTION_OCCURRED:

            //
            // check if exception is what we need or not
            //
            if (CurrentEvent->OptionalParam1 != DEBUGGER_EVENT_EXCEPTIONS_ALL_FIRST_32_ENTRIES && CurrentEvent->OptionalParam1 != Context)
            {
                //
                // The exception is not what we want
                //
                continue;
            }

            break;

        case IN_INSTRUCTION_EXECUTION:
        case OUT_INSTRUCTION_EXECUTION:

            //
            // check if I/O port is what we want or not
            //
            if (CurrentEvent->OptionalParam1 != DEBUGGER_EVENT_ALL_IO_PORTS && CurrentEvent->OptionalParam1 != Context)
            {
                //
                // The port is not what we want
                //
                continue;
            }

            break;

        case SYSCALL_HOOK_EFER_SYSCALL:

            //
            // case SYSCALL_HOOK_EFER_SYSRET:
            //
            // I don't know how to find syscall number when sysret is executed so
            // that's why we don't support extra argument for sysret
            //

            //
            // check syscall number
            //
            if (CurrentEvent->OptionalParam1 != DEBUGGER_EVENT_SYSCALL_ALL_SYSRET_OR_SYSCALLS && CurrentEvent->OptionalParam1 != Context)
            {
                //
                // The syscall number is not what we want
                //
                continue;
            }

            break;

        case CPUID_INSTRUCTION_EXECUTION:

            //
            // check if CPUID is what we want or not
            //
            if (CurrentEvent->OptionalParam1 != NULL /*FALSE*/ && CurrentEvent->OptionalParam2 != Context)
            {
                //
                // The CPUID is not what we want (and the user didn't intend to get all CPUIDs)
                //
                continue;
            }

            break;

        case CONTROL_REGISTER_MODIFIED:

            //
            // check if CR exit is what we want or not
            //
            if (CurrentEvent->OptionalParam1 != Context)
            {
                //
                // The CR is not what we want
                //
                continue;
            }

            break;

        default:
            break;
        }

        //
        // Check the stage of calling (pre, all, or post event)
        //
        if (CallingStage == VMM_CALLBACK_CALLING_STAGE_PRE_EVENT_EMULATION &&
            (CurrentEvent->EventMode == VMM_CALLBACK_CALLING_STAGE_ALL_EVENT_EMULATION ||
             CurrentEvent->EventMode == VMM_CALLBACK_CALLING_STAGE_POST_EVENT_EMULATION))
        {
            //
            // Here it means that the current event is a post, or all event event
            // and the current stage of calling is for the pre-event events, thus
            // this event is not supposed to be runned at the current stage.
            // However, we'll set a flag so the caller will know that there is
            // a valid post-event available for the parameters related to this
            // event.
            // This mechanism notifies the caller to trigger the event after
            // emulation, we implement it in a way that the caller knows when
            // to trigger a post-event thus it optimizes the number of times
            // that the caller triggers the events and avoid unnecessary triggering
            // of the event (for post-event) but at the same time we have the
            // flexibility of having both pre-event and post-event concepts
            //
            *PostEventRequired = TRUE;

            if (CurrentEvent->EventMode == VMM_CALLBACK_CALLING_STAGE_POST_EVENT_EMULATION)
            {
                //
                // If it's not an 'all' event and it is only the 'post' event,
                // then we ignore the trigger stage
                //
                continue;
            }
        }

        //
        // Check if condtion is met or not , if the condition
        // is not met then we have to avoid performing the actions
        //
        if (CurrentEvent->ConditionsBufferSize != 0)
        {
            //
            // Means that there is some conditions
            //
            ConditionFunc = CurrentEvent->ConditionBufferAddress;

            //
            // Run and check for results
            //
            // Because the user might change the nonvolatile registers, we save fastcall nonvolatile registers
            //
            if (AsmDebuggerConditionCodeHandler(DbgState->Regs, Context, ConditionFunc) == 0)
            {
                //
                // The condition function returns null, mean that the
                // condition didn't met, we can ignore this event
                //
                continue;
            }
        }

        //
        // Reset the event ignorance mechanism (apply 'sc on/off' to the events)
        //
        DbgState->ShortCircuitingEvent = CurrentEvent->EnableShortCircuiting;

        //
        // Setup event trigger detail
        //
        EventTriggerDetail.Context = Context;
        EventTriggerDetail.Tag     = CurrentEvent->Tag;
        EventTriggerDetail.Stage   = CallingStage;

        //
        // perform the actions
        //
        DebuggerPerformActions(DbgState, CurrentEvent, &EventTriggerDetail);
    }

    //
    // Check if the event should be ignored or not
    //
    if (DbgState->ShortCircuitingEvent)
    {
        //
        // Reset the event ignorance (short-circuit) mechanism
        //
        DbgState->ShortCircuitingEvent = FALSE;

        //
        // Event should be ignored
        //
        return VMM_CALLBACK_TRIGGERING_EVENT_STATUS_SUCCESSFUL_IGNORE_EVENT;
    }
    else
    {
        //
        // Event shouldn't be ignored
        //
        return VMM_CALLBACK_TRIGGERING_EVENT_STATUS_SUCCESSFUL;
    }
}

/**
 * @brief Run a special event's action(s)
 *
 * @param DbgState The state of the debugger on the current core
 * @param Event Event Object
 * @param EventTriggerDetail Event trigger details
 *
 * @return VOID
 */
VOID
DebuggerPerformActions(PROCESSOR_DEBUGGING_STATE *        DbgState,
                       DEBUGGER_EVENT *                   Event,
                       DEBUGGER_TRIGGERED_EVENT_DETAILS * EventTriggerDetail)
{
    PLIST_ENTRY TempList = 0;

    //
    // Find and run all the actions in this Event
    //
    TempList = &Event->ActionsListHead;
    while (&Event->ActionsListHead != TempList->Flink)
    {
        TempList                             = TempList->Flink;
        PDEBUGGER_EVENT_ACTION CurrentAction = CONTAINING_RECORD(TempList, DEBUGGER_EVENT_ACTION, ActionsList);

        //
        // Perform the action
        //
        switch (CurrentAction->ActionType)
        {
        case BREAK_TO_DEBUGGER:

            DebuggerPerformBreakToDebugger(DbgState, CurrentAction, EventTriggerDetail);

            break;

        case RUN_SCRIPT:

            DebuggerPerformRunScript(DbgState, CurrentAction, NULL, EventTriggerDetail);

            break;

        case RUN_CUSTOM_CODE:

            DebuggerPerformRunTheCustomCode(DbgState, CurrentAction, EventTriggerDetail);

            break;

        default:

            //
            // Invalid action type
            //
            break;
        }
    }
}

/**
 * @brief Managing run script action
 *
 * @param DbgState The state of the debugger on the current core
 * @param Action Action object
 * @param ScriptDetails Details of script
 * @param EventTriggerDetail Event trigger detail
 * @return BOOLEAN
 */
BOOLEAN
DebuggerPerformRunScript(PROCESSOR_DEBUGGING_STATE *        DbgState,
                         DEBUGGER_EVENT_ACTION *            Action,
                         DEBUGGEE_SCRIPT_PACKET *           ScriptDetails,
                         DEBUGGER_TRIGGERED_EVENT_DETAILS * EventTriggerDetail)
{
    SYMBOL_BUFFER                CodeBuffer    = {0};
    ACTION_BUFFER                ActionBuffer  = {0};
    SYMBOL                       ErrorSymbol   = {0};
    SCRIPT_ENGINE_VARIABLES_LIST VariablesList = {0};

    if (Action != NULL)
    {
        //
        // Fill the action buffer's calling stage
        //
        if (EventTriggerDetail->Stage == VMM_CALLBACK_CALLING_STAGE_POST_EVENT_EMULATION)
        {
            ActionBuffer.CallingStage = 1;
        }
        else
        {
            ActionBuffer.CallingStage = 0;
        }

        //
        // Fill the action buffer
        //
        ActionBuffer.Context                   = EventTriggerDetail->Context;
        ActionBuffer.Tag                       = EventTriggerDetail->Tag;
        ActionBuffer.ImmediatelySendTheResults = Action->ImmediatelySendTheResults;
        ActionBuffer.CurrentAction             = Action;

        //
        // Context point to the registers
        //
        CodeBuffer.Head    = Action->ScriptConfiguration.ScriptBuffer;
        CodeBuffer.Size    = Action->ScriptConfiguration.ScriptLength;
        CodeBuffer.Pointer = Action->ScriptConfiguration.ScriptPointer;
    }
    else if (ScriptDetails != NULL)
    {
        //
        // Fill the action buffer's calling stage
        //
        if (EventTriggerDetail->Stage == VMM_CALLBACK_CALLING_STAGE_POST_EVENT_EMULATION)
        {
            ActionBuffer.CallingStage = 1;
        }
        else
        {
            ActionBuffer.CallingStage = 0;
        }

        //
        // Fill the action buffer
        //
        ActionBuffer.Context                   = EventTriggerDetail->Context;
        ActionBuffer.Tag                       = EventTriggerDetail->Tag;
        ActionBuffer.ImmediatelySendTheResults = TRUE;
        ActionBuffer.CurrentAction             = NULL;

        //
        // Context point to the registers
        //
        CodeBuffer.Head    = ((CHAR *)ScriptDetails + sizeof(DEBUGGEE_SCRIPT_PACKET));
        CodeBuffer.Size    = ScriptDetails->ScriptBufferSize;
        CodeBuffer.Pointer = ScriptDetails->ScriptBufferPointer;
    }
    else
    {
        //
        // The parameters are wrong !
        //
        return FALSE;
    }

    //
    // Fill the variables list for this run
    //
    VariablesList.GlobalVariablesList = g_ScriptGlobalVariables;
    VariablesList.LocalVariablesList  = DbgState->ScriptEngineCoreSpecificLocalVariable;
    VariablesList.TempList            = DbgState->ScriptEngineCoreSpecificTempVariable;

    for (int i = 0; i < CodeBuffer.Pointer;)
    {
        //
        // If has error, show error message and abort.
        //

        if (ScriptEngineExecute(DbgState->Regs,
                                &ActionBuffer,
                                &VariablesList,
                                &CodeBuffer,
                                &i,
                                &ErrorSymbol) == TRUE)
        {
            CHAR NameOfOperator[MAX_FUNCTION_NAME_LENGTH] = {0};
            ScriptEngineGetOperatorName(&ErrorSymbol, NameOfOperator);
            LogInfo("Invalid returning address for operator: %s", NameOfOperator);
            break;
        }
    }

    return TRUE;
}

/**
 * @brief Manage running the custom code action
 *
 * @param DbgState The state of the debugger on the current core
 * @param Action Action object
 * @param EventTriggerDetail Event trigger detail
 *
 * @return VOID
 */
VOID
DebuggerPerformRunTheCustomCode(PROCESSOR_DEBUGGING_STATE *        DbgState,
                                DEBUGGER_EVENT_ACTION *            Action,
                                DEBUGGER_TRIGGERED_EVENT_DETAILS * EventTriggerDetail)
{
    if (Action->CustomCodeBufferSize == 0)
    {
        //
        // Sth went wrong ! the buffer size for custom code shouldn't be zero
        //
        return;
    }

    //
    // -----------------------------------------------------------------------------------------------------
    // Test
    //
    // LogInfo("%X       Called from : %llx", Tag, Context);
    //
    //
    // LogInfo("Process Id : %x , Rax : %llx , R8 : %llx , Context : 0x%llx ", PsGetCurrentProcessId(), Regs->rax, Regs->r8, Context);
    // return;
    //
    // -----------------------------------------------------------------------------------------------------
    //

    //
    // Run the custom code
    //
    if (Action->RequestedBuffer.RequestBufferSize == 0)
    {
        //
        // Because the user might change the nonvolatile registers, we save fastcall nonvolatile registers
        //
        AsmDebuggerCustomCodeHandler(NULL,
                                     DbgState->Regs,
                                     EventTriggerDetail->Context,
                                     Action->CustomCodeBufferAddress);
    }
    else
    {
        //
        // Because the user might change the nonvolatile registers, we save fastcall nonvolatile registers
        //
        AsmDebuggerCustomCodeHandler(Action->RequestedBuffer.RequstBufferAddress,
                                     DbgState->Regs,
                                     EventTriggerDetail->Context,
                                     Action->CustomCodeBufferAddress);
    }
}

/**
 * @brief Manage breaking to the debugger action
 *
 * @param DbgState The state of the debugger on the current core
 * @param Tag Tag of event
 * @param Action Action object
 * @param Context Optional parameter
 * @param EventTriggerDetail Event trigger detail
 *
 * @return VOID
 */
VOID
DebuggerPerformBreakToDebugger(PROCESSOR_DEBUGGING_STATE *        DbgState,
                               DEBUGGER_EVENT_ACTION *            Action,
                               DEBUGGER_TRIGGERED_EVENT_DETAILS * EventTriggerDetail)
{
    if (VmFuncVmxGetCurrentExecutionMode() == TRUE)
    {
        //
        // The guest is already in vmx-root mode
        // Halt other cores
        //

        KdHandleBreakpointAndDebugBreakpoints(
            DbgState,
            DEBUGGEE_PAUSING_REASON_DEBUGGEE_EVENT_TRIGGERED,
            EventTriggerDetail);
    }
    else
    {
        //
        // The guest is on vmx non-root mode and this is an event
        //
        VmFuncVmxVmcall(DEBUGGER_VMCALL_VM_EXIT_HALT_SYSTEM_AS_A_RESULT_OF_TRIGGERING_EVENT, EventTriggerDetail, DbgState->Regs, NULL);
    }
}

/**
 * @brief Find event object by tag
 *
 * @param Tag Tag of event
 * @return PDEBUGGER_EVENT Returns null if not found and event object if found
 */
PDEBUGGER_EVENT
DebuggerGetEventByTag(UINT64 Tag)
{
    PLIST_ENTRY TempList  = 0;
    PLIST_ENTRY TempList2 = 0;

    //
    // We have to iterate through all events
    //
    for (size_t i = 0; i < sizeof(DEBUGGER_CORE_EVENTS) / sizeof(LIST_ENTRY); i++)
    {
        TempList  = (PLIST_ENTRY)((UINT64)(g_Events) + (i * sizeof(LIST_ENTRY)));
        TempList2 = TempList;

        while (TempList2 != TempList->Flink)
        {
            TempList                     = TempList->Flink;
            PDEBUGGER_EVENT CurrentEvent = CONTAINING_RECORD(TempList, DEBUGGER_EVENT, EventsOfSameTypeList);

            //
            // Check if we find the event or not
            //
            if (CurrentEvent->Tag == Tag)
            {
                return CurrentEvent;
            }
        }
    }

    //
    // We didn't find anything, so return null
    //
    return NULL;
}

/**
 * @brief Enable or disable all events from all the types
 *
 * @param IsEnable If you want to enable then true and if
 * you want to disable then false
 * @return BOOLEAN if at least one event enabled/disabled then
 * it returns true, and otherwise false
 */
BOOLEAN
DebuggerEnableOrDisableAllEvents(BOOLEAN IsEnable)
{
    BOOLEAN     FindAtLeastOneEvent = FALSE;
    PLIST_ENTRY TempList            = 0;
    PLIST_ENTRY TempList2           = 0;

    //
    // We have to iterate through all events
    //
    for (size_t i = 0; i < sizeof(DEBUGGER_CORE_EVENTS) / sizeof(LIST_ENTRY); i++)
    {
        TempList  = (PLIST_ENTRY)((UINT64)(g_Events) + (i * sizeof(LIST_ENTRY)));
        TempList2 = TempList;

        while (TempList2 != TempList->Flink)
        {
            TempList                     = TempList->Flink;
            PDEBUGGER_EVENT CurrentEvent = CONTAINING_RECORD(TempList, DEBUGGER_EVENT, EventsOfSameTypeList);

            //
            // Check if we find at least one event or not
            //
            if (!FindAtLeastOneEvent)
            {
                FindAtLeastOneEvent = TRUE;
            }

            //
            // Enable or disable event
            //
            CurrentEvent->Enabled = IsEnable;
        }
    }

    return FindAtLeastOneEvent;
}

/**
 * @brief Terminate effect and configuration to vmx-root
 * and non-root for all the events
 *
 * @return BOOLEAN if at least one event terminated then
 * it returns true, and otherwise false
 */
BOOLEAN
DebuggerTerminateAllEvents()
{
    BOOLEAN     FindAtLeastOneEvent = FALSE;
    PLIST_ENTRY TempList            = 0;
    PLIST_ENTRY TempList2           = 0;

    //
    // We have to iterate through all events
    //
    for (size_t i = 0; i < sizeof(DEBUGGER_CORE_EVENTS) / sizeof(LIST_ENTRY); i++)
    {
        TempList  = (PLIST_ENTRY)((UINT64)(g_Events) + (i * sizeof(LIST_ENTRY)));
        TempList2 = TempList;

        while (TempList2 != TempList->Flink)
        {
            TempList                     = TempList->Flink;
            PDEBUGGER_EVENT CurrentEvent = CONTAINING_RECORD(TempList, DEBUGGER_EVENT, EventsOfSameTypeList);

            //
            // Check if we find at least one event or not
            //
            if (!FindAtLeastOneEvent)
            {
                FindAtLeastOneEvent = TRUE;
            }

            //
            // Terminate the current event
            //
            DebuggerTerminateEvent(CurrentEvent->Tag);
        }
    }

    return FindAtLeastOneEvent;
}

/**
 * @brief Remove all the events from all the lists
 * and also de-allocate their structures and actions
 *
 * @details should not be called from vmx-root mode, also
 * it won't terminate their effects, so the events should
 * be terminated first then we can remove them
 *
 * @return BOOLEAN if at least one event removed then
 * it returns true, and otherwise false
 */
BOOLEAN
DebuggerRemoveAllEvents()
{
    BOOLEAN     FindAtLeastOneEvent = FALSE;
    PLIST_ENTRY TempList            = 0;
    PLIST_ENTRY TempList2           = 0;

    //
    // We have to iterate through all events
    //
    for (size_t i = 0; i < sizeof(DEBUGGER_CORE_EVENTS) / sizeof(LIST_ENTRY); i++)
    {
        TempList  = (PLIST_ENTRY)((UINT64)(g_Events) + (i * sizeof(LIST_ENTRY)));
        TempList2 = TempList;

        while (TempList2 != TempList->Flink)
        {
            TempList                     = TempList->Flink;
            PDEBUGGER_EVENT CurrentEvent = CONTAINING_RECORD(TempList, DEBUGGER_EVENT, EventsOfSameTypeList);

            //
            // Check if we find at least one event or not
            //
            if (!FindAtLeastOneEvent)
            {
                FindAtLeastOneEvent = TRUE;
            }

            //
            // Remove the current event
            //
            DebuggerRemoveEvent(CurrentEvent->Tag);
        }
    }

    return FindAtLeastOneEvent;
}

/**
 * @brief Count the list of events in a special list
 *
 * @param TargetEventList target event list
 * @return UINT32 count of events on the list
 */
UINT32
DebuggerEventListCount(PLIST_ENTRY TargetEventList)
{
    PLIST_ENTRY TempList = 0;
    UINT32      Counter  = 0;

    //
    // We have to iterate through all events of this list
    //
    TempList = TargetEventList;

    while (TargetEventList != TempList->Flink)
    {
        TempList                     = TempList->Flink;
        PDEBUGGER_EVENT CurrentEvent = CONTAINING_RECORD(TempList, DEBUGGER_EVENT, EventsOfSameTypeList);

        //
        // Increase the counter
        //
        Counter++;
    }

    return Counter;
}

/**
 * @brief Get List of event based on event type
 *
 * @param EventType type of event
 * @return PLIST_ENTRY
 */
PLIST_ENTRY
DebuggerGetEventListByEventType(VMM_EVENT_TYPE_ENUM EventType)
{
    PLIST_ENTRY ResultList = NULL;
    //
    // Register the event
    //
    switch (EventType)
    {
    case HIDDEN_HOOK_READ_AND_WRITE_AND_EXECUTE:
        ResultList = &g_Events->HiddenHookReadAndWriteAndExecuteEventsHead;
        break;
    case HIDDEN_HOOK_READ_AND_WRITE:
        ResultList = &g_Events->HiddenHookReadAndWriteEventsHead;
        break;
    case HIDDEN_HOOK_READ_AND_EXECUTE:
        ResultList = &g_Events->HiddenHookReadAndExecuteEventsHead;
        break;
    case HIDDEN_HOOK_WRITE_AND_EXECUTE:
        ResultList = &g_Events->HiddenHookWriteAndExecuteEventsHead;
        break;
    case HIDDEN_HOOK_READ:
        ResultList = &g_Events->HiddenHookReadEventsHead;
        break;
    case HIDDEN_HOOK_WRITE:
        ResultList = &g_Events->HiddenHookWriteEventsHead;
        break;
    case HIDDEN_HOOK_EXECUTE:
        ResultList = &g_Events->HiddenHookExecuteEventsHead;
        break;
    case HIDDEN_HOOK_EXEC_DETOURS:
        ResultList = &g_Events->EptHook2sExecDetourEventsHead;
        break;
    case HIDDEN_HOOK_EXEC_CC:
        ResultList = &g_Events->EptHookExecCcEventsHead;
        break;
    case SYSCALL_HOOK_EFER_SYSCALL:
        ResultList = &g_Events->SyscallHooksEferSyscallEventsHead;
        break;
    case SYSCALL_HOOK_EFER_SYSRET:
        ResultList = &g_Events->SyscallHooksEferSysretEventsHead;
        break;
    case CPUID_INSTRUCTION_EXECUTION:
        ResultList = &g_Events->CpuidInstructionExecutionEventsHead;
        break;
    case RDMSR_INSTRUCTION_EXECUTION:
        ResultList = &g_Events->RdmsrInstructionExecutionEventsHead;
        break;
    case WRMSR_INSTRUCTION_EXECUTION:
        ResultList = &g_Events->WrmsrInstructionExecutionEventsHead;
        break;
    case EXCEPTION_OCCURRED:
        ResultList = &g_Events->ExceptionOccurredEventsHead;
        break;
    case TSC_INSTRUCTION_EXECUTION:
        ResultList = &g_Events->TscInstructionExecutionEventsHead;
        break;
    case PMC_INSTRUCTION_EXECUTION:
        ResultList = &g_Events->PmcInstructionExecutionEventsHead;
        break;
    case IN_INSTRUCTION_EXECUTION:
        ResultList = &g_Events->InInstructionExecutionEventsHead;
        break;
    case OUT_INSTRUCTION_EXECUTION:
        ResultList = &g_Events->OutInstructionExecutionEventsHead;
        break;
    case DEBUG_REGISTERS_ACCESSED:
        ResultList = &g_Events->DebugRegistersAccessedEventsHead;
        break;
    case EXTERNAL_INTERRUPT_OCCURRED:
        ResultList = &g_Events->ExternalInterruptOccurredEventsHead;
        break;
    case VMCALL_INSTRUCTION_EXECUTION:
        ResultList = &g_Events->VmcallInstructionExecutionEventsHead;
        break;
    case TRAP_EXECUTION_USER_MODE:
        ResultList = &g_Events->TrapExecutionUserModeEventsHead;
        break;
    case TRAP_EXECUTION_KERNEL_MODE:
        ResultList = &g_Events->TrapExecutionKernelModeEventsHead;
        break;
    case TRAP_EXECUTION_MEMORY:
        ResultList = &g_Events->TrapExecutionMemoryEventsHead;
        break;
    case CONTROL_REGISTER_3_MODIFIED:
        ResultList = &g_Events->ControlRegister3ModifiedEventsHead;
        break;
    case CONTROL_REGISTER_MODIFIED:
        ResultList = &g_Events->ControlRegisterModifiedEventsHead;
        break;
    default:

        //
        // Wrong event type
        //
        LogError("Err, wrong event type is specified");
        ResultList = NULL;
        break;
    }

    return ResultList;
}

/**
 * @brief Count the list of events in a special list that
 * are activate on a target core
 *
 * @param TargetEventList target event list
 * @param TargetCore target core
 * @return UINT32 count of events on the list which is activated
 * on the target core
 */
UINT32
DebuggerEventListCountByCore(PLIST_ENTRY TargetEventList, UINT32 TargetCore)
{
    PLIST_ENTRY TempList = 0;
    UINT32      Counter  = 0;

    //
    // We have to iterate through all events of this list
    //
    TempList = TargetEventList;

    while (TargetEventList != TempList->Flink)
    {
        TempList                     = TempList->Flink;
        PDEBUGGER_EVENT CurrentEvent = CONTAINING_RECORD(TempList, DEBUGGER_EVENT, EventsOfSameTypeList);

        if (CurrentEvent->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES || CurrentEvent->CoreId == TargetCore)
        {
            //
            // Increase the counter
            //
            Counter++;
        }
    }

    return Counter;
}

/**
 * @brief Count the list of events by a special event type that
 * are activate on a target core
 *
 * @param EventType target event type
 * @param TargetCore target core
 *
 * @return UINT32 count of events on the list which is activated
 * on the target core
 */
UINT32
DebuggerEventListCountByEventType(VMM_EVENT_TYPE_ENUM EventType, UINT32 TargetCore)
{
    PLIST_ENTRY TempList = 0;
    UINT32      Counter  = 0;

    PLIST_ENTRY TargetEventList = DebuggerGetEventListByEventType(EventType);

    //
    // We have to iterate through all events of this list
    //
    TempList = TargetEventList;

    while (TargetEventList != TempList->Flink)
    {
        TempList                     = TempList->Flink;
        PDEBUGGER_EVENT CurrentEvent = CONTAINING_RECORD(TempList, DEBUGGER_EVENT, EventsOfSameTypeList);

        if (CurrentEvent->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES || CurrentEvent->CoreId == TargetCore)
        {
            //
            // Increase the counter
            //
            Counter++;
        }
    }

    return Counter;
}

/**
 * @brief Get the mask related to the !exception command for the
 * target core
 *
 * @param CoreIndex The index of core
 *
 * @return UINT32 Returns the current mask for the core
 */
UINT32
DebuggerExceptionEventBitmapMask(UINT32 CoreIndex)
{
    PLIST_ENTRY TempList      = 0;
    UINT32      ExceptionMask = 0;

    //
    // We have to iterate through all events of this list
    //
    TempList = &g_Events->ExceptionOccurredEventsHead;

    while (&g_Events->ExceptionOccurredEventsHead != TempList->Flink)
    {
        TempList                     = TempList->Flink;
        PDEBUGGER_EVENT CurrentEvent = CONTAINING_RECORD(TempList, DEBUGGER_EVENT, EventsOfSameTypeList);

        if (CurrentEvent->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES || CurrentEvent->CoreId == CoreIndex)
        {
            ExceptionMask |= CurrentEvent->OptionalParam1;
        }
    }

    return ExceptionMask;
}

/**
 * @brief Enable an event by tag
 *
 * @param Tag Tag of target event
 * @return BOOLEAN TRUE if event enabled and FALSE if event not
 * found
 */
BOOLEAN
DebuggerEnableEvent(UINT64 Tag)
{
    PDEBUGGER_EVENT Event;
    //
    // Search all the cores for enable this event
    //
    Event = DebuggerGetEventByTag(Tag);

    //
    // Check if tag is valid or not
    //
    if (Event == NULL)
    {
        return FALSE;
    }

    //
    // Enable the event
    //
    Event->Enabled = TRUE;

    return TRUE;
}

/**
 * @brief returns whether an event is enabled/disabled by tag
 * @details this function won't check for Tag validity and if
 * not found then returns false
 *
 * @param Tag Tag of target event
 * @return BOOLEAN TRUE if event enabled and FALSE if event not
 * found
 */
BOOLEAN
DebuggerQueryStateEvent(UINT64 Tag)
{
    PDEBUGGER_EVENT Event;
    //
    // Search all the cores for enable this event
    //
    Event = DebuggerGetEventByTag(Tag);

    //
    // Check if tag is valid or not
    //
    if (Event == NULL)
    {
        return FALSE;
    }

    return Event->Enabled;
}

/**
 * @brief Disable an event by tag
 *
 * @param Tag Tag of target event
 * @return BOOLEAN TRUE if event enabled and FALSE if event not
 * found
 */
BOOLEAN
DebuggerDisableEvent(UINT64 Tag)
{
    PDEBUGGER_EVENT Event;

    //
    // Search all the cores for enable this event
    //
    Event = DebuggerGetEventByTag(Tag);

    //
    // Check if tag is valid or not
    //
    if (Event == NULL)
    {
        return FALSE;
    }

    //
    // Disable the event
    //
    Event->Enabled = FALSE;

    return TRUE;
}

/**
 * @brief Detect whether the tag exists or not
 *
 * @param Tag Tag of target event
 * @return BOOLEAN TRUE if event found and FALSE if event not found
 */
BOOLEAN
DebuggerIsTagValid(UINT64 Tag)
{
    PDEBUGGER_EVENT Event;

    //
    // Search this event
    //
    Event = DebuggerGetEventByTag(Tag);

    //
    // Check if tag is valid or not
    //
    if (Event == NULL)
    {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Detect whether the user or kernel debugger
 * is active or not
 *
 * @return BOOLEAN TRUE if any of the are activated and FALSE if not
 */
BOOLEAN
DebuggerQueryDebuggerStatus()
{
    if (g_KernelDebuggerState || g_UserDebuggerState)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/**
 * @brief Remove the event from event list by its tag
 *
 * @details should not be called from vmx-root mode, also
 * it won't terminate their effects, so the events should
 * be terminated first then we can remove them
 *
 * @param Tag Target events tag
 * @return BOOLEAN If the event was removed then TRUE and FALSE
 * if not found
 */
BOOLEAN
DebuggerRemoveEventFromEventList(UINT64 Tag)
{
    PLIST_ENTRY TempList  = 0;
    PLIST_ENTRY TempList2 = 0;

    //
    // We have to iterate through all events
    //
    for (size_t i = 0; i < sizeof(DEBUGGER_CORE_EVENTS) / sizeof(LIST_ENTRY); i++)
    {
        TempList  = (PLIST_ENTRY)((UINT64)(g_Events) + (i * sizeof(LIST_ENTRY)));
        TempList2 = TempList;

        while (TempList2 != TempList->Flink)
        {
            TempList                     = TempList->Flink;
            PDEBUGGER_EVENT CurrentEvent = CONTAINING_RECORD(TempList, DEBUGGER_EVENT, EventsOfSameTypeList);

            //
            // Check if we find the event or not
            //
            if (CurrentEvent->Tag == Tag)
            {
                //
                // We have to remove the event from the list
                //
                RemoveEntryList(&CurrentEvent->EventsOfSameTypeList);
                return TRUE;
            }
        }
    }

    //
    // We didn't find anything, so return null
    //
    return FALSE;
}

/**
 * @brief Remove the actions and de-allocate its buffer
 *
 * @details should not be called from vmx-root mode, also
 * it won't terminate their effects, so the events should
 * be terminated first then we can remove them *
 *
 * @param Event Event Object
 * @return BOOLEAN TRUE if it was successful and FALSE if not successful
 */
BOOLEAN
DebuggerRemoveAllActionsFromEvent(PDEBUGGER_EVENT Event)
{
    PLIST_ENTRY TempList  = 0;
    PLIST_ENTRY TempList2 = 0;

    //
    // Remove all actions
    //
    TempList  = &Event->ActionsListHead;
    TempList2 = TempList;

    while (TempList2 != TempList->Flink)
    {
        TempList                             = TempList->Flink;
        PDEBUGGER_EVENT_ACTION CurrentAction = CONTAINING_RECORD(TempList, DEBUGGER_EVENT_ACTION, ActionsList);

        //
        // Check if it has a OptionalRequestedBuffer probably for
        // CustomCode
        //
        if (CurrentAction->RequestedBuffer.RequestBufferSize != 0 && CurrentAction->RequestedBuffer.RequstBufferAddress != NULL)
        {
            //
            // There is a buffer
            //
            CrsFreePool(CurrentAction->RequestedBuffer.RequstBufferAddress);
        }

        //
        // Remove the action and free the pool,
        // if it's a custom buffer then the buffer
        // is appended to the Action
        //
        CrsFreePool(CurrentAction);
    }
    //
    // Remember to free the pool
    //
    return TRUE;
}

/**
 * @brief Remove the event by its tags and also remove its actions
 * and de-allocate their buffers
 *
 * @details should not be called from vmx-root mode, also
 * it won't terminate their effects, so the events should
 * be terminated first then we can remove them
 *
 * @param Tag Target event tag
 * @return BOOLEAN TRUE if it was successful and FALSE if not successful
 */
BOOLEAN
DebuggerRemoveEvent(UINT64 Tag)
{
    PDEBUGGER_EVENT Event;
    PLIST_ENTRY     TempList  = 0;
    PLIST_ENTRY     TempList2 = 0;

    //
    // First of all, we disable event
    //
    if (!DebuggerDisableEvent(Tag))
    {
        //
        // Not found, tag is wrong !
        //
        return FALSE;
    }

    //
    // When we're here, we are sure that the tag is valid
    // because if it was not valid, then we have to return
    // for the above function (DebuggerDisableEvent)
    //
    Event = DebuggerGetEventByTag(Tag);

    //
    // Now we get the PDEBUGGER_EVENT so we have to remove
    // it from the event list
    //
    if (!DebuggerRemoveEventFromEventList(Tag))
    {
        return FALSE;
    }

    //
    // Remove all of the actions and free its pools
    //
    DebuggerRemoveAllActionsFromEvent(Event);

    //
    // Free the pools of Event, when we free the pool,
    // ConditionsBufferAddress is also a part of the
    // event pool (ConditionBufferAddress and event
    // are both allocate in a same pool ) so both of
    // them are freed
    //
    CrsFreePool(Event);

    return TRUE;
}

/**
 * @brief Routine for validating and parsing events
 * that came from user-mode
 *
 * @param EventDetails The structure that describes event that came
 * from the user-mode
 * @param BufferLength Length of the buffer
 * @param ResultsToReturnUsermode Result buffer that should be returned to
 * the user-mode
 * @return BOOLEAN TRUE if the event was valid an regisered without error,
 * otherwise returns FALSE
 */
BOOLEAN
DebuggerParseEventFromUsermode(PDEBUGGER_GENERAL_EVENT_DETAIL EventDetails, UINT32 BufferLength, PDEBUGGER_EVENT_AND_ACTION_REG_BUFFER ResultsToReturnUsermode)
{
    PDEBUGGER_EVENT Event;
    UINT64          PagesBytes;
    UINT32          TempPid;
    UINT32          ProcessorCount;
    BOOLEAN         ResultOfApplyingEvent = FALSE;

    ProcessorCount = KeQueryActiveProcessorCount(0);

    //
    // ----------------------------------------------------------------------------------
    // ***                     Validating the Event's parameters                      ***
    // ----------------------------------------------------------------------------------
    //

    //
    // Check whether the event mode (calling stage)  to see whether
    // short-cicuiting event is used along with the post-event,
    // it is because using the short-circuiting mechanism with
    // post-events doesn't make sense; it's not supported!
    //
    if ((EventDetails->EventStage == VMM_CALLBACK_CALLING_STAGE_POST_EVENT_EMULATION ||
         EventDetails->EventStage == VMM_CALLBACK_CALLING_STAGE_ALL_EVENT_EMULATION) &&
        EventDetails->EnableShortCircuiting == TRUE)
    {
        ResultsToReturnUsermode->IsSuccessful = FALSE;
        ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_USING_SHORT_CIRCUITING_EVENT_WITH_POST_EVENT_MODE_IS_FORBIDDEDN;
        return FALSE;
    }

    //
    // Check whether the core Id is valid or not, we read cores count
    // here because we use it in later parts
    //
    if (EventDetails->CoreId != DEBUGGER_EVENT_APPLY_TO_ALL_CORES)
    {
        //
        // Check if the core number is not invalid
        //
        if (EventDetails->CoreId >= ProcessorCount)
        {
            //
            // CoreId is invalid (Set the error)
            //

            ResultsToReturnUsermode->IsSuccessful = FALSE;
            ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_INVALID_CORE_ID;
            return FALSE;
        }
    }

    //
    // Check if process id is valid or not, we won't touch process id here
    // because some of the events use the exact value of DEBUGGER_EVENT_APPLY_TO_ALL_PROCESSES
    //
    if (EventDetails->ProcessId != DEBUGGER_EVENT_APPLY_TO_ALL_PROCESSES && EventDetails->ProcessId != 0)
    {
        //
        // The used specified a special pid, let's check if it's valid or not
        //
        if (!CommonIsProcessExist(EventDetails->ProcessId))
        {
            ResultsToReturnUsermode->IsSuccessful = FALSE;
            ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_INVALID_PROCESS_ID;
            return FALSE;
        }
    }

    if (EventDetails->EventType == EXCEPTION_OCCURRED)
    {
        //
        // Check if the exception entry doesn't exceed the first 32 entry (start from zero)
        //
        if (EventDetails->OptionalParam1 != DEBUGGER_EVENT_EXCEPTIONS_ALL_FIRST_32_ENTRIES && EventDetails->OptionalParam1 >= 31)
        {
            //
            // We don't support entries other than first 32 IDT indexes,
            // it is because we use exception bitmaps and in order to support
            // more than 32 indexes we should use pin-based external interrupt
            // exiting which is completely different
            //
            ResultsToReturnUsermode->IsSuccessful = FALSE;
            ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_EXCEPTION_INDEX_EXCEED_FIRST_32_ENTRIES;
            return FALSE;
        }
    }
    else if (EventDetails->EventType == EXTERNAL_INTERRUPT_OCCURRED)
    {
        //
        // Check if the exception entry is between 32 to 255
        //
        if (!(EventDetails->OptionalParam1 >= 32 && EventDetails->OptionalParam1 <= 0xff))
        {
            //
            // The IDT Entry is either invalid or is not in the range
            // of the pin-based external interrupt exiting controls
            //
            ResultsToReturnUsermode->IsSuccessful = FALSE;
            ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_INTERRUPT_INDEX_IS_NOT_VALID;
            return FALSE;
        }
    }
    else if (EventDetails->EventType == HIDDEN_HOOK_EXEC_DETOURS || EventDetails->EventType == HIDDEN_HOOK_EXEC_CC)
    {
        //
        // First check if the address are valid
        //
        TempPid = EventDetails->ProcessId;
        if (TempPid == DEBUGGER_EVENT_APPLY_TO_ALL_PROCESSES)
        {
            TempPid = PsGetCurrentProcessId();
        }

        if (VirtualAddressToPhysicalAddressByProcessId(EventDetails->OptionalParam1, TempPid) == NULL)
        {
            //
            // Address is invalid (Set the error)
            //

            ResultsToReturnUsermode->IsSuccessful = FALSE;
            ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_INVALID_ADDRESS;
            return FALSE;
        }
    }
    else if (EventDetails->EventType == HIDDEN_HOOK_READ_AND_WRITE_AND_EXECUTE ||
             EventDetails->EventType == HIDDEN_HOOK_READ_AND_WRITE ||
             EventDetails->EventType == HIDDEN_HOOK_READ_AND_EXECUTE ||
             EventDetails->EventType == HIDDEN_HOOK_WRITE_AND_EXECUTE ||
             EventDetails->EventType == HIDDEN_HOOK_READ ||
             EventDetails->EventType == HIDDEN_HOOK_WRITE ||
             EventDetails->EventType == HIDDEN_HOOK_EXECUTE)
    {
        //
        // First check if the address are valid
        //
        TempPid = EventDetails->ProcessId;
        if (TempPid == DEBUGGER_EVENT_APPLY_TO_ALL_PROCESSES)
        {
            TempPid = PsGetCurrentProcessId();
        }

        if (VirtualAddressToPhysicalAddressByProcessId(EventDetails->OptionalParam1, TempPid) == NULL || VirtualAddressToPhysicalAddressByProcessId(EventDetails->OptionalParam2, TempPid) == NULL)
        {
            //
            // Address is invalid (Set the error)
            //

            ResultsToReturnUsermode->IsSuccessful = FALSE;
            ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_INVALID_ADDRESS;
            return FALSE;
        }

        //
        // Check if the 'to' is greater that 'from'
        //
        if (EventDetails->OptionalParam1 >= EventDetails->OptionalParam2)
        {
            ResultsToReturnUsermode->IsSuccessful = FALSE;
            ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_INVALID_ADDRESS;
            return FALSE;
        }
    }

    //
    // ----------------------------------------------------------------------------------
    // Create Event
    // ----------------------------------------------------------------------------------
    //

    //
    // We initialize event with disabled mode as it doesn't have action yet
    //
    if (EventDetails->ConditionBufferSize != 0)
    {
        //
        // Conditional Event
        //
        Event = DebuggerCreateEvent(FALSE,
                                    EventDetails->CoreId,
                                    EventDetails->ProcessId,
                                    EventDetails->EventType,
                                    EventDetails->Tag,
                                    EventDetails->OptionalParam1,
                                    EventDetails->OptionalParam2,
                                    EventDetails->OptionalParam3,
                                    EventDetails->OptionalParam4,
                                    EventDetails->ConditionBufferSize,
                                    (UINT64)EventDetails + sizeof(DEBUGGER_GENERAL_EVENT_DETAIL));
    }
    else
    {
        //
        // Unconditional Event
        //
        Event = DebuggerCreateEvent(FALSE,
                                    EventDetails->CoreId,
                                    EventDetails->ProcessId,
                                    EventDetails->EventType,
                                    EventDetails->Tag,
                                    EventDetails->OptionalParam1,
                                    EventDetails->OptionalParam2,
                                    EventDetails->OptionalParam3,
                                    EventDetails->OptionalParam4,
                                    0,
                                    NULL);
    }

    if (Event == NULL)
    {
        //
        // Set the error
        //
        ResultsToReturnUsermode->IsSuccessful = FALSE;
        ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_UNABLE_TO_CREATE_EVENT;
        return FALSE;
    }

    //
    // Register the event
    //
    DebuggerRegisterEvent(Event);

    //
    // ----------------------------------------------------------------------------------
    // Apply & Enable Event
    // ----------------------------------------------------------------------------------
    //

    //
    // Now we should configure the cpu to generate the events
    //
    switch (EventDetails->EventType)
    {
    case HIDDEN_HOOK_READ_AND_WRITE_AND_EXECUTE:
    case HIDDEN_HOOK_READ_AND_WRITE:
    case HIDDEN_HOOK_READ_AND_EXECUTE:
    case HIDDEN_HOOK_WRITE_AND_EXECUTE:
    case HIDDEN_HOOK_READ:
    case HIDDEN_HOOK_WRITE:
    case HIDDEN_HOOK_EXECUTE:
    {
        //
        // Check if process id is equal to DEBUGGER_EVENT_APPLY_TO_ALL_PROCESSES
        // or if process id is 0 then we use the cr3 of current process
        //
        if (EventDetails->ProcessId == DEBUGGER_EVENT_APPLY_TO_ALL_PROCESSES || EventDetails->ProcessId == 0)
        {
            EventDetails->ProcessId = PsGetCurrentProcessId();
        }

        PagesBytes = PAGE_ALIGN(EventDetails->OptionalParam1);
        PagesBytes = EventDetails->OptionalParam2 - PagesBytes;

        for (size_t i = 0; i <= PagesBytes / PAGE_SIZE; i++)
        {
            //
            // In all the cases we should set both read/write, even if it's only
            // read we should set the write too!
            // Also execute bit has the same conditions here, because if write is set
            // read should be also set
            //
            switch (EventDetails->EventType)
            {
            case HIDDEN_HOOK_READ_AND_WRITE_AND_EXECUTE:
            case HIDDEN_HOOK_READ_AND_EXECUTE:

                ResultOfApplyingEvent = DebuggerEventEnableMonitorReadWriteExec((UINT64)EventDetails->OptionalParam1 + (i * PAGE_SIZE),
                                                                                EventDetails->ProcessId,
                                                                                TRUE,
                                                                                TRUE,
                                                                                TRUE);
                break;

            case HIDDEN_HOOK_WRITE_AND_EXECUTE:

                ResultOfApplyingEvent = DebuggerEventEnableMonitorReadWriteExec((UINT64)EventDetails->OptionalParam1 + (i * PAGE_SIZE),
                                                                                EventDetails->ProcessId,
                                                                                FALSE,
                                                                                TRUE,
                                                                                FALSE);
                break;

            case HIDDEN_HOOK_READ_AND_WRITE:
            case HIDDEN_HOOK_READ:
                ResultOfApplyingEvent = DebuggerEventEnableMonitorReadWriteExec((UINT64)EventDetails->OptionalParam1 + (i * PAGE_SIZE),
                                                                                EventDetails->ProcessId,
                                                                                TRUE,
                                                                                TRUE,
                                                                                FALSE);

                break;

            case HIDDEN_HOOK_WRITE:
                ResultOfApplyingEvent = DebuggerEventEnableMonitorReadWriteExec((UINT64)EventDetails->OptionalParam1 + (i * PAGE_SIZE),
                                                                                EventDetails->ProcessId,
                                                                                FALSE,
                                                                                TRUE,
                                                                                FALSE);

                break;

            case HIDDEN_HOOK_EXECUTE:
                ResultOfApplyingEvent = DebuggerEventEnableMonitorReadWriteExec((UINT64)EventDetails->OptionalParam1 + (i * PAGE_SIZE),
                                                                                EventDetails->ProcessId,
                                                                                FALSE,
                                                                                FALSE,
                                                                                TRUE);
                break;

            default:
                LogError("Err, Invalid monitor hook type");

                ResultsToReturnUsermode->IsSuccessful = FALSE;
                ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_EVENT_TYPE_IS_INVALID;

                goto ClearTheEventAfterCreatingEvent;

                break;
            }

            if (!ResultOfApplyingEvent)
            {
                //
                // The event is not applied, won't apply other EPT modifications
                // as we want to remove this event
                //

                //
                // Now we should restore the previously applied events (if any)
                //
                for (size_t j = 0; j < i; j++)
                {
                    ConfigureEptHookUnHookSingleAddress((UINT64)EventDetails->OptionalParam1 + (j * PAGE_SIZE), NULL, Event->ProcessId);
                }

                break;
            }
            else
            {
                //
                // We applied the hook and the pre-allocated buffers are used
                // for this hook, as here is a safe PASSIVE_LEVEL we can force
                // the Windows to reallocate some pools for us, thus, if this
                // hook is continued to other pages, we still have pre-alloated
                // buffers ready for our future hooks
                //
                PoolManagerCheckAndPerformAllocationAndDeallocation();
            }
        }

        //
        // We convert the Event's optional parameters physical address because
        // vm-exit occurs and we have the physical address to compare in the case of
        // hidden hook rw events.
        //
        Event->OptionalParam1 = VirtualAddressToPhysicalAddressByProcessId(EventDetails->OptionalParam1, EventDetails->ProcessId);
        Event->OptionalParam2 = VirtualAddressToPhysicalAddressByProcessId(EventDetails->OptionalParam2, EventDetails->ProcessId);
        Event->OptionalParam3 = EventDetails->OptionalParam1;
        Event->OptionalParam4 = EventDetails->OptionalParam2;

        //
        // Check if we should restore the event if it was not successful
        //
        if (!ResultOfApplyingEvent)
        {
            ResultsToReturnUsermode->IsSuccessful = FALSE;
            ResultsToReturnUsermode->Error        = DebuggerGetLastError();

            goto ClearTheEventAfterCreatingEvent;
        }

        break;
    }
    case HIDDEN_HOOK_EXEC_CC:
    {
        //
        // Check if process id is equal to DEBUGGER_EVENT_APPLY_TO_ALL_PROCESSES
        // or if process id is 0 then we use the cr3 of current process
        //
        if (EventDetails->ProcessId == DEBUGGER_EVENT_APPLY_TO_ALL_PROCESSES || EventDetails->ProcessId == 0)
        {
            EventDetails->ProcessId = PsGetCurrentProcessId();
        }

        //
        // Invoke the hooker
        //
        if (!ConfigureEptHook(EventDetails->OptionalParam1, EventDetails->ProcessId))
        {
            //
            // There was an error applying this event, so we're setting
            // the event
            //
            ResultsToReturnUsermode->IsSuccessful = FALSE;
            ResultsToReturnUsermode->Error        = DebuggerGetLastError();
            goto ClearTheEventAfterCreatingEvent;
        }

        //
        // We set events OptionalParam1 here to make sure that our event is
        // executed not for all hooks but for this special hook
        //
        Event->OptionalParam1 = EventDetails->OptionalParam1;

        break;
    }
    case HIDDEN_HOOK_EXEC_DETOURS:
    {
        //
        // Check if process id is equal to DEBUGGER_EVENT_APPLY_TO_ALL_PROCESSES
        // or if process id is 0 then we use the cr3 of current process
        //
        if (EventDetails->ProcessId == DEBUGGER_EVENT_APPLY_TO_ALL_PROCESSES || EventDetails->ProcessId == 0)
        {
            EventDetails->ProcessId = PsGetCurrentProcessId();
        }

        //
        // Invoke the hooker
        //
        if (!ConfigureEptHook2(PsGetCurrentProcessId(), EventDetails->OptionalParam1, NULL, EventDetails->ProcessId, FALSE, FALSE, FALSE, TRUE))
        {
            //
            // There was an error applying this event, so we're setting
            // the event
            //
            ResultsToReturnUsermode->IsSuccessful = FALSE;
            ResultsToReturnUsermode->Error        = DebuggerGetLastError();
            goto ClearTheEventAfterCreatingEvent;
        }

        //
        // We set events OptionalParam1 here to make sure that our event is
        // executed not for all hooks but for this special hook
        // Also, we are sure that this is not null because we checked it before
        //
        Event->OptionalParam1 = VirtualAddressToPhysicalAddressByProcessId(EventDetails->OptionalParam1, EventDetails->ProcessId);

        break;
    }
    case RDMSR_INSTRUCTION_EXECUTION:
    {
        //
        // KEEP IN MIND, WE USED THIS METHOD TO RE-APPLY THE EVENT ON
        // TERMINATION ROUTINES, IF YOU WANT TO CHANGE IT, YOU SHOULD
        // CHANGE THE TERMINATION.C RELATED FUNCTION TOO
        //

        //
        // Let's see if it is for all cores or just one core
        //
        if (EventDetails->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES)
        {
            //
            // All cores
            //
            ExtensionCommandChangeAllMsrBitmapReadAllCores(EventDetails->OptionalParam1);
        }
        else
        {
            //
            // Just one core
            //
            ConfigureChangeMsrBitmapReadOnSingleCore(EventDetails->CoreId, EventDetails->OptionalParam1);
        }

        //
        // Setting an indicator to MSR
        //
        Event->OptionalParam1 = EventDetails->OptionalParam1;

        break;
    }
    case WRMSR_INSTRUCTION_EXECUTION:
    {
        //
        // KEEP IN MIND, WE USED THIS METHOD TO RE-APPLY THE EVENT ON
        // TERMINATION ROUTINES, IF YOU WANT TO CHANGE IT, YOU SHOULD
        // CHANGE THE TERMINATION.C RELATED FUNCTION TOO
        //

        //
        // Let's see if it is for all cores or just one core
        //
        if (EventDetails->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES)
        {
            //
            // All cores
            //
            ExtensionCommandChangeAllMsrBitmapWriteAllCores(EventDetails->OptionalParam1);
        }
        else
        {
            //
            // Just one core
            //
            ConfigureChangeMsrBitmapWriteOnSingleCore(EventDetails->CoreId, EventDetails->OptionalParam1);
        }

        //
        // Setting an indicator to MSR
        //
        Event->OptionalParam1 = EventDetails->OptionalParam1;

        break;
    }
    case IN_INSTRUCTION_EXECUTION:
    case OUT_INSTRUCTION_EXECUTION:
    {
        //
        // Let's see if it is for all cores or just one core
        //
        if (EventDetails->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES)
        {
            //
            // All cores
            //
            ExtensionCommandIoBitmapChangeAllCores(EventDetails->OptionalParam1);
        }
        else
        {
            //
            // Just one core
            //
            ConfigureChangeIoBitmapOnSingleCore(EventDetails->CoreId, EventDetails->OptionalParam1);
        }

        //
        // Setting an indicator to MSR
        //
        Event->OptionalParam1 = EventDetails->OptionalParam1;

        break;
    }
    case TSC_INSTRUCTION_EXECUTION:
    {
        //
        // KEEP IN MIND, WE USED THIS METHOD TO RE-APPLY THE EVENT ON
        // TERMINATION ROUTINES, IF YOU WANT TO CHANGE IT, YOU SHOULD
        // CHANGE THE TERMINATION.C RELATED FUNCTION TOO
        //

        //
        // Let's see if it is for all cores or just one core
        //
        if (EventDetails->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES)
        {
            //
            // All cores
            //
            ExtensionCommandEnableRdtscExitingAllCores();
        }
        else
        {
            //
            // Just one core
            //
            ConfigureEnableRdtscExitingOnSingleCore(EventDetails->CoreId);
        }

        break;
    }
    case PMC_INSTRUCTION_EXECUTION:
    {
        //
        // KEEP IN MIND, WE USED THIS METHOD TO RE-APPLY THE EVENT ON
        // TERMINATION ROUTINES, IF YOU WANT TO CHANGE IT, YOU SHOULD
        // CHANGE THE TERMINATION.C RELATED FUNCTION TOO
        //

        //
        // Let's see if it is for all cores or just one core
        //
        if (EventDetails->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES)
        {
            //
            // All cores
            //
            ExtensionCommandEnableRdpmcExitingAllCores();
        }
        else
        {
            //
            // Just one core
            //
            ConfigureEnableRdpmcExitingOnSingleCore(EventDetails->CoreId);
        }

        break;
    }
    case DEBUG_REGISTERS_ACCESSED:
    {
        //
        // KEEP IN MIND, WE USED THIS METHOD TO RE-APPLY THE EVENT ON
        // TERMINATION ROUTINES, IF YOU WANT TO CHANGE IT, YOU SHOULD
        // CHANGE THE TERMINATION.C RELATED FUNCTION TOO
        //

        //
        // Let's see if it is for all cores or just one core
        //
        if (EventDetails->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES)
        {
            //
            // All cores
            //
            ExtensionCommandEnableMovDebugRegistersExitingAllCores();
        }
        else
        {
            //
            // Just one core
            //
            ConfigureEnableMovToDebugRegistersExitingOnSingleCore(EventDetails->CoreId);
        }

        break;
    }
    case CONTROL_REGISTER_MODIFIED:
    {
        //
        // KEEP IN MIND, WE USED THIS METHOD TO RE-APPLY THE EVENT ON
        // TERMINATION ROUTINES, IF YOU WANT TO CHANGE IT, YOU SHOULD
        // CHANGE THE TERMINATION.C RELATED FUNCTION TOO
        //

        //
        // Setting an indicator to CR
        //
        Event->OptionalParam1 = EventDetails->OptionalParam1;
        Event->OptionalParam2 = EventDetails->OptionalParam2;

        //
        // Let's see if it is for all cores or just one core
        //
        if (EventDetails->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES)
        {
            //
            // All cores
            //
            ExtensionCommandEnableMovControlRegisterExitingAllCores(Event);
        }
        else
        {
            //
            // Just one core
            //
            DEBUGGER_BROADCASTING_OPTIONS BroadcastingOption = {0};

            BroadcastingOption.OptionalParam1 = Event->OptionalParam1;
            BroadcastingOption.OptionalParam2 = Event->OptionalParam2;

            ConfigureEnableMovToControlRegisterExitingOnSingleCore(EventDetails->CoreId, &BroadcastingOption);
        }

        break;
    }
    case EXCEPTION_OCCURRED:
    {
        //
        // KEEP IN MIND, WE USED THIS METHOD TO RE-APPLY THE EVENT ON
        // TERMINATION ROUTINES, IF YOU WANT TO CHANGE IT, YOU SHOULD
        // CHANGE THE TERMINATION.C RELATED FUNCTION TOO
        //

        //
        // Let's see if it is for all cores or just one core
        //

        if (EventDetails->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES)
        {
            //
            // All cores
            //
            ExtensionCommandSetExceptionBitmapAllCores(EventDetails->OptionalParam1);
        }
        else
        {
            //
            // Just one core
            //
            ConfigureSetExceptionBitmapOnSingleCore(EventDetails->CoreId, EventDetails->OptionalParam1);
        }

        //
        // Set the event's target exception
        //
        Event->OptionalParam1 = EventDetails->OptionalParam1;

        break;
    }
    case EXTERNAL_INTERRUPT_OCCURRED:
    {
        //
        // KEEP IN MIND, WE USED THIS METHOD TO RE-APPLY THE EVENT ON
        // TERMINATION ROUTINES, IF YOU WANT TO CHANGE IT, YOU SHOULD
        // CHANGE THE TERMINATION.C RELATED FUNCTION TOO
        //

        //
        // Let's see if it is for all cores or just one core
        //
        if (EventDetails->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES)
        {
            //
            // All cores
            //
            ExtensionCommandSetExternalInterruptExitingAllCores();
        }
        else
        {
            //
            // Just one core
            //
            ConfigureSetExternalInterruptExitingOnSingleCore(EventDetails->CoreId);
        }

        //
        // Set the event's target interrupt
        //
        Event->OptionalParam1 = EventDetails->OptionalParam1;

        break;
    }
    case SYSCALL_HOOK_EFER_SYSCALL:
    {
        //
        // KEEP IN MIND, WE USED THIS METHOD TO RE-APPLY THE EVENT ON
        // TERMINATION ROUTINES, IF YOU WANT TO CHANGE IT, YOU SHOULD
        // CHANGE THE TERMINATION.C RELATED FUNCTION TOO
        //

        DEBUGGER_EVENT_SYSCALL_SYSRET_TYPE SyscallHookType = DEBUGGER_EVENT_SYSCALL_SYSRET_SAFE_ACCESS_MEMORY;

        //
        // whether it's a !syscall2 or !sysret2
        //
        if (EventDetails->OptionalParam2 == DEBUGGER_EVENT_SYSCALL_SYSRET_HANDLE_ALL_UD)
        {
            SyscallHookType = DEBUGGER_EVENT_SYSCALL_SYSRET_HANDLE_ALL_UD;
        }
        else if (EventDetails->OptionalParam2 == DEBUGGER_EVENT_SYSCALL_SYSRET_SAFE_ACCESS_MEMORY)
        {
            SyscallHookType = DEBUGGER_EVENT_SYSCALL_SYSRET_SAFE_ACCESS_MEMORY;
        }

        //
        // Let's see if it is for all cores or just one core
        //
        if (EventDetails->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES)
        {
            //
            // All cores
            //
            DebuggerEventEnableEferOnAllProcessors(SyscallHookType);
        }
        else
        {
            //
            // Just one core
            //
            ConfigureEnableEferSyscallHookOnSingleCore(EventDetails->CoreId, SyscallHookType);
        }

        //
        // Set the event's target syscall number and save the approach
        // of handling event details
        //
        Event->OptionalParam1 = EventDetails->OptionalParam1;
        Event->OptionalParam2 = SyscallHookType;

        break;
    }
    case SYSCALL_HOOK_EFER_SYSRET:
    {
        //
        // KEEP IN MIND, WE USED THIS METHOD TO RE-APPLY THE EVENT ON
        // TERMINATION ROUTINES, IF YOU WANT TO CHANGE IT, YOU SHOULD
        // CHANGE THE TERMINATION.C RELATED FUNCTION TOO
        //

        DEBUGGER_EVENT_SYSCALL_SYSRET_TYPE SyscallHookType = DEBUGGER_EVENT_SYSCALL_SYSRET_SAFE_ACCESS_MEMORY;

        //
        // whether it's a !syscall2 or !sysret2
        //
        if (EventDetails->OptionalParam2 == DEBUGGER_EVENT_SYSCALL_SYSRET_HANDLE_ALL_UD)
        {
            SyscallHookType = DEBUGGER_EVENT_SYSCALL_SYSRET_HANDLE_ALL_UD;
        }
        else if (EventDetails->OptionalParam2 == DEBUGGER_EVENT_SYSCALL_SYSRET_SAFE_ACCESS_MEMORY)
        {
            SyscallHookType = DEBUGGER_EVENT_SYSCALL_SYSRET_SAFE_ACCESS_MEMORY;
        }

        //
        // Let's see if it is for all cores or just one core
        //
        if (EventDetails->CoreId == DEBUGGER_EVENT_APPLY_TO_ALL_CORES)
        {
            //
            // All cores
            //
            DebuggerEventEnableEferOnAllProcessors(SyscallHookType);
        }
        else
        {
            //
            // Just one core
            //
            ConfigureEnableEferSyscallHookOnSingleCore(EventDetails->CoreId, SyscallHookType);
        }

        //
        // Set the event's target syscall number and save the approach
        // of handling event details
        //
        Event->OptionalParam1 = EventDetails->OptionalParam1;
        Event->OptionalParam2 = SyscallHookType;

        break;
    }
    case VMCALL_INSTRUCTION_EXECUTION:
    {
        //
        // Enable triggering events for VMCALLs
        // This event doesn't support custom optional
        // parameter(s) because it's unconditional
        // users can use condition(s) to check for
        // their custom optional parameters
        //
        VmFuncSetTriggerEventForVmcalls(TRUE);

        break;
    }
    case TRAP_EXECUTION_USER_MODE:
    {
        //
        // Enable triggering events for user-mode execution
        // traps. This event doesn't support custom optional
        // parameter(s) because it's unconditional users can
        // use condition(s) to check for their custom optional
        // parameters
        //
        ConfigureInitializeExecTrapOnAllProcessors();

        //
        // Add the process to the watching list
        //
        ConfigureExecTrapAddProcessToWatchingList(EventDetails->ProcessId);

        break;
    }
    case CPUID_INSTRUCTION_EXECUTION:
    {
        //
        // Enable triggering events for CPUIDs
        // This event doesn't support custom optional
        // parameter(s) because it's unconditional
        // users can use condition(s) to check for
        // their custom optional parameters
        //
        VmFuncSetTriggerEventForCpuids(TRUE);

        break;
    }
    default:
    {
        //
        // Set the error
        //
        ResultsToReturnUsermode->IsSuccessful = FALSE;
        ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_EVENT_TYPE_IS_INVALID;
        goto ClearTheEventAfterCreatingEvent;

        break;
    }
    }

    //
    // Set the short-circuiting state
    //
    Event->EnableShortCircuiting = EventDetails->EnableShortCircuiting;

    //
    // Set the event stage (pre- post- event)
    //
    if (EventDetails->EventStage == VMM_CALLBACK_CALLING_STAGE_POST_EVENT_EMULATION)
    {
        Event->EventMode = VMM_CALLBACK_CALLING_STAGE_POST_EVENT_EMULATION;
    }
    else if (EventDetails->EventStage == VMM_CALLBACK_CALLING_STAGE_ALL_EVENT_EMULATION)
    {
        Event->EventMode = VMM_CALLBACK_CALLING_STAGE_ALL_EVENT_EMULATION;
    }
    else
    {
        //
        // Any other value results to be pre-event
        //
        Event->EventMode = VMM_CALLBACK_CALLING_STAGE_PRE_EVENT_EMULATION;
    }

    //
    // Set the status
    //
    ResultsToReturnUsermode->IsSuccessful = TRUE;
    ResultsToReturnUsermode->Error        = 0;

    //
    // Event was applied successfully
    //
    return TRUE;

ClearTheEventAfterCreatingEvent:

    //
    // Remove the event as it was not successfull
    //
    if (Event != NULL)
    {
        DebuggerRemoveEvent(Event->Tag);
    }

    return FALSE;
}

/**
 * @brief Routine for validating and parsing actions that are comming from
 * the user-mode
 * @details should be called in vmx root-mode
 *
 * @param Action Structure that describes the action that comes from the
 * user-mode
 * @param BufferLength Length of the buffer that comes from user-mode
 * @param ResultsToReturnUsermode The buffer address that should be returned
 * to the user-mode as the result
 * @return BOOLEAN if action was parsed and added successfully, return TRUE
 * otherwise, returns FALSE
 */
BOOLEAN
DebuggerParseActionFromUsermode(PDEBUGGER_GENERAL_ACTION Action, UINT32 BufferLength, PDEBUGGER_EVENT_AND_ACTION_REG_BUFFER ResultsToReturnUsermode)
{
    //
    // Check if Tag is valid or not
    //
    PDEBUGGER_EVENT Event = DebuggerGetEventByTag(Action->EventTag);

    if (Event == NULL)
    {
        //
        // Set the appropriate error
        //
        ResultsToReturnUsermode->IsSuccessful = FALSE;
        ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_TAG_NOT_EXISTS;

        //
        // Show that the
        //
        return FALSE;
    }

    if (Action->ActionType == RUN_CUSTOM_CODE)
    {
        //
        // Check if buffer is not invalid
        //
        if (Action->CustomCodeBufferSize == 0)
        {
            //
            // Set the appropriate error
            //
            ResultsToReturnUsermode->IsSuccessful = FALSE;
            ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_ACTION_BUFFER_SIZE_IS_ZERO;

            //
            // Show that the
            //
            return FALSE;
        }

        //
        // Add action for RUN_CUSTOM_CODE
        //
        DEBUGGER_EVENT_REQUEST_CUSTOM_CODE CustomCode = {0};

        CustomCode.CustomCodeBufferSize        = Action->CustomCodeBufferSize;
        CustomCode.CustomCodeBufferAddress     = (UINT64)Action + sizeof(DEBUGGER_GENERAL_ACTION);
        CustomCode.OptionalRequestedBufferSize = Action->PreAllocatedBuffer;

        //
        // Add action to event
        //
        DebuggerAddActionToEvent(Event, RUN_CUSTOM_CODE, Action->ImmediateMessagePassing, &CustomCode, NULL);

        //
        // Enable the event
        //
        DebuggerEnableEvent(Event->Tag);
    }
    else if (Action->ActionType == RUN_SCRIPT)
    {
        //
        // Check if buffer is not invalid
        //
        if (Action->ScriptBufferSize == 0)
        {
            //
            // Set the appropriate error
            //
            ResultsToReturnUsermode->IsSuccessful = FALSE;
            ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_ACTION_BUFFER_SIZE_IS_ZERO;

            //
            // Show that the
            //
            return FALSE;
        }

        //
        // Add action for RUN_SCRIPT
        //
        DEBUGGER_EVENT_ACTION_RUN_SCRIPT_CONFIGURATION UserScriptConfig = {0};
        UserScriptConfig.ScriptBuffer                                   = (UINT64)Action + sizeof(DEBUGGER_GENERAL_ACTION);
        UserScriptConfig.ScriptLength                                   = Action->ScriptBufferSize;
        UserScriptConfig.ScriptPointer                                  = Action->ScriptBufferPointer;
        UserScriptConfig.OptionalRequestedBufferSize                    = Action->PreAllocatedBuffer;

        DebuggerAddActionToEvent(Event, RUN_SCRIPT, Action->ImmediateMessagePassing, NULL, &UserScriptConfig);

        //
        // Enable the event
        //
        DebuggerEnableEvent(Event->Tag);
    }
    else if (Action->ActionType == BREAK_TO_DEBUGGER)
    {
        //
        // Add action BREAK_TO_DEBUGGER to event
        //
        DebuggerAddActionToEvent(Event, BREAK_TO_DEBUGGER, Action->ImmediateMessagePassing, NULL, NULL);

        //
        // Enable the event
        //
        DebuggerEnableEvent(Event->Tag);
    }
    else
    {
        //
        // Set the appropriate error
        //
        ResultsToReturnUsermode->IsSuccessful = FALSE;
        ResultsToReturnUsermode->Error        = DEBUGGER_ERROR_INVALID_ACTION_TYPE;

        //
        // Show that the
        //
        return FALSE;
    }

    ResultsToReturnUsermode->IsSuccessful = TRUE;
    ResultsToReturnUsermode->Error        = 0;

    return TRUE;
}

/**
 * @brief Terminate one event's effect by its tag
 *
 * @details This function won't remove the event from
 * the lists of event or de-allocated them, this should
 * be called BEFORE the removing function
 *
 * @param Tag Target event's tag
 * @return BOOLEAN if it was found and terminated without error
 * then it returns TRUE, otherwise FALSE
 */
BOOLEAN
DebuggerTerminateEvent(UINT64 Tag)
{
    PDEBUGGER_EVENT Event;

    //
    // Find the event by its tag
    //
    Event = DebuggerGetEventByTag(Tag);

    if (Event == NULL)
    {
        //
        // event, not found
        //
        return FALSE;
    }

    //
    // Check the event type of our specific tag
    //
    switch (Event->EventType)
    {
    case EXTERNAL_INTERRUPT_OCCURRED:
    {
        //
        // Call external interrupt terminator
        //
        TerminateExternalInterruptEvent(Event);

        break;
    }
    case HIDDEN_HOOK_READ_AND_WRITE_AND_EXECUTE:
    case HIDDEN_HOOK_READ_AND_WRITE:
    case HIDDEN_HOOK_READ_AND_EXECUTE:
    case HIDDEN_HOOK_WRITE_AND_EXECUTE:
    case HIDDEN_HOOK_READ:
    case HIDDEN_HOOK_WRITE:
    case HIDDEN_HOOK_EXECUTE:
    {
        //
        // Call read and write and execute ept hook terminator
        //
        TerminateHiddenHookReadAndWriteAndExecuteEvent(Event);

        break;
    }
    case HIDDEN_HOOK_EXEC_CC:
    {
        //
        // Call ept hook (hidden breakpoint) terminator
        //
        TerminateHiddenHookExecCcEvent(Event);

        break;
    }
    case HIDDEN_HOOK_EXEC_DETOURS:
    {
        //
        // Call ept hook (hidden inline hook) terminator
        //
        TerminateHiddenHookExecDetoursEvent(Event);

        break;
    }
    case RDMSR_INSTRUCTION_EXECUTION:
    {
        //
        // Call rdmsr execution event terminator
        //
        TerminateRdmsrExecutionEvent(Event);

        break;
    }
    case WRMSR_INSTRUCTION_EXECUTION:
    {
        //
        // Call wrmsr execution event terminator
        //
        TerminateWrmsrExecutionEvent(Event);

        break;
    }
    case EXCEPTION_OCCURRED:
    {
        //
        // Call exception events terminator
        //
        TerminateExceptionEvent(Event);

        break;
    }
    case IN_INSTRUCTION_EXECUTION:
    {
        //
        // Call IN instruction execution event terminator
        //
        TerminateInInstructionExecutionEvent(Event);

        break;
    }
    case OUT_INSTRUCTION_EXECUTION:
    {
        //
        // Call OUT instruction execution event terminator
        //
        TerminateOutInstructionExecutionEvent(Event);

        break;
    }
    case SYSCALL_HOOK_EFER_SYSCALL:
    {
        //
        // Call syscall hook event terminator
        //
        TerminateSyscallHookEferEvent(Event);

        break;
    }
    case SYSCALL_HOOK_EFER_SYSRET:
    {
        //
        // Call sysret hook event terminator
        //
        TerminateSysretHookEferEvent(Event);

        break;
    }
    case VMCALL_INSTRUCTION_EXECUTION:
    {
        //
        // Call vmcall instruction execution event terminator
        //
        TerminateVmcallExecutionEvent(Event);

        break;
    }
    case TRAP_EXECUTION_USER_MODE:
    {
        //
        // To-do
        //

        break;
    }
    case TSC_INSTRUCTION_EXECUTION:
    {
        //
        // Call rdtsc/rdtscp instruction execution event terminator
        //
        TerminateTscEvent(Event);

        break;
    }
    case PMC_INSTRUCTION_EXECUTION:
    {
        //
        // Call rdtsc/rdtscp instructions execution event terminator
        //
        TerminatePmcEvent(Event);

        break;
    }
    case DEBUG_REGISTERS_ACCESSED:
    {
        //
        // Call mov to debugger register event terminator
        //
        TerminateDebugRegistersEvent(Event);

        break;
    }
    case CPUID_INSTRUCTION_EXECUTION:
    {
        //
        // Call cpuid instruction execution event terminator
        //
        TerminateCpuidExecutionEvent(Event);

        break;
    }
    case CONTROL_REGISTER_MODIFIED:
    {
        //
        // Call mov to control register event terminator
        //
        TerminateControlRegistersEvent(Event);
        break;
    }
    default:
        LogError("Err, unknown event for termination");
        break;
    }
}

/**
 * @brief Parse and validate requests to enable/disable/clear
 * from the user-mode
 *
 * @param DebuggerEventModificationRequest event modification request details
 * @return BOOLEAN returns TRUE if there was no error, and FALSE if there was
 * an error
 */
BOOLEAN
DebuggerParseEventsModificationFromUsermode(PDEBUGGER_MODIFY_EVENTS DebuggerEventModificationRequest)
{
    BOOLEAN IsForAllEvents = FALSE;

    //
    // Check if the tag is valid or not
    //
    if (DebuggerEventModificationRequest->Tag == DEBUGGER_MODIFY_EVENTS_APPLY_TO_ALL_TAG)
    {
        IsForAllEvents = TRUE;
    }
    else if (!DebuggerIsTagValid(DebuggerEventModificationRequest->Tag))
    {
        //
        // Tag is invalid
        //
        DebuggerEventModificationRequest->KernelStatus = DEBUGGER_ERROR_MODIFY_EVENTS_INVALID_TAG;

        return FALSE;
    }

    //
    // ***************************************************************************
    //

    //
    // Check if it's a ENABLE, DISABLE or CLEAR
    //
    if (DebuggerEventModificationRequest->TypeOfAction == DEBUGGER_MODIFY_EVENTS_ENABLE)
    {
        if (IsForAllEvents)
        {
            //
            // Enable all events
            //
            DebuggerEnableOrDisableAllEvents(TRUE);
        }
        else
        {
            //
            // Enable just one event
            //
            DebuggerEnableEvent(DebuggerEventModificationRequest->Tag);
        }
    }
    else if (DebuggerEventModificationRequest->TypeOfAction == DEBUGGER_MODIFY_EVENTS_DISABLE)
    {
        if (IsForAllEvents)
        {
            //
            // Disable all events
            //
            DebuggerEnableOrDisableAllEvents(FALSE);
        }
        else
        {
            //
            // Disable just one event
            //
            DebuggerDisableEvent(DebuggerEventModificationRequest->Tag);
        }
    }
    else if (DebuggerEventModificationRequest->TypeOfAction == DEBUGGER_MODIFY_EVENTS_CLEAR)
    {
        if (IsForAllEvents)
        {
            //
            // Clear all events
            //

            //
            // Because we want to delete all the objects and buffers (pools)
            // after we finished termination, the debugger might still use
            // the buffers for events and action, for solving this problem
            // we first disable the tag(s) and this way the debugger no longer
            // use that event and this way we can safely remove and deallocate
            // the buffers later after termination
            //

            //
            // First, disable all events
            //
            DebuggerEnableOrDisableAllEvents(FALSE);

            //
            // Second, terminate all events
            //
            DebuggerTerminateAllEvents();

            //
            // Third, remove all events
            //
            DebuggerRemoveAllEvents();
        }
        else
        {
            //
            // Clear just one event
            //

            //
            // Because we want to delete all the objects and buffers (pools)
            // after we finished termination, the debugger might still use
            // the buffers for events and action, for solving this problem
            // we first disable the tag(s) and this way the debugger no longer
            // use that event and this way we can safely remove and deallocate
            // the buffers later after termination
            //

            //
            // First, disable just one event
            //
            DebuggerDisableEvent(DebuggerEventModificationRequest->Tag);

            //
            // Second, terminate it
            //
            DebuggerTerminateEvent(DebuggerEventModificationRequest->Tag);

            //
            // Third, remove it from the list
            //
            DebuggerRemoveEvent(DebuggerEventModificationRequest->Tag);
        }
    }
    else if (DebuggerEventModificationRequest->TypeOfAction == DEBUGGER_MODIFY_EVENTS_QUERY_STATE)
    {
        //
        // check if tag is valid or not
        //
        if (!DebuggerIsTagValid(DebuggerEventModificationRequest->Tag))
        {
            DebuggerEventModificationRequest->KernelStatus = DEBUGGER_ERROR_TAG_NOT_EXISTS;
            return FALSE;
        }

        //
        // Set event state
        //
        if (DebuggerQueryStateEvent(DebuggerEventModificationRequest->Tag))
        {
            DebuggerEventModificationRequest->IsEnabled = TRUE;
        }
        else
        {
            DebuggerEventModificationRequest->IsEnabled = FALSE;
        }
    }
    else
    {
        //
        // Invalid parameter specifed in TypeOfAction
        //
        DebuggerEventModificationRequest->KernelStatus = DEBUGGER_ERROR_MODIFY_EVENTS_INVALID_TYPE_OF_ACTION;

        return FALSE;
    }

    //
    // The function was successful
    //
    DebuggerEventModificationRequest->KernelStatus = DEBUGGER_OPERATION_WAS_SUCCESSFUL;
    return TRUE;
}

//
//   //
//   //---------------------------------------------------------------------------
//   // Example of using events and actions
//   //
//
//   //
//   // Create condition buffer
//   //
//   char CondtionBuffer[8];
//   CondtionBuffer[0] = 0x90; //nop
//   CondtionBuffer[1] = 0x90; //nop
//   CondtionBuffer[2] = 0xcc; //int 3
//   CondtionBuffer[3] = 0x90; //nop
//   CondtionBuffer[4] = 0xcc; //int 3
//   CondtionBuffer[5] = 0x90; //nop
//   CondtionBuffer[6] = 0x90; //nop
//   CondtionBuffer[7] = 0xc3; // ret
//
//   //
//   // Create event based on condition buffer
//   //
//   PDEBUGGER_EVENT Event1 = DebuggerCreateEvent(TRUE, DEBUGGER_EVENT_APPLY_TO_ALL_CORES, DEBUGGER_EVENT_APPLY_TO_ALL_PROCESSES, SYSCALL_HOOK_EFER, 0x85858585, sizeof(CondtionBuffer), CondtionBuffer);
//
//   if (!Event1)
//   {
//       LogError("Err, in creating event");
//   }
//
//   //
//   // *** Add Actions example ***
//   //
//
//   //
//   // Add action for RUN_SCRIPT
//   //
//   DEBUGGER_EVENT_ACTION_RUN_SCRIPT_CONFIGURATION LogConfiguration = {0};
//   LogConfiguration.LogType                                 = GUEST_LOG_READ_GENERAL_PURPOSE_REGISTERS;
//   LogConfiguration.LogLength                               = 0x10;
//   LogConfiguration.LogMask                                 = 0x1;
//   LogConfiguration.LogValue                                = 0x4;
//
//   DebuggerAddActionToEvent(Event1, RUN_SCRIPT, TRUE, NULL, &LogConfiguration);
//
//   //
//   // Add action for RUN_CUSTOM_CODE
//   //
//   DEBUGGER_EVENT_REQUEST_CUSTOM_CODE CustomCode = {0};
//
//   char CustomCodeBuffer[8];
//   CustomCodeBuffer[0] = 0x90; //nop
//   CustomCodeBuffer[1] = 0x90; //nop
//   CustomCodeBuffer[2] = 0xcc; //int 3
//   CustomCodeBuffer[3] = 0x90; //nop
//   CustomCodeBuffer[4] = 0xcc; //int 3
//   CustomCodeBuffer[5] = 0x90; //nop
//   CustomCodeBuffer[6] = 0x90; //nop
//   CustomCodeBuffer[7] = 0xc3; // ret
//
//   CustomCode.CustomCodeBufferSize        = sizeof(CustomCodeBuffer);
//   CustomCode.CustomCodeBufferAddress     = CustomCodeBuffer;
//   CustomCode.OptionalRequestedBufferSize = 0x100;
//
//   DebuggerAddActionToEvent(Event1, RUN_CUSTOM_CODE, TRUE, &CustomCode, NULL);
//
//   //
//   // Add action for BREAK_TO_DEBUGGER
//   //
//   DebuggerAddActionToEvent(Event1, BREAK_TO_DEBUGGER, FALSE, NULL, NULL);
//
//   //
//   // Call to register
//   //
//   DebuggerRegisterEvent(Event1);
//
//   //
//   //---------------------------------------------------------------------------
//   //
//
