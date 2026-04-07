#include "IllustratorSDK.h"
#include "ProcessDuctworkSelection.h"
#include "ProcessDuctworkSuites.h"

#include <set>

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

	bool HasSelectedDescendant(AIArtHandle art)
	{
		if (!art || !sAIArt) {
			return false;
		}

		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return false;
		}
		if (type != kGroupArt && type != kCompoundPathArt) {
			return false;
		}

		AIArtHandle child = nullptr;
		if (sAIArt->GetArtFirstChild(art, &child) != kNoErr || !child) {
			return false;
		}

		AIArtHandle current = child;
		while (current) {
			if (IsArtSelected(current) || HasSelectedDescendant(current)) {
				return true;
			}
			AIArtHandle next = nullptr;
			if (sAIArt->GetArtSibling(current, &next) != kNoErr) {
				break;
			}
			current = next;
		}

		return false;
	}

	void CollectSelectedTopLevelArt(std::vector<AIArtHandle>& outArt)
	{
		outArt.clear();
		if (!sAIArtSet || !sAIArt) {
			return;
		}

		AIArtSet selectedSet = nullptr;
		if (sAIArtSet->NewArtSet(&selectedSet) != kNoErr) {
			return;
		}

		size_t selectedCount = 0;
		if (sAIArtSet->SelectedArtSet(selectedSet) == kNoErr) {
			sAIArtSet->CountArtSet(selectedSet, &selectedCount);
		}
		if (selectedCount == 0) {
			AIArtSpec specs[1];
			specs[0].type = kAnyArt;
			specs[0].whichAttr = kArtSelected | kArtFullySelected;
			specs[0].attr = kArtSelected | kArtFullySelected;
			if (sAIArtSet->MatchingArtSet(specs, 1, selectedSet) == kNoErr) {
				sAIArtSet->CountArtSet(selectedSet, &selectedCount);
			}
		}

		if (selectedCount > 0) {
			outArt.reserve(selectedCount);
			for (size_t i = 0; i < selectedCount; ++i) {
				AIArtHandle art = nullptr;
				if (sAIArtSet->IndexArtSet(selectedSet, i, &art) != kNoErr || !art) {
					continue;
				}
				outArt.push_back(art);
			}
		}

		sAIArtSet->DisposeArtSet(&selectedSet);

		if (outArt.empty()) {
			return;
		}

		std::vector<AIArtHandle> filtered;
		filtered.reserve(outArt.size());
		for (size_t i = 0; i < outArt.size(); ++i) {
			AIArtHandle art = outArt[i];
			if (!art || HasSelectedDescendant(art)) {
				continue;
			}
			filtered.push_back(art);
		}
		outArt.swap(filtered);
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
	std::vector<AIArtHandle> selectedArt;
	CollectSelectedTopLevelArt(selectedArt);
	if (selectedArt.empty()) {
		return 0;
	}

	std::set<AIArtHandle> uniquePaths;
	for (size_t i = 0; i < selectedArt.size(); ++i) {
		AIArtHandle art = selectedArt[i];
		if (!art) {
			continue;
		}

		std::vector<AIArtHandle> collectedPaths;
		// Respect direct-selection by collecting from filtered top-level art first.
		// If the actual selected item is a path, only that path is returned. If the
		// selected item is a group with no selected descendants, its child paths are
		// still included for marquee/group selections.
		CollectPathsFromArt(art, true, collectedPaths);
		for (size_t pathIndex = 0; pathIndex < collectedPaths.size(); ++pathIndex) {
			AIArtHandle path = collectedPaths[pathIndex];
			if (!path || !uniquePaths.insert(path).second) {
				continue;
			}
			outPaths.push_back(path);
		}
	}

	return outPaths.size();
}
