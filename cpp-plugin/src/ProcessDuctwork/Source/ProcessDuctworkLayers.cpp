#include "IllustratorSDK.h"
#include "ProcessDuctworkLayers.h"

namespace
{
	bool MatchesList(const std::string& name, const char* const* list, size_t count)
	{
		for (size_t i = 0; i < count; ++i) {
			if (name == list[i]) {
				return true;
			}
		}
		return false;
	}
}

bool DuctworkLayers::IsLineLayerName(const std::string& name)
{
	return MatchesList(name, DuctworkConstants::kLineLayers, DuctworkConstants::kLineLayerCount);
}

bool DuctworkLayers::IsOrthoEligibleLineLayerName(const std::string& name)
{
	return MatchesList(name, DuctworkConstants::kDuctworkColorLayers, DuctworkConstants::kDuctworkColorLayerCount);
}

bool DuctworkLayers::IsColorLayerName(const std::string& name)
{
	return MatchesList(name, DuctworkConstants::kDuctworkColorLayers, DuctworkConstants::kDuctworkColorLayerCount);
}

bool DuctworkLayers::IsPartLayerName(const std::string& name)
{
	return MatchesList(name, DuctworkConstants::kPartLayers, DuctworkConstants::kPartLayerCount);
}
