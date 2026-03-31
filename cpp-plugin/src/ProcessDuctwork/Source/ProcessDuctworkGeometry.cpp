#include "IllustratorSDK.h"
#include "ProcessDuctworkConnections.h"
#include "ProcessDuctworkGeometry.h"
#include "ProcessDuctworkLayers.h"
#include "ProcessDuctworkLog.h"
#include "ProcessDuctworkMath.h"
#include "ProcessDuctworkMetadata.h"
#include "ProcessDuctworkNotes.h"
#include "ProcessDuctworkSelection.h"
#include "ProcessDuctworkSuites.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>

namespace
{
	const double kPointEpsilon = 0.01;
	const double kDefaultDuctWidth = 8.0;
	const double kMinDuctWidth = 0.25;
	const double kGuideStrokeWidth = 1.0;
	const double kTrimMultiplier = 0.75;
	const double kCollinearThreshold = 0.9848;
	const double kRoundMinCenterlineRadiusMultiplier = 0.33;
	const char* const kEmoryRoleKey = "MDUX_EmoryRole";
	const char* const kEmorySourceIdKey = "MDUX_EmorySourceId";
	const char* const kEmoryBodyWidthKey = "MDUX_EmoryBodyWidth";
	const char* const kEmorySourceBodyWidthKey = "MDUX_EmorySourceBodyWidth";
	const char* const kEmorySourceStrokeWidthKey = "MDUX_EmorySourceStrokeWidth";
	const char* const kEmorySegmentWidthsKey = "MDUX_EmorySegmentWidths";
	const char* const kEmoryStartSegmentIndexKey = "MDUX_EmoryStartSegmentIndex";
	const char* const kEmorySegmentIndexKey = "MDUX_EmorySegmentIndex";
	const char* const kEmoryJointIndexKey = "MDUX_EmoryJointIndex";
	const char* const kEmoryConnectorStyleKey = "MDUX_EmoryConnectorStyle";
	const char* const kEmoryCornerStylesKey = "MDUX_EmoryCornerStyles";
	const char* const kEmoryRoleSegment = "segment";
	const char* const kEmoryRoleConnector = "connector";
	const char* const kEmoryRoleCenterline = "centerline";
	const char* const kEmoryGeneratedToken = "MD:EMORY_GENERATED";
	const char* const kEmoryCenterlineToken = "MD:EMORY_CENTERLINE";
	const char* const kEmoryBodyToken = "MD:EMORY_BODY";
	const char* const kEmorySourceIdPrefix = "MD:EMORY_SOURCE_ID=";

	struct Vec2
	{
		double x = 0.0;
		double y = 0.0;
	};

	struct EmoryColorSpec
	{
		AIColor fill;
		AIColor stroke;
	};

	struct CornerPairing
	{
		bool valid = false;
		DuctworkPoint startInner;
		DuctworkPoint startOuter;
		DuctworkPoint endInner;
		DuctworkPoint endOuter;
		DuctworkPoint cornerInner;
		DuctworkPoint cornerOuter;
	};

	struct ConnectorSpec
	{
		AIArtHandle sourceArt = nullptr;
		std::string sourceId;
		std::string layerName;
		int jointIndex = -1;
		DuctworkPoint joint;
		DuctworkPoint prevTrimPoint;
		DuctworkPoint nextTrimPoint;
		Vec2 prevDir;
		Vec2 nextDir;
		double prevWidth = 0.0;
		double nextWidth = 0.0;
		double prevTrimDistance = 0.0;
		double nextTrimDistance = 0.0;
		double trimDistance = 0.0;
		std::string style;
	};

	struct EmorySourceState
	{
		AIArtHandle art = nullptr;
		std::string sourceId;
		DuctworkPath path;
		std::string ductRole;
		int segmentCount = 0;
		int startSegmentIndex = 0;
		double defaultWidth = 0.0;
		std::vector<double> originalWidths;
		std::vector<double> widths;
		bool touched = false;
		bool selectedSeed = false;
	};

	struct PathConnectionAttachment
	{
		int endpointSlot = -1;
		int segmentIndex = -1;
	};

	struct EmoryRunTransform
	{
		bool valid = false;
		AIRealMatrix matrix{};
	};

	struct EmorySourceIdCandidate
	{
		AIArtHandle art = nullptr;
		DuctworkPath path;
		std::string oldSourceId;
		std::string newSourceId;
		bool selected = false;
	};

	struct SideCandidate
	{
		int signPrev = 0;
		int signNext = 0;
		DuctworkPoint corner;
		double dist2 = 0.0;
		bool inForwardWedge = false;
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

	EmoryColorSpec GetEmoryColorSpec(const std::string& layerName)
	{
		EmoryColorSpec spec;
		if (layerName == "Blue Ductwork") {
			spec.fill = MakeRGBColor(128, 225, 255);
			spec.stroke = MakeRGBColor(0, 0, 255);
			return spec;
		}
		if (layerName == "Orange Ductwork") {
			spec.fill = MakeRGBColor(255, 145, 28);
			spec.stroke = MakeRGBColor(255, 60, 0);
			return spec;
		}
		if (layerName == "Light Orange Ductwork") {
			spec.fill = MakeRGBColor(255, 193, 122);
			spec.stroke = MakeRGBColor(255, 144, 41);
			return spec;
		}

		spec.fill = MakeRGBColor(17, 166, 0);
		spec.stroke = MakeRGBColor(0, 255, 0);
		return spec;
	}

	double Dot(const Vec2& a, const Vec2& b)
	{
		return (a.x * b.x) + (a.y * b.y);
	}

	double Cross(const Vec2& a, const Vec2& b)
	{
		return (a.x * b.y) - (a.y * b.x);
	}

	double Length(const Vec2& value)
	{
		return std::sqrt((value.x * value.x) + (value.y * value.y));
	}

	bool Normalize(const Vec2& value, Vec2& out)
	{
		const double length = Length(value);
		if (length < 1e-9) {
			out.x = 0.0;
			out.y = 0.0;
			return false;
		}
		out.x = value.x / length;
		out.y = value.y / length;
		return true;
	}

	Vec2 Subtract(const DuctworkPoint& a, const DuctworkPoint& b)
	{
		Vec2 result;
		result.x = a.x - b.x;
		result.y = a.y - b.y;
		return result;
	}

	Vec2 Scale(const Vec2& value, double scale)
	{
		Vec2 result;
		result.x = value.x * scale;
		result.y = value.y * scale;
		return result;
	}

	Vec2 PerpCCW(const Vec2& value)
	{
		Vec2 result;
		result.x = -value.y;
		result.y = value.x;
		return result;
	}

	Vec2 PerpCW(const Vec2& value)
	{
		Vec2 result;
		result.x = value.y;
		result.y = -value.x;
		return result;
	}

	DuctworkPoint Add(const DuctworkPoint& point, const Vec2& direction, double distance)
	{
		DuctworkPoint result;
		result.x = point.x + (direction.x * distance);
		result.y = point.y + (direction.y * distance);
		return result;
	}

	bool NearlyEqual(double a, double b, double epsilon = kPointEpsilon)
	{
		return std::fabs(a - b) <= epsilon;
	}

	bool PointsEqual(const DuctworkPoint& a, const DuctworkPoint& b, double epsilon = kPointEpsilon)
	{
		return NearlyEqual(a.x, b.x, epsilon) && NearlyEqual(a.y, b.y, epsilon);
	}

	bool BuildUnitDirection(const DuctworkPoint& start, const DuctworkPoint& end, Vec2& outDirection, Vec2& outNormal)
	{
		Vec2 raw = Subtract(end, start);
		if (!Normalize(raw, outDirection)) {
			return false;
		}
		outNormal = PerpCCW(outDirection);
		return true;
	}

	std::vector<std::string> ReadNoteTokens(AIArtHandle art)
	{
		return DuctworkNotes::SplitTokens(DuctworkNotes::GetNote(art));
	}

	void WriteNoteTokens(AIArtHandle art, std::vector<std::string>& tokens)
	{
		DuctworkNotes::SetNote(art, DuctworkNotes::JoinTokens(tokens));
	}

	void UpsertNoteToken(std::vector<std::string>& tokens, const std::string& prefix, const std::string& value)
	{
		for (std::vector<std::string>::iterator it = tokens.begin(); it != tokens.end();) {
			if (it->find(prefix) == 0) {
				it = tokens.erase(it);
			} else {
				++it;
			}
		}
		tokens.push_back(prefix + value);
	}

	std::string FindNoteValue(const std::vector<std::string>& tokens, const std::string& prefix)
	{
		for (size_t i = 0; i < tokens.size(); ++i) {
			if (tokens[i].find(prefix) == 0) {
				return tokens[i].substr(prefix.size());
			}
		}
		return std::string();
	}

	std::string GenerateSourceId()
	{
		static ai::int32 counter = 0;
		const long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		std::ostringstream out;
		out << "emory-" << nowMs << "-" << ++counter;
		return out.str();
	}

	std::string JsonEscape(const std::string& value)
	{
		std::ostringstream out;
		for (size_t i = 0; i < value.size(); ++i) {
			const char ch = value[i];
			switch (ch) {
			case '\\':
				out << "\\\\";
				break;
			case '"':
				out << "\\\"";
				break;
			case '\r':
				out << "\\r";
				break;
			case '\n':
				out << "\\n";
				break;
			case '\t':
				out << "\\t";
				break;
			default:
				out << ch;
				break;
			}
		}
		return out.str();
	}

	bool IsGeneratedRole(const std::string& role)
	{
		return role == kEmoryRoleSegment || role == kEmoryRoleConnector;
	}

	void CollectAllPathsFromArt(AIArtHandle art, std::vector<AIArtHandle>& outPaths)
	{
		if (!art || !sAIArt) {
			return;
		}

		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type) != kNoErr) {
			return;
		}

		if (type == kPathArt) {
			outPaths.push_back(art);
		}

		AIArtHandle child = nullptr;
		if (sAIArt->GetArtFirstChild(art, &child) == kNoErr && child) {
			AIArtHandle current = child;
			while (current) {
				CollectAllPathsFromArt(current, outPaths);
				AIArtHandle next = nullptr;
				if (sAIArt->GetArtSibling(current, &next) != kNoErr) {
					break;
				}
				current = next;
			}
		}
	}

	void CollectAllLineLayerPaths(std::vector<AIArtHandle>& outPaths)
	{
		outPaths.clear();
		if (!sAILayer || !sAIArt) {
			return;
		}

		AILayerHandle layer = nullptr;
		if (sAILayer->GetFirstLayer(&layer) != kNoErr) {
			return;
		}

		while (layer) {
			ai::UnicodeString title;
			if (sAILayer->GetLayerTitle(layer, title) == kNoErr) {
				const std::string layerName = title.as_UTF8();
				if (DuctworkLayers::IsLineLayerName(layerName)) {
					AIArtHandle layerGroup = nullptr;
					if (sAIArt->GetFirstArtOfLayer(layer, &layerGroup) == kNoErr && layerGroup) {
						CollectAllPathsFromArt(layerGroup, outPaths);
					}
				}
			}

			AILayerHandle next = nullptr;
			if (sAILayer->GetNextLayer(layer, &next) != kNoErr) {
				break;
			}
			layer = next;
		}
	}

	bool GetSimpleStrokeWidth(AIArtHandle path, double& outWidth)
	{
		outWidth = 0.0;
		if (!path || !sAIPathStyle) {
			return false;
		}

		AIPathStyle style;
		AIBoolean fillVisible = false;
		AIBoolean strokeVisible = false;
		if (sAIPathStyle->GetPathStyleEx(path, &style, &fillVisible, &strokeVisible) != kNoErr) {
			return false;
		}
		if (!strokeVisible || !style.strokePaint) {
			return false;
		}

		outWidth = static_cast<double>(style.stroke.width);
		return outWidth > 0.0;
	}

	bool GetMaxStyleStrokeWidth(AIArtHandle art, double& outWidth)
	{
		outWidth = 0.0;
		if (!art || !sAIArtStyle || !sAIArtStyleParser) {
			return false;
		}

		AIArtStyleHandle artStyle = nullptr;
		if (sAIArtStyle->GetArtStyle(art, &artStyle) != kNoErr || !artStyle) {
			return false;
		}

		AIStyleParser parser = nullptr;
		if (sAIArtStyleParser->NewParser(&parser) != kNoErr || !parser) {
			return false;
		}

		double maxWidth = 0.0;
		const bool parsed = (sAIArtStyleParser->ParseStyle(parser, artStyle) == kNoErr);
		if (parsed) {
			const ai::int32 fieldCount = sAIArtStyleParser->CountPaintFields(parser);
			for (ai::int32 i = 0; i < fieldCount; ++i) {
				AIParserPaintField field = nullptr;
				if (sAIArtStyleParser->GetNthPaintField(parser, i, &field) != kNoErr || !field) {
					continue;
				}
				if (!sAIArtStyleParser->IsStroke(field)) {
					continue;
				}

				AIStrokeStyle stroke;
				stroke.Init();
				AIArtStylePaintData paintData;
				paintData.InitToUnknown();
				if (sAIArtStyleParser->GetStroke(field, &stroke, &paintData) == kNoErr) {
					maxWidth = (std::max)(maxWidth, static_cast<double>(stroke.width));
				}
			}
		}

		sAIArtStyleParser->DisposeParser(parser);
		if (maxWidth <= 0.0) {
			return false;
		}
		outWidth = maxWidth;
		return true;
	}

	void SanitizePolyline(const std::vector<DuctworkPoint>& input, std::vector<DuctworkPoint>& output)
	{
		output.clear();
		for (size_t i = 0; i < input.size(); ++i) {
			if (output.empty() || !PointsEqual(output.back(), input[i])) {
				output.push_back(input[i]);
			}
		}
		while (output.size() > 1 && PointsEqual(output.front(), output.back())) {
			output.pop_back();
		}
	}

	void AppendIfDistinct(std::vector<DuctworkPoint>& polygon, const DuctworkPoint& point)
	{
		if (polygon.empty() || !PointsEqual(polygon.back(), point)) {
			polygon.push_back(point);
		}
	}

	double ComputeBodyStrokeWidth(double bodyWidth)
	{
		double strokeWidth = bodyWidth * 0.16;
		if (strokeWidth < 1.0) {
			strokeWidth = 1.0;
		}
		if (strokeWidth > 4.0) {
			strokeWidth = 4.0;
		}
		return strokeWidth;
	}

	bool ResetToSimpleArtStyle(AIArtHandle art)
	{
		if (!art || !sAIArtStyle) {
			return false;
		}
		AIArtStyleHandle defaultStyle = nullptr;
		if (sAIArtStyle->GetDefaultArtStyle(&defaultStyle) != kNoErr || !defaultStyle) {
			return false;
		}
		return sAIArtStyle->SetArtStyle(art, defaultStyle) == kNoErr;
	}

	bool CreateClosedPath(AIArtHandle referenceArt, const std::vector<DuctworkPoint>& polygon, AIArtHandle& outPath)
	{
		outPath = nullptr;
		if (!referenceArt || polygon.size() < 3 || !sAIArt || !sAIPath) {
			return false;
		}

		AIArtHandle path = nullptr;
		if (sAIArt->NewArt(kPathArt, kPlaceAbove, referenceArt, &path) != kNoErr || !path) {
			return false;
		}

		const ai::int16 segmentCount = static_cast<ai::int16>(polygon.size());
		if (sAIPath->SetPathSegmentCount(path, segmentCount) != kNoErr) {
			sAIArt->DisposeArt(path);
			return false;
		}

		std::vector<AIPathSegment> segments(polygon.size());
		for (size_t i = 0; i < polygon.size(); ++i) {
			segments[i].p.h = static_cast<AIReal>(polygon[i].x);
			segments[i].p.v = static_cast<AIReal>(polygon[i].y);
			segments[i].in = segments[i].p;
			segments[i].out = segments[i].p;
			segments[i].corner = true;
		}

		if (sAIPath->SetPathSegments(path, 0, segmentCount, &segments[0]) != kNoErr ||
			sAIPath->SetPathClosed(path, true) != kNoErr) {
			sAIArt->DisposeArt(path);
			return false;
		}

		sAIArt->SetArtUserAttr(path, kArtLocked | kArtHidden, 0);
		outPath = path;
		return true;
	}

	bool ApplyFilledPathStyle(AIArtHandle art, const EmoryColorSpec& colors, double strokeWidth)
	{
		if (!art || !sAIPathStyle) {
			return false;
		}

		ResetToSimpleArtStyle(art);

		AIPathStyle style;
		AIBoolean fillVisible = false;
		AIBoolean strokeVisible = false;
		if (sAIPathStyle->GetPathStyleEx(art, &style, &fillVisible, &strokeVisible) != kNoErr) {
			return false;
		}

		fillVisible = true;
		strokeVisible = true;
		style.fillPaint = true;
		style.strokePaint = true;
		style.fill.color = colors.fill;
		style.stroke.color = colors.stroke;
		style.stroke.width = static_cast<AIReal>(strokeWidth > 0.0 ? strokeWidth : 1.0);
		return sAIPathStyle->SetPathStyleEx(art, &style, fillVisible, strokeVisible) == kNoErr;
	}

	bool ApplyGuideStyleInternal(AIArtHandle art, const EmoryColorSpec& colors)
	{
		if (!art || !sAIPathStyle) {
			return false;
		}

		ResetToSimpleArtStyle(art);

		AIPathStyle style;
		AIBoolean fillVisible = false;
		AIBoolean strokeVisible = false;
		if (sAIPathStyle->GetPathStyleEx(art, &style, &fillVisible, &strokeVisible) != kNoErr) {
			return false;
		}

		fillVisible = false;
		strokeVisible = true;
		style.fillPaint = false;
		style.strokePaint = true;
		style.stroke.width = static_cast<AIReal>(kGuideStrokeWidth);
		style.stroke.color = colors.stroke;
		return sAIPathStyle->SetPathStyleEx(art, &style, fillVisible, strokeVisible) == kNoErr;
	}

	void UpdateEmoryTokens(AIArtHandle art, const std::string& role, const std::string& sourceId)
	{
		std::vector<std::string> tokens = ReadNoteTokens(art);
		DuctworkNotes::RemoveToken(tokens, kEmoryGeneratedToken);
		DuctworkNotes::RemoveToken(tokens, kEmoryCenterlineToken);
		DuctworkNotes::RemoveToken(tokens, kEmoryBodyToken);
		if (role == kEmoryRoleCenterline) {
			DuctworkNotes::AddToken(tokens, kEmoryCenterlineToken);
		} else if (IsGeneratedRole(role)) {
			DuctworkNotes::AddToken(tokens, kEmoryGeneratedToken);
			DuctworkNotes::AddToken(tokens, kEmoryBodyToken);
		}
		if (!sourceId.empty()) {
			UpsertNoteToken(tokens, kEmorySourceIdPrefix, sourceId);
		}
		WriteNoteTokens(art, tokens);
	}

	std::string ReadEmorySourceIdFromNote(AIArtHandle art)
	{
		const std::vector<std::string> tokens = ReadNoteTokens(art);
		return FindNoteValue(tokens, kEmorySourceIdPrefix);
	}

	bool IsGeneratedEmoryArtInternal(AIArtHandle art)
	{
		if (!art) {
			return false;
		}

		std::string role;
		if (DuctworkMetadata::GetString(art, kEmoryRoleKey, role) && IsGeneratedRole(role)) {
			return true;
		}

		const std::vector<std::string> tokens = ReadNoteTokens(art);
		return DuctworkNotes::HasToken(tokens, kEmoryGeneratedToken) || DuctworkNotes::HasToken(tokens, kEmoryBodyToken);
	}

	bool BuildBandPolygon(const DuctworkPoint& start, const DuctworkPoint& end, double fullWidth, std::vector<DuctworkPoint>& outPolygon)
	{
		outPolygon.clear();
		Vec2 dir;
		Vec2 normal;
		if (!BuildUnitDirection(start, end, dir, normal)) {
			return false;
		}

		const double halfWidth = (std::max)(fullWidth, kMinDuctWidth) * 0.5;
		AppendIfDistinct(outPolygon, Add(start, normal, halfWidth));
		AppendIfDistinct(outPolygon, Add(end, normal, halfWidth));
		AppendIfDistinct(outPolygon, Add(end, normal, -halfWidth));
		AppendIfDistinct(outPolygon, Add(start, normal, -halfWidth));
		return outPolygon.size() >= 3;
	}

	bool LineIntersection(const DuctworkPoint& pointA, const Vec2& dirA,
		const DuctworkPoint& pointB, const Vec2& dirB, DuctworkPoint& outPoint)
	{
		const double denom = Cross(dirA, dirB);
		if (std::fabs(denom) < 1e-6) {
			return false;
		}

		Vec2 delta = Subtract(pointB, pointA);
		const double t = Cross(delta, dirB) / denom;
		outPoint = Add(pointA, dirA, t);
		return true;
	}

	void ParseCornerStyles(const std::string& serialized, std::map<int, std::string>& outStyles)
	{
		outStyles.clear();
		std::stringstream ss(serialized);
		std::string token;
		while (std::getline(ss, token, ',')) {
			if (token.empty()) {
				continue;
			}
			const size_t sep = token.find(':');
			if (sep == std::string::npos) {
				continue;
			}
			const int index = std::atoi(token.substr(0, sep).c_str());
			std::string style = token.substr(sep + 1);
			if (index < 0 || style.empty()) {
				continue;
			}
			outStyles[index] = style;
		}
	}

	std::string SerializeCornerStyles(const std::map<int, std::string>& styles)
	{
		std::ostringstream out;
		bool first = true;
		for (std::map<int, std::string>::const_iterator it = styles.begin(); it != styles.end(); ++it) {
			if (!first) {
				out << ",";
			}
			first = false;
			out << it->first << ":" << it->second;
		}
		return out.str();
	}

	std::string ReadCornerStyle(AIArtHandle sourceArt, int jointIndex)
	{
		std::string serialized;
		if (!DuctworkMetadata::GetString(sourceArt, kEmoryCornerStylesKey, serialized) || serialized.empty()) {
			return "round";
		}

		std::map<int, std::string> styles;
		ParseCornerStyles(serialized, styles);
		std::map<int, std::string>::const_iterator it = styles.find(jointIndex);
		if (it == styles.end() || it->second.empty()) {
			return "round";
		}
		return it->second;
	}

	void WriteCornerStyle(AIArtHandle sourceArt, int jointIndex, const std::string& style)
	{
		std::string serialized;
		std::map<int, std::string> styles;
		if (DuctworkMetadata::GetString(sourceArt, kEmoryCornerStylesKey, serialized) && !serialized.empty()) {
			ParseCornerStyles(serialized, styles);
		}
		styles[jointIndex] = style;
		DuctworkMetadata::SetString(sourceArt, kEmoryCornerStylesKey, SerializeCornerStyles(styles));
	}

	bool ReadStoredSourceBodyWidth(AIArtHandle sourceArt, double& outWidth)
	{
		outWidth = 0.0;
		return sourceArt && DuctworkMetadata::GetDouble(sourceArt, kEmorySourceBodyWidthKey, outWidth) && outWidth > 0.0;
	}

	void WriteStoredSourceBodyWidth(AIArtHandle sourceArt, double width)
	{
		if (!sourceArt || width <= 0.0) {
			return;
		}
		DuctworkMetadata::SetDouble(sourceArt, kEmorySourceBodyWidthKey, width);
	}

	bool ReadStoredSourceStrokeWidth(AIArtHandle sourceArt, double& outWidth)
	{
		outWidth = 0.0;
		return sourceArt && DuctworkMetadata::GetDouble(sourceArt, kEmorySourceStrokeWidthKey, outWidth) && outWidth > 0.0;
	}

	void WriteStoredSourceStrokeWidth(AIArtHandle sourceArt, double width)
	{
		if (!sourceArt || width <= 0.0) {
			return;
		}
		DuctworkMetadata::SetDouble(sourceArt, kEmorySourceStrokeWidthKey, width);
	}

	bool FindGeneratedBodyWidthForSourceId(const std::string& sourceId, double& outWidth)
	{
		outWidth = 0.0;
		if (sourceId.empty()) {
			return false;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || !IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			std::string artSourceId;
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, artSourceId) || artSourceId.empty()) {
				artSourceId = ReadEmorySourceIdFromNote(art);
			}
			if (artSourceId != sourceId) {
				continue;
			}

			double width = 0.0;
			if (DuctworkMetadata::GetDouble(art, kEmoryBodyWidthKey, width) && width > outWidth) {
				outWidth = width;
			}
		}
		return outWidth > 0.0;
	}

	bool FindGeneratedStrokeWidthForSourceId(const std::string& sourceId, double& outWidth)
	{
		outWidth = 0.0;
		if (sourceId.empty()) {
			return false;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || !IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			std::string artSourceId;
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, artSourceId) || artSourceId.empty()) {
				artSourceId = ReadEmorySourceIdFromNote(art);
			}
			if (artSourceId != sourceId) {
				continue;
			}

			double width = 0.0;
			if ((GetMaxStyleStrokeWidth(art, width) || GetSimpleStrokeWidth(art, width)) && width > outWidth) {
				outWidth = width;
			}
		}
		return outWidth > 0.0;
	}

	bool ResolveSourceBodyWidth(AIArtHandle sourceArt, const std::string& sourceId, double& outWidth)
	{
		outWidth = 0.0;
		if (ReadStoredSourceBodyWidth(sourceArt, outWidth)) {
			return true;
		}

		if (GetMaxStyleStrokeWidth(sourceArt, outWidth) || GetSimpleStrokeWidth(sourceArt, outWidth)) {
			if (outWidth > 0.0 && std::fabs(outWidth - kGuideStrokeWidth) > 1e-6) {
				WriteStoredSourceBodyWidth(sourceArt, outWidth);
				return true;
			}
		}

		if (FindGeneratedBodyWidthForSourceId(sourceId, outWidth)) {
			WriteStoredSourceBodyWidth(sourceArt, outWidth);
			return true;
		}

		if ((GetMaxStyleStrokeWidth(sourceArt, outWidth) || GetSimpleStrokeWidth(sourceArt, outWidth)) && outWidth > 0.0) {
			WriteStoredSourceBodyWidth(sourceArt, outWidth);
			return true;
		}
		return false;
	}

	bool ResolveSourceStrokeWidth(AIArtHandle sourceArt, const std::string& sourceId, double bodyWidth, double& outWidth)
	{
		outWidth = 0.0;
		if (ReadStoredSourceStrokeWidth(sourceArt, outWidth)) {
			return true;
		}

		if (FindGeneratedStrokeWidthForSourceId(sourceId, outWidth)) {
			WriteStoredSourceStrokeWidth(sourceArt, outWidth);
			return true;
		}

		outWidth = ComputeBodyStrokeWidth(bodyWidth);
		if (outWidth > 0.0) {
			WriteStoredSourceStrokeWidth(sourceArt, outWidth);
			return true;
		}
		return false;
	}

	std::string SerializeSegmentWidths(const std::vector<double>& widths)
	{
		std::ostringstream out;
		for (size_t i = 0; i < widths.size(); ++i) {
			if (i > 0) {
				out << ",";
			}
			out << widths[i];
		}
		return out.str();
	}

	void ReadSegmentWidths(AIArtHandle sourceArt, size_t segmentCount, double defaultWidth, std::vector<double>& outWidths)
	{
		outWidths.assign(segmentCount, defaultWidth);
		if (!sourceArt || segmentCount == 0) {
			return;
		}

		std::string serialized;
		if (!DuctworkMetadata::GetString(sourceArt, kEmorySegmentWidthsKey, serialized) || serialized.empty()) {
			return;
		}

		std::stringstream ss(serialized);
		std::string token;
		size_t index = 0;
		while (std::getline(ss, token, ',') && index < segmentCount) {
			const double width = std::atof(token.c_str());
			outWidths[index] = width > 0.0 ? width : defaultWidth;
			++index;
		}

		for (size_t i = 0; i < outWidths.size(); ++i) {
			if (outWidths[i] < kMinDuctWidth) {
				outWidths[i] = kMinDuctWidth;
			}
		}
	}

	void WriteSegmentWidths(AIArtHandle sourceArt, const std::vector<double>& widths)
	{
		if (!sourceArt) {
			return;
		}
		DuctworkMetadata::SetString(sourceArt, kEmorySegmentWidthsKey, SerializeSegmentWidths(widths));
	}

	int ReadStartSegmentIndex(AIArtHandle sourceArt, size_t segmentCount)
	{
		if (!sourceArt || segmentCount == 0) {
			return 0;
		}

		double startValue = 0.0;
		if (!DuctworkMetadata::GetDouble(sourceArt, kEmoryStartSegmentIndexKey, startValue)) {
			return 0;
		}

		int startIndex = static_cast<int>(startValue);
		if (startIndex < 0) {
			startIndex = 0;
		}
		if (startIndex >= static_cast<int>(segmentCount)) {
			startIndex = static_cast<int>(segmentCount - 1);
		}
		return startIndex;
	}

	void WriteStartSegmentIndex(AIArtHandle sourceArt, int startSegmentIndex)
	{
		if (!sourceArt || startSegmentIndex < 0) {
			return;
		}
		DuctworkMetadata::SetDouble(sourceArt, kEmoryStartSegmentIndexKey, static_cast<double>(startSegmentIndex));
	}

	bool BuildProcessPathForArt(AIArtHandle art, DuctworkPath& outPath);
	void ClearSelectionInternal();
	void SelectArtListInternal(const std::vector<AIArtHandle>& artList);

	bool ReadGeneratedSegmentIndex(AIArtHandle art, int& outSegmentIndex)
	{
		outSegmentIndex = -1;
		double segmentIndexValue = -1.0;
		if (!art || !DuctworkMetadata::GetDouble(art, kEmorySegmentIndexKey, segmentIndexValue)) {
			return false;
		}
		const int segmentIndex = static_cast<int>(segmentIndexValue);
		if (segmentIndex < 0) {
			return false;
		}
		outSegmentIndex = segmentIndex;
		return true;
	}

	bool ReadGeneratedJointIndex(AIArtHandle art, int& outJointIndex)
	{
		outJointIndex = -1;
		double jointIndexValue = -1.0;
		if (!art || !DuctworkMetadata::GetDouble(art, kEmoryJointIndexKey, jointIndexValue)) {
			return false;
		}
		const int jointIndex = static_cast<int>(jointIndexValue);
		if (jointIndex < 0) {
			return false;
		}
		outJointIndex = jointIndex;
		return true;
	}

	bool ComputeArtCenterPoint(AIArtHandle art, DuctworkPoint& outCenter)
	{
		outCenter.x = 0.0;
		outCenter.y = 0.0;

		std::vector<DuctworkPoint> points;
		bool closed = false;
		if (!art || !DuctworkGeometry::GetPathPoints(art, points, closed) || points.empty()) {
			return false;
		}

		double minX = points[0].x;
		double minY = points[0].y;
		double maxX = points[0].x;
		double maxY = points[0].y;
		for (size_t i = 1; i < points.size(); ++i) {
			minX = (std::min)(minX, points[i].x);
			minY = (std::min)(minY, points[i].y);
			maxX = (std::max)(maxX, points[i].x);
			maxY = (std::max)(maxY, points[i].y);
		}

		outCenter.x = (minX + maxX) * 0.5;
		outCenter.y = (minY + maxY) * 0.5;
		return true;
	}

	bool ComputeGeneratedArtCandidateScore(AIArtHandle art, const std::string& role,
		const DuctworkPath& candidatePath, double& outScore)
	{
		outScore = 0.0;
		if (!art || candidatePath.art == nullptr) {
			return false;
		}

		const std::string artLayerName = DuctworkGeometry::GetArtLayerName(art);
		if (!artLayerName.empty() && !candidatePath.layerName.empty() && artLayerName != candidatePath.layerName) {
			return false;
		}

		DuctworkPoint artCenter;
		if (!ComputeArtCenterPoint(art, artCenter)) {
			return false;
		}

		std::vector<DuctworkPoint> points;
		SanitizePolyline(candidatePath.points, points);
		if (points.size() < 2) {
			return false;
		}

		DuctworkPoint targetPoint;
		if (role == kEmoryRoleSegment) {
			int segmentIndex = -1;
			if (!ReadGeneratedSegmentIndex(art, segmentIndex) ||
				segmentIndex < 0 ||
				segmentIndex + 1 >= static_cast<int>(points.size())) {
				return false;
			}
			targetPoint.x = (points[segmentIndex].x + points[segmentIndex + 1].x) * 0.5;
			targetPoint.y = (points[segmentIndex].y + points[segmentIndex + 1].y) * 0.5;
		} else if (role == kEmoryRoleConnector) {
			int jointIndex = -1;
			if (!ReadGeneratedJointIndex(art, jointIndex) ||
				jointIndex <= 0 ||
				jointIndex >= static_cast<int>(points.size() - 1)) {
				return false;
			}
			targetPoint = points[jointIndex];
		} else {
			return false;
		}

		const double dx = artCenter.x - targetPoint.x;
		const double dy = artCenter.y - targetPoint.y;
		outScore = (dx * dx) + (dy * dy);
		return true;
	}

	bool CollectSourceIdCandidateGroups(const std::set<std::string>& sourceIds,
		std::map<std::string, std::vector<EmorySourceIdCandidate> >& outGroups)
	{
		outGroups.clear();
		if (sourceIds.empty()) {
			return false;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			DuctworkPath path;
			if (!BuildProcessPathForArt(art, path) ||
				!DuctworkGeometry::IsCenterlineCandidate(path.art, path.points, path.closed, path.layerName)) {
				continue;
			}

			std::string sourceId;
			if (!DuctworkGeometry::EnsureEmorySourceId(art, sourceId) || sourceId.empty() ||
				sourceIds.find(sourceId) == sourceIds.end()) {
				continue;
			}

			EmorySourceIdCandidate candidate;
			candidate.art = art;
			candidate.path = path;
			candidate.oldSourceId = sourceId;
			outGroups[sourceId].push_back(candidate);
		}

		return !outGroups.empty();
	}

	bool CollectAllSourceIdCandidateGroups(std::map<std::string, std::vector<EmorySourceIdCandidate> >& outGroups)
	{
		outGroups.clear();

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			DuctworkPath path;
			if (!BuildProcessPathForArt(art, path) ||
				!DuctworkGeometry::IsCenterlineCandidate(path.art, path.points, path.closed, path.layerName)) {
				continue;
			}

			std::string sourceId;
			if (!DuctworkGeometry::EnsureEmorySourceId(art, sourceId) || sourceId.empty()) {
				continue;
			}

			EmorySourceIdCandidate candidate;
			candidate.art = art;
			candidate.path = path;
			candidate.oldSourceId = sourceId;
			outGroups[sourceId].push_back(candidate);
		}

		return !outGroups.empty();
	}

	void RebindGeneratedArtForDuplicateSourceIds(const std::map<std::string, std::vector<EmorySourceIdCandidate> >& groups)
	{
		if (!sAIArt || groups.empty()) {
			return;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || !IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			std::string sourceId;
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
				sourceId = ReadEmorySourceIdFromNote(art);
			}
			if (sourceId.empty()) {
				continue;
			}

			std::map<std::string, std::vector<EmorySourceIdCandidate> >::const_iterator groupIt = groups.find(sourceId);
			if (groupIt == groups.end()) {
				continue;
			}

			bool hasReassignment = false;
			for (size_t candidateIndex = 0; candidateIndex < groupIt->second.size(); ++candidateIndex) {
				const EmorySourceIdCandidate& candidate = groupIt->second[candidateIndex];
				if (!candidate.newSourceId.empty() && candidate.newSourceId != candidate.oldSourceId) {
					hasReassignment = true;
					break;
				}
			}
			if (!hasReassignment) {
				continue;
			}

			std::string role;
			if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) || !IsGeneratedRole(role)) {
				continue;
			}

			const EmorySourceIdCandidate* bestCandidate = nullptr;
			double bestScore = 0.0;
			bool bestScoreSet = false;
			for (size_t candidateIndex = 0; candidateIndex < groupIt->second.size(); ++candidateIndex) {
				const EmorySourceIdCandidate& candidate = groupIt->second[candidateIndex];
				double candidateScore = 0.0;
				if (!ComputeGeneratedArtCandidateScore(art, role, candidate.path, candidateScore)) {
					continue;
				}

				if (!bestScoreSet || candidateScore < bestScore) {
					bestScore = candidateScore;
					bestCandidate = &candidate;
					bestScoreSet = true;
				}
			}

			if (!bestCandidate || bestCandidate->newSourceId.empty() || bestCandidate->newSourceId == sourceId) {
				continue;
			}

			DuctworkMetadata::SetString(art, kEmorySourceIdKey, bestCandidate->newSourceId);
			UpdateEmoryTokens(art, role, bestCandidate->newSourceId);
		}
	}

	void ApplySourceIdReassignments(const std::map<std::string, std::vector<EmorySourceIdCandidate> >& groups)
	{
		for (std::map<std::string, std::vector<EmorySourceIdCandidate> >::const_iterator groupIt = groups.begin();
			groupIt != groups.end(); ++groupIt) {
			for (size_t candidateIndex = 0; candidateIndex < groupIt->second.size(); ++candidateIndex) {
				const EmorySourceIdCandidate& candidate = groupIt->second[candidateIndex];
				if (candidate.newSourceId.empty() || candidate.newSourceId == candidate.oldSourceId) {
					continue;
				}

				DuctworkMetadata::SetString(candidate.art, kEmorySourceIdKey, candidate.newSourceId);
				DuctworkMetadata::SetString(candidate.art, kEmoryRoleKey, kEmoryRoleCenterline);
				UpdateEmoryTokens(candidate.art, kEmoryRoleCenterline, candidate.newSourceId);
			}
		}
	}

	void NormalizeDuplicateEmorySourceIds()
	{
		std::map<std::string, std::vector<EmorySourceIdCandidate> > groups;
		if (!CollectAllSourceIdCandidateGroups(groups)) {
			return;
		}

		bool anyReassigned = false;
		for (std::map<std::string, std::vector<EmorySourceIdCandidate> >::iterator groupIt = groups.begin();
			groupIt != groups.end(); ++groupIt) {
			if (groupIt->second.size() <= 1) {
				continue;
			}

			for (size_t candidateIndex = 1; candidateIndex < groupIt->second.size(); ++candidateIndex) {
				EmorySourceIdCandidate& candidate = groupIt->second[candidateIndex];
				candidate.newSourceId = GenerateSourceId();
				anyReassigned = true;
			}
		}

		if (!anyReassigned) {
			return;
		}

		RebindGeneratedArtForDuplicateSourceIds(groups);
		ApplySourceIdReassignments(groups);
	}

	bool FindSourceArtForSourceId(const std::string& sourceId, AIArtHandle& outSourceArt, DuctworkPath& outPath)
	{
		outSourceArt = nullptr;
		outPath.art = nullptr;
		outPath.points.clear();
		outPath.closed = false;
		outPath.layerName.clear();

		if (sourceId.empty()) {
			return false;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			std::string artSourceId;
			if (!DuctworkGeometry::EnsureEmorySourceId(art, artSourceId) || artSourceId.empty() || artSourceId != sourceId) {
				continue;
			}

			if (!BuildProcessPathForArt(art, outPath) || !DuctworkGeometry::IsCenterlineCandidate(outPath.art, outPath.points, outPath.closed, outPath.layerName)) {
				return false;
			}

			outSourceArt = art;
			return true;
		}

		return false;
	}

	int DetermineCascadeDirection(size_t segmentCount, int startSegmentIndex, int selectedSegmentIndex)
	{
		if (segmentCount == 0 || selectedSegmentIndex < 0 || selectedSegmentIndex >= static_cast<int>(segmentCount)) {
			return 0;
		}

		int clampedStart = startSegmentIndex;
		if (clampedStart < 0) {
			clampedStart = 0;
		}
		if (clampedStart >= static_cast<int>(segmentCount)) {
			clampedStart = static_cast<int>(segmentCount - 1);
		}

		if (selectedSegmentIndex == clampedStart) {
			return 0;
		}
		return selectedSegmentIndex < clampedStart ? -1 : 1;
	}

	void ApplyCascadeFromAnchor(const std::vector<double>& originalWidths, std::vector<double>& widths, int anchorIndex, int direction)
	{
		if (direction == 0 || anchorIndex < 0 || anchorIndex >= static_cast<int>(widths.size()) || originalWidths.size() != widths.size()) {
			return;
		}

		int upstreamIndex = anchorIndex;
		for (int currentIndex = anchorIndex + direction;
			currentIndex >= 0 && currentIndex < static_cast<int>(widths.size());
			currentIndex += direction) {
			double ratio = 1.0;
			if (upstreamIndex >= 0 && upstreamIndex < static_cast<int>(originalWidths.size()) &&
				currentIndex >= 0 && currentIndex < static_cast<int>(originalWidths.size()) &&
				originalWidths[upstreamIndex] > kPointEpsilon) {
				ratio = originalWidths[currentIndex] / originalWidths[upstreamIndex];
			}

			double nextWidth = widths[upstreamIndex] * ratio;
			if (!std::isfinite(nextWidth) || nextWidth < kMinDuctWidth) {
				nextWidth = kMinDuctWidth;
			}
			widths[currentIndex] = nextWidth;
			upstreamIndex = currentIndex;
		}
	}

	bool ReadDuctRole(AIArtHandle art, std::string& outRole)
	{
		outRole.clear();
		return art && DuctworkMetadata::GetString(art, "ductRole", outRole) && !outRole.empty();
	}

	bool IsBranchRole(const std::string& ductRole)
	{
		return ductRole == "branch";
	}

	bool CollectEmorySourceStates(std::vector<EmorySourceState>& outStates, std::map<std::string, int>& outIndexBySourceId)
	{
		outStates.clear();
		outIndexBySourceId.clear();

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			DuctworkPath path;
			if (!BuildProcessPathForArt(art, path) || !DuctworkGeometry::IsCenterlineCandidate(path.art, path.points, path.closed, path.layerName)) {
				continue;
			}

			std::vector<DuctworkPoint> points;
			SanitizePolyline(path.points, points);
			const int segmentCount = points.size() > 1 ? static_cast<int>(points.size() - 1) : 0;
			if (segmentCount <= 0) {
				continue;
			}

			std::string sourceId;
			if (!DuctworkGeometry::EnsureEmorySourceId(art, sourceId) || sourceId.empty()) {
				continue;
			}

			EmorySourceState state;
			state.art = art;
			state.sourceId = sourceId;
			state.path = path;
			state.path.points = points;
			state.segmentCount = segmentCount;
			state.startSegmentIndex = ReadStartSegmentIndex(art, static_cast<size_t>(segmentCount));
			ReadDuctRole(art, state.ductRole);

			if (!ResolveSourceBodyWidth(art, sourceId, state.defaultWidth) || state.defaultWidth <= 0.0) {
				state.defaultWidth = kDefaultDuctWidth;
			}
			if (state.defaultWidth < kMinDuctWidth) {
				state.defaultWidth = kMinDuctWidth;
			}
			double strokeWidth = 0.0;
			ResolveSourceStrokeWidth(art, sourceId, state.defaultWidth, strokeWidth);

			ReadSegmentWidths(art, static_cast<size_t>(segmentCount), state.defaultWidth, state.widths);
			state.originalWidths = state.widths;

			outIndexBySourceId[sourceId] = static_cast<int>(outStates.size());
			outStates.push_back(state);
		}

		return !outStates.empty();
	}

	bool GetEndpointSlotForPath(const DuctworkPath& path, int endpointIndex, int& outEndpointSlot)
	{
		outEndpointSlot = -1;
		if (path.points.size() < 2) {
			return false;
		}
		if (endpointIndex == 0) {
			outEndpointSlot = 0;
			return true;
		}
		if (endpointIndex == static_cast<int>(path.points.size() - 1)) {
			outEndpointSlot = 1;
			return true;
		}
		return false;
	}

	bool DescribeConnectionForPath(const DuctworkConnection& connection,
		int pathIndex,
		const EmorySourceState& state,
		PathConnectionAttachment& outAttachment)
	{
		outAttachment = PathConnectionAttachment();
		if (state.segmentCount <= 0) {
			return false;
		}

		const bool isA = (pathIndex == connection.a);
		const bool isB = (pathIndex == connection.b);
		if (!isA && !isB) {
			return false;
		}

		if (connection.type == kConnectionEndpointToEndpoint) {
			const int endpointIndex = isA ? connection.endpointA : connection.endpointB;
			if (!GetEndpointSlotForPath(state.path, endpointIndex, outAttachment.endpointSlot)) {
				return false;
			}
			outAttachment.segmentIndex = (outAttachment.endpointSlot == 0) ? 0 : (state.segmentCount - 1);
			return outAttachment.segmentIndex >= 0;
		}

		if (connection.type == kConnectionEndpointToSegment) {
			const int endpointIndex = isA ? connection.endpointA : connection.endpointB;
			const int segmentIndex = isA ? connection.segA : connection.segB;
			if (endpointIndex >= 0) {
				if (!GetEndpointSlotForPath(state.path, endpointIndex, outAttachment.endpointSlot)) {
					return false;
				}
				outAttachment.segmentIndex = (outAttachment.endpointSlot == 0) ? 0 : (state.segmentCount - 1);
				return outAttachment.segmentIndex >= 0;
			}
			if (segmentIndex < 0 || segmentIndex >= state.segmentCount) {
				return false;
			}
			outAttachment.segmentIndex = segmentIndex;
			return true;
		}

		const int segmentIndex = isA ? connection.segA : connection.segB;
		if (segmentIndex < 0 || segmentIndex >= state.segmentCount) {
			return false;
		}
		outAttachment.segmentIndex = segmentIndex;
		return true;
	}

	void AppendCrossLayerConnections(const std::vector<DuctworkPath>& paths,
		double maxDist,
		double tJunctionDist,
		std::vector<DuctworkConnection>& ioConnections)
	{
		const double maxDist2 = maxDist * maxDist;
		const double tJunctionDist2 = tJunctionDist * tJunctionDist;

		for (size_t i = 0; i < paths.size(); ++i) {
			const DuctworkPath& a = paths[i];
			if (a.points.size() < 2) {
				continue;
			}
			const DuctworkPoint aStart = a.points.front();
			const DuctworkPoint aEnd = a.points.back();

			for (size_t j = i + 1; j < paths.size(); ++j) {
				const DuctworkPath& b = paths[j];
				if (b.points.size() < 2 || a.layerName == b.layerName) {
					continue;
				}

				const DuctworkPoint bStart = b.points.front();
				const DuctworkPoint bEnd = b.points.back();

				if (DuctworkMath::Dist2(aStart, bStart) <= maxDist2 ||
					DuctworkMath::Dist2(aStart, bEnd) <= maxDist2 ||
					DuctworkMath::Dist2(aEnd, bStart) <= maxDist2 ||
					DuctworkMath::Dist2(aEnd, bEnd) <= maxDist2) {
					DuctworkConnection connection;
					connection.a = static_cast<int>(i);
					connection.b = static_cast<int>(j);
					connection.type = kConnectionEndpointToEndpoint;
					connection.segA = -1;
					connection.segB = -1;
					connection.endpointA = -1;
					connection.endpointB = -1;
					if (DuctworkMath::Dist2(aStart, bStart) <= maxDist2) {
						connection.point = aStart;
						connection.endpointA = 0;
						connection.endpointB = 0;
					} else if (DuctworkMath::Dist2(aStart, bEnd) <= maxDist2) {
						connection.point = aStart;
						connection.endpointA = 0;
						connection.endpointB = static_cast<int>(b.points.size() - 1);
					} else if (DuctworkMath::Dist2(aEnd, bStart) <= maxDist2) {
						connection.point = aEnd;
						connection.endpointA = static_cast<int>(a.points.size() - 1);
						connection.endpointB = 0;
					} else {
						connection.point = aEnd;
						connection.endpointA = static_cast<int>(a.points.size() - 1);
						connection.endpointB = static_cast<int>(b.points.size() - 1);
					}
					ioConnections.push_back(connection);
					continue;
				}

				for (size_t aSeg = 0; aSeg + 1 < a.points.size(); ++aSeg) {
					const DuctworkPoint& a1 = a.points[aSeg];
					const DuctworkPoint& a2 = a.points[aSeg + 1];

					double t = 0.0;
					DuctworkPoint nearAFromBStart = DuctworkMath::ClosestPointOnSegment(a1, a2, bStart, t);
					if (t > 0.0 && t < 1.0 && DuctworkMath::Dist2(nearAFromBStart, bStart) <= tJunctionDist2) {
						DuctworkConnection connection;
						connection.a = static_cast<int>(i);
						connection.b = static_cast<int>(j);
						connection.type = kConnectionEndpointToSegment;
						connection.point = nearAFromBStart;
						connection.segA = static_cast<int>(aSeg);
						connection.segB = -1;
						connection.endpointA = -1;
						connection.endpointB = 0;
						ioConnections.push_back(connection);
						break;
					}

					DuctworkPoint nearAFromBEnd = DuctworkMath::ClosestPointOnSegment(a1, a2, bEnd, t);
					if (t > 0.0 && t < 1.0 && DuctworkMath::Dist2(nearAFromBEnd, bEnd) <= tJunctionDist2) {
						DuctworkConnection connection;
						connection.a = static_cast<int>(i);
						connection.b = static_cast<int>(j);
						connection.type = kConnectionEndpointToSegment;
						connection.point = nearAFromBEnd;
						connection.segA = static_cast<int>(aSeg);
						connection.segB = -1;
						connection.endpointA = -1;
						connection.endpointB = static_cast<int>(b.points.size() - 1);
						ioConnections.push_back(connection);
						break;
					}
				}

				for (size_t bSeg = 0; bSeg + 1 < b.points.size(); ++bSeg) {
					const DuctworkPoint& b1 = b.points[bSeg];
					const DuctworkPoint& b2 = b.points[bSeg + 1];

					double t = 0.0;
					DuctworkPoint nearBFromAStart = DuctworkMath::ClosestPointOnSegment(b1, b2, aStart, t);
					if (t > 0.0 && t < 1.0 && DuctworkMath::Dist2(nearBFromAStart, aStart) <= tJunctionDist2) {
						DuctworkConnection connection;
						connection.a = static_cast<int>(i);
						connection.b = static_cast<int>(j);
						connection.type = kConnectionEndpointToSegment;
						connection.point = nearBFromAStart;
						connection.segA = -1;
						connection.segB = static_cast<int>(bSeg);
						connection.endpointA = 0;
						connection.endpointB = -1;
						ioConnections.push_back(connection);
						break;
					}

					DuctworkPoint nearBFromAEnd = DuctworkMath::ClosestPointOnSegment(b1, b2, aEnd, t);
					if (t > 0.0 && t < 1.0 && DuctworkMath::Dist2(nearBFromAEnd, aEnd) <= tJunctionDist2) {
						DuctworkConnection connection;
						connection.a = static_cast<int>(i);
						connection.b = static_cast<int>(j);
						connection.type = kConnectionEndpointToSegment;
						connection.point = nearBFromAEnd;
						connection.segA = -1;
						connection.segB = static_cast<int>(bSeg);
						connection.endpointA = static_cast<int>(a.points.size() - 1);
						connection.endpointB = -1;
						ioConnections.push_back(connection);
						break;
					}
				}
			}
		}
	}

	void CollectEmoryNetworkConnections(const std::vector<EmorySourceState>& states, std::vector<DuctworkConnection>& outConnections)
	{
		outConnections.clear();
		if (states.size() < 2) {
			return;
		}

		std::vector<DuctworkPath> paths;
		paths.reserve(states.size());
		for (size_t i = 0; i < states.size(); ++i) {
			paths.push_back(states[i].path);
		}

		DuctworkConnections::FindConnections(paths, 2.0, 3.0, 15.0, 10.0, true, outConnections);
	}

	bool IsEndpointToSegmentBranchConnection(const DuctworkConnection& connection, int& outTrunkIndex, int& outBranchIndex)
	{
		outTrunkIndex = -1;
		outBranchIndex = -1;
		if (connection.type != kConnectionEndpointToSegment) {
			return false;
		}

		if (connection.endpointA >= 0 && connection.segB >= 0) {
			outBranchIndex = connection.a;
			outTrunkIndex = connection.b;
			return true;
		}
		if (connection.endpointB >= 0 && connection.segA >= 0) {
			outBranchIndex = connection.b;
			outTrunkIndex = connection.a;
			return true;
		}
		return false;
	}

	bool PathIsBranchChild(int pathIndex, const std::vector<DuctworkConnection>& connections)
	{
		for (size_t i = 0; i < connections.size(); ++i) {
			int trunkIndex = -1;
			int branchIndex = -1;
			if (IsEndpointToSegmentBranchConnection(connections[i], trunkIndex, branchIndex) &&
				branchIndex == pathIndex) {
				return true;
			}
		}
		return false;
	}

	void ApplySelectedWidthToPathState(EmorySourceState& state, const std::vector<int>& selectedSegmentIndices, double newWidth)
	{
		if (selectedSegmentIndices.empty() || state.widths.size() != state.originalWidths.size() || state.segmentCount <= 0) {
			return;
		}

		const bool startIsFirstSegment = (state.startSegmentIndex <= 0);
		const bool startIsLastSegment = (state.startSegmentIndex >= (state.segmentCount - 1));
		const bool startIsEndpoint = startIsFirstSegment || startIsLastSegment;

		for (size_t i = 0; i < selectedSegmentIndices.size(); ++i) {
			const int segmentIndex = selectedSegmentIndices[i];
			if (segmentIndex >= 0 && segmentIndex < state.segmentCount) {
				state.widths[segmentIndex] = newWidth;
			}
		}

		auto applySelectionDirection = [&](int direction) {
			bool hasNonStartAnchorOnDirection = false;
			for (size_t i = 0; i < selectedSegmentIndices.size(); ++i) {
				const int segmentIndex = selectedSegmentIndices[i];
				if (segmentIndex == state.startSegmentIndex) {
					continue;
				}
				if (direction < 0 && segmentIndex < state.startSegmentIndex) {
					hasNonStartAnchorOnDirection = true;
					break;
				}
				if (direction > 0 && segmentIndex > state.startSegmentIndex) {
					hasNonStartAnchorOnDirection = true;
					break;
				}
			}

			std::vector<int> anchors;
			for (size_t i = 0; i < selectedSegmentIndices.size(); ++i) {
				const int segmentIndex = selectedSegmentIndices[i];
				if (segmentIndex == state.startSegmentIndex) {
					if (hasNonStartAnchorOnDirection) {
						continue;
					}
					if (!startIsEndpoint) {
						anchors.push_back(segmentIndex);
					} else if (startIsFirstSegment && direction > 0) {
						anchors.push_back(segmentIndex);
					} else if (startIsLastSegment && direction < 0) {
						anchors.push_back(segmentIndex);
					}
				} else if (direction < 0 && segmentIndex < state.startSegmentIndex) {
					anchors.push_back(segmentIndex);
				} else if (direction > 0 && segmentIndex > state.startSegmentIndex) {
					anchors.push_back(segmentIndex);
				}
			}

			if (anchors.empty()) {
				return;
			}

			if (direction < 0) {
				std::sort(anchors.begin(), anchors.end(), [](int a, int b) { return a > b; });
			} else {
				std::sort(anchors.begin(), anchors.end());
			}
			anchors.erase(std::unique(anchors.begin(), anchors.end()), anchors.end());

			for (size_t i = 0; i < anchors.size(); ++i) {
				const int anchorIndex = anchors[i];
				const int stopExclusive = (i + 1 < anchors.size()) ? anchors[i + 1] : (direction < 0 ? -1 : state.segmentCount);
				int upstreamIndex = anchorIndex;
				for (int currentIndex = anchorIndex + direction;
					currentIndex >= 0 && currentIndex < state.segmentCount && currentIndex != stopExclusive;
					currentIndex += direction) {
					double ratio = 1.0;
					if (state.originalWidths[upstreamIndex] > kPointEpsilon) {
						ratio = state.originalWidths[currentIndex] / state.originalWidths[upstreamIndex];
					}

					double nextWidth = state.widths[upstreamIndex] * ratio;
					if (!std::isfinite(nextWidth) || nextWidth < kMinDuctWidth) {
						nextWidth = kMinDuctWidth;
					}
					state.widths[currentIndex] = nextWidth;
					upstreamIndex = currentIndex;
				}
			}
		};

		if (startIsEndpoint) {
			applySelectionDirection(startIsFirstSegment ? 1 : -1);
		} else {
			applySelectionDirection(-1);
			applySelectionDirection(1);
		}
		state.touched = true;
		state.selectedSeed = true;
	}

	bool PropagateWidthToBranchState(const EmorySourceState& upstreamState,
		const PathConnectionAttachment& upstreamAttachment,
		EmorySourceState& branchState,
		const PathConnectionAttachment& branchAttachment)
	{
		if (upstreamAttachment.segmentIndex < 0 ||
			upstreamAttachment.segmentIndex >= static_cast<int>(upstreamState.widths.size()) ||
			branchAttachment.endpointSlot < 0 ||
			branchState.segmentCount <= 0 ||
			branchState.widths.size() != branchState.originalWidths.size()) {
			return false;
		}

		const int branchRootSegmentIndex = (branchAttachment.endpointSlot == 0) ? 0 : (branchState.segmentCount - 1);
		if (branchRootSegmentIndex < 0 || branchRootSegmentIndex >= static_cast<int>(branchState.widths.size())) {
			return false;
		}

		double upstreamOriginalWidth = upstreamState.defaultWidth;
		if (upstreamAttachment.segmentIndex < static_cast<int>(upstreamState.originalWidths.size()) &&
			upstreamState.originalWidths[upstreamAttachment.segmentIndex] > kPointEpsilon) {
			upstreamOriginalWidth = upstreamState.originalWidths[upstreamAttachment.segmentIndex];
		}

		double branchOriginalWidth = branchState.defaultWidth;
		if (branchRootSegmentIndex < static_cast<int>(branchState.originalWidths.size()) &&
			branchState.originalWidths[branchRootSegmentIndex] > kPointEpsilon) {
			branchOriginalWidth = branchState.originalWidths[branchRootSegmentIndex];
		}

		double ratio = 1.0;
		if (upstreamOriginalWidth > kPointEpsilon) {
			ratio = branchOriginalWidth / upstreamOriginalWidth;
		}

		double branchRootWidth = upstreamState.widths[upstreamAttachment.segmentIndex] * ratio;
		if (!std::isfinite(branchRootWidth) || branchRootWidth < kMinDuctWidth) {
			branchRootWidth = kMinDuctWidth;
		}

		const bool changed = !NearlyEqual(branchState.widths[branchRootSegmentIndex], branchRootWidth, 0.001);
		branchState.widths[branchRootSegmentIndex] = branchRootWidth;

		const int direction = (branchRootSegmentIndex == 0) ? 1 : -1;
		ApplyCascadeFromAnchor(branchState.originalWidths, branchState.widths, branchRootSegmentIndex, direction);
		branchState.touched = true;
		return changed;
	}

	void CascadeConnectedBranchWidths(std::vector<EmorySourceState>& states, const std::vector<DuctworkConnection>& connections)
	{
		std::vector<int> queue;

		for (size_t i = 0; i < states.size(); ++i) {
			if (states[i].touched) {
				queue.push_back(static_cast<int>(i));
			}
		}

		for (size_t queueIndex = 0; queueIndex < queue.size(); ++queueIndex) {
			const int currentIndex = queue[queueIndex];
			for (size_t connectionIndex = 0; connectionIndex < connections.size(); ++connectionIndex) {
				const DuctworkConnection& connection = connections[connectionIndex];
				if (connection.type == kConnectionSegmentIntersection) {
					continue;
				}

				if (connection.type == kConnectionEndpointToEndpoint) {
					int neighborIndex = -1;
					if (connection.a == currentIndex) {
						neighborIndex = connection.b;
					} else if (connection.b == currentIndex) {
						neighborIndex = connection.a;
					}
					if (neighborIndex < 0 || neighborIndex >= static_cast<int>(states.size()) || neighborIndex == currentIndex ||
						states[neighborIndex].selectedSeed) {
						continue;
					}

					PathConnectionAttachment currentAttachment;
					PathConnectionAttachment neighborAttachment;
					if (!DescribeConnectionForPath(connection, currentIndex, states[currentIndex], currentAttachment) ||
						!DescribeConnectionForPath(connection, neighborIndex, states[neighborIndex], neighborAttachment) ||
						currentAttachment.endpointSlot < 0 ||
						neighborAttachment.endpointSlot < 0) {
						continue;
					}

					if (PropagateWidthToBranchState(states[currentIndex], currentAttachment, states[neighborIndex], neighborAttachment)) {
						queue.push_back(neighborIndex);
					}
					continue;
				}

				int trunkIndex = -1;
				int branchIndex = -1;
				if (!IsEndpointToSegmentBranchConnection(connection, trunkIndex, branchIndex) ||
					currentIndex != trunkIndex ||
					branchIndex < 0 || branchIndex >= static_cast<int>(states.size()) ||
					states[branchIndex].selectedSeed) {
					continue;
				}

				PathConnectionAttachment trunkAttachment;
				PathConnectionAttachment branchAttachment;
				if (!DescribeConnectionForPath(connection, trunkIndex, states[trunkIndex], trunkAttachment) ||
					!DescribeConnectionForPath(connection, branchIndex, states[branchIndex], branchAttachment) ||
					branchAttachment.endpointSlot < 0) {
					continue;
				}

				if (PropagateWidthToBranchState(states[trunkIndex], trunkAttachment, states[branchIndex], branchAttachment)) {
					queue.push_back(branchIndex);
				}
			}
		}
	}

	void ResolveBranchOrderAnchors(const std::vector<EmorySourceState>& states,
		const std::vector<DuctworkConnection>& connections,
		std::map<std::string, AIArtHandle>& outAnchorBySourceId)
	{
		outAnchorBySourceId.clear();
		std::map<int, AIArtHandle> anchorByIndex;

		bool changed = true;
		while (changed) {
			changed = false;
			for (size_t i = 0; i < states.size(); ++i) {
				if (!PathIsBranchChild(static_cast<int>(i), connections) ||
					anchorByIndex.find(static_cast<int>(i)) != anchorByIndex.end()) {
					continue;
				}

				for (size_t connectionIndex = 0; connectionIndex < connections.size(); ++connectionIndex) {
					const DuctworkConnection& connection = connections[connectionIndex];
					int trunkIndex = -1;
					int branchIndex = -1;
					if (!IsEndpointToSegmentBranchConnection(connection, trunkIndex, branchIndex) ||
						branchIndex != static_cast<int>(i) ||
						trunkIndex < 0 || trunkIndex >= static_cast<int>(states.size())) {
						continue;
					}

					if (!PathIsBranchChild(trunkIndex, connections)) {
						anchorByIndex[static_cast<int>(i)] = states[trunkIndex].art;
						changed = true;
						break;
					}

					std::map<int, AIArtHandle>::const_iterator neighborAnchor = anchorByIndex.find(trunkIndex);
					if (neighborAnchor != anchorByIndex.end() && neighborAnchor->second) {
						anchorByIndex[static_cast<int>(i)] = neighborAnchor->second;
						changed = true;
						break;
					}
				}
			}
		}

		for (std::map<int, AIArtHandle>::const_iterator it = anchorByIndex.begin(); it != anchorByIndex.end(); ++it) {
			if (it->first < 0 || it->first >= static_cast<int>(states.size()) || !it->second) {
				continue;
			}
			outAnchorBySourceId[states[it->first].sourceId] = it->second;
		}
	}

	void ReorderGeneratedBranchArtBehindParents(const std::vector<std::string>& affectedSourceIds)
	{
		if (!sAIArt || affectedSourceIds.empty()) {
			return;
		}

		std::vector<EmorySourceState> states;
		std::map<std::string, int> stateIndexBySourceId;
		if (!CollectEmorySourceStates(states, stateIndexBySourceId)) {
			return;
		}

		std::vector<DuctworkConnection> connections;
		CollectEmoryNetworkConnections(states, connections);

		std::map<std::string, AIArtHandle> anchorBySourceId;
		ResolveBranchOrderAnchors(states, connections, anchorBySourceId);
		if (anchorBySourceId.empty()) {
			return;
		}

		std::set<std::string> affected(affectedSourceIds.begin(), affectedSourceIds.end());
		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);

		std::map<std::string, std::vector<AIArtHandle> > artBySourceId;
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || !IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			std::string sourceId;
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
				sourceId = ReadEmorySourceIdFromNote(art);
			}
			if (sourceId.empty() || affected.find(sourceId) == affected.end() || anchorBySourceId.find(sourceId) == anchorBySourceId.end()) {
				continue;
			}

			artBySourceId[sourceId].push_back(art);
		}

		for (std::map<std::string, std::vector<AIArtHandle> >::iterator it = artBySourceId.begin(); it != artBySourceId.end(); ++it) {
			AIArtHandle anchorArt = anchorBySourceId[it->first];
			if (!anchorArt) {
				continue;
			}
			AIArtHandle lowestPlacedArt = nullptr;
			for (size_t artIndex = it->second.size(); artIndex > 0; --artIndex) {
				AIArtHandle art = it->second[artIndex - 1];
				if (art) {
					sAIArt->ReorderArt(art, kPlaceBelow, anchorArt);
					if (!lowestPlacedArt) {
						lowestPlacedArt = art;
					}
				}
			}

			std::map<std::string, int>::const_iterator stateIt = stateIndexBySourceId.find(it->first);
			if (stateIt != stateIndexBySourceId.end()) {
				AIArtHandle sourceArt = states[stateIt->second].art;
				if (sourceArt) {
					AIArtHandle centerlineAnchor = lowestPlacedArt ? lowestPlacedArt : anchorArt;
					if (centerlineAnchor != sourceArt) {
						sAIArt->ReorderArt(sourceArt, kPlaceBelow, centerlineAnchor);
					}
				}
			}
		}
	}

	bool SelectGeneratedSegmentsBySourceIdAndIndices(const std::string& sourceId, const std::vector<int>& segmentIndices)
	{
		if (sourceId.empty()) {
			return false;
		}

		std::set<int> targetIndices;
		for (size_t i = 0; i < segmentIndices.size(); ++i) {
			if (segmentIndices[i] >= 0) {
				targetIndices.insert(segmentIndices[i]);
			}
		}
		if (targetIndices.empty()) {
			return false;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);

		std::vector<AIArtHandle> reselection;
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art) {
				continue;
			}

			std::string role;
			if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) || role != kEmoryRoleSegment) {
				continue;
			}

			std::string artSourceId;
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, artSourceId) || artSourceId != sourceId) {
				continue;
			}

			int artSegmentIndex = -1;
			if (!ReadGeneratedSegmentIndex(art, artSegmentIndex) || targetIndices.find(artSegmentIndex) == targetIndices.end()) {
				continue;
			}

			reselection.push_back(art);
		}

		if (reselection.empty()) {
			return false;
		}

		ClearSelectionInternal();
		SelectArtListInternal(reselection);
		return true;
	}

	bool SelectGeneratedSegmentsBySourceMap(const std::map<std::string, std::vector<int> >& selectedBySource)
	{
		if (selectedBySource.empty()) {
			return false;
		}

		std::map<std::string, std::set<int> > targets;
		for (std::map<std::string, std::vector<int> >::const_iterator it = selectedBySource.begin(); it != selectedBySource.end(); ++it) {
			if (it->first.empty()) {
				continue;
			}
			for (size_t i = 0; i < it->second.size(); ++i) {
				if (it->second[i] >= 0) {
					targets[it->first].insert(it->second[i]);
				}
			}
		}
		if (targets.empty()) {
			return false;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);

		std::vector<AIArtHandle> reselection;
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art) {
				continue;
			}

			std::string role;
			if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) || role != kEmoryRoleSegment) {
				continue;
			}

			std::string artSourceId;
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, artSourceId) || artSourceId.empty()) {
				continue;
			}

			std::map<std::string, std::set<int> >::const_iterator targetIt = targets.find(artSourceId);
			if (targetIt == targets.end()) {
				continue;
			}

			int artSegmentIndex = -1;
			if (!ReadGeneratedSegmentIndex(art, artSegmentIndex) || targetIt->second.find(artSegmentIndex) == targetIt->second.end()) {
				continue;
			}

			reselection.push_back(art);
		}

		if (reselection.empty()) {
			return false;
		}

		ClearSelectionInternal();
		SelectArtListInternal(reselection);
		return true;
	}

	bool SelectGeneratedSegmentBySourceIdAndIndex(const std::string& sourceId, int segmentIndex)
	{
		std::vector<int> segmentIndices;
		segmentIndices.push_back(segmentIndex);
		return SelectGeneratedSegmentsBySourceIdAndIndices(sourceId, segmentIndices);
	}

	bool BuildCornerPairing(const ConnectorSpec& connector, CornerPairing& outPairing)
	{
		outPairing.valid = false;

		Vec2 incomingDir = Scale(connector.prevDir, -1.0);
		Vec2 outgoingDir = connector.nextDir;
		if (!Normalize(incomingDir, incomingDir) || !Normalize(outgoingDir, outgoingDir)) {
			return false;
		}

		Vec2 incomingLeft = PerpCCW(incomingDir);
		Vec2 outgoingLeft = PerpCCW(outgoingDir);
		const double halfPrev = connector.prevWidth * 0.5;
		const double halfNext = connector.nextWidth * 0.5;

		const DuctworkPoint incomingLeftPoint = Add(connector.prevTrimPoint, incomingLeft, halfPrev);
		const DuctworkPoint incomingRightPoint = Add(connector.prevTrimPoint, incomingLeft, -halfPrev);
		const DuctworkPoint outgoingLeftPoint = Add(connector.nextTrimPoint, outgoingLeft, halfNext);
		const DuctworkPoint outgoingRightPoint = Add(connector.nextTrimPoint, outgoingLeft, -halfNext);

		DuctworkPoint leftCorner;
		DuctworkPoint rightCorner;
		if (!LineIntersection(incomingLeftPoint, incomingDir, outgoingLeftPoint, outgoingDir, leftCorner) ||
			!LineIntersection(incomingRightPoint, incomingDir, outgoingRightPoint, outgoingDir, rightCorner)) {
			return false;
		}

		const bool leftTurn = Cross(incomingDir, outgoingDir) > 0.0;
		if (leftTurn) {
			outPairing.startInner = incomingLeftPoint;
			outPairing.endInner = outgoingLeftPoint;
			outPairing.cornerInner = leftCorner;
			outPairing.startOuter = incomingRightPoint;
			outPairing.endOuter = outgoingRightPoint;
			outPairing.cornerOuter = rightCorner;
		} else {
			outPairing.startInner = incomingRightPoint;
			outPairing.endInner = outgoingRightPoint;
			outPairing.cornerInner = rightCorner;
			outPairing.startOuter = incomingLeftPoint;
			outPairing.endOuter = outgoingLeftPoint;
			outPairing.cornerOuter = leftCorner;
		}
		outPairing.valid = true;
		return true;
	}

	bool BuildStraightCornerPolygon(const CornerPairing& pairing, std::vector<DuctworkPoint>& outPolygon)
	{
		outPolygon.clear();
		AppendIfDistinct(outPolygon, pairing.startOuter);
		AppendIfDistinct(outPolygon, pairing.startInner);
		AppendIfDistinct(outPolygon, pairing.cornerInner);
		AppendIfDistinct(outPolygon, pairing.endInner);
		AppendIfDistinct(outPolygon, pairing.endOuter);
		AppendIfDistinct(outPolygon, pairing.cornerOuter);
		return outPolygon.size() >= 4;
	}

bool BuildRoundCornerPolygon(const ConnectorSpec& connector, const CornerPairing& pairing, std::vector<DuctworkPoint>& outPolygon)
{
	outPolygon.clear();

	Vec2 axisA = connector.prevDir;
	Vec2 axisB = connector.nextDir;
	if (!Normalize(axisA, axisA) || !Normalize(axisB, axisB)) {
		return false;
	}

	struct ArcSide
	{
		Vec2 axis;
		DuctworkPoint inner;
		DuctworkPoint outer;
		double width = 0.0;
	};

	const bool aMoreHorizontal = std::fabs(axisA.x) >= std::fabs(axisA.y);
	ArcSide horiz;
	ArcSide vert;
	if (aMoreHorizontal) {
		horiz.axis = axisA;
		horiz.inner = pairing.startInner;
		horiz.outer = pairing.startOuter;
		horiz.width = connector.prevWidth;

		vert.axis = axisB;
		vert.inner = pairing.endInner;
		vert.outer = pairing.endOuter;
		vert.width = connector.nextWidth;
	} else {
		horiz.axis = axisB;
		horiz.inner = pairing.endInner;
		horiz.outer = pairing.endOuter;
		horiz.width = connector.nextWidth;

		vert.axis = axisA;
		vert.inner = pairing.startInner;
		vert.outer = pairing.startOuter;
		vert.width = connector.prevWidth;
	}

	double trim_len = std::fabs(connector.trimDistance);
	if (trim_len <= 0.0) {
		trim_len = (std::max)(horiz.width, vert.width);
	}
	if (trim_len <= 0.0) {
		return false;
	}

	Vec2 axisH = horiz.axis;
	Vec2 axisV = vert.axis;
	if (!Normalize(axisH, axisH) || !Normalize(axisV, axisV)) {
		return false;
	}

	const double planeCross = Cross(axisH, axisV);
	if (std::fabs(planeCross) <= 1e-6) {
		return false;
	}

	const double theta = std::acos((std::max)(-1.0, (std::min)(1.0, Dot(axisV, axisH))));
	if (!std::isfinite(theta) || theta <= 1e-3 || theta >= (3.141592653589793 - 1e-3)) {
		return false;
	}

	const double half_theta = theta * 0.5;
	const double sin_half = std::sin(half_theta);
	if (std::fabs(sin_half) <= 1e-6) {
		return false;
	}

	const double radius = trim_len * std::tan(half_theta);
	Vec2 bisector_source;
	bisector_source.x = axisV.x + axisH.x;
	bisector_source.y = axisV.y + axisH.y;
	Vec2 bisector;
	if (!Normalize(bisector_source, bisector)) {
		return false;
	}

	const double center_distance = radius / sin_half;
	const DuctworkPoint center = Add(connector.joint, bisector, center_distance);
	const DuctworkPoint start_centerline = Add(connector.joint, axisV, trim_len);
	const DuctworkPoint end_centerline = Add(connector.joint, axisH, trim_len);

	Vec2 start_vec = Subtract(start_centerline, center);
	Vec2 end_vec = Subtract(end_centerline, center);
	const double radius_len = Length(start_vec);
	if (radius_len <= 1e-6) {
		return false;
	}

	Vec2 x_axis;
	if (!Normalize(start_vec, x_axis)) {
		return false;
	}

	Vec2 y_axis = planeCross >= 0.0 ? PerpCCW(x_axis) : PerpCW(x_axis);
	if (!Normalize(y_axis, y_axis)) {
		return false;
	}

	double end_x = Dot(end_vec, x_axis);
	double end_y = Dot(end_vec, y_axis);
	double angle_total = std::atan2(end_y, end_x);
	if (angle_total <= 0.0) {
		angle_total += (2.0 * 3.141592653589793);
	}
	if (angle_total > 3.141592653589793) {
		y_axis = Scale(y_axis, -1.0);
		end_y = Dot(end_vec, y_axis);
		angle_total = std::atan2(end_y, end_x);
		if (angle_total <= 0.0) {
			angle_total += (2.0 * 3.141592653589793);
		}
	}
	if (!std::isfinite(angle_total) || angle_total <= 1e-3 || angle_total > (3.141592653589793 + 1e-3)) {
		return false;
	}

	int subdivisions = static_cast<int>(std::ceil(std::fabs(angle_total) / (3.141592653589793 / 12.0)));
	if (subdivisions < 6) {
		subdivisions = 6;
	}
	if (subdivisions > 48) {
		subdivisions = 48;
	}

	std::vector<DuctworkPoint> outerPoints;
	std::vector<DuctworkPoint> innerPoints;
	outerPoints.reserve(static_cast<size_t>(subdivisions + 1));
	innerPoints.reserve(static_cast<size_t>(subdivisions + 1));

	bool orientationSet = false;
	for (int i = 0; i <= subdivisions; ++i) {
		const double t = static_cast<double>(i) / static_cast<double>(subdivisions);
		const double angle = angle_total * t;

		Vec2 vec1 = Scale(x_axis, radius_len * std::cos(angle));
		Vec2 vec2 = Scale(y_axis, radius_len * std::sin(angle));
		DuctworkPoint point = Add(center, vec1, 1.0);
		point = Add(point, vec2, 1.0);

		Vec2 radial = Subtract(point, center);
		Vec2 width_vec;
		if (!Normalize(radial, width_vec)) {
			continue;
		}

		const double width_current = vert.width + ((horiz.width - vert.width) * t);
		const double half_width = width_current * 0.5;
		if (!orientationSet) {
			const DuctworkPoint test_outer = Add(point, width_vec, half_width);
			const DuctworkPoint test_inner = Add(point, width_vec, -half_width);
			const double dist_outer = std::hypot(test_outer.x - vert.outer.x, test_outer.y - vert.outer.y);
			const double dist_inner = std::hypot(test_inner.x - vert.outer.x, test_inner.y - vert.outer.y);
			if (dist_inner < dist_outer) {
				width_vec = Scale(width_vec, -1.0);
			}
			orientationSet = true;
		}

		outerPoints.push_back(Add(point, width_vec, half_width));
		innerPoints.push_back(Add(point, width_vec, -half_width));
	}

	if (outerPoints.empty() || innerPoints.empty()) {
		return false;
	}

	outerPoints.front() = vert.outer;
	outerPoints.back() = horiz.outer;
	innerPoints.front() = vert.inner;
	innerPoints.back() = horiz.inner;

	for (size_t i = 0; i < outerPoints.size(); ++i) {
		AppendIfDistinct(outPolygon, outerPoints[i]);
	}
	for (size_t i = innerPoints.size(); i > 0; --i) {
		AppendIfDistinct(outPolygon, innerPoints[i - 1]);
	}
	return outPolygon.size() >= 4;
}

	bool BuildConnectorPolygon(const ConnectorSpec& connector, std::vector<DuctworkPoint>& outPolygon)
	{
		outPolygon.clear();
		CornerPairing pairing;
		if (!BuildCornerPairing(connector, pairing)) {
			return false;
		}

		if (connector.style == "straight") {
			return BuildStraightCornerPolygon(pairing, outPolygon);
		}

		if (BuildRoundCornerPolygon(connector, pairing, outPolygon)) {
			return true;
		}
		return BuildStraightCornerPolygon(pairing, outPolygon);
	}

	void ResolveSegmentTrimRequests(const std::vector<DuctworkPoint>& points,
		std::vector<double>& trimAtStart, std::vector<double>& trimAtEnd)
	{
		const size_t segmentCount = points.size() > 1 ? (points.size() - 1) : 0;
		if (trimAtStart.size() != segmentCount || trimAtEnd.size() != segmentCount) {
			return;
		}

		for (size_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
			const DuctworkPoint& start = points[segmentIndex];
			const DuctworkPoint& end = points[segmentIndex + 1];
			const double length = std::hypot(end.x - start.x, end.y - start.y);
			const double maxUsable = length - 0.1;
			if (maxUsable <= 0.1) {
				trimAtStart[segmentIndex] = 0.0;
				trimAtEnd[segmentIndex] = 0.0;
				continue;
			}

			double trimStart = (std::max)(0.0, trimAtStart[segmentIndex]);
			double trimEnd = (std::max)(0.0, trimAtEnd[segmentIndex]);
			if (trimStart > maxUsable) {
				trimStart = maxUsable;
			}
			if (trimEnd > maxUsable) {
				trimEnd = maxUsable;
			}

			const double totalTrim = trimStart + trimEnd;
			if (totalTrim > maxUsable && totalTrim > 1e-6) {
				const double scale = maxUsable / totalTrim;
				trimStart *= scale;
				trimEnd *= scale;
			}

			trimAtStart[segmentIndex] = trimStart;
			trimAtEnd[segmentIndex] = trimEnd;
		}
	}

	void ApplyResolvedConnectorTrims(std::vector<ConnectorSpec>& connectors,
		const std::vector<double>& trimAtStart, const std::vector<double>& trimAtEnd)
	{
		for (size_t i = 0; i < connectors.size(); ++i) {
			ConnectorSpec& connector = connectors[i];
			double prevTrim = connector.prevTrimDistance > 0.0 ? connector.prevTrimDistance : connector.trimDistance;
			double nextTrim = connector.nextTrimDistance > 0.0 ? connector.nextTrimDistance : connector.trimDistance;

			if (connector.jointIndex > 0) {
				const size_t prevSegmentIndex = static_cast<size_t>(connector.jointIndex - 1);
				if (prevSegmentIndex < trimAtEnd.size()) {
					prevTrim = trimAtEnd[prevSegmentIndex];
				}
			}
			if (connector.jointIndex >= 0) {
				const size_t nextSegmentIndex = static_cast<size_t>(connector.jointIndex);
				if (nextSegmentIndex < trimAtStart.size()) {
					nextTrim = trimAtStart[nextSegmentIndex];
				}
			}

			connector.prevTrimDistance = prevTrim;
			connector.nextTrimDistance = nextTrim;
			connector.trimDistance = (std::min)(prevTrim, nextTrim);
			connector.prevTrimPoint = Add(connector.joint, connector.prevDir, prevTrim);
			connector.nextTrimPoint = Add(connector.joint, connector.nextDir, nextTrim);
		}
	}

	void TagGeneratedArt(AIArtHandle art, const std::string& role, const std::string& sourceId,
		double bodyWidth, int segmentIndex, int jointIndex, const std::string& connectorStyle)
	{
		DuctworkMetadata::SetString(art, kEmoryRoleKey, role);
		DuctworkMetadata::SetString(art, kEmorySourceIdKey, sourceId);
		DuctworkMetadata::SetDouble(art, kEmoryBodyWidthKey, bodyWidth);
		if (segmentIndex >= 0) {
			DuctworkMetadata::SetDouble(art, kEmorySegmentIndexKey, static_cast<double>(segmentIndex));
		} else {
			DuctworkMetadata::RemoveKey(art, kEmorySegmentIndexKey);
		}
		if (jointIndex >= 0) {
			DuctworkMetadata::SetDouble(art, kEmoryJointIndexKey, static_cast<double>(jointIndex));
		} else {
			DuctworkMetadata::RemoveKey(art, kEmoryJointIndexKey);
		}
		if (!connectorStyle.empty()) {
			DuctworkMetadata::SetString(art, kEmoryConnectorStyleKey, connectorStyle);
		} else {
			DuctworkMetadata::RemoveKey(art, kEmoryConnectorStyleKey);
		}
		UpdateEmoryTokens(art, role, sourceId);
	}

	bool BuildProcessPathForArt(AIArtHandle art, DuctworkPath& outPath)
	{
		outPath.art = art;
		outPath.points.clear();
		outPath.closed = false;
		outPath.layerName.clear();
		if (!art) {
			return false;
		}
		if (!DuctworkGeometry::GetPathPoints(art, outPath.points, outPath.closed)) {
			return false;
		}
		outPath.layerName = DuctworkGeometry::GetArtLayerName(art);
		return true;
	}

	void ClearSelectionInternal()
	{
		if (!sAIArtSet || !sAIArt) {
			return;
		}

		AIArtSet selectedSet = nullptr;
		if (sAIArtSet->NewArtSet(&selectedSet) != kNoErr || !selectedSet) {
			return;
		}
		if (sAIArtSet->SelectedArtSet(selectedSet) == kNoErr) {
			size_t count = 0;
			if (sAIArtSet->CountArtSet(selectedSet, &count) == kNoErr) {
				for (size_t i = 0; i < count; ++i) {
					AIArtHandle art = nullptr;
					if (sAIArtSet->IndexArtSet(selectedSet, i, &art) == kNoErr && art) {
						sAIArt->SetArtUserAttr(art, kArtSelected | kArtFullySelected, 0);
					}
				}
			}
		}
		sAIArtSet->DisposeArtSet(&selectedSet);
	}

	void SelectArtListInternal(const std::vector<AIArtHandle>& artList)
	{
		if (!sAIArt) {
			return;
		}
		for (size_t i = 0; i < artList.size(); ++i) {
			if (artList[i]) {
				sAIArt->SetArtUserAttr(artList[i], kArtSelected | kArtFullySelected,
					kArtSelected | kArtFullySelected);
			}
		}
	}

	bool GenerateEmoryForPath(const DuctworkPath& path, EmoryBodyStats& stats)
	{
		if (!DuctworkLayers::IsColorLayerName(path.layerName)) {
			++stats.skipped;
			return false;
		}
		if (!DuctworkGeometry::IsCenterlineCandidate(path.art, path.points, path.closed, path.layerName)) {
			++stats.skipped;
			return false;
		}

		std::string sourceId;
		if (!DuctworkGeometry::EnsureEmorySourceId(path.art, sourceId) || sourceId.empty()) {
			++stats.failed;
			return false;
		}

		double bodyWidth = 0.0;
		if (!ResolveSourceBodyWidth(path.art, sourceId, bodyWidth) || bodyWidth <= 0.0) {
			if (!DuctworkGeometry::GetEffectiveStrokeWidth(path.art, bodyWidth) || bodyWidth <= 0.0) {
				bodyWidth = kDefaultDuctWidth;
			}
		}
		if (bodyWidth <= 0.0) {
			bodyWidth = kDefaultDuctWidth;
		}
		if (bodyWidth < kMinDuctWidth) {
			bodyWidth = kMinDuctWidth;
		}
		WriteStoredSourceBodyWidth(path.art, bodyWidth);
		double sourceStrokeWidth = 0.0;
		if (!ResolveSourceStrokeWidth(path.art, sourceId, bodyWidth, sourceStrokeWidth) || sourceStrokeWidth <= 0.0) {
			sourceStrokeWidth = ComputeBodyStrokeWidth(bodyWidth);
		}
		WriteStoredSourceStrokeWidth(path.art, sourceStrokeWidth);

		const EmoryColorSpec colors = GetEmoryColorSpec(path.layerName);
		if (ApplyGuideStyleInternal(path.art, colors)) {
			++stats.guidesStyled;
		}

		std::vector<DuctworkPoint> points;
		SanitizePolyline(path.points, points);
		if (points.size() < 2) {
			++stats.failed;
			return false;
		}

		const size_t segmentCount = points.size() - 1;
		std::vector<double> segmentWidths;
		ReadSegmentWidths(path.art, segmentCount, bodyWidth, segmentWidths);
		std::vector<double> trimAtStart(segmentCount, 0.0);
		std::vector<double> trimAtEnd(segmentCount, 0.0);
		std::vector<ConnectorSpec> connectors;

		for (size_t jointIndex = 1; jointIndex + 1 < points.size(); ++jointIndex) {
			Vec2 prevDir;
			Vec2 prevNormal;
			Vec2 nextDir;
			Vec2 nextNormal;
			if (!BuildUnitDirection(points[jointIndex], points[jointIndex - 1], prevDir, prevNormal) ||
				!BuildUnitDirection(points[jointIndex], points[jointIndex + 1], nextDir, nextNormal)) {
				continue;
			}

			const double dirDot = Dot(prevDir, nextDir);
			if (std::fabs(dirDot) >= kCollinearThreshold) {
				continue;
			}

			const std::string connectorStyle = ReadCornerStyle(path.art, static_cast<int>(jointIndex));
			const double turnAngle = std::acos((std::max)(-1.0, (std::min)(1.0, dirDot)));
			const double prevSegmentWidth = segmentWidths[jointIndex - 1];
			const double nextSegmentWidth = segmentWidths[jointIndex];
			const double jointBodyWidth = (std::max)(prevSegmentWidth, nextSegmentWidth);

			const double prevLength = std::hypot(points[jointIndex - 1].x - points[jointIndex].x,
				points[jointIndex - 1].y - points[jointIndex].y);
			const double nextLength = std::hypot(points[jointIndex + 1].x - points[jointIndex].x,
				points[jointIndex + 1].y - points[jointIndex].y);
			double trimDistance = jointBodyWidth * kTrimMultiplier;
			if (connectorStyle == "round" && turnAngle > 1e-3 && turnAngle < (3.141592653589793 - 1e-3)) {
				const double desiredRadius = jointBodyWidth * kRoundMinCenterlineRadiusMultiplier;
				const double tanHalf = std::tan(turnAngle * 0.5);
				if (desiredRadius > 0.0 && std::fabs(tanHalf) > 1e-6) {
					const double requiredTrim = desiredRadius / tanHalf;
					if (requiredTrim > trimDistance) {
						trimDistance = requiredTrim;
					}
				}
			}
			const double maxAllowed = ((std::min)(prevLength, nextLength) * 0.5) - 0.1;
			if (maxAllowed <= 0.1) {
				continue;
			}
			if (trimDistance > maxAllowed) {
				trimDistance = maxAllowed;
			}
			if (trimDistance <= 0.1) {
				continue;
			}

			trimAtEnd[jointIndex - 1] = (std::max)(trimAtEnd[jointIndex - 1], trimDistance);
			trimAtStart[jointIndex] = (std::max)(trimAtStart[jointIndex], trimDistance);

			ConnectorSpec connector;
			connector.sourceArt = path.art;
			connector.sourceId = sourceId;
			connector.layerName = path.layerName;
			connector.jointIndex = static_cast<int>(jointIndex);
			connector.joint = points[jointIndex];
			connector.prevTrimPoint = Add(points[jointIndex], prevDir, trimDistance);
			connector.nextTrimPoint = Add(points[jointIndex], nextDir, trimDistance);
			connector.prevDir = prevDir;
			connector.nextDir = nextDir;
			connector.prevWidth = prevSegmentWidth;
			connector.nextWidth = nextSegmentWidth;
			connector.prevTrimDistance = trimDistance;
			connector.nextTrimDistance = trimDistance;
			connector.trimDistance = trimDistance;
			connector.style = connectorStyle;
			connectors.push_back(connector);
		}

		ResolveSegmentTrimRequests(points, trimAtStart, trimAtEnd);
		ApplyResolvedConnectorTrims(connectors, trimAtStart, trimAtEnd);

		AIArtHandle referenceArt = path.art;
		for (size_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
			const DuctworkPoint start = points[segmentIndex];
			const DuctworkPoint end = points[segmentIndex + 1];
			Vec2 dir;
			Vec2 normal;
			if (!BuildUnitDirection(start, end, dir, normal)) {
				continue;
			}
			const double segmentBodyWidth = segmentWidths[segmentIndex];

			const double length = std::hypot(end.x - start.x, end.y - start.y);
			double trimStart = trimAtStart[segmentIndex];
			double trimEnd = trimAtEnd[segmentIndex];
			const double maxUsable = length - 0.1;
			if (maxUsable <= 0.1) {
				continue;
			}
			if (trimStart > maxUsable) {
				trimStart = maxUsable;
			}
			if (trimEnd > maxUsable) {
				trimEnd = maxUsable;
			}

			const DuctworkPoint trimmedStart = Add(start, dir, trimStart);
			const DuctworkPoint trimmedEnd = Add(end, dir, -trimEnd);
			if (std::hypot(trimmedEnd.x - trimmedStart.x, trimmedEnd.y - trimmedStart.y) <= 0.1) {
				continue;
			}

			std::vector<DuctworkPoint> polygon;
			if (!BuildBandPolygon(trimmedStart, trimmedEnd, segmentBodyWidth, polygon)) {
				continue;
			}

			AIArtHandle segmentArt = nullptr;
			if (!CreateClosedPath(referenceArt, polygon, segmentArt) || !segmentArt) {
				++stats.failed;
				continue;
			}
			if (!ApplyFilledPathStyle(segmentArt, colors, sourceStrokeWidth)) {
				sAIArt->DisposeArt(segmentArt);
				++stats.failed;
				continue;
			}

			TagGeneratedArt(segmentArt, kEmoryRoleSegment, sourceId, segmentBodyWidth,
				static_cast<int>(segmentIndex), -1, std::string());
			referenceArt = segmentArt;
			++stats.segmentsCreated;
			++stats.created;
		}

		for (size_t i = 0; i < connectors.size(); ++i) {
			std::vector<DuctworkPoint> polygon;
			if (!BuildConnectorPolygon(connectors[i], polygon)) {
				++stats.failed;
				continue;
			}

			AIArtHandle connectorArt = nullptr;
			if (!CreateClosedPath(referenceArt, polygon, connectorArt) || !connectorArt) {
				++stats.failed;
				continue;
			}
			const double connectorBodyWidth = (std::max)(connectors[i].prevWidth, connectors[i].nextWidth);
			if (!ApplyFilledPathStyle(connectorArt, colors, sourceStrokeWidth)) {
				sAIArt->DisposeArt(connectorArt);
				++stats.failed;
				continue;
			}

			TagGeneratedArt(connectorArt, kEmoryRoleConnector, sourceId, connectorBodyWidth, -1,
				connectors[i].jointIndex, connectors[i].style);
			referenceArt = connectorArt;
			++stats.connectorsCreated;
			++stats.created;
		}

		return true;
	}
}

bool DuctworkGeometry::GetPathPoints(AIArtHandle path, std::vector<DuctworkPoint>& outPoints, bool& outClosed)
{
	outPoints.clear();
	outClosed = false;
	if (!path || !sAIPath || !sAIArt) {
		return false;
	}

	short artType = kUnknownArt;
	if (sAIArt->GetArtType(path, &artType) != kNoErr || artType != kPathArt) {
		return false;
	}

	ai::int16 count = 0;
	if (sAIPath->GetPathSegmentCount(path, &count) != kNoErr || count <= 0) {
		return false;
	}

	std::vector<AIPathSegment> segments(static_cast<size_t>(count));
	if (sAIPath->GetPathSegments(path, 0, count, &segments[0]) != kNoErr) {
		return false;
	}

	outPoints.reserve(static_cast<size_t>(count));
	for (ai::int16 i = 0; i < count; ++i) {
		DuctworkPoint point;
		point.x = segments[i].p.h;
		point.y = segments[i].p.v;
		outPoints.push_back(point);
	}

	AIBoolean closed = false;
	if (sAIPath->GetPathClosed(path, &closed) == kNoErr) {
		outClosed = (closed != 0);
	}
	return true;
}

std::string DuctworkGeometry::GetArtLayerName(AIArtHandle art)
{
	if (!art || !sAIArt || !sAILayer) {
		return std::string();
	}

	AILayerHandle layer = nullptr;
	if (sAIArt->GetLayerOfArt(art, &layer) != kNoErr || !layer) {
		return std::string();
	}

	ai::UnicodeString title;
	if (sAILayer->GetLayerTitle(layer, title) != kNoErr) {
		return std::string();
	}
	return title.as_UTF8();
}

bool DuctworkGeometry::IsGeneratedEmoryBody(AIArtHandle art)
{
	return IsGeneratedEmoryArtInternal(art);
}

bool DuctworkGeometry::IsCenterlineCandidate(AIArtHandle art,
	const std::vector<DuctworkPoint>& points,
	bool closed,
	const std::string& layerName)
{
	if (!art || closed || points.size() < 2) {
		return false;
	}
	if (!DuctworkLayers::IsLineLayerName(layerName)) {
		return false;
	}
	if (IsGeneratedEmoryArtInternal(art)) {
		return false;
	}
	return true;
}

bool DuctworkGeometry::GetEffectiveStrokeWidth(AIArtHandle art, double& outWidth)
{
	if (GetMaxStyleStrokeWidth(art, outWidth)) {
		return true;
	}
	return GetSimpleStrokeWidth(art, outWidth);
}

bool DuctworkGeometry::EnsureEmorySourceId(AIArtHandle art, std::string& outId)
{
	outId.clear();
	if (!art) {
		return false;
	}

	if (DuctworkMetadata::GetString(art, kEmorySourceIdKey, outId) && !outId.empty()) {
		UpdateEmoryTokens(art, kEmoryRoleCenterline, outId);
		DuctworkMetadata::SetString(art, kEmoryRoleKey, kEmoryRoleCenterline);
		return true;
	}

	outId = ReadEmorySourceIdFromNote(art);
	if (outId.empty()) {
		outId = GenerateSourceId();
	}

	DuctworkMetadata::SetString(art, kEmorySourceIdKey, outId);
	DuctworkMetadata::SetString(art, kEmoryRoleKey, kEmoryRoleCenterline);
	UpdateEmoryTokens(art, kEmoryRoleCenterline, outId);
	return !outId.empty();
}

bool DuctworkGeometry::PrepareSelectedEmorySourceIdsForProcessing(const std::vector<DuctworkPath>& paths,
	std::vector<std::string>& outCleanupIds)
{
	outCleanupIds.clear();

	struct SelectedSourceArt
	{
		AIArtHandle art = nullptr;
		std::string sourceId;
	};

	std::vector<SelectedSourceArt> selectedSources;
	std::set<std::string> sourceIds;
	for (size_t i = 0; i < paths.size(); ++i) {
		if (!paths[i].art || !DuctworkLayers::IsColorLayerName(paths[i].layerName)) {
			continue;
		}

		std::string sourceId;
		if (!EnsureEmorySourceId(paths[i].art, sourceId) || sourceId.empty()) {
			continue;
		}

		SelectedSourceArt selected;
		selected.art = paths[i].art;
		selected.sourceId = sourceId;
		selectedSources.push_back(selected);
		sourceIds.insert(sourceId);
	}

	if (selectedSources.empty()) {
		return false;
	}

	std::map<std::string, std::vector<EmorySourceIdCandidate> > groups;
	CollectSourceIdCandidateGroups(sourceIds, groups);

	bool anyReassigned = false;
	for (std::map<std::string, std::vector<EmorySourceIdCandidate> >::iterator groupIt = groups.begin();
		groupIt != groups.end(); ++groupIt) {
		for (size_t candidateIndex = 0; candidateIndex < groupIt->second.size(); ++candidateIndex) {
			EmorySourceIdCandidate& candidate = groupIt->second[candidateIndex];
			for (size_t selectedIndex = 0; selectedIndex < selectedSources.size(); ++selectedIndex) {
				if (selectedSources[selectedIndex].art == candidate.art) {
					candidate.selected = true;
					break;
				}
			}
		}

		if (groupIt->second.size() <= 1) {
			continue;
		}

		for (size_t candidateIndex = 0; candidateIndex < groupIt->second.size(); ++candidateIndex) {
			EmorySourceIdCandidate& candidate = groupIt->second[candidateIndex];
			if (!candidate.selected) {
				continue;
			}
			candidate.newSourceId = GenerateSourceId();
			anyReassigned = true;
		}
	}

	if (anyReassigned) {
		RebindGeneratedArtForDuplicateSourceIds(groups);
		ApplySourceIdReassignments(groups);
	}

	for (size_t selectedIndex = 0; selectedIndex < selectedSources.size(); ++selectedIndex) {
		std::string cleanupId = selectedSources[selectedIndex].sourceId;
		std::map<std::string, std::vector<EmorySourceIdCandidate> >::const_iterator groupIt = groups.find(cleanupId);
		if (groupIt != groups.end()) {
			for (size_t candidateIndex = 0; candidateIndex < groupIt->second.size(); ++candidateIndex) {
				const EmorySourceIdCandidate& candidate = groupIt->second[candidateIndex];
				if (candidate.art == selectedSources[selectedIndex].art &&
					!candidate.newSourceId.empty()) {
					cleanupId = candidate.newSourceId;
					break;
				}
			}
		}
		if (!cleanupId.empty()) {
			outCleanupIds.push_back(cleanupId);
		}
	}

	std::sort(outCleanupIds.begin(), outCleanupIds.end());
	outCleanupIds.erase(std::unique(outCleanupIds.begin(), outCleanupIds.end()), outCleanupIds.end());
	return !outCleanupIds.empty();
}

size_t DuctworkGeometry::DeleteGeneratedEmoryBodies(const std::vector<std::string>& sourceIds)
{
	if (sourceIds.empty() || !sAIArt) {
		return 0;
	}

	std::set<std::string> ids(sourceIds.begin(), sourceIds.end());
	std::vector<AIArtHandle> allPaths;
	CollectAllLineLayerPaths(allPaths);

	std::vector<AIArtHandle> toDelete;
	std::map<std::string, double> strokeWidthBySourceId;
	for (size_t i = 0; i < allPaths.size(); ++i) {
		AIArtHandle art = allPaths[i];
		if (!IsGeneratedEmoryArtInternal(art)) {
			continue;
		}

		std::string sourceId;
		if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
			sourceId = ReadEmorySourceIdFromNote(art);
		}
		if (sourceId.empty() || ids.find(sourceId) == ids.end()) {
			continue;
		}

		double strokeWidth = 0.0;
		if ((GetMaxStyleStrokeWidth(art, strokeWidth) || GetSimpleStrokeWidth(art, strokeWidth)) && strokeWidth > 0.0) {
			std::map<std::string, double>::iterator existing = strokeWidthBySourceId.find(sourceId);
			if (existing == strokeWidthBySourceId.end() || strokeWidth > existing->second) {
				strokeWidthBySourceId[sourceId] = strokeWidth;
			}
		}

		toDelete.push_back(art);
	}

	for (std::map<std::string, double>::const_iterator it = strokeWidthBySourceId.begin(); it != strokeWidthBySourceId.end(); ++it) {
		AIArtHandle sourceArt = nullptr;
		DuctworkPath sourcePath;
		if (FindSourceArtForSourceId(it->first, sourceArt, sourcePath) && sourceArt && it->second > 0.0) {
			WriteStoredSourceStrokeWidth(sourceArt, it->second);
		}
	}

	size_t removed = 0;
	for (size_t i = 0; i < toDelete.size(); ++i) {
		sAIArt->SetArtUserAttr(toDelete[i], kArtLocked | kArtHidden, 0);
		if (sAIArt->DisposeArt(toDelete[i]) == kNoErr) {
			++removed;
		}
	}
	return removed;
}

EmoryBodyStats DuctworkGeometry::GenerateEmoryBodies(const std::vector<DuctworkPath>& paths)
{
	EmoryBodyStats stats;
	if (!sAIArt || !sAIPath || !sAIPathStyle) {
		return stats;
	}

	std::vector<std::string> affectedSourceIds;
	affectedSourceIds.reserve(paths.size());

	for (size_t i = 0; i < paths.size(); ++i) {
		std::string sourceId;
		if (paths[i].art && DuctworkGeometry::EnsureEmorySourceId(paths[i].art, sourceId) && !sourceId.empty()) {
			affectedSourceIds.push_back(sourceId);
		}
		GenerateEmoryForPath(paths[i], stats);
	}

	if (!affectedSourceIds.empty()) {
		std::sort(affectedSourceIds.begin(), affectedSourceIds.end());
		affectedSourceIds.erase(std::unique(affectedSourceIds.begin(), affectedSourceIds.end()), affectedSourceIds.end());
		ReorderGeneratedBranchArtBehindParents(affectedSourceIds);
	}

	return stats;
}

bool DuctworkGeometry::ToggleSelectedEmoryConnectorStyles(std::string& outMessage)
{
	outMessage = "Select one or more Emory connector pieces first.";
	if (!sAIArt || !sAILayer) {
		return false;
	}

	NormalizeDuplicateEmorySourceIds();

	std::vector<AIArtHandle> selection;
	DuctworkSelection::CollectSelectedPaths(selection);
	if (selection.empty()) {
		return false;
	}

	std::map<std::string, std::set<int> > togglesBySource;
	for (size_t i = 0; i < selection.size(); ++i) {
		AIArtHandle art = selection[i];
		if (!art) {
			continue;
		}

		std::string role;
		if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) || role != kEmoryRoleConnector) {
			continue;
		}

		std::string sourceId;
		if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
			sourceId = ReadEmorySourceIdFromNote(art);
		}
		if (sourceId.empty()) {
			continue;
		}

		double jointIndexValue = -1.0;
		if (!DuctworkMetadata::GetDouble(art, kEmoryJointIndexKey, jointIndexValue)) {
			continue;
		}
		const int jointIndex = static_cast<int>(jointIndexValue);
		if (jointIndex < 0) {
			continue;
		}

		togglesBySource[sourceId].insert(jointIndex);
	}

	if (togglesBySource.empty()) {
		return false;
	}

	std::vector<AIArtHandle> allPaths;
	CollectAllLineLayerPaths(allPaths);

	std::vector<std::string> sourceIds;
	std::vector<DuctworkPath> regeneratePaths;
	for (size_t i = 0; i < allPaths.size(); ++i) {
		AIArtHandle art = allPaths[i];
		if (!art || IsGeneratedEmoryArtInternal(art)) {
			continue;
		}

		std::string sourceId;
		if (!EnsureEmorySourceId(art, sourceId) || sourceId.empty()) {
			continue;
		}

		std::map<std::string, std::set<int> >::const_iterator toggleIt = togglesBySource.find(sourceId);
		if (toggleIt == togglesBySource.end()) {
			continue;
		}

		for (std::set<int>::const_iterator it = toggleIt->second.begin(); it != toggleIt->second.end(); ++it) {
			const std::string currentStyle = ReadCornerStyle(art, *it);
			WriteCornerStyle(art, *it, currentStyle == "straight" ? "round" : "straight");
		}

		DuctworkPath path;
		if (BuildProcessPathForArt(art, path) && IsCenterlineCandidate(path.art, path.points, path.closed, path.layerName)) {
			regeneratePaths.push_back(path);
			sourceIds.push_back(sourceId);
		}
	}

	if (regeneratePaths.empty() || sourceIds.empty()) {
		outMessage = "No matching Emory source lines were found for the selected connectors.";
		return false;
	}

	DeleteGeneratedEmoryBodies(sourceIds);
	EmoryBodyStats stats = GenerateEmoryBodies(regeneratePaths);

	std::vector<AIArtHandle> reselection;
	CollectAllLineLayerPaths(allPaths);
	for (size_t i = 0; i < allPaths.size(); ++i) {
		AIArtHandle art = allPaths[i];
		if (!art) {
			continue;
		}

		std::string role;
		if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) || role != kEmoryRoleConnector) {
			continue;
		}

		std::string sourceId;
		if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
			continue;
		}

		std::map<std::string, std::set<int> >::const_iterator toggleIt = togglesBySource.find(sourceId);
		if (toggleIt == togglesBySource.end()) {
			continue;
		}

		double jointIndexValue = -1.0;
		if (!DuctworkMetadata::GetDouble(art, kEmoryJointIndexKey, jointIndexValue)) {
			continue;
		}
		const int jointIndex = static_cast<int>(jointIndexValue);
		if (toggleIt->second.find(jointIndex) != toggleIt->second.end()) {
			reselection.push_back(art);
		}
	}

	if (!reselection.empty()) {
		ClearSelectionInternal();
		SelectArtListInternal(reselection);
	}

	std::ostringstream message;
	message << "Toggled " << reselection.size() << " connector";
	if (reselection.size() != 1) {
		message << "s";
	}
	message << ". Rebuilt " << stats.segmentsCreated << " segments and " << stats.connectorsCreated << " connectors.";
	outMessage = message.str();
	return true;
}

bool DuctworkGeometry::GetSelectedEmorySegmentState(std::string& outJson)
{
	outJson = "{\"ok\":true,\"available\":false,\"reason\":\"no-selection\"}";
	if (!sAIArt) {
		outJson = "{\"ok\":false,\"available\":false,\"reason\":\"sdk-unavailable\"}";
		return false;
	}

	NormalizeDuplicateEmorySourceIds();

	std::vector<AIArtHandle> selection;
	DuctworkSelection::CollectSelectedPaths(selection);
	if (selection.empty()) {
		return true;
	}

	std::map<std::string, std::vector<int> > selectedBySource;
	for (size_t i = 0; i < selection.size(); ++i) {
		AIArtHandle art = selection[i];
		if (!art) {
			continue;
		}

		std::string role;
		if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) || role != kEmoryRoleSegment) {
			continue;
		}

		std::string artSourceId;
		if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, artSourceId) || artSourceId.empty()) {
			artSourceId = ReadEmorySourceIdFromNote(art);
		}
		if (artSourceId.empty()) {
			continue;
		}

		int segmentIndex = -1;
		if (!ReadGeneratedSegmentIndex(art, segmentIndex)) {
			continue;
		}

		selectedBySource[artSourceId].push_back(segmentIndex);
	}

	if (selectedBySource.empty()) {
		outJson = "{\"ok\":true,\"available\":false,\"reason\":\"no-segment-selection\"}";
		return true;
	}

	if (selectedBySource.size() > 1) {
		bool mixedWidths = false;
		double firstWidth = 0.0;
		bool firstWidthSet = false;
		size_t selectedCount = 0;

		for (std::map<std::string, std::vector<int> >::iterator it = selectedBySource.begin(); it != selectedBySource.end(); ++it) {
			std::sort(it->second.begin(), it->second.end());
			it->second.erase(std::unique(it->second.begin(), it->second.end()), it->second.end());
			selectedCount += it->second.size();

			AIArtHandle sourceArt = nullptr;
			DuctworkPath sourcePath;
			if (!FindSourceArtForSourceId(it->first, sourceArt, sourcePath)) {
				continue;
			}

			std::vector<DuctworkPoint> points;
			SanitizePolyline(sourcePath.points, points);
			const size_t segmentCount = points.size() > 1 ? (points.size() - 1) : 0;
			if (segmentCount == 0) {
				continue;
			}

			double defaultWidth = 0.0;
			if (!ResolveSourceBodyWidth(sourceArt, it->first, defaultWidth) || defaultWidth <= 0.0) {
				defaultWidth = kDefaultDuctWidth;
			}
			if (defaultWidth < kMinDuctWidth) {
				defaultWidth = kMinDuctWidth;
			}

			std::vector<double> widths;
			ReadSegmentWidths(sourceArt, segmentCount, defaultWidth, widths);
			for (size_t i = 0; i < it->second.size(); ++i) {
				const int segmentIndex = it->second[i];
				if (segmentIndex < 0 || segmentIndex >= static_cast<int>(widths.size())) {
					continue;
				}
				const double width = widths[segmentIndex];
				if (!firstWidthSet) {
					firstWidth = width;
					firstWidthSet = true;
				} else if (std::fabs(width - firstWidth) > 1e-6) {
					mixedWidths = true;
				}
			}
		}

		std::ostringstream out;
		out << "{\"ok\":true,\"available\":true"
			<< ",\"runCount\":" << selectedBySource.size()
			<< ",\"selectedCount\":" << selectedCount
			<< ",\"mixedWidths\":" << (mixedWidths ? "true" : "false")
			<< ",\"selectedSegmentIndex\":-1"
			<< ",\"selectedWidth\":0"
			<< ",\"referenceWidth\":" << (firstWidthSet ? firstWidth : kDefaultDuctWidth)
			<< ",\"isStartSegment\":false"
			<< ",\"canSetStart\":false"
			<< ",\"canApplyWidth\":true"
			<< ",\"cascadeDirection\":\"multiple-runs\""
			<< "}";
		outJson = out.str();
		return true;
	}

	const std::string sourceId = selectedBySource.begin()->first;
	std::vector<int>& selectedSegmentIndices = selectedBySource.begin()->second;
	std::sort(selectedSegmentIndices.begin(), selectedSegmentIndices.end());
	selectedSegmentIndices.erase(std::unique(selectedSegmentIndices.begin(), selectedSegmentIndices.end()), selectedSegmentIndices.end());

	AIArtHandle sourceArt = nullptr;
	DuctworkPath sourcePath;
	if (!FindSourceArtForSourceId(sourceId, sourceArt, sourcePath)) {
		outJson = "{\"ok\":true,\"available\":false,\"reason\":\"source-not-found\"}";
		return true;
	}

	std::vector<DuctworkPoint> points;
	SanitizePolyline(sourcePath.points, points);
	const size_t segmentCount = points.size() > 1 ? (points.size() - 1) : 0;
	if (segmentCount == 0) {
		outJson = "{\"ok\":true,\"available\":false,\"reason\":\"no-segments\"}";
		return true;
	}

	double defaultWidth = 0.0;
	if (!ResolveSourceBodyWidth(sourceArt, sourceId, defaultWidth) || defaultWidth <= 0.0) {
		defaultWidth = kDefaultDuctWidth;
	}
	if (defaultWidth < kMinDuctWidth) {
		defaultWidth = kMinDuctWidth;
	}

	std::vector<double> segmentWidths;
	ReadSegmentWidths(sourceArt, segmentCount, defaultWidth, segmentWidths);
	const int startSegmentIndex = ReadStartSegmentIndex(sourceArt, segmentCount);

	bool mixedWidths = false;
	double firstWidth = 0.0;
	bool firstWidthSet = false;
	for (size_t i = 0; i < selectedSegmentIndices.size(); ++i) {
		const int segmentIndex = selectedSegmentIndices[i];
		if (segmentIndex < 0 || segmentIndex >= static_cast<int>(segmentWidths.size())) {
			continue;
		}
		const double width = segmentWidths[segmentIndex];
		if (!firstWidthSet) {
			firstWidth = width;
			firstWidthSet = true;
		} else if (std::fabs(width - firstWidth) > 1e-6) {
			mixedWidths = true;
			break;
		}
	}

	std::ostringstream out;
	out << "{\"ok\":true,\"available\":true"
		<< ",\"sourceId\":\"" << JsonEscape(sourceId) << "\""
		<< ",\"runCount\":1"
		<< ",\"selectedCount\":" << selectedSegmentIndices.size()
		<< ",\"segmentCount\":" << segmentCount
		<< ",\"startSegmentIndex\":" << startSegmentIndex
		<< ",\"mixedWidths\":" << (mixedWidths ? "true" : "false")
		<< ",\"canSetStart\":" << (selectedSegmentIndices.size() == 1 ? "true" : "false")
		<< ",\"isEndpointSelection\":";
	if (selectedSegmentIndices.size() == 1) {
		const int onlyIndex = selectedSegmentIndices[0];
		out << ((onlyIndex == 0 || onlyIndex == static_cast<int>(segmentCount - 1)) ? "true" : "false");
	} else {
		out << "false";
	}
	out << ",\"selectedIndices\":[";
	for (size_t i = 0; i < selectedSegmentIndices.size(); ++i) {
		if (i > 0) {
			out << ",";
		}
		out << selectedSegmentIndices[i];
	}
	out << "]";

	if (selectedSegmentIndices.size() == 1) {
		const int selectedIndex = selectedSegmentIndices[0];
		const int cascadeDirection = DetermineCascadeDirection(segmentCount, startSegmentIndex, selectedIndex);
		out << ",\"selectedSegmentIndex\":" << selectedIndex
			<< ",\"selectedWidth\":" << segmentWidths[selectedIndex]
			<< ",\"referenceWidth\":" << segmentWidths[selectedIndex]
			<< ",\"isStartSegment\":" << (selectedIndex == startSegmentIndex ? "true" : "false")
			<< ",\"cascadeDirection\":\"" << (cascadeDirection == 0 ? "both" : (cascadeDirection < 0 ? "lower" : "higher")) << "\""
			<< ",\"canApplyWidth\":true";
	} else {
		out << ",\"selectedSegmentIndex\":-1"
			<< ",\"selectedWidth\":0"
			<< ",\"referenceWidth\":" << (firstWidthSet ? firstWidth : defaultWidth)
			<< ",\"isStartSegment\":false"
			<< ",\"cascadeDirection\":\"multiple\""
			<< ",\"canApplyWidth\":true";
		if (!mixedWidths && firstWidthSet) {
			out << ",\"sharedWidth\":" << firstWidth;
		}
	}

	out << "}";
	outJson = out.str();
	return true;
}

bool DuctworkGeometry::SetSelectedEmoryStartSegment(std::string& outMessage)
{
	outMessage = "Select exactly one Emory duct segment first.";
	if (!sAIArt) {
		outMessage = "Illustrator SDK is not available.";
		return false;
	}

	NormalizeDuplicateEmorySourceIds();

	std::vector<AIArtHandle> selection;
	DuctworkSelection::CollectSelectedPaths(selection);
	if (selection.empty()) {
		return false;
	}

	AIArtHandle selectedSegment = nullptr;
	std::string sourceId;
	int selectedSegmentIndex = -1;
	for (size_t i = 0; i < selection.size(); ++i) {
		AIArtHandle art = selection[i];
		if (!art) {
			continue;
		}

		std::string role;
		if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) || role != kEmoryRoleSegment) {
			continue;
		}

		if (selectedSegment) {
			outMessage = "Select only one Emory duct segment to mark the cascade start.";
			return false;
		}

		std::string artSourceId;
		if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, artSourceId) || artSourceId.empty()) {
			artSourceId = ReadEmorySourceIdFromNote(art);
		}
		if (artSourceId.empty()) {
			continue;
		}

		int segmentIndex = -1;
		if (!ReadGeneratedSegmentIndex(art, segmentIndex)) {
			continue;
		}

		selectedSegment = art;
		sourceId = artSourceId;
		selectedSegmentIndex = segmentIndex;
	}

	if (!selectedSegment || sourceId.empty() || selectedSegmentIndex < 0) {
		return false;
	}

	AIArtHandle sourceArt = nullptr;
	DuctworkPath sourcePath;
	if (!FindSourceArtForSourceId(sourceId, sourceArt, sourcePath)) {
		outMessage = "Unable to find the source centerline for the selected duct segment.";
		return false;
	}

	std::vector<DuctworkPoint> points;
	SanitizePolyline(sourcePath.points, points);
	const size_t segmentCount = points.size() > 1 ? (points.size() - 1) : 0;
	if (segmentCount == 0) {
		outMessage = "The selected run has no segments.";
		return false;
	}

	if (selectedSegmentIndex < 0 || selectedSegmentIndex >= static_cast<int>(segmentCount)) {
		outMessage = "The selected segment is out of range for this run.";
		return false;
	}

	WriteStartSegmentIndex(sourceArt, selectedSegmentIndex);
	SelectGeneratedSegmentBySourceIdAndIndex(sourceId, selectedSegmentIndex);

	std::ostringstream message;
	message << "Cascade start set to segment " << (selectedSegmentIndex + 1)
		<< " of " << segmentCount << ".";
	outMessage = message.str();
	return true;
}

bool DuctworkGeometry::ApplySelectedEmorySegmentWidth(double newWidth, std::string& outMessage)
{
	outMessage = "Select one or more Emory duct segments first.";
	if (!sAIArt) {
		outMessage = "Illustrator SDK is not available.";
		return false;
	}

	NormalizeDuplicateEmorySourceIds();

	if (!std::isfinite(newWidth) || newWidth <= 0.0) {
		outMessage = "Width must be greater than zero.";
		return false;
	}
	if (newWidth < kMinDuctWidth) {
		newWidth = kMinDuctWidth;
	}

	std::vector<AIArtHandle> selection;
	DuctworkSelection::CollectSelectedPaths(selection);
	if (selection.empty()) {
		return false;
	}

	std::map<std::string, std::vector<int> > selectedBySource;
	for (size_t i = 0; i < selection.size(); ++i) {
		AIArtHandle art = selection[i];
		if (!art) {
			continue;
		}

		std::string role;
		if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) || role != kEmoryRoleSegment) {
			continue;
		}

		std::string artSourceId;
		if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, artSourceId) || artSourceId.empty()) {
			artSourceId = ReadEmorySourceIdFromNote(art);
		}
		if (artSourceId.empty()) {
			continue;
		}

		int segmentIndex = -1;
		if (!ReadGeneratedSegmentIndex(art, segmentIndex)) {
			continue;
		}

		selectedBySource[artSourceId].push_back(segmentIndex);
	}

	if (selectedBySource.empty()) {
		return false;
	}

	std::vector<EmorySourceState> states;
	std::map<std::string, int> stateIndexBySourceId;
	if (!CollectEmorySourceStates(states, stateIndexBySourceId)) {
		outMessage = "Unable to read the Emory source centerlines for the selected ductwork.";
		return false;
	}

	std::map<int, std::vector<int> > selectedIndicesByState;
	std::vector<std::string> sourceIds;
	std::vector<DuctworkPath> regeneratePaths;
	size_t totalSelectedCount = 0;

	for (std::map<std::string, std::vector<int> >::iterator it = selectedBySource.begin(); it != selectedBySource.end(); ++it) {
		std::vector<int>& selectedSegmentIndices = it->second;
		std::sort(selectedSegmentIndices.begin(), selectedSegmentIndices.end());
		selectedSegmentIndices.erase(std::unique(selectedSegmentIndices.begin(), selectedSegmentIndices.end()), selectedSegmentIndices.end());
		totalSelectedCount += selectedSegmentIndices.size();

		std::map<std::string, int>::const_iterator stateIt = stateIndexBySourceId.find(it->first);
		if (stateIt == stateIndexBySourceId.end()) {
			outMessage = "Unable to find the source centerline for one of the selected duct runs.";
			return false;
		}

		EmorySourceState& state = states[stateIt->second];
		const int segmentCount = state.segmentCount;
		if (segmentCount == 0) {
			outMessage = "One of the selected runs has no valid segments.";
			return false;
		}
		for (size_t i = 0; i < selectedSegmentIndices.size(); ++i) {
			if (selectedSegmentIndices[i] < 0 || selectedSegmentIndices[i] >= segmentCount) {
				outMessage = "One or more selected segments are out of range for a run.";
				return false;
			}
		}

		selectedIndicesByState[stateIt->second] = selectedSegmentIndices;
	}

	for (std::map<int, std::vector<int> >::const_iterator it = selectedIndicesByState.begin(); it != selectedIndicesByState.end(); ++it) {
		ApplySelectedWidthToPathState(states[it->first], it->second, newWidth);
	}

	std::vector<DuctworkConnection> connections;
	CollectEmoryNetworkConnections(states, connections);
	CascadeConnectedBranchWidths(states, connections);

	for (size_t i = 0; i < states.size(); ++i) {
		EmorySourceState& state = states[i];
		if (!state.touched || !state.art || state.widths.empty()) {
			continue;
		}

		WriteSegmentWidths(state.art, state.widths);
		int storedIndex = state.startSegmentIndex;
		if (storedIndex < 0 || storedIndex >= static_cast<int>(state.widths.size())) {
			storedIndex = 0;
		}
		WriteStoredSourceBodyWidth(state.art, state.widths[storedIndex]);

		sourceIds.push_back(state.sourceId);
		regeneratePaths.push_back(state.path);
	}

	if (sourceIds.empty() || regeneratePaths.empty()) {
		outMessage = "No Emory runs were updated from the current selection.";
		return false;
	}

	std::sort(sourceIds.begin(), sourceIds.end());
	sourceIds.erase(std::unique(sourceIds.begin(), sourceIds.end()), sourceIds.end());

	DeleteGeneratedEmoryBodies(sourceIds);
	EmoryBodyStats stats = GenerateEmoryBodies(regeneratePaths);
	SelectGeneratedSegmentsBySourceMap(selectedBySource);

	const size_t cascadedRunCount = sourceIds.size() > selectedBySource.size()
		? (sourceIds.size() - selectedBySource.size())
		: 0;

	std::ostringstream message;
	message << "Updated " << totalSelectedCount << " segment";
	if (totalSelectedCount != 1) {
		message << "s";
	}
	message << " to width " << newWidth;
	if (cascadedRunCount > 0) {
		message << " and cascaded into " << cascadedRunCount << " connected branch run";
		if (cascadedRunCount != 1) {
			message << "s";
		}
	}
	message
		<< ". Rebuilt " << stats.segmentsCreated << " segments and "
		<< stats.connectorsCreated << " connectors.";
	outMessage = message.str();
	return true;
}
