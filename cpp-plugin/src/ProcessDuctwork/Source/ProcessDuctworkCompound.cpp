#include "IllustratorSDK.h"
#include "ProcessDuctworkCompound.h"
#include "ProcessDuctworkLog.h"
#include "ProcessDuctworkNotes.h"
#include "ProcessDuctworkSuites.h"

#include <cmath>
#include <cctype>
#include <cstdlib>
#include <map>
#include <set>

namespace
{
	bool IsValidIndex(int index, size_t count)
	{
		return index >= 0 && static_cast<size_t>(index) < count;
	}

	std::string ToUpper(const std::string& value)
	{
		std::string out = value;
		for (size_t i = 0; i < out.size(); ++i) {
			out[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[i])));
		}
		return out;
	}

	bool TryParseNumber(const std::string& raw, double& out)
	{
		std::string trimmed = raw;
		if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
			trimmed = trimmed.substr(1, trimmed.size() - 2);
		}
		if (trimmed.empty()) {
			return false;
		}
		char* end = nullptr;
		double value = std::strtod(trimmed.c_str(), &end);
		if (end == trimmed.c_str()) {
			return false;
		}
		out = value;
		return true;
	}

	bool TryGetRotationOverride(AIArtHandle art, double& outRotation)
	{
		std::string note = DuctworkNotes::GetNote(art);
		if (note.empty()) {
			return false;
		}
		std::vector<std::string> tokens = DuctworkNotes::SplitTokens(note);
		for (size_t i = 0; i < tokens.size(); ++i) {
			std::string upper = ToUpper(tokens[i]);
			const std::string prefix = "MD:ROT=";
			if (upper.find(prefix) == 0) {
				std::string raw = tokens[i].substr(prefix.size());
				double value = 0.0;
				if (TryParseNumber(raw, value)) {
					outRotation = value;
					return true;
				}
			}
		}
		return false;
	}

	bool TryGetMetaField(AIArtHandle art, const std::string& key, std::string& outValue)
	{
		std::string note = DuctworkNotes::GetNote(art);
		std::string json;
		std::vector<std::string> mdTags;
		if (!DuctworkNotes::ExtractMDUXMeta(note, json, mdTags)) {
			return false;
		}
		std::map<std::string, std::string> fields;
		if (!DuctworkNotes::ParseMetaJson(json, fields)) {
			return false;
		}
		std::map<std::string, std::string>::const_iterator it = fields.find(key);
		if (it == fields.end()) {
			return false;
		}
		outValue = it->second;
		return true;
	}

	void CollectMDTags(const std::string& note, std::vector<std::string>& outTags)
	{
		outTags.clear();
		if (note.empty()) {
			return;
		}
		std::vector<std::string> tokens = DuctworkNotes::SplitTokens(note);
		for (size_t i = 0; i < tokens.size(); ++i) {
			if (tokens[i].find("MD:") == 0) {
				outTags.push_back(tokens[i]);
			}
		}
	}

	bool WriteCompoundMeta(AIArtHandle compound, const std::map<std::string, std::string>& updates)
	{
		if (updates.empty()) {
			return false;
		}
		std::string note = DuctworkNotes::GetNote(compound);
		std::string json;
		std::vector<std::string> mdTags;
		std::map<std::string, std::string> fields;
		if (DuctworkNotes::ExtractMDUXMeta(note, json, mdTags)) {
			DuctworkNotes::ParseMetaJson(json, fields);
		} else {
			CollectMDTags(note, mdTags);
		}
		for (std::map<std::string, std::string>::const_iterator it = updates.begin(); it != updates.end(); ++it) {
			fields[it->first] = it->second;
		}
		if (fields.empty()) {
			return false;
		}
		std::string newJson = DuctworkNotes::SerializeMetaJson(fields);
		std::string newNote = DuctworkNotes::BuildNoteWithMDUXMeta(newJson, mdTags);
		return DuctworkNotes::SetNote(compound, newNote);
	}

	bool GetPathStrokeWidth(AIArtHandle art, double& outWidth)
	{
		if (!art || !sAIPathStyle) {
			return false;
		}
		AIPathStyle style;
		style.Init();
		AIBoolean hasAdvFill = false;
		if (sAIPathStyle->GetPathStyle(art, &style, &hasAdvFill)) {
			return false;
		}
		outWidth = style.stroke.width;
		return true;
	}

	AIArtHandle GetLayerGroup(AIArtHandle art)
	{
		if (!art || !sAIArt) {
			return nullptr;
		}
		AILayerHandle layer = nullptr;
		if (sAIArt->GetLayerOfArt(art, &layer) || !layer) {
			return nullptr;
		}
		AIArtHandle layerGroup = nullptr;
		if (sAIArt->GetFirstArtOfLayer(layer, &layerGroup)) {
			return nullptr;
		}
		return layerGroup;
	}
}

size_t DuctworkCompound::ReleaseCompoundPaths(AIDocumentHandle document)
{
	size_t releasedCount = 0;
	if (!sAIArt || !sAILayer || !document) {
		return releasedCount;
	}

	// Ductwork layer names to check for compound paths
	const char* ductworkLayers[] = {
		"Blue Ductwork",
		"Green Ductwork",
		"Light Green Ductwork",
		"Orange Ductwork",
		"Light Orange Ductwork",
		"Thermostat Lines"
	};
	const size_t layerCount = sizeof(ductworkLayers) / sizeof(ductworkLayers[0]);

	for (size_t layerIdx = 0; layerIdx < layerCount; ++layerIdx) {
		ai::UnicodeString layerName(ductworkLayers[layerIdx]);
		AILayerHandle layer = nullptr;
		if (sAILayer->GetLayerByTitle(&layer, layerName) != kNoErr || !layer) {
			continue;
		}

		AIArtHandle layerGroup = nullptr;
		if (sAIArt->GetFirstArtOfLayer(layer, &layerGroup) != kNoErr || !layerGroup) {
			continue;
		}

		// Collect all compound paths on this layer first (to avoid modifying while iterating)
		std::vector<AIArtHandle> compoundsToRelease;
		AIArtHandle art = nullptr;
		sAIArt->GetArtFirstChild(layerGroup, &art);
		while (art) {
			short type = 0;
			if (sAIArt->GetArtType(art, &type) == kNoErr && type == kCompoundPathArt) {
				compoundsToRelease.push_back(art);
			}
			AIArtHandle next = nullptr;
			sAIArt->GetArtSibling(art, &next);
			art = next;
		}

		// Release each compound path
		for (size_t i = 0; i < compoundsToRelease.size(); ++i) {
			AIArtHandle compound = compoundsToRelease[i];

			// Collect all children first (safer than modifying while iterating)
			std::vector<AIArtHandle> children;
			AIArtHandle child = nullptr;
			sAIArt->GetArtFirstChild(compound, &child);
			while (child) {
				children.push_back(child);
				AIArtHandle nextChild = nullptr;
				sAIArt->GetArtSibling(child, &nextChild);
				child = nextChild;
			}

			// Move all children out of the compound path to the layer
			for (size_t c = 0; c < children.size(); ++c) {
				sAIArt->ReorderArt(children[c], kPlaceAbove, compound);
			}

			// Delete the now-empty compound path
			sAIArt->DisposeArt(compound);
			++releasedCount;
		}
	}

	if (releasedCount > 0) {
		DuctworkLog::Write("Released " + std::to_string(releasedCount) + " compound path(s)");
	}

	return releasedCount;
}

DuctworkCompoundStats DuctworkCompound::MergeConnectedPaths(const std::vector<DuctworkPath>& paths,
	const std::vector<DuctworkConnection>& connections,
	std::vector<AIArtHandle>& outCompounds)
{
	DuctworkCompoundStats stats = {};
	outCompounds.clear();
	if (!sAIArt || paths.empty()) {
		return stats;
	}

	const size_t count = paths.size();
	std::vector<int> parent(count);
	for (size_t i = 0; i < count; ++i) {
		parent[i] = static_cast<int>(i);
	}
	auto findRoot = [&parent](int idx) {
		int root = idx;
		while (parent[root] != root) {
			root = parent[root];
		}
		while (parent[idx] != idx) {
			int next = parent[idx];
			parent[idx] = root;
			idx = next;
		}
		return root;
	};
	auto unite = [&parent, &findRoot](int a, int b) {
		int ra = findRoot(a);
		int rb = findRoot(b);
		if (ra != rb) {
			parent[ra] = rb;
		}
	};

	for (size_t i = 0; i < connections.size(); ++i) {
		const DuctworkConnection& conn = connections[i];
		if (!IsValidIndex(conn.a, count) || !IsValidIndex(conn.b, count)) {
			continue;
		}
		if (paths[conn.a].layerName != paths[conn.b].layerName) {
			continue;
		}
		unite(conn.a, conn.b);
	}

	std::map<int, std::vector<int> > groups;
	for (size_t i = 0; i < count; ++i) {
		int root = findRoot(static_cast<int>(i));
		groups[root].push_back(static_cast<int>(i));
	}

	stats.components = groups.size();
	for (std::map<int, std::vector<int> >::const_iterator it = groups.begin(); it != groups.end(); ++it) {
		const std::vector<int>& indices = it->second;
		if (indices.size() <= 1) {
			++stats.skippedSingle;
			continue;
		}

		const std::string& layerName = paths[indices[0]].layerName;
		bool mixedLayer = false;
		for (size_t i = 1; i < indices.size(); ++i) {
			if (paths[indices[i]].layerName != layerName) {
				mixedLayer = true;
				break;
			}
		}
		if (mixedLayer) {
			++stats.skippedFailed;
			continue;
		}

		AIArtHandle layerGroup = GetLayerGroup(paths[indices[0]].art);
		if (!layerGroup) {
			++stats.skippedFailed;
			continue;
		}

		AIArtHandle firstParent = nullptr;
		bool needsParentReconcile = false;
		for (size_t i = 0; i < indices.size(); ++i) {
			AIArtHandle parentArt = nullptr;
			if (sAIArt->GetArtParent(paths[indices[i]].art, &parentArt) || !parentArt) {
				continue;
			}
			if (!firstParent) {
				firstParent = parentArt;
			} else if (parentArt != firstParent) {
				needsParentReconcile = true;
				break;
			}
		}

		if (needsParentReconcile) {
			for (size_t i = 0; i < indices.size(); ++i) {
				AIArtHandle art = paths[indices[i]].art;
				if (sAIArt->ReorderArt(art, kPlaceInsideOnTop, layerGroup) == kNoErr) {
					++stats.movedToLayer;
				}
			}
		}

		AIArtHandle compound = nullptr;
		if (sAIArt->NewArt(kCompoundPathArt, kPlaceInsideOnTop, layerGroup, &compound) || !compound) {
			++stats.skippedFailed;
			continue;
		}

		for (size_t i = 0; i < indices.size(); ++i) {
			AIArtHandle art = paths[indices[i]].art;
			sAIArt->ReorderArt(art, kPlaceInsideOnTop, compound);
		}

		outCompounds.push_back(compound);
		++stats.compoundsCreated;

		double rotationOverride = 0.0;
		bool hasRotationOverride = false;
		for (size_t i = 0; i < indices.size() && !hasRotationOverride; ++i) {
			if (TryGetRotationOverride(paths[indices[i]].art, rotationOverride)) {
				hasRotationOverride = true;
			}
		}

		double preCompoundScale = -1.0;
		for (size_t i = 0; i < indices.size() && preCompoundScale < 0.0; ++i) {
			std::string rawValue;
			if (TryGetMetaField(paths[indices[i]].art, "MDUX_PreCompoundScale", rawValue)) {
				double parsed = 0.0;
				if (TryParseNumber(rawValue, parsed)) {
					preCompoundScale = parsed;
				}
			}
		}

		if (preCompoundScale < 0.0) {
			double maxWidth = 0.0;
			for (size_t i = 0; i < indices.size(); ++i) {
				double width = 0.0;
				if (GetPathStrokeWidth(paths[indices[i]].art, width) && width > maxWidth) {
					maxWidth = width;
				}
			}
			if (maxWidth > 0.1) {
				preCompoundScale = std::floor(((maxWidth / 4.0) * 100.0) + 0.5);
			}
		}

		std::map<std::string, std::string> updates;
		if (hasRotationOverride) {
			updates["MDUX_RotationOverride"] = std::to_string(rotationOverride);
		}
		if (preCompoundScale >= 0.0) {
			updates["MDUX_PreCompoundScale"] = std::to_string(preCompoundScale);
		}
		if (!updates.empty() && WriteCompoundMeta(compound, updates)) {
			++stats.metaWritten;
		}
	}

	return stats;
}
