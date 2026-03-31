#ifndef __ProcessDuctworkLayers_H__
#define __ProcessDuctworkLayers_H__

#include "ProcessDuctworkConstants.h"

#include <string>

namespace DuctworkLayers
{
	bool IsLineLayerName(const std::string& name);
	bool IsOrthoEligibleLineLayerName(const std::string& name);
	bool IsColorLayerName(const std::string& name);
	bool IsPartLayerName(const std::string& name);
}

#endif // __ProcessDuctworkLayers_H__
