#include "IllustratorSDK.h"
#include "ProcessDuctworkGeometry.h"
#include "ProcessDuctworkLayers.h"
#include "ProcessDuctworkLog.h"
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

	bool ApplyFilledPathStyle(AIArtHandle art, const EmoryColorSpec& colors, double bodyWidth)
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
		style.stroke.width = static_cast<AIReal>(ComputeBodyStrokeWidth(bodyWidth));
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
		if (!DuctworkGeometry::GetEffectiveStrokeWidth(path.art, bodyWidth) || bodyWidth <= 0.0) {
			bodyWidth = kDefaultDuctWidth;
		}
		if (bodyWidth < kMinDuctWidth) {
			bodyWidth = kMinDuctWidth;
		}

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

			const double prevLength = std::hypot(points[jointIndex - 1].x - points[jointIndex].x,
				points[jointIndex - 1].y - points[jointIndex].y);
			const double nextLength = std::hypot(points[jointIndex + 1].x - points[jointIndex].x,
				points[jointIndex + 1].y - points[jointIndex].y);
			double trimDistance = bodyWidth * kTrimMultiplier;
			if (connectorStyle == "round" && turnAngle > 1e-3 && turnAngle < (3.141592653589793 - 1e-3)) {
				const double desiredRadius = bodyWidth * kRoundMinCenterlineRadiusMultiplier;
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
			connector.prevWidth = bodyWidth;
			connector.nextWidth = bodyWidth;
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
			if (!BuildBandPolygon(trimmedStart, trimmedEnd, bodyWidth, polygon)) {
				continue;
			}

			AIArtHandle segmentArt = nullptr;
			if (!CreateClosedPath(referenceArt, polygon, segmentArt) || !segmentArt) {
				++stats.failed;
				continue;
			}
			if (!ApplyFilledPathStyle(segmentArt, colors, bodyWidth)) {
				sAIArt->DisposeArt(segmentArt);
				++stats.failed;
				continue;
			}

			TagGeneratedArt(segmentArt, kEmoryRoleSegment, sourceId, bodyWidth,
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
			if (!ApplyFilledPathStyle(connectorArt, colors, bodyWidth)) {
				sAIArt->DisposeArt(connectorArt);
				++stats.failed;
				continue;
			}

			TagGeneratedArt(connectorArt, kEmoryRoleConnector, sourceId, bodyWidth, -1,
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

size_t DuctworkGeometry::DeleteGeneratedEmoryBodies(const std::vector<std::string>& sourceIds)
{
	if (sourceIds.empty() || !sAIArt) {
		return 0;
	}

	std::set<std::string> ids(sourceIds.begin(), sourceIds.end());
	std::vector<AIArtHandle> allPaths;
	CollectAllLineLayerPaths(allPaths);

	std::vector<AIArtHandle> toDelete;
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

		toDelete.push_back(art);
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

	for (size_t i = 0; i < paths.size(); ++i) {
		GenerateEmoryForPath(paths[i], stats);
	}

	return stats;
}

bool DuctworkGeometry::ToggleSelectedEmoryConnectorStyles(std::string& outMessage)
{
	outMessage = "Select one or more Emory connector pieces first.";
	if (!sAIArt || !sAILayer) {
		return false;
	}

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
