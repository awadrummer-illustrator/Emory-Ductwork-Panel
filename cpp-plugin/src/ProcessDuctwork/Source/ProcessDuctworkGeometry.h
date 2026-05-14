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
	void SetEmoryBranchTaperReductionPercent(double reductionPercent);
	bool IsCenterlineCandidate(AIArtHandle art, const std::vector<DuctworkPoint>& points, bool closed, const std::string& layerName);
	bool GetEffectiveStrokeWidth(AIArtHandle art, double& outWidth);
	bool EnsureEmorySourceId(AIArtHandle art, std::string& outId);
	bool CopyEmoryCenterlineIdentity(AIArtHandle sourceArt, AIArtHandle targetArt);
	void SplitEmoryCenterlineMetadata(AIArtHandle sourceArt, size_t splitSegmentIndex, AIArtHandle firstArt, AIArtHandle secondArt);
	void EnsureEmoryBackupCenterlines(const std::vector<AIArtHandle>& selection);
	bool PrepareSelectedEmorySourceIdsForProcessing(const std::vector<DuctworkPath>& paths, std::vector<std::string>& outCleanupIds);
	void UpdateSelectedEmoryFinalSegmentThicknessMetadata(const std::vector<DuctworkPath>& paths, bool enabled);
	size_t DeleteGeneratedEmoryBodies(const std::vector<std::string>& sourceIds);
	EmoryBodyStats GenerateEmoryBodies(const std::vector<DuctworkPath>& paths);
	bool ToggleSelectedEmoryConnectorStyles(std::string& outMessage);
	bool MarkSelectedEmoryConnectorSeparate(std::string& outMessage);
	bool ToggleSelectedEmoryTerminalSegmentStyle(std::string& outMessage);
	bool SetSelectedEmoryTerminalSegmentStyle(const std::string& targetStyle, std::string& outMessage);
	bool GetSelectedEmorySegmentState(std::string& outJson);
	bool RevertSelectedEmoryToCenterlines(std::string& outMessage);
	bool SelectSelectedEmoryCenterlines(std::string& outMessage);
	bool PurgeSelectedEmoryState(std::string& outMessage);
	bool SetSelectedEmoryTaperAlignment(const std::string& alignment, std::string& outMessage);
	bool SetSelectedEmoryCenterlineVisibility(bool hidden, std::string& outMessage);
	bool SelectSelectedEmoryFinalSegments(std::string& outMessage);
	bool SetSelectedEmoryStartSegment(std::string& outMessage);
	bool ClearSelectedEmoryStartSegment(std::string& outMessage);
	bool SetSelectedEmoryCascadeStopSegment(bool enabled, std::string& outMessage);
	bool ApplySelectedEmorySegmentWidth(double newWidth, std::string& outMessage);
	bool ApplySelectedEmoryStrokeWidth(double newWidth, std::string& outMessage, bool includeThermostatLines = true);
}

#endif // __ProcessDuctworkGeometry_H__
