#include "IllustratorSDK.h"
#include "ProcessDuctworkSelection.h"
#include "ProcessDuctworkSuites.h"

namespace
{
	bool IsArtSelected(AIArtHandle art)
	{
		if (!art || !sAIArt) {
			return false;
		}
		ai::int32 attr = 0;
		if (sAIArt->GetArtUserAttr(art, kArtSelected | kArtFullySelected, &attr)) {
			return false;
		}
		return (attr & (kArtSelected | kArtFullySelected)) != 0;
	}

	void CollectPathsFromArt(AIArtHandle art, bool includeAllChildren, std::vector<AIArtHandle>& outPaths)
	{
		if (!art || !sAIArt) {
			return;
		}

		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return;
		}

		if (type == kPathArt) {
			if (includeAllChildren || IsArtSelected(art)) {
				outPaths.push_back(art);
			}
			return;
		}

		if (type == kCompoundPathArt) {
			AIArtHandle child = nullptr;
			if (!sAIArt->GetArtFirstChild(art, &child) && child) {
				bool childIncludeAll = (type == kCompoundPathArt);
				AIArtHandle current = child;
				while (current) {
					CollectPathsFromArt(current, childIncludeAll, outPaths);
					AIArtHandle next = nullptr;
					if (sAIArt->GetArtSibling(current, &next)) {
						break;
					}
					current = next;
				}
			}
			return;
		}

		if (type == kGroupArt) {
			AIArtHandle child = nullptr;
			if (!sAIArt->GetArtFirstChild(art, &child) && child) {
				const bool childIncludeAll = includeAllChildren || IsArtSelected(art);
				AIArtHandle current = child;
				while (current) {
					CollectPathsFromArt(current, childIncludeAll, outPaths);
					AIArtHandle next = nullptr;
					if (sAIArt->GetArtSibling(current, &next)) {
						break;
					}
					current = next;
				}
			}
		}
	}
}

size_t DuctworkSelection::CollectSelectedPaths(std::vector<AIArtHandle>& outPaths)
{
	outPaths.clear();
	if (!sAIArtSet || !sAIArt) {
		return 0;
	}

	AIArtSet selectedSet = nullptr;
	if (sAIArtSet->NewArtSet(&selectedSet)) {
		return 0;
	}

	size_t selectedCount = 0;
	if (!sAIArtSet->SelectedArtSet(selectedSet)) {
		if (!sAIArtSet->CountArtSet(selectedSet, &selectedCount)) {
			for (size_t i = 0; i < selectedCount; ++i) {
				AIArtHandle art = nullptr;
				if (sAIArtSet->IndexArtSet(selectedSet, i, &art) || !art) {
					continue;
				}
				short type = kUnknownArt;
				if (sAIArt->GetArtType(art, &type)) {
					continue;
				}
				if (type == kPathArt || type == kCompoundPathArt) {
					CollectPathsFromArt(art, true, outPaths);
				}
			}
		}
	}

	sAIArtSet->DisposeArtSet(&selectedSet);
	return selectedCount;
}
