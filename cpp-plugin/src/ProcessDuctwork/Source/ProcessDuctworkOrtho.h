#ifndef __ProcessDuctworkOrtho_H__
#define __ProcessDuctworkOrtho_H__

#include "ProcessDuctworkGeometry.h"
#include "ProcessDuctworkConnections.h"

#include <vector>

struct OrthoResult
{
	size_t pathsTouched;
	size_t segmentsSnapped;
};

namespace DuctworkOrtho
{
	OrthoResult ApplyToPaths(std::vector<DuctworkPath>& paths, double snapThresholdDegrees,
		bool hasRotationOverride, double rotationOverrideDegrees,
		const std::vector<DuctworkConnection>& preConnections,
		const std::vector<DuctworkPath>& documentContextPaths,
		bool skipAllBranchSegments,
		bool skipFinalRegisterSegment);
}

#endif // __ProcessDuctworkOrtho_H__
