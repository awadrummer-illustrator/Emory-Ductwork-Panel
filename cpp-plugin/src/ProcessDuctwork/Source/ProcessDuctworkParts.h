#ifndef __ProcessDuctworkParts_H__
#define __ProcessDuctworkParts_H__

#include "ProcessDuctworkGeometry.h"
#include "ProcessDuctworkConnections.h"

#include <vector>

struct DuctworkPartStats
{
	size_t anchorsCreated;
	size_t graphicsPlaced;
	size_t skippedExisting;
	size_t skippedMissingAsset;
	size_t skippedConnected;
};

struct DuctworkUnitStats
{
	size_t anchorsCreated;
	size_t graphicsPlaced;
	size_t skippedExisting;
	size_t skippedMissingAsset;
};

namespace DuctworkParts
{
	void SetGlobalRotationOverride(bool hasOverride, double rotationOverride);
	void SetGlobalRotateRegisters(bool enabled);

	DuctworkUnitStats CreateUnitAnchorsAndGraphics(const std::vector<DuctworkPath>& paths,
		double closeDist,
		double anchorTolerance,
		double defaultScalePercent,
		std::vector<DuctworkPoint>& outUnitAnchors,
		bool skipGraphics,
		bool skipPlacedMetadata,
		bool directPlaceGraphics,
		bool placedApiGraphics);

	DuctworkPartStats CreateAnchorsAndGraphics(const std::vector<DuctworkPath>& paths,
		const std::vector<DuctworkConnection>& connections,
		const std::vector<DuctworkPoint>& skipAnchors,
		double anchorTolerance,
		double defaultScalePercent,
		bool skipGraphics,
		bool skipPlacedMetadata,
		bool directPlaceGraphics,
		bool placedApiGraphics);
}

#endif // __ProcessDuctworkParts_H__
