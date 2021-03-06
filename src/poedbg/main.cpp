// Part of 'poedbg'. Copyright (c) 2018 maper. Copies must retain this attribution.

#include "common.h"
#include "globals.h"
#include "callbacks.h"
#include "security.hpp"
#include "memory.hpp"
#include "game.hpp"

//////////////////////////////////////////////////////////////////////////
// Event Handlers
//////////////////////////////////////////////////////////////////////////

/*
Starts the main debugging loop, which waits for and processes debug events
and exceptions as they're sent to our process.
*/
DWORD __stdcall DllDebugEventHandler(LPVOID Parameter)
{
	UNREFERENCED_PARAMETER(Parameter);

	// First we should start debugging the game. We must do this on the
	// same thread that waits for debug events.

	if (FALSE == DebugActiveProcess(_g_GameId))
	{
		POEDBG_NOTIFY_CALLBACK(Error, POEDBG_STATUS_GAME_HOOK_NOT_SET);
		return 0;
	}

	if (FALSE == DebugSetProcessKillOnExit(FALSE))
	{
		POEDBG_NOTIFY_CALLBACK(Error, POEDBG_STATUS_GAME_HOOK_BEHAVIOR_NOT_SET);
		return 0;
	}

	// Initialize status.
	DWORD Status = DBG_CONTINUE;

	DEBUG_EVENT Event;

	for (;;)
	{
		// Wait for a debugging event to occur. The second parameter indicates
		// that the function does not return until a debugging event occurs. 

		WaitForDebugEvent(&Event, INFINITE);

		// Process the debugging event. It may be an exception, or another type
		// of event, so pass it to the appropriate handler.

		switch (Event.dwDebugEventCode)
		{
		case EXCEPTION_DEBUG_EVENT:

			// Default to not handle exception.
			Status = DBG_EXCEPTION_NOT_HANDLED;

			switch (Event.u.Exception.ExceptionRecord.ExceptionCode)
			{
			case EXCEPTION_SINGLE_STEP:
				Status = _PoeDbgGameProcessHooks(Event.dwThreadId, Event.u.Exception);
				break;
			default:
			{
				// We should check if this is a first-chance exception. If so, we can give
				// it a chance to be handled by the process. If the exception is continuable,
				// don't even pass it to the process, and just continue execution.

				if (Event.u.Exception.dwFirstChance)
				{
					if (Event.u.Exception.ExceptionRecord.ExceptionFlags != EXCEPTION_NONCONTINUABLE)
					{
						Status = DBG_CONTINUE;
					}
				}
				else
				{
					// Try to report the error.
					POEDBG_NOTIFY_CALLBACK(Error, POEDBG_STATUS_EXCEPTION_NOT_HANDLED);
				}
				break;
			}
			}
			break;
		case CREATE_PROCESS_DEBUG_EVENT:
			Status = _PoeDbgGameInitializeProcess(&Event);
			break;
		case CREATE_THREAD_DEBUG_EVENT:
			Status = _PoeDbgGameInitializeThread(&Event);
			break;
		default:
			break;
		}

		// Resume executing the thread that reported the debugging event. 
		ContinueDebugEvent(Event.dwProcessId, Event.dwThreadId, Status);
	}
}

//////////////////////////////////////////////////////////////////////////
// Module Entry Point
//////////////////////////////////////////////////////////////////////////

/*
Performs module initialization when first loaded, as well as each time
a new thread is spawned.
*/
BOOL __stdcall DllMain(HMODULE Module, DWORD Reason, LPVOID Reserved)
{
	UNREFERENCED_PARAMETER(Module);
	UNREFERENCED_PARAMETER(Reason);
	UNREFERENCED_PARAMETER(Reserved);

	return TRUE;
}
