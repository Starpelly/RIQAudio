/// https://medium.com/@lakshjain/unity-logging-from-native-plugin-dlls-the-simple-way-e9081eb96420

#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "IUnityInterface.h"
#include "IUnityLog.h"

// For holding the reference pointer until plugin unloads
static IUnityLog* unityLogPtr = nullptr;

extern "C"
{
	// Formats a char array like printf
	char* Pelly_FormatString(const char* fmt, ...)
	{
		va_list args;
		size_t len;
		char* space;

		va_start(args, fmt);
		len = vsnprintf(0, 0, fmt, args);
		va_end(args);

		if ((space = (char*)malloc(len + 1)) != 0)
		{
			va_start(args, fmt);
			vsnprintf(space, len + 1, fmt, args);
			va_end(args);
			return space;
			free(space);
		}
		else
		{
			// well shit?
		}

		return NULL;
	}

	//function will get called by unity when it loads the plugin automatically
	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfacesPtr)
	{
		//Get the unity log pointer once the Unity plugin gets loaded
		unityLogPtr = unityInterfacesPtr->Get<IUnityLog>();

		
	}

	//function will get called by unity when it un-loads the plugin automatically
	UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload()
	{
		//Clearing the log ptr on unloading the plugin
		unityLogPtr = nullptr;
	}
}

#define FORMAT(MESSAGE) std::string("[" + std::string(__FILE__) + ":" + std::to_string(__LINE__) + "] " + MESSAGE).c_str()

#define DEBUG_LOG_FMT(PTR, MESSAGE, ...) UNITY_LOG(PTR, FORMAT(Pelly_FormatString(MESSAGE, __VA_ARGS__)))
#define DEBUG_LOG(PTR, MESSAGE) UNITY_LOG(PTR, FORMAT(MESSAGE))

#define DEBUG_WARNING_FMT(PTR, MESSAGE, ...) UNITY_LOG_WARNING(PTR, FORMAT(Pelly_FormatString(MESSAGE, __VA_ARGS__)))
#define DEBUG_WARNING(PTR, MESSAGE) UNITY_LOG_WARNING(PTR, FORMAT(MESSAGE))

#define DEBUG_ERROR_FMT(PTR, MESSAGE, ...) UNITY_LOG_ERROR(PTR, FORMAT(Pelly_FormatString(MESSAGE, __VA_ARGS__)))
#define DEBUG_ERROR(PTR, MESSAGE) UNITY_LOG_ERROR(PTR, FORMAT(MESSAGE))