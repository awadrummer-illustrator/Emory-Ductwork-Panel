#include "IllustratorSDK.h"
#include "ProcessDuctworkLog.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>

bool DuctworkLog::sEnabled = false; // Disabled for performance

void DuctworkLog::Write(const std::string& message)
{
	if (!sEnabled) {
		return;
	}

	const char* temp = std::getenv("TEMP");
	if (!temp || temp[0] == '\0') {
		temp = std::getenv("TMP");
	}
	if (!temp || temp[0] == '\0') {
		return;
	}

	std::string path(temp);
	if (!path.empty()) {
		const char last = path[path.size() - 1];
		if (last != '\\' && last != '/') {
			path += "\\";
		}
	}
	path += "ProcessDuctwork.log";

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "a") != 0 || !file) {
		return;
	}

	std::time_t now = std::time(nullptr);
	std::tm localTime{};
	localtime_s(&localTime, &now);
	char stamp[32] = { 0 };
	std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &localTime);
	std::fprintf(file, "[%s] %s\n", stamp, message.c_str());
	std::fclose(file);
}

void DuctworkLog::Error(const char* label, int error)
{
	if (!sEnabled) {
		return;
	}
	std::ostringstream stream;
	stream << label << " error=" << error;
	Write(stream.str());
}
