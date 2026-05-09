#include "IllustratorSDK.h"
#include "ProcessDuctworkOptions.h"

#include <cctype>

namespace
{
	static bool ExtractKey(const std::string& json, const std::string& key, size_t& outPos)
	{
		std::string needle = "\"" + key + "\"";
		size_t pos = json.find(needle);
		if (pos == std::string::npos) {
			return false;
		}
		pos = json.find(':', pos + needle.size());
		if (pos == std::string::npos) {
			return false;
		}
		outPos = pos + 1;
		return true;
	}

	static bool ParseBoolAt(const std::string& json, size_t pos, bool& outValue)
	{
		while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
			++pos;
		}
		if (json.compare(pos, 4, "true") == 0) {
			outValue = true;
			return true;
		}
		if (json.compare(pos, 5, "false") == 0) {
			outValue = false;
			return true;
		}
		return false;
	}

	static bool ParseNumberAt(const std::string& json, size_t pos, double& outValue)
	{
		while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
			++pos;
		}
		size_t start = pos;
		bool sawDigit = false;
		if (pos < json.size() && (json[pos] == '-' || json[pos] == '+')) {
			++pos;
		}
		while (pos < json.size() && (std::isdigit(static_cast<unsigned char>(json[pos])) || json[pos] == '.')) {
			sawDigit = true;
			++pos;
		}
		if (!sawDigit) {
			return false;
		}
		outValue = std::atof(json.substr(start, pos - start).c_str());
		return true;
	}
}

ProcessDuctworkOptions::ProcessDuctworkOptions()
	: skipOrtho(false),
	skipAllBranchSegments(false),
	skipFinalRegisterSegment(false),
	skipFinalSegmentThickness(true),
	skipRegisterRotation(false),
	enableRegisterCarve(false),
	enableOverlapCarve(false),
	skipCompounding(false),
	skipStyles(false),
	skipParts(false),
	skipGraphics(false),
	skipPlacedMetadata(false),
	directPlaceGraphics(false),
	placedApiGraphics(false),
	hasRotationOverride(false),
	rotationOverride(0.0),
	branchTaperReductionPercent(25.0)
{
}

bool ProcessDuctworkOptions::ParseFromJson(const std::string& json)
{
	size_t pos = 0;
	if (ExtractKey(json, "skipOrtho", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			skipOrtho = value;
		}
	}
	if (ExtractKey(json, "skipAllBranchSegments", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			skipAllBranchSegments = value;
		}
	}
	if (ExtractKey(json, "skipFinalRegisterSegment", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			skipFinalRegisterSegment = value;
		}
	}
	if (ExtractKey(json, "skipFinalSegmentThickness", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			skipFinalSegmentThickness = value;
		}
	}
	if (ExtractKey(json, "skipRegisterRotation", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			skipRegisterRotation = value;
		}
	}
	if (ExtractKey(json, "enableRegisterCarve", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			enableRegisterCarve = value;
		}
	}
	if (ExtractKey(json, "enableOverlapCarve", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			enableOverlapCarve = value;
		}
	}
	if (ExtractKey(json, "skipCompounding", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			skipCompounding = value;
		}
	}
	if (ExtractKey(json, "skipStyles", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			skipStyles = value;
		}
	}
	if (ExtractKey(json, "skipParts", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			skipParts = value;
		}
	}
	if (ExtractKey(json, "skipGraphics", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			skipGraphics = value;
		}
	}
	if (ExtractKey(json, "skipPlacedMetadata", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			skipPlacedMetadata = value;
		}
	}
	if (ExtractKey(json, "directPlaceGraphics", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			directPlaceGraphics = value;
		}
	}
	if (ExtractKey(json, "placedApiGraphics", pos)) {
		bool value = false;
		if (ParseBoolAt(json, pos, value)) {
			placedApiGraphics = value;
		}
	}
	if (ExtractKey(json, "rotationOverride", pos)) {
		double value = 0.0;
		if (ParseNumberAt(json, pos, value)) {
			hasRotationOverride = true;
			rotationOverride = value;
		}
	}
	if (ExtractKey(json, "branchTaperReductionPercent", pos)) {
		double value = 25.0;
		if (ParseNumberAt(json, pos, value)) {
			branchTaperReductionPercent = value;
		}
	}
	return true;
}
