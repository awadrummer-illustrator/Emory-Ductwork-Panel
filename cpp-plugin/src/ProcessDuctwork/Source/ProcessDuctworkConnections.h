#ifndef __ProcessDuctworkConnections_H__
#define __ProcessDuctworkConnections_H__

#include "ProcessDuctworkGeometry.h"

#include <vector>

enum DuctworkConnectionType
{
	kConnectionEndpointToEndpoint,
	kConnectionEndpointToSegment,
	kConnectionSegmentIntersection
};

struct DuctworkConnection
{
	int a;
	int b;
	DuctworkConnectionType type;
	DuctworkPoint point;
	int segA;
	int segB;
	int endpointA;
	int endpointB;
};

namespace DuctworkConnections
{
	void FindConnections(const std::vector<DuctworkPath>& paths,
		double maxDist,
		double tJunctionDist,
		double endpointTolerance,
		double vertexTolerance,
		bool allowSegmentIntersectionConnect,
		std::vector<DuctworkConnection>& outConnections);
}

#endif // __ProcessDuctworkConnections_H__
