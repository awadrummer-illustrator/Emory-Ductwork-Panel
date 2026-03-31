#ifndef __ProcessDuctworkOptions_H__
#define __ProcessDuctworkOptions_H__

#include <string>

struct ProcessDuctworkOptions
{
	bool skipOrtho;
	bool skipAllBranchSegments;
	bool skipFinalRegisterSegment;
	bool skipRegisterRotation;
	bool enableRegisterCarve;
	bool enableOverlapCarve;
	bool skipCompounding;
	bool skipStyles;
	bool skipParts;
	bool skipGraphics;
	bool skipPlacedMetadata;
	bool directPlaceGraphics;
	bool placedApiGraphics;
	bool hasRotationOverride;
	double rotationOverride;

	ProcessDuctworkOptions();

	bool ParseFromJson(const std::string& json);
};

#endif // __ProcessDuctworkOptions_H__
