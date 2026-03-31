#ifndef __ProcessDuctworkArt_H__
#define __ProcessDuctworkArt_H__

#include "IllustratorSDK.h"

#include <vector>

namespace DuctworkArt
{
	bool IsLayerChainEditableVisible(AILayerHandle layer);
	AILayerHandle FindLayerByTitle(const char* name);
	bool IsArtSelectable(AIArtHandle art);
	void CollectLayerArt(AILayerHandle layer, std::vector<AIArtHandle>& outArt);
}

#endif // __ProcessDuctworkArt_H__
