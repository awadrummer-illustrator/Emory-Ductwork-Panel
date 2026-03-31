#include "IllustratorSDK.h"
#include "ProcessDuctworkMetadata.h"
#include "ProcessDuctworkNotes.h"
#include "ProcessDuctworkSuites.h"
#include "AIEntry.h"

#include <cmath>
#include <map>
#include <sstream>

namespace
{
	AIDictKey GetKeyCached(const std::string& key)
	{
		static std::map<std::string, AIDictKey> cache;
		std::map<std::string, AIDictKey>::const_iterator it = cache.find(key);
		if (it != cache.end()) {
			return it->second;
		}
		if (!sAIDictionary) {
			return nullptr;
		}
		AIDictKey dictKey = sAIDictionary->Key(key.c_str());
		cache[key] = dictKey;
		return dictKey;
	}

	bool GetArtDictionary(AIArtHandle art, AIDictionaryRef& outDict)
	{
		outDict = nullptr;
		if (!art || !sAIArt || !sAIDictionary) {
			return false;
		}
		if (sAIArt->GetDictionary(art, &outDict) != kNoErr || !outDict) {
			return false;
		}
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

	void SetNoteFromFields(AIArtHandle art, const std::map<std::string, std::string>& fields)
	{
		std::string note = DuctworkNotes::GetNote(art);
		std::vector<std::string> mdTags;
		std::string json;
		if (!DuctworkNotes::ExtractMDUXMeta(note, json, mdTags)) {
			CollectMDTags(note, mdTags);
		}

		if (fields.empty()) {
			if (mdTags.empty()) {
				DuctworkNotes::ClearNote(art);
			} else {
				DuctworkNotes::SetNote(art, DuctworkNotes::JoinTokens(mdTags));
			}
			return;
		}

		std::string updated = DuctworkNotes::BuildNoteWithMDUXMeta(DuctworkNotes::SerializeMetaJson(fields), mdTags);
		DuctworkNotes::SetNote(art, updated);
	}

	void CollectFieldsFromDict(AIDictionaryRef dict, std::map<std::string, std::string>& fields)
	{
		fields.clear();
		if (!dict || !sAIDictionary) {
			return;
		}
		const char* keys[] = {
			"MDUX_CurrentScale",
			"MDUX_CumulativeRotation",
			"MDUX_RotationOverride",
			"MDUX_OriginalWidth",
			"MDUX_OriginalHeight",
			"MDUX_OriginalStrokeWidth",
			"ductRole",
			"ductRoleReason",
			"ductRoleVersion"
		};
		for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
			AIDictKey dictKey = GetKeyCached(keys[i]);
			if (!dictKey) {
				continue;
			}
			if (!sAIDictionary->IsKnown(dict, dictKey)) {
				continue;
			}
			AIEntryType entryType = UnknownType;
			if (sAIDictionary->GetEntryType(dict, dictKey, &entryType) != kNoErr) {
				continue;
			}
			if (entryType == RealType) {
				AIReal value = 0;
				if (sAIDictionary->GetRealEntry(dict, dictKey, &value) == kNoErr) {
					fields[keys[i]] = std::to_string(static_cast<double>(value));
				}
			} else if (entryType == IntegerType) {
				ASInt32 value = 0;
				if (sAIDictionary->GetIntegerEntry(dict, dictKey, &value) == kNoErr) {
					fields[keys[i]] = std::to_string(static_cast<int>(value));
				}
			} else if (entryType == StringType) {
				const char* value = nullptr;
				if (sAIDictionary->GetStringEntry(dict, dictKey, &value) == kNoErr && value) {
					fields[keys[i]] = std::string("\"") + value + "\"";
				}
			}
		}
	}
}

namespace DuctworkMetadata
{
	bool GetDouble(AIArtHandle art, const std::string& key, double& outValue)
	{
		outValue = 0.0;
		AIDictionaryRef dict = nullptr;
		if (!GetArtDictionary(art, dict)) {
			return false;
		}
		AIDictKey dictKey = GetKeyCached(key);
		if (!dictKey || !sAIDictionary->IsKnown(dict, dictKey)) {
			sAIDictionary->Release(dict);
			return false;
		}
		AIEntryType entryType = UnknownType;
		if (sAIDictionary->GetEntryType(dict, dictKey, &entryType) != kNoErr) {
			sAIDictionary->Release(dict);
			return false;
		}
		bool ok = false;
		if (entryType == RealType) {
			AIReal value = 0;
			if (sAIDictionary->GetRealEntry(dict, dictKey, &value) == kNoErr) {
				outValue = static_cast<double>(value);
				ok = true;
			}
		} else if (entryType == IntegerType) {
			ASInt32 value = 0;
			if (sAIDictionary->GetIntegerEntry(dict, dictKey, &value) == kNoErr) {
				outValue = static_cast<double>(value);
				ok = true;
			}
		}
		sAIDictionary->Release(dict);
		return ok;
	}

	bool GetString(AIArtHandle art, const std::string& key, std::string& outValue)
	{
		outValue.clear();
		AIDictionaryRef dict = nullptr;
		if (!GetArtDictionary(art, dict)) {
			return false;
		}
		AIDictKey dictKey = GetKeyCached(key);
		if (!dictKey || !sAIDictionary->IsKnown(dict, dictKey)) {
			sAIDictionary->Release(dict);
			return false;
		}
		AIEntryType entryType = UnknownType;
		if (sAIDictionary->GetEntryType(dict, dictKey, &entryType) != kNoErr) {
			sAIDictionary->Release(dict);
			return false;
		}
		bool ok = false;
		if (entryType == StringType) {
			const char* value = nullptr;
			if (sAIDictionary->GetStringEntry(dict, dictKey, &value) == kNoErr && value) {
				outValue = value;
				ok = true;
			}
		}
		sAIDictionary->Release(dict);
		return ok;
	}

	void SetDouble(AIArtHandle art, const std::string& key, double value)
	{
		AIDictionaryRef dict = nullptr;
		if (!GetArtDictionary(art, dict)) {
			return;
		}
		AIDictKey dictKey = GetKeyCached(key);
		if (dictKey) {
			sAIDictionary->SetRealEntry(dict, dictKey, static_cast<AIReal>(value));
		}
		std::map<std::string, std::string> fields;
		CollectFieldsFromDict(dict, fields);
		SetNoteFromFields(art, fields);
		sAIDictionary->Release(dict);
	}

	void SetString(AIArtHandle art, const std::string& key, const std::string& value)
	{
		AIDictionaryRef dict = nullptr;
		if (!GetArtDictionary(art, dict)) {
			return;
		}
		AIDictKey dictKey = GetKeyCached(key);
		if (dictKey) {
			sAIDictionary->SetStringEntry(dict, dictKey, value.c_str());
		}
		std::map<std::string, std::string> fields;
		CollectFieldsFromDict(dict, fields);
		SetNoteFromFields(art, fields);
		sAIDictionary->Release(dict);
	}

	void RemoveKey(AIArtHandle art, const std::string& key)
	{
		AIDictionaryRef dict = nullptr;
		if (!GetArtDictionary(art, dict)) {
			return;
		}
		AIDictKey dictKey = GetKeyCached(key);
		if (dictKey && sAIDictionary->IsKnown(dict, dictKey)) {
			sAIDictionary->DeleteEntry(dict, dictKey);
		}
		std::map<std::string, std::string> fields;
		CollectFieldsFromDict(dict, fields);
		SetNoteFromFields(art, fields);
		sAIDictionary->Release(dict);
	}

	double ReadScaleOrDefault(AIArtHandle art, double defaultValue)
	{
		double value = 0.0;
		if (GetDouble(art, "MDUX_CurrentScale", value)) {
			return value;
		}
		return defaultValue;
	}

	double ReadRotationOrDefault(AIArtHandle art, double defaultValue)
	{
		double value = 0.0;
		if (GetDouble(art, "MDUX_CumulativeRotation", value)) {
			return value;
		}
		return defaultValue;
	}

	TransformSummary SummarizeSelectionTransform(const std::vector<AIArtHandle>& selection)
	{
		TransformSummary summary;
		double firstScale = 0.0;
		double firstRotation = 0.0;
		bool hasFirst = false;

		for (size_t i = 0; i < selection.size(); ++i) {
			AIArtHandle art = selection[i];
			if (!art) {
				continue;
			}
			double scale = ReadScaleOrDefault(art, 100.0);
			double rotation = ReadRotationOrDefault(art, 0.0);
			if (!hasFirst) {
				firstScale = scale;
				firstRotation = rotation;
				hasFirst = true;
				summary.hasScale = true;
				summary.hasRotation = true;
			} else {
				if (std::fabs(scale - firstScale) > 0.1) {
					summary.mixedScale = true;
				}
				if (std::fabs(rotation - firstRotation) > 0.1) {
					summary.mixedRotation = true;
				}
			}
			++summary.count;
			if (summary.mixedScale && summary.mixedRotation) {
				break;
			}
		}

		if (hasFirst) {
			summary.scale = firstScale;
			summary.rotation = firstRotation;
		}

		return summary;
	}

	void MirrorMetadataToNote(AIArtHandle art)
	{
		AIDictionaryRef dict = nullptr;
		if (!GetArtDictionary(art, dict)) {
			return;
		}
		std::map<std::string, std::string> fields;
		CollectFieldsFromDict(dict, fields);
		SetNoteFromFields(art, fields);
		sAIDictionary->Release(dict);
	}

	void MirrorSelectionToNotes(const std::vector<AIArtHandle>& selection)
	{
		for (size_t i = 0; i < selection.size(); ++i) {
			if (selection[i]) {
				MirrorMetadataToNote(selection[i]);
			}
		}
	}
}
