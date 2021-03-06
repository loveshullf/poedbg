// Part of 'poedbg'. Copyright (c) 2018 maper. Copies must retain this attribution.

__pragma(warning(push, 0))
#include <windows.h>
#include <stdio.h>
__pragma(warning(pop))

// Handy macro for extracting functions from the poedbg module.
#define POEDBG_DEFINE_FUNCTION(name, type) type name = reinterpret_cast<##type##>(GetProcAddress(ModuleHandle, #name))

// These are the function pointer types for the functions that
// we'll be extracting from poedbg.
typedef int(__stdcall *POEDBG_STANDARD_ROUTINE)();
typedef int(__stdcall *POEDBG_REGISTER_CALLBACK_ROUTINE)(PVOID Callback);

// Callback function pointer types that we'll be using.
typedef void(__stdcall *POEDBG_ERROR_CALLBACK)(int Status);
typedef void(__stdcall *POEDBG_PACKET_CALLBACK)(unsigned int Length, BYTE Id, PBYTE Data);

// Sizes.
#define DEFAULT_BUFFER_SIZE 0x10000

// Packet definitions.
#define POE_C2S_HEARTBEAT 0x0d
#define POE_C2S_MOVE_BEGIN 0xd4
#define POE_C2S_MOVE_SUSTAIN 0xd6
#define POE_C2S_MOVE_END 0xd8
#define POE_C2S_FLASK_DRINK 0x36
#define POE_S2C_LOGIN 0x02
#define POE_S2C_CHARACTERS 0x04
#define POE_S2C_TRANSITION 0x05
#define POE_S2C_HEARTBEAT 0x22
#define POE_S2C_CHAT 0x0a
#define POE_S2C_ENTITY_POSITION_UPDATE 0xe5
#define POE_S2C_ENTITY_STATE_UPDATE 0x0e
#define POE_S2C_FLASK_DRINK_EFFECT 0xf4

// Local string buffers.
__declspec(selectany) BYTE _g_PacketSendStringBuffer[DEFAULT_BUFFER_SIZE];
__declspec(selectany) BYTE _g_PacketReceiveStringBuffer[DEFAULT_BUFFER_SIZE];

bool GetPacketData(unsigned int PacketLength, PBYTE LocalPacketBuffer, PBYTE LocalPacketString)
{
	__try
	{
		for (unsigned int i = 0, j = 0; i < PacketLength; ++i)
		{
			if ((DEFAULT_BUFFER_SIZE - j) <= 1)
			{
				return false;
			}

			// Write each byte to the output string.
			int Written = sprintf_s(reinterpret_cast<char*>(LocalPacketString + j), (DEFAULT_BUFFER_SIZE - j), "%02x ", LocalPacketBuffer[i]);

			// After we've written a byte, move the position in the output string
			// forward by the number of characters written, minus the automatic null.

			j += Written;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}

void __stdcall HandleError(int Status)
{
	printf("[ERROR] The 'poedbg' module reported an error code of '%i'.\n", Status);
}

void __stdcall HandlePacketReceive(unsigned int Length, BYTE PacketId, PBYTE PacketData)
{
	// Note: There are actually two different packet receive hooks that would each call
	// this same function. The reason we are able to still use the string buffer without
	// some kind of lock is because the game process is suspended while we deal with one
	// of these events, so no other threads will be running.

	if (POE_S2C_HEARTBEAT != PacketId &&
		POE_S2C_ENTITY_STATE_UPDATE != PacketId)
	{
		if (GetPacketData(Length, PacketData, _g_PacketReceiveStringBuffer))
		{
			printf("[RECEIVED] Packet with ID of '0x%02x' and length of '%u'. Full packet data: %s\n", PacketId, Length, _g_PacketReceiveStringBuffer);
		}
		else
		{
			printf("[RECEIVED] Packet with ID of '0x%02x' and length of '%u'. Too large to display.\n", PacketId, Length);
		}
	}
}

void __stdcall HandlePacketSend(unsigned int Length, BYTE PacketId, PBYTE PacketData)
{
	if (POE_C2S_HEARTBEAT != PacketId)
	{
		if (GetPacketData(Length, PacketData, _g_PacketSendStringBuffer))
		{
			printf("[SENT] Packet with ID of '0x%02x' and length of '%u'. Full packet data: %s\n", PacketId, Length, _g_PacketSendStringBuffer);
		}
		else
		{
			printf("[SENT] Packet with ID of '0x%02x' and length of '%u'. Too large to display.\n", PacketId, Length);
		}
	}
}

int main()
{
	printf("Starting 'poedbg' C++ sample...\n");

	// Load and return the handle.
	HMODULE ModuleHandle = LoadLibraryW(L"poedbg.dll");

	if (NULL == ModuleHandle)
	{
		printf("Unable to load the 'poedbg' module.\n");

		getchar();
		return 1;
	}

	// Next we will get the pointers for all of the exports that we might use
	// from poedbg. You only need to get the ones you'll need. We've defined
	// a handy macro to make it look less disgusting.

	POEDBG_DEFINE_FUNCTION(PoeDbgInitialize, POEDBG_STANDARD_ROUTINE);
	POEDBG_DEFINE_FUNCTION(PoeDbgDestroy, POEDBG_STANDARD_ROUTINE);
	POEDBG_DEFINE_FUNCTION(PoeDbgRegisterErrorCallback, POEDBG_REGISTER_CALLBACK_ROUTINE);
	POEDBG_DEFINE_FUNCTION(PoeDbgRegisterPacketSendCallback, POEDBG_REGISTER_CALLBACK_ROUTINE);
	POEDBG_DEFINE_FUNCTION(PoeDbgRegisterPacketReceiveCallback, POEDBG_REGISTER_CALLBACK_ROUTINE);
	POEDBG_DEFINE_FUNCTION(PoeDbgUnregisterErrorCallback, POEDBG_STANDARD_ROUTINE);
	POEDBG_DEFINE_FUNCTION(PoeDbgUnregisterPacketSendCallback, POEDBG_STANDARD_ROUTINE);
	POEDBG_DEFINE_FUNCTION(PoeDbgUnregisterPacketReceiveCallback, POEDBG_STANDARD_ROUTINE);

	// Now we'll register all of our callbacks. It is important, especially,
	// that we register the error handling callback before we attempt to
	// initialize the poedbg module.

	int Result = PoeDbgRegisterErrorCallback(HandleError);
	if (Result < 0)
	{
		printf("[ERROR] Could not register error callback. Error code: '%i'.\n", Result);

		getchar();
		return 1;
	}

	Result = PoeDbgRegisterPacketReceiveCallback(HandlePacketReceive);
	if (Result < 0)
	{
		printf("[ERROR] Could not register packet send callback. Error code: '%i'.\n", Result);

		getchar();
		return 1;
	}
	Result = PoeDbgRegisterPacketSendCallback(HandlePacketSend);

	if (Result < 0)
	{
		printf("[ERROR] Could not register packet receive callback. Error code: '%i'.\n", Result);

		getchar();
		return 1;
	}

	Result = PoeDbgInitialize();
	if (Result < 0)
	{
		printf("[ERROR] Could not initialize. Error code: '%i'.\n", Result);

		getchar();
		return 1;
	}

	printf("Started successfully.\n");

	getchar();

	Result = PoeDbgDestroy();
	if (Result < 0)
	{
		printf("[ERROR] Could not destroy. Error code: '%i'.\n", Result);

		getchar();
		return 1;
	}

	printf("Removed successfully.\n");

	getchar();
	return 0;
}
