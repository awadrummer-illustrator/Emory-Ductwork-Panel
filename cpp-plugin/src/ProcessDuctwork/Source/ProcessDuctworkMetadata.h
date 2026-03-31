#ifndef __ProcessDuctworkMetadata_H__
#define __ProcessDuctworkMetadata_H__

#include "IllustratorSDK.h"

#include <map>
#include <string>
#include <vector>

namespace DuctworkMetadata
{
	struct TransformSummary
	{
		bool hasScale = false;
		bool hasRotation = false;
		bool mixedScale = false;
		bool mixedRotation = false;
		double scale = 100.0;
		double rotation = 0.0;
		size_t count = 0;
	};

	bool GetDouble(AIArtHandle art, const std::string& key, double& outValue);
	bool GetString(AIArtHandle art, const std::string& key, std::string& outValue);
	void SetDouble(AIArtHandle art, const std::string& key, double value);
	void SetString(AIArtHandle art, const std::string& key, const std::string& value);
	void RemoveKey(AIArtHandle art, const std::string& key);

	TransformSummary SummarizeSelectionTransform(const std::vector<AIArtHandle>& selection);

	void MirrorMetadataToNote(AIArtHandle art);
	void MirrorSelectionToNotes(const std::vector<AIArtHandle>& selection);

	double ReadScaleOrDefault(AIArtHandle art, double defaultValue);
	double ReadRotationOrDefault(AIArtHandle art, double defaultValue);
}

#endif // __ProcessDuctworkMetadata_H__
