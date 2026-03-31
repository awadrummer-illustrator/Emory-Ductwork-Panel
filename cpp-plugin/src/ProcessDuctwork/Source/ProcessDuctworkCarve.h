#ifndef __ProcessDuctworkCarve_H__
#define __ProcessDuctworkCarve_H__

#include "IllustratorSDK.h"
#include "ProcessDuctworkGeometry.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace DuctworkCarve
{
	struct CarveStats
	{
		int registerCarves = 0;
		int overlapCarves = 0;
		int pathsSplit = 0;
	};

	CarveStats ApplyRegisterCarve(const std::vector<DuctworkPath>& paths,
		std::vector<AIArtHandle>& selection);

	CarveStats ApplyOverlapCarve(const std::vector<DuctworkPath>& paths,
		std::vector<AIArtHandle>& selection);

	struct GapToolResult
	{
		bool applied = false;
		bool carved = false;
		bool healed = false;
		bool toggled = false;
	};

	struct GapToolPreview
	{
		bool valid = false;
		bool showHeal = false;
		bool showGap = false;
		bool isRegister = false;
		DuctworkPoint start{};
		DuctworkPoint end{};
		double halfWidth = 0.0;
	};

	bool ApplyGapToggleAtPoint(const DuctworkPoint& click,
		const std::string& layerHint,
		AIArtHandle preferredArt,
		GapToolResult& outResult);

	bool ApplyGapHealCreateAtPoint(const DuctworkPoint& click,
		const std::string& layerHint,
		AIArtHandle preferredArt,
		GapToolResult& outResult);

	bool FindPreferredArtNearPoint(const DuctworkPoint& click,
		const std::string& layerHint,
		AIArtHandle& outArt,
		DuctworkPoint* outClosest = nullptr);

	bool ComputeGapToolPreview(const DuctworkPoint& click,
		const std::string& layerHint,
		AIArtHandle preferredArt,
		GapToolPreview& outPreview,
		AIDocumentViewHandle view = nullptr,
		const AIRealPoint* viewClick = nullptr);
}

#endif // __ProcessDuctworkCarve_H__
