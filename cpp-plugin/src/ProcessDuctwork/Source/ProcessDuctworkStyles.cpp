#include "IllustratorSDK.h"
#include "ProcessDuctworkStyles.h"
#include "ProcessDuctworkLayers.h"
#include "ProcessDuctworkSuites.h"

#include <map>

namespace
{
	struct StrokeSpec
	{
		AIReal width;
		AIColor color;
	};

	AIColor MakeRGBColor(int red, int green, int blue)
	{
		AIColor color;
		color.kind = kThreeColor;
		color.c.rgb.red = static_cast<AIReal>(red) / 255.0f;
		color.c.rgb.green = static_cast<AIReal>(green) / 255.0f;
		color.c.rgb.blue = static_cast<AIReal>(blue) / 255.0f;
		return color;
	}

	AIColor MakeRGBColorFromHex(unsigned int hex)
	{
		const int red = (hex >> 16) & 0xFF;
		const int green = (hex >> 8) & 0xFF;
		const int blue = hex & 0xFF;
		return MakeRGBColor(red, green, blue);
	}

	StrokeSpec MakeStrokeSpec(AIReal width, unsigned int hex)
	{
		StrokeSpec spec;
		spec.width = width;
		spec.color = MakeRGBColorFromHex(hex);
		return spec;
	}

	bool BuildStyleSpec(const std::string& name, std::vector<StrokeSpec>& out)
	{
		out.clear();
		if (name == "Blue Ductwork") {
			out.push_back(MakeStrokeSpec(4.0f, 0x80E1FF));
			out.push_back(MakeStrokeSpec(8.0f, 0x0000FF));
			return true;
		}
		if (name == "Green Ductwork") {
			out.push_back(MakeStrokeSpec(4.0f, 0x00B713));
			out.push_back(MakeStrokeSpec(8.0f, 0x7EFE82));
			return true;
		}
		if (name == "Orange Ductwork") {
			out.push_back(MakeStrokeSpec(4.0f, 0xFF911C));
			out.push_back(MakeStrokeSpec(8.0f, 0xFF3C00));
			return true;
		}
		if (name == "Light Green Ductwork") {
			out.push_back(MakeStrokeSpec(4.0f, 0x00B713));
			out.push_back(MakeStrokeSpec(8.0f, 0x7EFE82));
			return true;
		}
		if (name == "Light Orange Ductwork") {
			out.push_back(MakeStrokeSpec(4.0f, 0xFFC17A));
			out.push_back(MakeStrokeSpec(8.0f, 0xFF9029));
			return true;
		}
		if (name == "Thermostat Lines") {
			out.push_back(MakeStrokeSpec(4.0f, 0xFF1E26));
			return true;
		}
		return false;
	}

	bool GetNamedStyle(AIDocumentHandle document, const std::string& name, AIArtStyleHandle& outStyle)
	{
		outStyle = nullptr;
		if (!sAIArtStyle || name.empty()) {
			return false;
		}
		ai::UnicodeString styleName = ai::UnicodeString::FromUTF8(name);
		if (sAIArtStyle->GetArtStyleByNameFromDocument(&outStyle, styleName, document)) {
			return false;
		}
		return outStyle != nullptr;
	}

	bool CreateNamedStyleFromSpecs(AIDocumentHandle document, const std::string& name, const std::vector<StrokeSpec>& strokes)
	{
		if (!sAIArtStyle || !sAIArtStyleParser || !document || name.empty() || strokes.empty()) {
			return false;
		}
		AIArtStyleHandle baseStyle = nullptr;
		if (sAIArtStyle->GetDefaultArtStyle(&baseStyle) || !baseStyle) {
			return false;
		}

		AIStyleParser parser = nullptr;
		if (sAIArtStyleParser->NewParser(&parser) || !parser) {
			return false;
		}

		bool created = false;
		if (sAIArtStyleParser->ParseStyle(parser, baseStyle) == kNoErr) {
			for (ai::int32 i = sAIArtStyleParser->CountPaintFields(parser) - 1; i >= 0; --i) {
				AIParserPaintField field = nullptr;
				if (sAIArtStyleParser->GetNthPaintField(parser, i, &field) == kNoErr) {
					sAIArtStyleParser->RemovePaintField(parser, field, true);
				}
			}

			for (size_t i = 0; i < strokes.size(); ++i) {
				AIStrokeStyle stroke;
				stroke.Init();
				stroke.color = strokes[i].color;
				stroke.width = strokes[i].width;
				stroke.overprint = false;

				AIParserPaintField paintField = nullptr;
				if (sAIArtStyleParser->NewPaintFieldStroke(&stroke, nullptr, &paintField) == kNoErr && paintField) {
					sAIArtStyleParser->InsertNthPaintField(parser, sAIArtStyleParser->CountPaintFields(parser), paintField);
				}
			}

			AIArtStyleHandle newStyle = nullptr;
			if (sAIArtStyleParser->CreateNewStyle(parser, &newStyle) == kNoErr && newStyle) {
				AIArtStyleHandle namedStyle = nullptr;
				ai::UnicodeString styleName = ai::UnicodeString::FromUTF8(name);
				if (sAIArtStyle->AddNamedStyle(newStyle, styleName, false, &namedStyle) == kNoErr && namedStyle) {
					created = true;
				}
			}
		}

		sAIArtStyleParser->DisposeParser(parser);
		return created;
	}

	bool RedefineNamedStyleFromSpecs(AIArtStyleHandle namedStyle, const std::vector<StrokeSpec>& strokes)
	{
		if (!sAIArtStyle || !sAIArtStyleParser || !namedStyle || strokes.empty()) {
			return false;
		}

		AIArtStyleHandle baseStyle = nullptr;
		if (sAIArtStyle->GetDefaultArtStyle(&baseStyle) || !baseStyle) {
			return false;
		}

		AIStyleParser parser = nullptr;
		if (sAIArtStyleParser->NewParser(&parser) || !parser) {
			return false;
		}

		bool redefined = false;
		if (sAIArtStyleParser->ParseStyle(parser, baseStyle) == kNoErr) {
			for (ai::int32 i = sAIArtStyleParser->CountPaintFields(parser) - 1; i >= 0; --i) {
				AIParserPaintField field = nullptr;
				if (sAIArtStyleParser->GetNthPaintField(parser, i, &field) == kNoErr) {
					sAIArtStyleParser->RemovePaintField(parser, field, true);
				}
			}

			for (size_t i = 0; i < strokes.size(); ++i) {
				AIStrokeStyle stroke;
				stroke.Init();
				stroke.color = strokes[i].color;
				stroke.width = strokes[i].width;
				stroke.overprint = false;

				AIParserPaintField paintField = nullptr;
				if (sAIArtStyleParser->NewPaintFieldStroke(&stroke, nullptr, &paintField) == kNoErr && paintField) {
					sAIArtStyleParser->InsertNthPaintField(parser, sAIArtStyleParser->CountPaintFields(parser), paintField);
				}
			}

			AIArtStyleHandle replacementStyle = nullptr;
			if (sAIArtStyleParser->CreateNewStyle(parser, &replacementStyle) == kNoErr && replacementStyle &&
				sAIArtStyle->RedefineNamedStyle(namedStyle, replacementStyle) == kNoErr) {
				redefined = true;
			}
		}

		sAIArtStyleParser->DisposeParser(parser);
		return redefined;
	}
}

DuctworkStyleStats DuctworkStyles::ApplyLineStyles(AIDocumentHandle document, const std::vector<DuctworkPath>& paths)
{
	DuctworkStyleStats stats = {};
	if (!sAIArtStyle || !document) {
		return stats;
	}

	std::map<std::string, bool> needed;
	for (size_t i = 0; i < paths.size(); ++i) {
		const DuctworkPath& path = paths[i];
		if (!DuctworkLayers::IsLineLayerName(path.layerName)) {
			continue;
		}
		needed[path.layerName] = true;
	}

	for (std::map<std::string, bool>::const_iterator it = needed.begin(); it != needed.end(); ++it) {
		std::vector<StrokeSpec> strokes;
		if (!BuildStyleSpec(it->first, strokes)) {
			++stats.skippedNoSample;
			continue;
		}
		AIArtStyleHandle existing = nullptr;
		if (GetNamedStyle(document, it->first, existing)) {
			if (!RedefineNamedStyleFromSpecs(existing, strokes)) {
				++stats.skippedNoSample;
			}
		} else {
			if (CreateNamedStyleFromSpecs(document, it->first, strokes)) {
			++stats.created;
			} else {
				++stats.skippedNoSample;
			}
		}
	}

	for (size_t i = 0; i < paths.size(); ++i) {
		const DuctworkPath& path = paths[i];
		if (!DuctworkLayers::IsLineLayerName(path.layerName)) {
			++stats.skippedNonLineLayer;
			continue;
		}
		AIArtStyleHandle style = nullptr;
		if (!GetNamedStyle(document, path.layerName, style)) {
			++stats.skippedMissingStyle;
			continue;
		}
		if (sAIArtStyle->SetArtStyle(path.art, style) == kNoErr) {
			++stats.applied;
		}
	}

	return stats;
}
