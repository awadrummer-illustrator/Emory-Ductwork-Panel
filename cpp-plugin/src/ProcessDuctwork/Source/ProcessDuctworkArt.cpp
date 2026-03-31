#include "IllustratorSDK.h"
#include "ProcessDuctworkArt.h"
#include "ProcessDuctworkSuites.h"

bool DuctworkArt::IsLayerChainEditableVisible(AILayerHandle layer)
{
	AILayerHandle current = layer;
	int guard = 0;
	while (current && guard++ < 256) {
		AIBoolean editable = false;
		AIBoolean visible = false;
		if (sAILayer->GetLayerEditable(current, &editable) || sAILayer->GetLayerVisible(current, &visible)) {
			return false;
		}
		if (!editable || !visible) {
			return false;
		}

		AILayerHandle parent = nullptr;
		if (sAILayer->GetLayerParent(current, &parent) || parent == nullptr || parent == current) {
			break;
		}
		current = parent;
	}
	return true;
}

AILayerHandle DuctworkArt::FindLayerByTitle(const char* name)
{
	if (!sAILayer || !name) {
		return nullptr;
	}
	AILayerHandle layer = nullptr;
	ai::UnicodeString title = ai::UnicodeString::FromUTF8(name);
	if (sAILayer->GetLayerByTitle(&layer, title)) {
		return nullptr;
	}
	return layer;
}

bool DuctworkArt::IsArtSelectable(AIArtHandle art)
{
	if (!art || !sAIArt) {
		return false;
	}
	ai::int32 attr = 0;
	if (sAIArt->GetArtUserAttr(art, kArtLocked | kArtHidden, &attr)) {
		return false;
	}
	return (attr & (kArtLocked | kArtHidden)) == 0;
}

void DuctworkArt::CollectLayerArt(AILayerHandle layer, std::vector<AIArtHandle>& outArt)
{
	if (!layer || !sAIArtSet) {
		return;
	}
	AIArtSet layerSet = nullptr;
	if (sAIArtSet->NewArtSet(&layerSet)) {
		return;
	}
	if (!sAIArtSet->LayerArtSet(layer, layerSet)) {
		size_t count = 0;
		if (!sAIArtSet->CountArtSet(layerSet, &count)) {
			for (size_t i = 0; i < count; ++i) {
				AIArtHandle art = nullptr;
				if (!sAIArtSet->IndexArtSet(layerSet, i, &art) && art) {
					outArt.push_back(art);
				}
			}
		}
	}
	sAIArtSet->DisposeArtSet(&layerSet);
}
