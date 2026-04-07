#ifndef __ProcessDuctworkLog_H__
#define __ProcessDuctworkLog_H__

#include <string>

struct DuctworkLog
{
	static void Write(const std::string& message);
	static void WriteAlways(const std::string& message); // Bypasses sEnabled check
	static void Error(const char* label, int error);
	static bool sEnabled; // Set to false to disable all logging for performance
};

#endif // __ProcessDuctworkLog_H__
