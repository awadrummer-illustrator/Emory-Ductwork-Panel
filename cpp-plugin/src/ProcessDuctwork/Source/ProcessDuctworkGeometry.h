#ifndef __ProcessDuctworkGeometry_H__
#define __ProcessDuctworkGeometry_H__

#include "IllustratorSDK.h"

#include <string>
#include <vector>

struct DuctworkPoint
{
	double x;
	double y;
};

struct DuctworkPath
{
	AIArtHandle art;
	std::vector<DuctworkPoint> points;
	bool closed;
	std::string layerName;
};

struct EmoryBodyStats
{
	size_t created = 0;
	size_t deleted = 0;
	size_t skipped = 0;
	size_t failed = 0;
	size_t segmentsCreated = 0;
	size_t connectorsCreated = 0;
	size_t guidesStyled = 0;
};

namespace DuctworkGeometry
{
	bool GetPathPoints(AIArtHandle path, std::vector<DuctworkPoint>& outPoints, bool& outClosed);
	std::string GetArtLayerName(AIArtHandle art);
	bool IsGeneratedEmoryBody(AIArtHandle art);
	bool IsCenterlineCandidate(AIArtHandle art, const std::vector<DuctworkPoint>& points, bool closed, const std::string& layerName);
	bool GetEffectiveStrokeWidth(AIArtHandle art, double& outWidth);
	bool EnsureEmorySourceId(AIArtHandle art, std::string& outId);
	size_t DeleteGeneratedEmoryBodies(const std::vector<std::string>& sourceIds);
	EmoryBodyStats GenerateEmoryBodies(const std::vector<DuctworkPath>& paths);
	bool ToggleSelectedEmoryConnectorStyles(std::string& outMessage);
}

#endif // __ProcessDuctworkGeometry_H__
