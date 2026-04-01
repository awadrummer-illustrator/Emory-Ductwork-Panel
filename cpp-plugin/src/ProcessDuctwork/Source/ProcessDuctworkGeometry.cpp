#include "IllustratorSDK.h"
#include "ProcessDuctworkConstants.h"
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
	const double kStraightTaperRatio = 0.8;
	const double kStraightTaperConnectorMinHalfLengthMultiplier = 0.15;
	const double kStraightTaperConnectorDiffHalfLengthMultiplier = 1.0;
	const double kRoundMinCenterlineRadiusMultiplier = 0.33;
	const double kRoundBezierOuterHandleMultiplier = 2.5;
	const double kRoundBezierMaxLongHandleScale = 3.0;
	const double kRoundBezierShallowAngleStart = 3.141592653589793 * 0.5;
	const double kRoundBezierShallowAngleEnd = 3.141592653589793 / 12.0;
	const double kRoundBezierStandardHandleReduction = 0.85;
	const double kRoundBezierLongHandlePreserveThreshold = 0.5;
	const double kRoundBezierOuterReduceMinAngle = 40.0 * (3.141592653589793 / 180.0);
	const double kRoundBezierOuterReduceMaxAngle = 50.0 * (3.141592653589793 / 180.0);
	const double kRoundBezierInnerReduceMinAngle = 85.0 * (3.141592653589793 / 180.0);
	const double kRoundBezierInnerReduceMaxAngle = 95.0 * (3.141592653589793 / 180.0);
	const double kRoundBezierOuterTargetAngleReduction = 0.60;
	const double kRoundBezierInnerTargetAngleReduction = 0.75;
	const double kRoundBezierLargeEndHandleConvergeMax = 0.16;
	const double kRoundBezierOuterChordCapMultiplier = 2.5;
	const double kRoundBezierInnerChordCapMultiplier = 1.75;
	const double kAxisAlignmentTolerance = 0.01;
	const double kNetworkConnectorDesiredLengthMultiplier = 0.9;
	const char* const kEmoryRoleKey = "MDUX_EmoryRole";
	const char* const kEmorySourceIdKey = "MDUX_EmorySourceId";
	const char* const kEmoryLinkedSourceIdsKey = "MDUX_EmoryLinkedSourceIds";
	const char* const kEmoryBodyWidthKey = "MDUX_EmoryBodyWidth";
	const char* const kEmorySourceBodyWidthKey = "MDUX_EmorySourceBodyWidth";
	const char* const kEmorySourceStrokeWidthKey = "MDUX_EmorySourceStrokeWidth";
	const char* const kEmorySegmentWidthsKey = "MDUX_EmorySegmentWidths";
	const char* const kEmoryStartSegmentIndexKey = "MDUX_EmoryStartSegmentIndex";
	const char* const kEmoryTaperAlignmentsKey = "MDUX_EmoryTaperAlignments";
	const char* const kEmoryCenterlinesHiddenKey = "MDUX_EmoryCenterlinesHidden";
	const char* const kEmoryOmitStartSegmentThicknessKey = "MDUX_EmoryOmitStartSegmentThickness";
	const char* const kEmoryOmitEndSegmentThicknessKey = "MDUX_EmoryOmitEndSegmentThickness";
	const char* const kEmorySegmentIndexKey = "MDUX_EmorySegmentIndex";
	const char* const kEmoryJointIndexKey = "MDUX_EmoryJointIndex";
	const char* const kEmoryConnectorStyleKey = "MDUX_EmoryConnectorStyle";
	const char* const kEmoryCornerStylesKey = "MDUX_EmoryCornerStyles";
	const char* const kEmoryRoleSegment = "segment";
	const char* const kEmoryRoleConnector = "connector";
	const char* const kEmoryRoleCenterline = "centerline";
	const char* const kEmoryRoleRunGroup = "run-group";
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
		bool isStraightTaper = false;
		bool chainHorizontal = false;
		bool chainVertical = false;
		std::string taperAlignment;
		double taperReferenceWidth = 0.0;
		bool prevChainHorizontal = false;
		bool prevChainVertical = false;
		std::string prevTaperAlignment;
		double prevTaperReferenceWidth = 0.0;
		bool nextChainHorizontal = false;
		bool nextChainVertical = false;
		std::string nextTaperAlignment;
		double nextTaperReferenceWidth = 0.0;
	};

	struct StraightChainInfo
	{
		int startSegmentIndex = -1;
		int endSegmentIndex = -1;
		bool horizontal = false;
		bool vertical = false;
		std::string alignment;
		double referenceWidth = 0.0;
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
		double sourceStrokeWidth = 0.0;
		std::vector<double> originalWidths;
		std::vector<double> widths;
		std::vector<StraightChainInfo> straightChains;
		std::vector<int> straightChainIndexBySegment;
		bool touched = false;
		bool selectedSeed = false;
	};

	struct NetworkConnectorArm
	{
		int stateIndex = -1;
		int segmentIndex = -1;
		Vec2 dir;
		double angle = 0.0;
		double width = 0.0;
		double availableLength = 0.0;
		bool chainHorizontal = false;
		bool chainVertical = false;
		std::string taperAlignment;
		double taperReferenceWidth = 0.0;
		DuctworkPoint innerLeft;
		DuctworkPoint innerRight;
		DuctworkPoint outerLeft;
		DuctworkPoint outerRight;
	};

	struct NetworkConnectorSpec
	{
		AIArtHandle referenceArt = nullptr;
		std::string primarySourceId;
		std::vector<std::string> linkedSourceIds;
		std::string layerName;
		std::string style;
		DuctworkPoint point;
		double bodyWidth = 0.0;
		double strokeWidth = 0.0;
		std::vector<NetworkConnectorArm> arms;
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

	bool BuildProcessPathForArt(AIArtHandle art, DuctworkPath& outPath);

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

	DuctworkPoint Lerp(const DuctworkPoint& a, const DuctworkPoint& b, double t)
	{
		DuctworkPoint result;
		result.x = a.x + ((b.x - a.x) * t);
		result.y = a.y + ((b.y - a.y) * t);
		return result;
	}

	DuctworkPoint EvaluateCubicBezier(const DuctworkPoint& p0, const DuctworkPoint& p1,
		const DuctworkPoint& p2, const DuctworkPoint& p3, double t)
	{
		const DuctworkPoint a = Lerp(p0, p1, t);
		const DuctworkPoint b = Lerp(p1, p2, t);
		const DuctworkPoint c = Lerp(p2, p3, t);
		const DuctworkPoint d = Lerp(a, b, t);
		const DuctworkPoint e = Lerp(b, c, t);
		return Lerp(d, e, t);
	}

	Vec2 EvaluateCubicBezierTangent(const DuctworkPoint& p0, const DuctworkPoint& p1,
		const DuctworkPoint& p2, const DuctworkPoint& p3, double t)
	{
		const double omt = 1.0 - t;
		Vec2 tangent;
		tangent.x =
			(3.0 * omt * omt * (p1.x - p0.x)) +
			(6.0 * omt * t * (p2.x - p1.x)) +
			(3.0 * t * t * (p3.x - p2.x));
		tangent.y =
			(3.0 * omt * omt * (p1.y - p0.y)) +
			(6.0 * omt * t * (p2.y - p1.y)) +
			(3.0 * t * t * (p3.y - p2.y));
		return tangent;
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

	void CollectRegisterAttachmentPointsFromArt(AIArtHandle art, std::vector<DuctworkPoint>& outPoints)
	{
		if (!art || !sAIArt) {
			return;
		}

		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type) != kNoErr) {
			return;
		}

		if (type == kPathArt) {
			std::vector<DuctworkPoint> points;
			bool closed = false;
			if (DuctworkGeometry::GetPathPoints(art, points, closed) && points.size() == 1) {
				outPoints.push_back(points[0]);
			}
		} else if (type == kPlacedArt) {
			AIRealRect bounds{};
			if (sAIArt->GetArtBounds(art, &bounds) == kNoErr) {
				DuctworkPoint center;
				center.x = (static_cast<double>(bounds.left) + static_cast<double>(bounds.right)) * 0.5;
				center.y = (static_cast<double>(bounds.top) + static_cast<double>(bounds.bottom)) * 0.5;
				outPoints.push_back(center);
			}
		}

		AIArtHandle child = nullptr;
		if (sAIArt->GetArtFirstChild(art, &child) == kNoErr && child) {
			for (AIArtHandle current = child; current; ) {
				CollectRegisterAttachmentPointsFromArt(current, outPoints);
				AIArtHandle next = nullptr;
				if (sAIArt->GetArtSibling(current, &next) != kNoErr) {
					break;
				}
				current = next;
			}
		}
	}

	void CollectRegisterAttachmentPoints(std::vector<DuctworkPoint>& outPoints)
	{
		outPoints.clear();
		if (!sAILayer || !sAIArt) {
			return;
		}

		for (size_t layerIndex = 0; layerIndex < DuctworkConstants::kRegisterLayerCount; ++layerIndex) {
			AILayerHandle layer = nullptr;
			const ai::UnicodeString layerName = ai::UnicodeString::FromUTF8(DuctworkConstants::kRegisterLayers[layerIndex]);
			if (sAILayer->GetLayerByTitle(&layer, layerName) != kNoErr || !layer) {
				continue;
			}

			AIArtHandle layerGroup = nullptr;
			if (sAIArt->GetFirstArtOfLayer(layer, &layerGroup) != kNoErr || !layerGroup) {
				continue;
			}
			CollectRegisterAttachmentPointsFromArt(layerGroup, outPoints);
		}
	}

	bool IsPointNearAny(const DuctworkPoint& point, const std::vector<DuctworkPoint>& candidates, double tolerance)
	{
		const double toleranceSq = tolerance * tolerance;
		for (size_t i = 0; i < candidates.size(); ++i) {
			const double dx = point.x - candidates[i].x;
			const double dy = point.y - candidates[i].y;
			if ((dx * dx + dy * dy) <= toleranceSq) {
				return true;
			}
		}
		return false;
	}

	bool EndpointHasAnyCenterlineConnection(const std::vector<DuctworkConnection>& connections,
		int pathIndex, int endpointIndex)
	{
		for (size_t i = 0; i < connections.size(); ++i) {
			const DuctworkConnection& connection = connections[i];
			if (connection.a == pathIndex && connection.endpointA == endpointIndex) {
				return true;
			}
			if (connection.b == pathIndex && connection.endpointB == endpointIndex) {
				return true;
			}
		}
		return false;
	}

	void WriteFinalSegmentThicknessMetadata(AIArtHandle art, bool omitStart, bool omitEnd)
	{
		if (!art) {
			return;
		}
		if (omitStart) {
			DuctworkMetadata::SetDouble(art, kEmoryOmitStartSegmentThicknessKey, 1.0);
		} else {
			DuctworkMetadata::RemoveKey(art, kEmoryOmitStartSegmentThicknessKey);
		}
		if (omitEnd) {
			DuctworkMetadata::SetDouble(art, kEmoryOmitEndSegmentThicknessKey, 1.0);
		} else {
			DuctworkMetadata::RemoveKey(art, kEmoryOmitEndSegmentThicknessKey);
		}
	}

	bool ReadFinalSegmentThicknessFlag(AIArtHandle art, const char* key)
	{
		double value = 0.0;
		return DuctworkMetadata::GetDouble(art, key, value) && value > 0.5;
	}

	bool IsSegmentThicknessOmitted(size_t segmentIndex, size_t segmentCount, bool omitStart, bool omitEnd)
	{
		return (omitStart && segmentIndex == 0) ||
			(omitEnd && segmentIndex + 1 == segmentCount);
	}

	bool UngroupSingleArtRecursive(AIArtHandle group)
	{
		if (!group || !sAIArt) {
			return false;
		}

		short artType = kUnknownArt;
		if (sAIArt->GetArtType(group, &artType) != kNoErr || artType != kGroupArt) {
			return false;
		}

		std::vector<AIArtHandle> children;
		AIArtHandle child = nullptr;
		if (sAIArt->GetArtFirstChild(group, &child) == kNoErr && child) {
			for (AIArtHandle current = child; current; ) {
				children.push_back(current);
				AIArtHandle next = nullptr;
				if (sAIArt->GetArtSibling(current, &next) != kNoErr) {
					break;
				}
				current = next;
			}
		}

		std::vector<AIArtHandle> nestedGroups;
		for (size_t i = 0; i < children.size(); ++i) {
			short childType = kUnknownArt;
			if (sAIArt->GetArtType(children[i], &childType) == kNoErr && childType == kGroupArt) {
				nestedGroups.push_back(children[i]);
			}
			sAIArt->ReorderArt(children[i], kPlaceAbove, group);
		}

		sAIArt->DisposeArt(group);
		for (size_t i = 0; i < nestedGroups.size(); ++i) {
			UngroupSingleArtRecursive(nestedGroups[i]);
		}
		return true;
	}

	bool IsLayerContainerGroup(AIArtHandle art, AIArtHandle parent)
	{
		if (!art || !parent || !sAIArt) {
			return false;
		}
		AILayerHandle layer = nullptr;
		if (sAIArt->GetLayerOfArt(art, &layer) != kNoErr || !layer) {
			return false;
		}
		AIArtHandle layerGroup = nullptr;
		if (sAIArt->GetFirstArtOfLayer(layer, &layerGroup) != kNoErr || !layerGroup) {
			return false;
		}
		return layerGroup == parent;
	}

	void UngroupAncestorGroupsRecursive(AIArtHandle art)
	{
		if (!art || !sAIArt) {
			return;
		}

		while (art) {
			AIArtHandle parent = nullptr;
			if (sAIArt->GetArtParent(art, &parent) != kNoErr || !parent) {
				return;
			}
			short parentType = kUnknownArt;
			if (sAIArt->GetArtType(parent, &parentType) != kNoErr || parentType != kGroupArt) {
				return;
			}
			if (IsLayerContainerGroup(art, parent)) {
				return;
			}
			if (!UngroupSingleArtRecursive(parent)) {
				return;
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

	bool SetSimpleStrokeWidth(AIArtHandle art, double strokeWidth, bool enforceEmoryStrokeStyle)
	{
		if (!art || !sAIPathStyle || strokeWidth <= 0.0) {
			return false;
		}

		AIPathStyle style;
		AIBoolean fillVisible = false;
		AIBoolean strokeVisible = false;
		if (sAIPathStyle->GetPathStyleEx(art, &style, &fillVisible, &strokeVisible) != kNoErr) {
			return false;
		}

		strokeVisible = true;
		style.strokePaint = true;
		style.stroke.width = static_cast<AIReal>(strokeWidth);
		if (enforceEmoryStrokeStyle) {
			style.stroke.join = kAIRoundJoin;
		}
		if (sAIPathStyle->SetPathStyleEx(art, &style, fillVisible, strokeVisible) != kNoErr) {
			return false;
		}
		if (enforceEmoryStrokeStyle && sAIPaintStyle) {
			sAIPaintStyle->SetStrokeAlignment(art, kAIStrokeAlignmentCenter);
		}
		return true;
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

	bool CreateClosedPathSegments(AIArtHandle referenceArt, const std::vector<AIPathSegment>& segments, AIArtHandle& outPath)
	{
		outPath = nullptr;
		if (!referenceArt || segments.size() < 3 || !sAIArt || !sAIPath) {
			return false;
		}

		AIArtHandle path = nullptr;
		if (sAIArt->NewArt(kPathArt, kPlaceAbove, referenceArt, &path) != kNoErr || !path) {
			return false;
		}

		const ai::int16 segmentCount = static_cast<ai::int16>(segments.size());
		if (sAIPath->SetPathSegmentCount(path, segmentCount) != kNoErr) {
			sAIArt->DisposeArt(path);
			return false;
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

	bool CreateClosedPath(AIArtHandle referenceArt, const std::vector<DuctworkPoint>& polygon, AIArtHandle& outPath)
	{
		if (polygon.size() < 3) {
			outPath = nullptr;
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
		return CreateClosedPathSegments(referenceArt, segments, outPath);
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
		style.stroke.join = kAIRoundJoin;
		if (sAIPathStyle->SetPathStyleEx(art, &style, fillVisible, strokeVisible) != kNoErr) {
			return false;
		}
		if (sAIPaintStyle) {
			sAIPaintStyle->SetStrokeAlignment(art, kAIStrokeAlignmentCenter);
		}
		return true;
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

	void ResolveAlignedOffsets(const Vec2& dir,
		double width,
		double referenceWidth,
		const std::string& alignment,
		bool chainHorizontal,
		bool chainVertical,
		double& outPrimaryOffset,
		double& outSecondaryOffset)
	{
		const double normalizedWidth = (std::max)(width, kMinDuctWidth);
		const double normalizedReferenceWidth = (std::max)(referenceWidth, normalizedWidth);
		outPrimaryOffset = normalizedWidth * 0.5;
		outSecondaryOffset = -normalizedWidth * 0.5;
		if (alignment.empty() || alignment == "center") {
			return;
		}

		Vec2 normal = PerpCCW(dir);
		const Vec2 up = { 0.0, 1.0 };
		const Vec2 right = { 1.0, 0.0 };

		if (chainHorizontal && (alignment == "top" || alignment == "bottom")) {
			const double sign = Dot(normal, up) >= 0.0 ? 1.0 : -1.0;
			double topOffsetGlobal = normalizedWidth * 0.5;
			double bottomOffsetGlobal = -normalizedWidth * 0.5;
			if (alignment == "top") {
				topOffsetGlobal = normalizedReferenceWidth * 0.5;
				bottomOffsetGlobal = topOffsetGlobal - normalizedWidth;
			} else {
				bottomOffsetGlobal = -(normalizedReferenceWidth * 0.5);
				topOffsetGlobal = bottomOffsetGlobal + normalizedWidth;
			}
			outPrimaryOffset = sign * topOffsetGlobal;
			outSecondaryOffset = sign * bottomOffsetGlobal;
			return;
		}

		if (chainVertical && (alignment == "left" || alignment == "right")) {
			const double sign = Dot(normal, right) >= 0.0 ? 1.0 : -1.0;
			double rightOffsetGlobal = normalizedWidth * 0.5;
			double leftOffsetGlobal = -normalizedWidth * 0.5;
			if (alignment == "right") {
				rightOffsetGlobal = normalizedReferenceWidth * 0.5;
				leftOffsetGlobal = rightOffsetGlobal - normalizedWidth;
			} else {
				leftOffsetGlobal = -(normalizedReferenceWidth * 0.5);
				rightOffsetGlobal = leftOffsetGlobal + normalizedWidth;
			}
			outPrimaryOffset = sign * rightOffsetGlobal;
			outSecondaryOffset = sign * leftOffsetGlobal;
		}
	}

	bool BuildAlignedBandPolygon(const DuctworkPoint& start,
		const DuctworkPoint& end,
		double fullWidth,
		double referenceWidth,
		const std::string& alignment,
		bool chainHorizontal,
		bool chainVertical,
		std::vector<DuctworkPoint>& outPolygon)
	{
		outPolygon.clear();
		Vec2 dir;
		Vec2 normal;
		if (!BuildUnitDirection(start, end, dir, normal)) {
			return false;
		}

		double primaryOffset = 0.0;
		double secondaryOffset = 0.0;
		ResolveAlignedOffsets(dir, fullWidth, referenceWidth, alignment, chainHorizontal, chainVertical, primaryOffset, secondaryOffset);
		AppendIfDistinct(outPolygon, Add(start, normal, primaryOffset));
		AppendIfDistinct(outPolygon, Add(end, normal, primaryOffset));
		AppendIfDistinct(outPolygon, Add(end, normal, secondaryOffset));
		AppendIfDistinct(outPolygon, Add(start, normal, secondaryOffset));
		return outPolygon.size() >= 3;
	}

	bool BuildStraightTaperConnectorPolygon(const ConnectorSpec& connector, std::vector<DuctworkPoint>& outPolygon)
	{
		outPolygon.clear();
		Vec2 axis = Scale(connector.prevDir, -1.0);
		Vec2 normal;
		if (!Normalize(axis, axis)) {
			return false;
		}
		normal = PerpCCW(axis);

		double prevPrimaryOffset = 0.0;
		double prevSecondaryOffset = 0.0;
		double nextPrimaryOffset = 0.0;
		double nextSecondaryOffset = 0.0;
		ResolveAlignedOffsets(axis, connector.prevWidth, connector.taperReferenceWidth, connector.taperAlignment,
			connector.chainHorizontal, connector.chainVertical, prevPrimaryOffset, prevSecondaryOffset);
		ResolveAlignedOffsets(axis, connector.nextWidth, connector.taperReferenceWidth, connector.taperAlignment,
			connector.chainHorizontal, connector.chainVertical, nextPrimaryOffset, nextSecondaryOffset);

		AppendIfDistinct(outPolygon, Add(connector.prevTrimPoint, normal, prevPrimaryOffset));
		AppendIfDistinct(outPolygon, Add(connector.nextTrimPoint, normal, nextPrimaryOffset));
		AppendIfDistinct(outPolygon, Add(connector.nextTrimPoint, normal, nextSecondaryOffset));
		AppendIfDistinct(outPolygon, Add(connector.prevTrimPoint, normal, prevSecondaryOffset));
		return outPolygon.size() >= 4;
	}

	void ResolveConnectorSidePoints(const DuctworkPoint& trimPoint,
		const Vec2& segmentDir,
		double width,
		bool chainHorizontal,
		bool chainVertical,
		const std::string& alignment,
		double referenceWidth,
		DuctworkPoint& outLeftPoint,
		DuctworkPoint& outRightPoint)
	{
		Vec2 axis = segmentDir;
		if (!Normalize(axis, axis)) {
			outLeftPoint = trimPoint;
			outRightPoint = trimPoint;
			return;
		}

		const Vec2 normal = PerpCCW(axis);
		double leftOffset = 0.0;
		double rightOffset = 0.0;
		const double resolvedReferenceWidth = (std::max)(referenceWidth, (std::max)(width, kMinDuctWidth));
		const std::string resolvedAlignment = alignment.empty() ? "center" : alignment;
		ResolveAlignedOffsets(axis, width, resolvedReferenceWidth, resolvedAlignment,
			chainHorizontal, chainVertical, leftOffset, rightOffset);
		outLeftPoint = Add(trimPoint, normal, leftOffset);
		outRightPoint = Add(trimPoint, normal, rightOffset);
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

	void DescribeStateSegmentDisplay(const EmorySourceState& state,
		int segmentIndex,
		bool& outChainHorizontal,
		bool& outChainVertical,
		std::string& outAlignment,
		double& outReferenceWidth)
	{
		outChainHorizontal = false;
		outChainVertical = false;
		outAlignment = "center";
		outReferenceWidth = kMinDuctWidth;

		if (segmentIndex >= 0 && segmentIndex < static_cast<int>(state.widths.size()) &&
			state.widths[segmentIndex] > outReferenceWidth) {
			outReferenceWidth = state.widths[segmentIndex];
		}

		if (segmentIndex < 0 ||
			segmentIndex >= static_cast<int>(state.straightChainIndexBySegment.size())) {
			return;
		}

		const int chainIndex = state.straightChainIndexBySegment[segmentIndex];
		if (chainIndex < 0 || chainIndex >= static_cast<int>(state.straightChains.size())) {
			return;
		}

		const StraightChainInfo& chain = state.straightChains[chainIndex];
		outChainHorizontal = chain.horizontal;
		outChainVertical = chain.vertical;
		outAlignment = chain.alignment.empty() ? "center" : chain.alignment;
		outReferenceWidth = (std::max)(chain.referenceWidth, outReferenceWidth);
	}

	bool BuildNetworkConnectorArm(const EmorySourceState& state,
		int stateIndex,
		int segmentIndex,
		const DuctworkPoint& connectionPoint,
		const Vec2& outwardDir,
		double availableLength,
		double desiredLength,
		NetworkConnectorArm& outArm)
	{
		outArm = NetworkConnectorArm();
		if (segmentIndex < 0 || segmentIndex >= static_cast<int>(state.widths.size()) ||
			availableLength <= 0.1) {
			return false;
		}

		Vec2 dir = outwardDir;
		if (!Normalize(dir, dir)) {
			return false;
		}

		double resolvedLength = desiredLength;
		const double maxAllowed = availableLength - 0.1;
		if (resolvedLength > maxAllowed) {
			resolvedLength = maxAllowed;
		}
		if (resolvedLength <= 0.1) {
			return false;
		}

		outArm.stateIndex = stateIndex;
		outArm.segmentIndex = segmentIndex;
		outArm.dir = dir;
		outArm.angle = std::atan2(dir.y, dir.x);
		if (outArm.angle < 0.0) {
			outArm.angle += (3.141592653589793 * 2.0);
		}
		outArm.width = state.widths[segmentIndex] > kMinDuctWidth ? state.widths[segmentIndex] : kMinDuctWidth;
		outArm.availableLength = availableLength;
		DescribeStateSegmentDisplay(state, segmentIndex,
			outArm.chainHorizontal, outArm.chainVertical,
			outArm.taperAlignment, outArm.taperReferenceWidth);

		const DuctworkPoint trimPoint = Add(connectionPoint, dir, resolvedLength);
		ResolveConnectorSidePoints(connectionPoint, dir, outArm.width,
			outArm.chainHorizontal, outArm.chainVertical,
			outArm.taperAlignment, outArm.taperReferenceWidth,
			outArm.innerLeft, outArm.innerRight);
		ResolveConnectorSidePoints(trimPoint, dir, outArm.width,
			outArm.chainHorizontal, outArm.chainVertical,
			outArm.taperAlignment, outArm.taperReferenceWidth,
			outArm.outerLeft, outArm.outerRight);
		return true;
	}

	bool BuildNetworkConnectorPolygon(const NetworkConnectorSpec& connector, std::vector<DuctworkPoint>& outPolygon)
	{
		outPolygon.clear();
		if (connector.arms.size() < 3) {
			return false;
		}

		std::vector<NetworkConnectorArm> arms = connector.arms;
		std::sort(arms.begin(), arms.end(), [](const NetworkConnectorArm& a, const NetworkConnectorArm& b) {
			return a.angle < b.angle;
		});

		NetworkConnectorArm previous = arms.back();
		for (size_t i = 0; i < arms.size(); ++i) {
			const NetworkConnectorArm& current = arms[i];
			double delta = current.angle - previous.angle;
			if (delta <= 0.0) {
				delta += (3.141592653589793 * 2.0);
			}

			Vec2 previousLeftDir = Subtract(previous.outerLeft, previous.innerLeft);
			Vec2 currentRightDir = Subtract(current.outerRight, current.innerRight);
			DuctworkPoint corner;
			if (delta < (3.141592653589793 - 0.01) &&
				Normalize(previousLeftDir, previousLeftDir) &&
				Normalize(currentRightDir, currentRightDir) &&
				LineIntersection(previous.innerLeft, previousLeftDir, current.innerRight, currentRightDir, corner)) {
				AppendIfDistinct(outPolygon, corner);
			} else {
				AppendIfDistinct(outPolygon, previous.innerLeft);
				AppendIfDistinct(outPolygon, current.innerRight);
			}

			AppendIfDistinct(outPolygon, current.outerRight);
			AppendIfDistinct(outPolygon, current.outerLeft);
			previous = current;
		}

		return outPolygon.size() >= 3;
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

	std::string MakeTaperChainKey(int startSegmentIndex, int endSegmentIndex)
	{
		std::ostringstream out;
		out << startSegmentIndex << "-" << endSegmentIndex;
		return out.str();
	}

	void ParseTaperAlignments(const std::string& serialized, std::map<std::string, std::string>& outAlignments)
	{
		outAlignments.clear();
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
			const std::string key = token.substr(0, sep);
			const std::string value = token.substr(sep + 1);
			if (!key.empty() && !value.empty()) {
				outAlignments[key] = value;
			}
		}
	}

	std::string SerializeTaperAlignments(const std::map<std::string, std::string>& alignments)
	{
		std::ostringstream out;
		bool first = true;
		for (std::map<std::string, std::string>::const_iterator it = alignments.begin(); it != alignments.end(); ++it) {
			if (!first) {
				out << ",";
			}
			first = false;
			out << it->first << ":" << it->second;
		}
		return out.str();
	}

	std::string ReadTaperAlignment(AIArtHandle sourceArt, int startSegmentIndex, int endSegmentIndex)
	{
		std::string serialized;
		if (!DuctworkMetadata::GetString(sourceArt, kEmoryTaperAlignmentsKey, serialized) || serialized.empty()) {
			return "center";
		}

		std::map<std::string, std::string> alignments;
		ParseTaperAlignments(serialized, alignments);
		const std::string key = MakeTaperChainKey(startSegmentIndex, endSegmentIndex);
		std::map<std::string, std::string>::const_iterator it = alignments.find(key);
		if (it == alignments.end() || it->second.empty()) {
			return "center";
		}
		return it->second;
	}

	void WriteTaperAlignment(AIArtHandle sourceArt, int startSegmentIndex, int endSegmentIndex, const std::string& alignment)
	{
		std::string serialized;
		std::map<std::string, std::string> alignments;
		if (DuctworkMetadata::GetString(sourceArt, kEmoryTaperAlignmentsKey, serialized) && !serialized.empty()) {
			ParseTaperAlignments(serialized, alignments);
		}
		alignments[MakeTaperChainKey(startSegmentIndex, endSegmentIndex)] = alignment;
		DuctworkMetadata::SetString(sourceArt, kEmoryTaperAlignmentsKey, SerializeTaperAlignments(alignments));
	}

	std::string SerializeSourceIds(const std::vector<std::string>& sourceIds)
	{
		std::ostringstream out;
		bool first = true;
		for (size_t i = 0; i < sourceIds.size(); ++i) {
			if (sourceIds[i].empty()) {
				continue;
			}
			if (!first) {
				out << "|";
			}
			first = false;
			out << sourceIds[i];
		}
		return out.str();
	}

	void ParseSourceIds(const std::string& serialized, std::vector<std::string>& outSourceIds)
	{
		outSourceIds.clear();
		if (serialized.empty()) {
			return;
		}

		std::stringstream ss(serialized);
		std::string token;
		while (std::getline(ss, token, '|')) {
			if (!token.empty()) {
				outSourceIds.push_back(token);
			}
		}
	}

	void NormalizeSourceIdList(std::vector<std::string>& ioSourceIds)
	{
		ioSourceIds.erase(std::remove_if(ioSourceIds.begin(), ioSourceIds.end(),
			[](const std::string& sourceId) { return sourceId.empty(); }),
			ioSourceIds.end());
		std::sort(ioSourceIds.begin(), ioSourceIds.end());
		ioSourceIds.erase(std::unique(ioSourceIds.begin(), ioSourceIds.end()), ioSourceIds.end());
	}

	void ReadLinkedSourceIds(AIArtHandle art, std::vector<std::string>& outSourceIds)
	{
		outSourceIds.clear();
		if (!art) {
			return;
		}

		std::string serialized;
		if (!DuctworkMetadata::GetString(art, kEmoryLinkedSourceIdsKey, serialized) || serialized.empty()) {
			return;
		}
		ParseSourceIds(serialized, outSourceIds);
		NormalizeSourceIdList(outSourceIds);
	}

	void WriteLinkedSourceIds(AIArtHandle art, const std::vector<std::string>& sourceIds)
	{
		if (!art) {
			return;
		}

		std::vector<std::string> normalized = sourceIds;
		NormalizeSourceIdList(normalized);
		if (normalized.empty()) {
			DuctworkMetadata::RemoveKey(art, kEmoryLinkedSourceIdsKey);
			return;
		}
		DuctworkMetadata::SetString(art, kEmoryLinkedSourceIdsKey, SerializeSourceIds(normalized));
	}

	void CollectArtAssociatedSourceIds(AIArtHandle art, std::set<std::string>& ioSourceIds)
	{
		if (!art) {
			return;
		}

		std::string sourceId;
		if ((!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty())) {
			sourceId = ReadEmorySourceIdFromNote(art);
		}
		if (!sourceId.empty()) {
			ioSourceIds.insert(sourceId);
		}

		std::vector<std::string> linkedSourceIds;
		ReadLinkedSourceIds(art, linkedSourceIds);
		for (size_t i = 0; i < linkedSourceIds.size(); ++i) {
			ioSourceIds.insert(linkedSourceIds[i]);
		}
	}

	bool IsNetworkConnectorStyle(const std::string& connectorStyle)
	{
		return connectorStyle == "tee" || connectorStyle == "cross";
	}

	bool IsNetworkConnectorArt(AIArtHandle art)
	{
		if (!art) {
			return false;
		}

		std::string role;
		if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) || role != kEmoryRoleConnector) {
			return false;
		}

		std::string connectorStyle;
		if (DuctworkMetadata::GetString(art, kEmoryConnectorStyleKey, connectorStyle) &&
			IsNetworkConnectorStyle(connectorStyle)) {
			return true;
		}

		std::vector<std::string> linkedSourceIds;
		ReadLinkedSourceIds(art, linkedSourceIds);
		return linkedSourceIds.size() > 1;
	}

	bool HasStoredSegmentWidths(AIArtHandle sourceArt)
	{
		std::string serialized;
		return sourceArt &&
			DuctworkMetadata::GetString(sourceArt, kEmorySegmentWidthsKey, serialized) &&
			!serialized.empty();
	}

	bool BuildSegmentDirection(const std::vector<DuctworkPoint>& points, int segmentIndex, Vec2& outDir)
	{
		Vec2 normal;
		if (segmentIndex < 0 || segmentIndex + 1 >= static_cast<int>(points.size())) {
			outDir.x = 0.0;
			outDir.y = 0.0;
			return false;
		}
		return BuildUnitDirection(points[segmentIndex], points[segmentIndex + 1], outDir, normal);
	}

	void ClassifyStraightChainOrientation(const Vec2& dir, bool& horizontal, bool& vertical)
	{
		horizontal = std::fabs(dir.y) <= kAxisAlignmentTolerance && std::fabs(dir.x) >= (1.0 - kAxisAlignmentTolerance);
		vertical = std::fabs(dir.x) <= kAxisAlignmentTolerance && std::fabs(dir.y) >= (1.0 - kAxisAlignmentTolerance);
	}

	void CollectStraightChainInfos(AIArtHandle sourceArt,
		const std::vector<DuctworkPoint>& points,
		const std::vector<double>& widths,
		std::vector<StraightChainInfo>& outChains,
		std::vector<int>& outChainIndexBySegment)
	{
		outChains.clear();
		const int segmentCount = points.size() > 1 ? static_cast<int>(points.size() - 1) : 0;
		outChainIndexBySegment.assign(segmentCount, -1);
		if (segmentCount <= 1) {
			return;
		}

		int chainStart = 0;
		while (chainStart < segmentCount) {
			int chainEnd = chainStart;
			Vec2 baseDir;
			if (!BuildSegmentDirection(points, chainStart, baseDir)) {
				++chainStart;
				continue;
			}

			while (chainEnd + 1 < segmentCount) {
				Vec2 nextDir;
				if (!BuildSegmentDirection(points, chainEnd + 1, nextDir)) {
					break;
				}
				if (Dot(baseDir, nextDir) < kCollinearThreshold) {
					break;
				}
				chainEnd += 1;
			}

			if (chainEnd > chainStart) {
				StraightChainInfo info;
				info.startSegmentIndex = chainStart;
				info.endSegmentIndex = chainEnd;
				ClassifyStraightChainOrientation(baseDir, info.horizontal, info.vertical);
				info.alignment = ReadTaperAlignment(sourceArt, chainStart, chainEnd);
				if (!info.horizontal && !info.vertical) {
					info.alignment = "center";
				}
				for (int segmentIndex = chainStart; segmentIndex <= chainEnd && segmentIndex < static_cast<int>(widths.size()); ++segmentIndex) {
					if (widths[segmentIndex] > info.referenceWidth) {
						info.referenceWidth = widths[segmentIndex];
					}
				}
				if (info.referenceWidth < kMinDuctWidth) {
					info.referenceWidth = kMinDuctWidth;
				}
				const int chainIndex = static_cast<int>(outChains.size());
				outChains.push_back(info);
				for (int segmentIndex = chainStart; segmentIndex <= chainEnd; ++segmentIndex) {
					outChainIndexBySegment[segmentIndex] = chainIndex;
				}
			}

			chainStart = chainEnd + 1;
		}
	}

	void ApplyStraightChainTaperFromAnchor(const StraightChainInfo& chain,
		const std::vector<double>& seedWidths,
		int anchorIndex,
		int direction,
		std::vector<double>& widths)
	{
		if (anchorIndex < chain.startSegmentIndex || anchorIndex > chain.endSegmentIndex ||
			anchorIndex < 0 || anchorIndex >= static_cast<int>(seedWidths.size()) ||
			seedWidths.size() != widths.size()) {
			return;
		}

		double currentWidth = chain.referenceWidth;
		if (anchorIndex >= 0 && anchorIndex < static_cast<int>(seedWidths.size()) &&
			seedWidths[anchorIndex] > currentWidth) {
			currentWidth = seedWidths[anchorIndex];
		}
		if (currentWidth < kMinDuctWidth) {
			currentWidth = kMinDuctWidth;
		}
		widths[anchorIndex] = currentWidth;

		for (int segmentIndex = anchorIndex + direction;
			segmentIndex >= chain.startSegmentIndex &&
			segmentIndex <= chain.endSegmentIndex &&
			segmentIndex < static_cast<int>(widths.size());
			segmentIndex += direction) {
			currentWidth *= kStraightTaperRatio;
			if (currentWidth < kMinDuctWidth) {
				currentWidth = kMinDuctWidth;
			}
			widths[segmentIndex] = currentWidth;
		}
	}

	void ApplyDefaultStraightChainTapers(AIArtHandle sourceArt,
		const std::vector<DuctworkPoint>& points,
		int startSegmentIndex,
		std::vector<double>& widths)
	{
		std::vector<StraightChainInfo> chains;
		std::vector<int> chainIndexBySegment;
		CollectStraightChainInfos(sourceArt, points, widths, chains, chainIndexBySegment);
		if (widths.empty()) {
			return;
		}

		int clampedStartSegmentIndex = startSegmentIndex;
		if (clampedStartSegmentIndex < 0) {
			clampedStartSegmentIndex = 0;
		}
		if (clampedStartSegmentIndex >= static_cast<int>(widths.size())) {
			clampedStartSegmentIndex = static_cast<int>(widths.size() - 1);
		}

		const std::vector<double> seedWidths = widths;
		for (size_t chainIndex = 0; chainIndex < chains.size(); ++chainIndex) {
			const StraightChainInfo& chain = chains[chainIndex];
			if (chain.startSegmentIndex < 0 || chain.endSegmentIndex <= chain.startSegmentIndex ||
				chain.startSegmentIndex >= static_cast<int>(widths.size())) {
				continue;
			}

			if (clampedStartSegmentIndex < chain.startSegmentIndex) {
				ApplyStraightChainTaperFromAnchor(chain, seedWidths, chain.startSegmentIndex, 1, widths);
				continue;
			}

			if (clampedStartSegmentIndex > chain.endSegmentIndex) {
				ApplyStraightChainTaperFromAnchor(chain, seedWidths, chain.endSegmentIndex, -1, widths);
				continue;
			}

			ApplyStraightChainTaperFromAnchor(chain, seedWidths, clampedStartSegmentIndex, -1, widths);
			ApplyStraightChainTaperFromAnchor(chain, seedWidths, clampedStartSegmentIndex, 1, widths);
		}
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

	bool HasExplicitStartSegmentIndex(AIArtHandle sourceArt)
	{
		double startValue = 0.0;
		return sourceArt && DuctworkMetadata::GetDouble(sourceArt, kEmoryStartSegmentIndexKey, startValue);
	}

	void WriteStartSegmentIndex(AIArtHandle sourceArt, int startSegmentIndex)
	{
		if (!sourceArt || startSegmentIndex < 0) {
			return;
		}
		DuctworkMetadata::SetDouble(sourceArt, kEmoryStartSegmentIndexKey, static_cast<double>(startSegmentIndex));
	}

	void ClearStartSegmentIndex(AIArtHandle sourceArt)
	{
		if (!sourceArt) {
			return;
		}
		DuctworkMetadata::RemoveKey(sourceArt, kEmoryStartSegmentIndexKey);
	}

	bool BuildProcessPathForArt(AIArtHandle art, DuctworkPath& outPath);
	void ClearSelectionInternal();
	void SelectArtListInternal(const std::vector<AIArtHandle>& artList);
	bool CollectEmorySourceStates(std::vector<EmorySourceState>& outStates, std::map<std::string, int>& outIndexBySourceId);
	bool DescribeConnectionForPath(const DuctworkConnection& connection,
		int pathIndex,
		const EmorySourceState& state,
		PathConnectionAttachment& outAttachment);
	void CollectEmoryNetworkConnections(const std::vector<EmorySourceState>& states, std::vector<DuctworkConnection>& outConnections);
	bool IsEndpointToSegmentBranchConnection(const DuctworkConnection& connection, int& outTrunkIndex, int& outBranchIndex);
	void TagGeneratedArt(AIArtHandle art, const std::string& role, const std::string& sourceId,
		double bodyWidth, int segmentIndex, int jointIndex, const std::string& connectorStyle);

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

	bool IsCenterlineHidden(AIArtHandle art)
	{
		if (!art || !sAIArt) {
			return false;
		}
		ai::int32 attr = 0;
		if (sAIArt->GetArtUserAttr(art, kArtHidden, &attr) != kNoErr) {
			return false;
		}
		return (attr & kArtHidden) != 0;
	}

	void SetCenterlineHidden(AIArtHandle art, bool hidden)
	{
		if (!art || !sAIArt) {
			return;
		}
		sAIArt->SetArtUserAttr(art, kArtHidden, hidden ? kArtHidden : 0);
		DuctworkMetadata::SetDouble(art, kEmoryCenterlinesHiddenKey, hidden ? 1.0 : 0.0);
	}

	void CollectGeneratedArtForSourceId(const std::string& sourceId, std::vector<AIArtHandle>& outArt)
	{
		outArt.clear();
		if (sourceId.empty()) {
			return;
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
			if (artSourceId == sourceId) {
				outArt.push_back(art);
			}
		}
	}

	bool GetExistingRunGroupForSource(AIArtHandle sourceArt, const std::string& sourceId, AIArtHandle& outGroup)
	{
		outGroup = nullptr;
		if (!sourceArt || !sAIArt) {
			return false;
		}

		AIArtHandle parent = nullptr;
		if (sAIArt->GetArtParent(sourceArt, &parent) != kNoErr || !parent) {
			return false;
		}

		short parentType = kUnknownArt;
		if (sAIArt->GetArtType(parent, &parentType) != kNoErr || parentType != kGroupArt) {
			return false;
		}

		std::string parentRole;
		std::string parentSourceId;
		if (!DuctworkMetadata::GetString(parent, kEmoryRoleKey, parentRole) ||
			!DuctworkMetadata::GetString(parent, kEmorySourceIdKey, parentSourceId) ||
			parentRole != kEmoryRoleRunGroup ||
			parentSourceId != sourceId) {
			return false;
		}

		outGroup = parent;
		return true;
	}

	bool ResolveSourceOrderingContainer(const EmorySourceState& state, AIArtHandle& outArt, bool& outIsGroup)
	{
		outArt = nullptr;
		outIsGroup = false;
		if (!state.art || state.sourceId.empty()) {
			return false;
		}

		if (GetExistingRunGroupForSource(state.art, state.sourceId, outArt) && outArt) {
			outIsGroup = true;
			return true;
		}

		outArt = state.art;
		return true;
	}

	bool FindLowestArtHandle(const std::vector<AIArtHandle>& arts, AIArtHandle& outLowestArt)
	{
		outLowestArt = nullptr;
		if (!sAIArt || arts.empty()) {
			return false;
		}

		for (size_t i = 0; i < arts.size(); ++i) {
			if (!arts[i]) {
				continue;
			}
			if (!outLowestArt) {
				outLowestArt = arts[i];
				continue;
			}

			short order = kUnknownOrder;
			if (sAIArt->GetArtOrder(arts[i], outLowestArt, &order) != kNoErr) {
				continue;
			}
			if (order == kFirstAfterSecond) {
				outLowestArt = arts[i];
			}
		}

		return outLowestArt != nullptr;
	}

	bool SourceHasGeneratedArt(const std::string& sourceId)
	{
		std::vector<AIArtHandle> generatedArt;
		CollectGeneratedArtForSourceId(sourceId, generatedArt);
		return !generatedArt.empty();
	}

	bool ResolveReferenceArtForSourceId(const EmorySourceState& state, AIArtHandle& outReferenceArt)
	{
		outReferenceArt = state.art;
		std::vector<AIArtHandle> generatedArt;
		CollectGeneratedArtForSourceId(state.sourceId, generatedArt);
		if (!generatedArt.empty()) {
			outReferenceArt = generatedArt.back();
		}
		return outReferenceArt != nullptr;
	}

	bool BuildEndpointToSegmentNetworkConnectorSpec(const DuctworkConnection& connection,
		const std::vector<EmorySourceState>& states,
		NetworkConnectorSpec& outSpec)
	{
		outSpec = NetworkConnectorSpec();

		int trunkIndex = -1;
		int branchIndex = -1;
		if (!IsEndpointToSegmentBranchConnection(connection, trunkIndex, branchIndex) ||
			trunkIndex < 0 || trunkIndex >= static_cast<int>(states.size()) ||
			branchIndex < 0 || branchIndex >= static_cast<int>(states.size())) {
			return false;
		}

		const EmorySourceState& trunkState = states[trunkIndex];
		const EmorySourceState& branchState = states[branchIndex];
		if (trunkState.path.layerName != branchState.path.layerName ||
			trunkState.segmentCount <= 0 ||
			branchState.segmentCount <= 0) {
			return false;
		}

		PathConnectionAttachment trunkAttachment;
		PathConnectionAttachment branchAttachment;
		if (!DescribeConnectionForPath(connection, trunkIndex, trunkState, trunkAttachment) ||
			!DescribeConnectionForPath(connection, branchIndex, branchState, branchAttachment) ||
			trunkAttachment.segmentIndex < 0 ||
			branchAttachment.endpointSlot < 0) {
			return false;
		}

		const int branchSegmentIndex = (branchAttachment.endpointSlot == 0) ? 0 : (branchState.segmentCount - 1);
		if (branchSegmentIndex < 0 ||
			branchSegmentIndex >= static_cast<int>(branchState.path.points.size() - 1) ||
			trunkAttachment.segmentIndex >= static_cast<int>(trunkState.path.points.size() - 1)) {
			return false;
		}

		Vec2 trunkDir;
		Vec2 trunkNormal;
		if (!BuildUnitDirection(trunkState.path.points[trunkAttachment.segmentIndex],
			trunkState.path.points[trunkAttachment.segmentIndex + 1],
			trunkDir, trunkNormal)) {
			return false;
		}

		const DuctworkPoint branchEndpoint = (branchAttachment.endpointSlot == 0)
			? branchState.path.points.front()
			: branchState.path.points.back();
		const DuctworkPoint branchOtherPoint = (branchAttachment.endpointSlot == 0)
			? branchState.path.points[1]
			: branchState.path.points[branchState.path.points.size() - 2];
		Vec2 branchDir = Subtract(branchOtherPoint, branchEndpoint);
		if (!Normalize(branchDir, branchDir)) {
			return false;
		}

		const double trunkWidth = trunkState.widths[trunkAttachment.segmentIndex];
		const double branchWidth = branchState.widths[branchSegmentIndex];
		const double connectorBodyWidth = (std::max)(trunkWidth, branchWidth);
		const double desiredLength = (std::max)(connectorBodyWidth * kNetworkConnectorDesiredLengthMultiplier, 3.0);

		NetworkConnectorArm arm;
		if (!BuildNetworkConnectorArm(trunkState, trunkIndex, trunkAttachment.segmentIndex, connection.point,
			trunkDir,
			std::hypot(trunkState.path.points[trunkAttachment.segmentIndex + 1].x - connection.point.x,
				trunkState.path.points[trunkAttachment.segmentIndex + 1].y - connection.point.y),
			desiredLength, arm)) {
			return false;
		}
		outSpec.arms.push_back(arm);

		if (!BuildNetworkConnectorArm(trunkState, trunkIndex, trunkAttachment.segmentIndex, connection.point,
			Scale(trunkDir, -1.0),
			std::hypot(trunkState.path.points[trunkAttachment.segmentIndex].x - connection.point.x,
				trunkState.path.points[trunkAttachment.segmentIndex].y - connection.point.y),
			desiredLength, arm)) {
			return false;
		}
		outSpec.arms.push_back(arm);

		if (!BuildNetworkConnectorArm(branchState, branchIndex, branchSegmentIndex, connection.point,
			branchDir,
			std::hypot(branchOtherPoint.x - connection.point.x, branchOtherPoint.y - connection.point.y),
			desiredLength, arm)) {
			return false;
		}
		outSpec.arms.push_back(arm);

		outSpec.primarySourceId = trunkState.sourceId;
		outSpec.linkedSourceIds.push_back(trunkState.sourceId);
		outSpec.linkedSourceIds.push_back(branchState.sourceId);
		NormalizeSourceIdList(outSpec.linkedSourceIds);
		outSpec.layerName = trunkState.path.layerName;
		outSpec.style = "tee";
		outSpec.point = connection.point;
		outSpec.bodyWidth = connectorBodyWidth;
		outSpec.strokeWidth = (std::max)(trunkState.sourceStrokeWidth, branchState.sourceStrokeWidth);
		return ResolveReferenceArtForSourceId(trunkState, outSpec.referenceArt);
	}

	bool BuildSegmentIntersectionNetworkConnectorSpec(const DuctworkConnection& connection,
		const std::vector<EmorySourceState>& states,
		NetworkConnectorSpec& outSpec)
	{
		outSpec = NetworkConnectorSpec();
		if (connection.type != kConnectionSegmentIntersection ||
			connection.a < 0 || connection.a >= static_cast<int>(states.size()) ||
			connection.b < 0 || connection.b >= static_cast<int>(states.size())) {
			return false;
		}

		const EmorySourceState& stateA = states[connection.a];
		const EmorySourceState& stateB = states[connection.b];
		if (stateA.path.layerName != stateB.path.layerName ||
			connection.segA < 0 || connection.segA >= stateA.segmentCount ||
			connection.segB < 0 || connection.segB >= stateB.segmentCount) {
			return false;
		}

		Vec2 dirA;
		Vec2 normalA;
		Vec2 dirB;
		Vec2 normalB;
		if (!BuildUnitDirection(stateA.path.points[connection.segA], stateA.path.points[connection.segA + 1], dirA, normalA) ||
			!BuildUnitDirection(stateB.path.points[connection.segB], stateB.path.points[connection.segB + 1], dirB, normalB)) {
			return false;
		}

		if (std::fabs(Dot(dirA, dirB)) >= kCollinearThreshold) {
			return false;
		}

		const double widthA = stateA.widths[connection.segA];
		const double widthB = stateB.widths[connection.segB];
		const double connectorBodyWidth = (std::max)(widthA, widthB);
		const double desiredLength = (std::max)(connectorBodyWidth * kNetworkConnectorDesiredLengthMultiplier, 3.0);

		NetworkConnectorArm arm;
		if (!BuildNetworkConnectorArm(stateA, connection.a, connection.segA, connection.point,
			dirA,
			std::hypot(stateA.path.points[connection.segA + 1].x - connection.point.x,
				stateA.path.points[connection.segA + 1].y - connection.point.y),
			desiredLength, arm)) {
			return false;
		}
		outSpec.arms.push_back(arm);

		if (!BuildNetworkConnectorArm(stateA, connection.a, connection.segA, connection.point,
			Scale(dirA, -1.0),
			std::hypot(stateA.path.points[connection.segA].x - connection.point.x,
				stateA.path.points[connection.segA].y - connection.point.y),
			desiredLength, arm)) {
			return false;
		}
		outSpec.arms.push_back(arm);

		if (!BuildNetworkConnectorArm(stateB, connection.b, connection.segB, connection.point,
			dirB,
			std::hypot(stateB.path.points[connection.segB + 1].x - connection.point.x,
				stateB.path.points[connection.segB + 1].y - connection.point.y),
			desiredLength, arm)) {
			return false;
		}
		outSpec.arms.push_back(arm);

		if (!BuildNetworkConnectorArm(stateB, connection.b, connection.segB, connection.point,
			Scale(dirB, -1.0),
			std::hypot(stateB.path.points[connection.segB].x - connection.point.x,
				stateB.path.points[connection.segB].y - connection.point.y),
			desiredLength, arm)) {
			return false;
		}
		outSpec.arms.push_back(arm);

		const EmorySourceState& primaryState = widthA >= widthB ? stateA : stateB;
		outSpec.primarySourceId = primaryState.sourceId;
		outSpec.linkedSourceIds.push_back(stateA.sourceId);
		outSpec.linkedSourceIds.push_back(stateB.sourceId);
		NormalizeSourceIdList(outSpec.linkedSourceIds);
		outSpec.layerName = primaryState.path.layerName;
		outSpec.style = "cross";
		outSpec.point = connection.point;
		outSpec.bodyWidth = connectorBodyWidth;
		outSpec.strokeWidth = (std::max)(stateA.sourceStrokeWidth, stateB.sourceStrokeWidth);
		return ResolveReferenceArtForSourceId(primaryState, outSpec.referenceArt);
	}

	void GenerateEmoryNetworkConnectors(const std::set<std::string>& affectedSourceIds, EmoryBodyStats& ioStats)
	{
		if (affectedSourceIds.empty()) {
			return;
		}

		std::vector<EmorySourceState> states;
		std::map<std::string, int> indexBySourceId;
		if (!CollectEmorySourceStates(states, indexBySourceId) || states.size() < 2) {
			return;
		}

		std::vector<DuctworkConnection> connections;
		CollectEmoryNetworkConnections(states, connections);
		for (size_t i = 0; i < connections.size(); ++i) {
			const DuctworkConnection& connection = connections[i];
			if (connection.type != kConnectionEndpointToSegment &&
				connection.type != kConnectionSegmentIntersection) {
				continue;
			}

			std::set<std::string> participantSourceIds;
			if (connection.a >= 0 && connection.a < static_cast<int>(states.size())) {
				participantSourceIds.insert(states[connection.a].sourceId);
			}
			if (connection.b >= 0 && connection.b < static_cast<int>(states.size())) {
				participantSourceIds.insert(states[connection.b].sourceId);
			}
			if (participantSourceIds.empty()) {
				continue;
			}

			bool touchesAffected = false;
			bool participantsReady = true;
			for (std::set<std::string>::const_iterator it = participantSourceIds.begin();
				it != participantSourceIds.end(); ++it) {
				if (affectedSourceIds.find(*it) != affectedSourceIds.end()) {
					touchesAffected = true;
				}
				if (!SourceHasGeneratedArt(*it)) {
					participantsReady = false;
				}
			}
			if (!touchesAffected || !participantsReady) {
				continue;
			}

			NetworkConnectorSpec connectorSpec;
			bool built = false;
			if (connection.type == kConnectionEndpointToSegment) {
				built = BuildEndpointToSegmentNetworkConnectorSpec(connection, states, connectorSpec);
			} else if (connection.type == kConnectionSegmentIntersection) {
				built = BuildSegmentIntersectionNetworkConnectorSpec(connection, states, connectorSpec);
			}
			if (!built || !connectorSpec.referenceArt || connectorSpec.arms.size() < 3) {
				continue;
			}

			std::vector<DuctworkPoint> polygon;
			if (!BuildNetworkConnectorPolygon(connectorSpec, polygon)) {
				continue;
			}

			AIArtHandle connectorArt = nullptr;
			if (!CreateClosedPath(connectorSpec.referenceArt, polygon, connectorArt) || !connectorArt) {
				++ioStats.failed;
				continue;
			}
			if (!ApplyFilledPathStyle(connectorArt, GetEmoryColorSpec(connectorSpec.layerName), connectorSpec.strokeWidth)) {
				sAIArt->DisposeArt(connectorArt);
				++ioStats.failed;
				continue;
			}

			TagGeneratedArt(connectorArt, kEmoryRoleConnector, connectorSpec.primarySourceId,
				connectorSpec.bodyWidth, -1, -1, connectorSpec.style);
			WriteLinkedSourceIds(connectorArt, connectorSpec.linkedSourceIds);
			++ioStats.connectorsCreated;
			++ioStats.created;
		}
	}

	bool EnsureRunGroupForSource(AIArtHandle sourceArt, const std::string& sourceId, AIArtHandle& outGroup)
	{
		outGroup = nullptr;
		if (!sourceArt || !sAIArt) {
			return false;
		}

		if (GetExistingRunGroupForSource(sourceArt, sourceId, outGroup) && outGroup) {
			return true;
		}

		AIArtHandle group = nullptr;
		if (sAIArt->NewArt(kGroupArt, kPlaceAbove, sourceArt, &group) != kNoErr || !group) {
			return false;
		}
		DuctworkMetadata::SetString(group, kEmoryRoleKey, kEmoryRoleRunGroup);
		DuctworkMetadata::SetString(group, kEmorySourceIdKey, sourceId);
		UpdateEmoryTokens(group, kEmoryRoleRunGroup, sourceId);

		std::vector<AIArtHandle> generatedArt;
		CollectGeneratedArtForSourceId(sourceId, generatedArt);
		std::vector<AIArtHandle> orderedMembers;
		orderedMembers.push_back(sourceArt);
		for (size_t i = 0; i < generatedArt.size(); ++i) {
			std::string role;
			if (generatedArt[i] &&
				DuctworkMetadata::GetString(generatedArt[i], kEmoryRoleKey, role) &&
				role == kEmoryRoleSegment) {
				orderedMembers.push_back(generatedArt[i]);
			}
		}
		for (size_t i = 0; i < generatedArt.size(); ++i) {
			std::string role;
			if (generatedArt[i] &&
				DuctworkMetadata::GetString(generatedArt[i], kEmoryRoleKey, role) &&
				role == kEmoryRoleConnector) {
				orderedMembers.push_back(generatedArt[i]);
			}
		}
		for (size_t i = 0; i < generatedArt.size(); ++i) {
			if (!generatedArt[i]) {
				continue;
			}
			bool alreadyAdded = false;
			for (size_t memberIndex = 0; memberIndex < orderedMembers.size(); ++memberIndex) {
				if (orderedMembers[memberIndex] == generatedArt[i]) {
					alreadyAdded = true;
					break;
				}
			}
			if (!alreadyAdded) {
				orderedMembers.push_back(generatedArt[i]);
			}
		}

		for (size_t i = 0; i < orderedMembers.size(); ++i) {
			if (orderedMembers[i]) {
				sAIArt->ReorderArt(orderedMembers[i], kPlaceInsideOnTop, group);
			}
		}
		outGroup = group;
		return true;
	}

	bool CollectSelectedEmorySourceIds(std::set<std::string>& outSourceIds)
	{
		outSourceIds.clear();
		std::vector<AIArtHandle> selection;
		DuctworkSelection::CollectSelectedPaths(selection);
		if (selection.empty()) {
			return false;
		}

		for (size_t i = 0; i < selection.size(); ++i) {
			AIArtHandle art = selection[i];
			if (!art) {
				continue;
			}

			std::string sourceId;
			bool matched = false;
			if (IsGeneratedEmoryArtInternal(art)) {
				if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
					sourceId = ReadEmorySourceIdFromNote(art);
				}
				matched = !sourceId.empty();
			} else {
				if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
					sourceId = ReadEmorySourceIdFromNote(art);
				}
				if (!sourceId.empty()) {
					DuctworkPath path;
					matched = BuildProcessPathForArt(art, path) &&
						DuctworkGeometry::IsCenterlineCandidate(path.art, path.points, path.closed, path.layerName);
				}
			}

			if (matched && !sourceId.empty()) {
				outSourceIds.insert(sourceId);
				CollectArtAssociatedSourceIds(art, outSourceIds);
			}
		}

		return !outSourceIds.empty();
	}

	void CollectSelectedThermostatLineArts(std::vector<AIArtHandle>& outThermostatArts)
	{
		outThermostatArts.clear();
		std::vector<AIArtHandle> selection;
		DuctworkSelection::CollectSelectedPaths(selection);
		if (selection.empty()) {
			return;
		}

		for (size_t i = 0; i < selection.size(); ++i) {
			AIArtHandle art = selection[i];
			if (!art || IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			DuctworkPath path;
			if (!BuildProcessPathForArt(art, path) || path.closed || path.points.size() < 2) {
				continue;
			}
			if (path.layerName == "Thermostat Lines") {
				outThermostatArts.push_back(art);
			}
		}

		std::sort(outThermostatArts.begin(), outThermostatArts.end());
		outThermostatArts.erase(std::unique(outThermostatArts.begin(), outThermostatArts.end()), outThermostatArts.end());
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
			ResolveSourceStrokeWidth(art, sourceId, state.defaultWidth, state.sourceStrokeWidth);

			ReadSegmentWidths(art, static_cast<size_t>(segmentCount), state.defaultWidth, state.widths);
			if (!HasStoredSegmentWidths(art)) {
				ApplyDefaultStraightChainTapers(art, points, state.startSegmentIndex, state.widths);
			}
			state.originalWidths = state.widths;
			CollectStraightChainInfos(art, points, state.widths, state.straightChains, state.straightChainIndexBySegment);

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
				const bool isBranchChild = PathIsBranchChild(static_cast<int>(i), connections);
				const bool isBranchRole = IsBranchRole(states[i].ductRole);
				if ((!isBranchChild && !isBranchRole) ||
					anchorByIndex.find(static_cast<int>(i)) != anchorByIndex.end()) {
					continue;
				}

				for (size_t connectionIndex = 0; connectionIndex < connections.size(); ++connectionIndex) {
					const DuctworkConnection& connection = connections[connectionIndex];
					int neighborIndex = -1;
					int trunkIndex = -1;
					int branchIndex = -1;
					if (IsEndpointToSegmentBranchConnection(connection, trunkIndex, branchIndex)) {
						if (branchIndex != static_cast<int>(i) ||
							trunkIndex < 0 || trunkIndex >= static_cast<int>(states.size())) {
							continue;
						}
						neighborIndex = trunkIndex;
					} else if (connection.a == static_cast<int>(i)) {
						neighborIndex = connection.b;
					} else if (connection.b == static_cast<int>(i)) {
						neighborIndex = connection.a;
					}

					if (neighborIndex < 0 || neighborIndex >= static_cast<int>(states.size())) {
						continue;
					}

					const bool neighborIsBranch = PathIsBranchChild(neighborIndex, connections) || IsBranchRole(states[neighborIndex].ductRole);
					AIArtHandle neighborAnchorArt = nullptr;
					bool neighborAnchorIsGroup = false;
					ResolveSourceOrderingContainer(states[neighborIndex], neighborAnchorArt, neighborAnchorIsGroup);
					if (!neighborIsBranch) {
						anchorByIndex[static_cast<int>(i)] = neighborAnchorArt ? neighborAnchorArt : states[neighborIndex].art;
						changed = true;
						break;
					}

					std::map<int, AIArtHandle>::const_iterator neighborAnchor = anchorByIndex.find(neighborIndex);
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
			if (IsNetworkConnectorArt(art)) {
				continue;
			}

			artBySourceId[sourceId].push_back(art);
		}

		for (std::map<std::string, std::vector<AIArtHandle> >::iterator it = artBySourceId.begin(); it != artBySourceId.end(); ++it) {
			AIArtHandle anchorArt = anchorBySourceId[it->first];
			if (!anchorArt) {
				continue;
			}
			std::map<std::string, int>::const_iterator stateIt = stateIndexBySourceId.find(it->first);
			if (stateIt == stateIndexBySourceId.end()) {
				continue;
			}

			AIArtHandle branchContainer = nullptr;
			bool branchUsesGroup = false;
			ResolveSourceOrderingContainer(states[stateIt->second], branchContainer, branchUsesGroup);
			if (branchUsesGroup && branchContainer) {
				sAIArt->ReorderArt(branchContainer, kPlaceBelow, anchorArt);
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
			AIArtHandle sourceArt = states[stateIt->second].art;
			if (sourceArt) {
				AIArtHandle centerlineAnchor = lowestPlacedArt ? lowestPlacedArt : anchorArt;
				if (centerlineAnchor != sourceArt) {
					sAIArt->ReorderArt(sourceArt, kPlaceBelow, centerlineAnchor);
				}
			}
		}
	}

	void PromoteNetworkConnectorsAboveParticipants(const std::vector<std::string>& affectedSourceIds)
	{
		if (!sAIArt || affectedSourceIds.empty()) {
			return;
		}

		std::set<std::string> affected(affectedSourceIds.begin(), affectedSourceIds.end());
		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		if (allPaths.empty()) {
			return;
		}

		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle connectorArt = allPaths[i];
			if (!connectorArt || !IsNetworkConnectorArt(connectorArt)) {
				continue;
			}

			std::set<std::string> connectorSourceIds;
			CollectArtAssociatedSourceIds(connectorArt, connectorSourceIds);
			if (connectorSourceIds.empty()) {
				continue;
			}

			bool touchesAffected = false;
			for (std::set<std::string>::const_iterator it = connectorSourceIds.begin();
				it != connectorSourceIds.end(); ++it) {
				if (affected.find(*it) != affected.end()) {
					touchesAffected = true;
					break;
				}
			}
			if (!touchesAffected) {
				continue;
			}

			std::map<std::string, AIArtHandle> groupedParticipants;
			for (std::set<std::string>::const_iterator it = connectorSourceIds.begin();
				it != connectorSourceIds.end(); ++it) {
				AIArtHandle sourceArt = nullptr;
				DuctworkPath sourcePath;
				AIArtHandle group = nullptr;
				if (FindSourceArtForSourceId(*it, sourceArt, sourcePath) &&
					sourceArt &&
					GetExistingRunGroupForSource(sourceArt, *it, group) &&
					group) {
					groupedParticipants[*it] = group;
				}
			}

			for (size_t artIndex = 0; artIndex < allPaths.size(); ++artIndex) {
				AIArtHandle art = allPaths[artIndex];
				if (!art || art == connectorArt || IsNetworkConnectorArt(art)) {
					continue;
				}

				std::set<std::string> artSourceIds;
				CollectArtAssociatedSourceIds(art, artSourceIds);
				bool sharesSource = false;
				bool belongsToGroupedParticipant = false;
				for (std::set<std::string>::const_iterator it = artSourceIds.begin();
					it != artSourceIds.end(); ++it) {
					if (connectorSourceIds.find(*it) != connectorSourceIds.end()) {
						sharesSource = true;
						if (groupedParticipants.find(*it) != groupedParticipants.end()) {
							belongsToGroupedParticipant = true;
						}
					}
				}
				if (!sharesSource || belongsToGroupedParticipant) {
					continue;
				}

				sAIArt->ReorderArt(art, kPlaceBelow, connectorArt);
			}

			for (std::map<std::string, AIArtHandle>::const_iterator it = groupedParticipants.begin();
				it != groupedParticipants.end(); ++it) {
				if (it->second) {
					sAIArt->ReorderArt(it->second, kPlaceBelow, connectorArt);
				}
			}
		}
	}

	void EnsureCenterlinesBehindAssociatedGeneratedArt(const std::vector<std::string>& affectedSourceIds)
	{
		if (!sAIArt || affectedSourceIds.empty()) {
			return;
		}

		std::set<std::string> affected(affectedSourceIds.begin(), affectedSourceIds.end());
		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		if (allPaths.empty()) {
			return;
		}

		std::map<std::string, std::vector<AIArtHandle> > associatedGeneratedBySource;
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || !IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			std::set<std::string> associatedIds;
			CollectArtAssociatedSourceIds(art, associatedIds);
			for (std::set<std::string>::const_iterator it = associatedIds.begin(); it != associatedIds.end(); ++it) {
				if (affected.find(*it) != affected.end()) {
					associatedGeneratedBySource[*it].push_back(art);
				}
			}
		}

		for (std::set<std::string>::const_iterator it = affected.begin(); it != affected.end(); ++it) {
			AIArtHandle sourceArt = nullptr;
			DuctworkPath sourcePath;
			if (!FindSourceArtForSourceId(*it, sourceArt, sourcePath) || !sourceArt) {
				continue;
			}

			AIArtHandle group = nullptr;
			if (GetExistingRunGroupForSource(sourceArt, *it, group) && group) {
				continue;
			}

			std::map<std::string, std::vector<AIArtHandle> >::const_iterator generatedIt = associatedGeneratedBySource.find(*it);
			if (generatedIt == associatedGeneratedBySource.end() || generatedIt->second.empty()) {
				continue;
			}

			AIArtHandle lowestGeneratedArt = nullptr;
			if (!FindLowestArtHandle(generatedIt->second, lowestGeneratedArt) || !lowestGeneratedArt || lowestGeneratedArt == sourceArt) {
				continue;
			}

			sAIArt->ReorderArt(sourceArt, kPlaceBelow, lowestGeneratedArt);
		}
	}

	void ApplyFinalEmoryOrdering(const std::vector<std::string>& affectedSourceIds)
	{
		if (affectedSourceIds.empty()) {
			return;
		}

		ReorderGeneratedBranchArtBehindParents(affectedSourceIds);
		PromoteNetworkConnectorsAboveParticipants(affectedSourceIds);
		EnsureCenterlinesBehindAssociatedGeneratedArt(affectedSourceIds);
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

		DuctworkPoint incomingLeftPoint;
		DuctworkPoint incomingRightPoint;
		DuctworkPoint outgoingLeftPoint;
		DuctworkPoint outgoingRightPoint;
		ResolveConnectorSidePoints(connector.prevTrimPoint, incomingDir, connector.prevWidth,
			connector.prevChainHorizontal, connector.prevChainVertical,
			connector.prevTaperAlignment, connector.prevTaperReferenceWidth,
			incomingLeftPoint, incomingRightPoint);
		ResolveConnectorSidePoints(connector.nextTrimPoint, outgoingDir, connector.nextWidth,
			connector.nextChainHorizontal, connector.nextChainVertical,
			connector.nextTaperAlignment, connector.nextTaperReferenceWidth,
			outgoingLeftPoint, outgoingRightPoint);

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

	Vec2 startAxis = Scale(connector.prevDir, -1.0);
	Vec2 endAxis = connector.nextDir;
	if (!Normalize(startAxis, startAxis) || !Normalize(endAxis, endAxis)) {
		return false;
	}

	const double theta = std::acos((std::max)(-1.0, (std::min)(1.0, Dot(startAxis, endAxis))));
	if (!std::isfinite(theta) || theta <= 1e-3 || theta >= (3.141592653589793 - 1e-3)) {
		return false;
	}

	const DuctworkPoint startCenterline = connector.prevTrimPoint;
	const DuctworkPoint endCenterline = connector.nextTrimPoint;
	const double chordLength = std::hypot(endCenterline.x - startCenterline.x, endCenterline.y - startCenterline.y);
	if (chordLength <= 1e-6) {
		return false;
	}

	double handleFactor = (4.0 / 3.0) * std::tan(theta * 0.25);
	if (!std::isfinite(handleFactor) || handleFactor <= 0.0) {
		handleFactor = 0.35;
	}
	const double maxHandle = chordLength * 0.5;
	double startHandle = connector.prevTrimDistance * handleFactor;
	double endHandle = connector.nextTrimDistance * handleFactor;
	if (startHandle > maxHandle) {
		startHandle = maxHandle;
	}
	if (endHandle > maxHandle) {
		endHandle = maxHandle;
	}
	if (startHandle <= 1e-6 || endHandle <= 1e-6) {
		return false;
	}

	const DuctworkPoint control1 = Add(startCenterline, startAxis, startHandle);
	const DuctworkPoint control2 = Add(endCenterline, endAxis, -endHandle);

	int subdivisions = static_cast<int>(std::ceil(std::fabs(theta) / (3.141592653589793 / 12.0)));
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
		const DuctworkPoint point = EvaluateCubicBezier(startCenterline, control1, control2, endCenterline, t);
		Vec2 tangent = EvaluateCubicBezierTangent(startCenterline, control1, control2, endCenterline, t);
		if (!Normalize(tangent, tangent)) {
			continue;
		}
		Vec2 width_vec = PerpCCW(tangent);
		if (!Normalize(width_vec, width_vec)) {
			continue;
		}

		const double width_current = connector.prevWidth + ((connector.nextWidth - connector.prevWidth) * t);
		const double half_width = width_current * 0.5;
		if (!orientationSet) {
			const DuctworkPoint test_outer = Add(point, width_vec, half_width);
			const DuctworkPoint test_inner = Add(point, width_vec, -half_width);
			const double dist_outer = std::hypot(test_outer.x - pairing.startOuter.x, test_outer.y - pairing.startOuter.y);
			const double dist_inner = std::hypot(test_inner.x - pairing.startOuter.x, test_inner.y - pairing.startOuter.y);
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

	outerPoints.front() = pairing.startOuter;
	outerPoints.back() = pairing.endOuter;
	innerPoints.front() = pairing.startInner;
	innerPoints.back() = pairing.endInner;

	for (size_t i = 0; i < outerPoints.size(); ++i) {
		AppendIfDistinct(outPolygon, outerPoints[i]);
	}
	for (size_t i = innerPoints.size(); i > 0; --i) {
		AppendIfDistinct(outPolygon, innerPoints[i - 1]);
	}
	return outPolygon.size() >= 4;
}

	bool BuildRoundCornerBezierSegments(const ConnectorSpec& connector, const CornerPairing& pairing,
		std::vector<AIPathSegment>& outSegments)
	{
		outSegments.clear();

		Vec2 startAxis = Scale(connector.prevDir, -1.0);
		Vec2 endAxis = connector.nextDir;
		if (!Normalize(startAxis, startAxis) || !Normalize(endAxis, endAxis)) {
			return false;
		}

		const double theta = std::acos((std::max)(-1.0, (std::min)(1.0, Dot(startAxis, endAxis))));
		if (!std::isfinite(theta) || theta <= 1e-3 || theta >= (3.141592653589793 - 1e-3)) {
			return false;
		}

		double handleFactor = (4.0 / 3.0) * std::tan(theta * 0.25);
		if (!std::isfinite(handleFactor) || handleFactor <= 0.0) {
			return false;
		}

		double shallowT = 0.0;
		if (theta < kRoundBezierShallowAngleStart) {
			const double denom = kRoundBezierShallowAngleStart - kRoundBezierShallowAngleEnd;
			shallowT = (kRoundBezierShallowAngleStart - theta) / denom;
			if (shallowT < 0.0) {
				shallowT = 0.0;
			} else if (shallowT > 1.0) {
				shallowT = 1.0;
			}
		}

		const double maxSegmentWidth = (std::max)((std::max)(connector.prevWidth, connector.nextWidth), kMinDuctWidth);
		const double avgTrimRatio = ((connector.prevTrimDistance + connector.nextTrimDistance) * 0.5) / maxSegmentWidth;
		double longConnectorT = 0.0;
		if (avgTrimRatio > kTrimMultiplier) {
			const double denom = 3.0 - kTrimMultiplier;
			longConnectorT = (avgTrimRatio - kTrimMultiplier) / denom;
			if (longConnectorT < 0.0) {
				longConnectorT = 0.0;
			} else if (longConnectorT > 1.0) {
				longConnectorT = 1.0;
			}
		}

		const double longHandleStrength = (std::max)(shallowT, longConnectorT);
		double handleReductionScale = kRoundBezierStandardHandleReduction;
		if (longHandleStrength > 0.0) {
			double preserveT = longHandleStrength / kRoundBezierLongHandlePreserveThreshold;
			if (preserveT > 1.0) {
				preserveT = 1.0;
			}
			handleReductionScale += (1.0 - kRoundBezierStandardHandleReduction) * preserveT;
		}

		const double connectorHandleScale =
			(1.0 + ((kRoundBezierMaxLongHandleScale - 1.0) * longHandleStrength)) * handleReductionScale;

		const double outerChord = std::hypot(pairing.endOuter.x - pairing.startOuter.x, pairing.endOuter.y - pairing.startOuter.y);
		const double innerChord = std::hypot(pairing.startInner.x - pairing.endInner.x, pairing.startInner.y - pairing.endInner.y);
		if (outerChord <= 1e-6 || innerChord <= 1e-6) {
			return false;
		}

		double outerStartHandle = connector.prevTrimDistance * handleFactor * connectorHandleScale * kRoundBezierOuterHandleMultiplier;
		double outerEndHandle = connector.nextTrimDistance * handleFactor * connectorHandleScale * kRoundBezierOuterHandleMultiplier;
		double innerStartHandle = connector.prevTrimDistance * handleFactor * connectorHandleScale;
		double innerEndHandle = connector.nextTrimDistance * handleFactor * connectorHandleScale;

		if (theta >= kRoundBezierOuterReduceMinAngle && theta <= kRoundBezierOuterReduceMaxAngle) {
			outerStartHandle *= kRoundBezierOuterTargetAngleReduction;
			outerEndHandle *= kRoundBezierOuterTargetAngleReduction;
		}
		if (theta >= kRoundBezierInnerReduceMinAngle && theta <= kRoundBezierInnerReduceMaxAngle) {
			innerStartHandle *= kRoundBezierInnerTargetAngleReduction;
			innerEndHandle *= kRoundBezierInnerTargetAngleReduction;
		}

		const double outerMaxHandle = outerChord * kRoundBezierOuterChordCapMultiplier;
		const double innerMaxHandle = innerChord * kRoundBezierInnerChordCapMultiplier;
		if (outerStartHandle > outerMaxHandle) {
			outerStartHandle = outerMaxHandle;
		}
		if (outerEndHandle > outerMaxHandle) {
			outerEndHandle = outerMaxHandle;
		}
		if (innerStartHandle > innerMaxHandle) {
			innerStartHandle = innerMaxHandle;
		}
		if (innerEndHandle > innerMaxHandle) {
			innerEndHandle = innerMaxHandle;
		}
		if (outerStartHandle <= 1e-6 || outerEndHandle <= 1e-6 || innerStartHandle <= 1e-6 || innerEndHandle <= 1e-6) {
			return false;
		}

		outSegments.resize(4);

		outSegments[0].p.h = static_cast<AIReal>(pairing.startOuter.x);
		outSegments[0].p.v = static_cast<AIReal>(pairing.startOuter.y);
		DuctworkPoint outerStartOut = Add(pairing.startOuter, startAxis, outerStartHandle);
		outSegments[0].out.h = static_cast<AIReal>(outerStartOut.x);
		outSegments[0].out.v = static_cast<AIReal>(outerStartOut.y);
		outSegments[0].in = outSegments[0].p;
		outSegments[0].corner = true;

		outSegments[1].p.h = static_cast<AIReal>(pairing.endOuter.x);
		outSegments[1].p.v = static_cast<AIReal>(pairing.endOuter.y);
		DuctworkPoint outerEndIn = Add(pairing.endOuter, endAxis, -outerEndHandle);
		outSegments[1].in.h = static_cast<AIReal>(outerEndIn.x);
		outSegments[1].in.v = static_cast<AIReal>(outerEndIn.y);
		outSegments[1].out = outSegments[1].p;
		outSegments[1].corner = true;

		outSegments[2].p.h = static_cast<AIReal>(pairing.endInner.x);
		outSegments[2].p.v = static_cast<AIReal>(pairing.endInner.y);
		outSegments[2].in = outSegments[2].p;
		DuctworkPoint innerStartOut = Add(pairing.endInner, endAxis, -innerEndHandle);
		outSegments[2].out.h = static_cast<AIReal>(innerStartOut.x);
		outSegments[2].out.v = static_cast<AIReal>(innerStartOut.y);
		outSegments[2].corner = true;

		outSegments[3].p.h = static_cast<AIReal>(pairing.startInner.x);
		outSegments[3].p.v = static_cast<AIReal>(pairing.startInner.y);
		DuctworkPoint innerEndIn = Add(pairing.startInner, startAxis, innerStartHandle);
		outSegments[3].in.h = static_cast<AIReal>(innerEndIn.x);
		outSegments[3].in.v = static_cast<AIReal>(innerEndIn.y);
		outSegments[3].out = outSegments[3].p;
		outSegments[3].corner = true;

		const double largerWidth = (std::max)(connector.prevWidth, connector.nextWidth);
		const double smallerWidth = (std::min)(connector.prevWidth, connector.nextWidth);
		if (largerWidth - smallerWidth > 1e-6 && largerWidth > 1e-6) {
			double convergeT = ((largerWidth - smallerWidth) / largerWidth) * kRoundBezierLargeEndHandleConvergeMax;
			if (convergeT > kRoundBezierLargeEndHandleConvergeMax) {
				convergeT = kRoundBezierLargeEndHandleConvergeMax;
			}

			if (connector.prevWidth > connector.nextWidth) {
				const DuctworkPoint startMid = Lerp(pairing.startOuter, pairing.startInner, 0.5);
				const DuctworkPoint outerTarget = Add(startMid, startAxis, outerStartHandle);
				const DuctworkPoint innerTarget = Add(startMid, startAxis, innerStartHandle);
				outerStartOut = Lerp(outerStartOut, outerTarget, convergeT);
				innerEndIn = Lerp(innerEndIn, innerTarget, convergeT);
				outSegments[0].out.h = static_cast<AIReal>(outerStartOut.x);
				outSegments[0].out.v = static_cast<AIReal>(outerStartOut.y);
				outSegments[3].in.h = static_cast<AIReal>(innerEndIn.x);
				outSegments[3].in.v = static_cast<AIReal>(innerEndIn.y);
			} else if (connector.nextWidth > connector.prevWidth) {
				const DuctworkPoint endMid = Lerp(pairing.endOuter, pairing.endInner, 0.5);
				const DuctworkPoint outerTarget = Add(endMid, endAxis, -outerEndHandle);
				const DuctworkPoint innerTarget = Add(endMid, endAxis, -innerEndHandle);
				outerEndIn = Lerp(outerEndIn, outerTarget, convergeT);
				innerStartOut = Lerp(innerStartOut, innerTarget, convergeT);
				outSegments[1].in.h = static_cast<AIReal>(outerEndIn.x);
				outSegments[1].in.v = static_cast<AIReal>(outerEndIn.y);
				outSegments[2].out.h = static_cast<AIReal>(innerStartOut.x);
				outSegments[2].out.v = static_cast<AIReal>(innerStartOut.y);
			}
		}

		return true;
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
		DuctworkMetadata::RemoveKey(art, kEmoryLinkedSourceIdsKey);
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
		const bool omitStartSegmentThickness = ReadFinalSegmentThicknessFlag(path.art, kEmoryOmitStartSegmentThicknessKey);
		const bool omitEndSegmentThickness = ReadFinalSegmentThicknessFlag(path.art, kEmoryOmitEndSegmentThicknessKey);
		if (!HasStoredSegmentWidths(path.art)) {
			const int startSegmentIndex = ReadStartSegmentIndex(path.art, segmentCount);
			ApplyDefaultStraightChainTapers(path.art, points, startSegmentIndex, segmentWidths);
			WriteSegmentWidths(path.art, segmentWidths);
		}
		std::vector<StraightChainInfo> straightChains;
		std::vector<int> straightChainIndexBySegment;
		CollectStraightChainInfos(path.art, points, segmentWidths, straightChains, straightChainIndexBySegment);
		std::vector<double> trimAtStart(segmentCount, 0.0);
		std::vector<double> trimAtEnd(segmentCount, 0.0);
		std::vector<ConnectorSpec> connectors;

		for (size_t jointIndex = 1; jointIndex + 1 < points.size(); ++jointIndex) {
			const size_t prevJointSegmentIndex = jointIndex - 1;
			const size_t nextJointSegmentIndex = jointIndex;
			if (IsSegmentThicknessOmitted(prevJointSegmentIndex, segmentCount, omitStartSegmentThickness, omitEndSegmentThickness) ||
				IsSegmentThicknessOmitted(nextJointSegmentIndex, segmentCount, omitStartSegmentThickness, omitEndSegmentThickness)) {
				continue;
			}

			Vec2 prevDir;
			Vec2 prevNormal;
			Vec2 nextDir;
			Vec2 nextNormal;
			if (!BuildUnitDirection(points[jointIndex], points[jointIndex - 1], prevDir, prevNormal) ||
				!BuildUnitDirection(points[jointIndex], points[jointIndex + 1], nextDir, nextNormal)) {
				continue;
			}

			const double dirDot = Dot(prevDir, nextDir);
			const bool isStraightContinuation = dirDot <= -kCollinearThreshold;
			if (!isStraightContinuation && std::fabs(dirDot) >= kCollinearThreshold) {
				continue;
			}

			const std::string connectorStyle = ReadCornerStyle(path.art, static_cast<int>(jointIndex));
			const double turnAngle = std::acos((std::max)(-1.0, (std::min)(1.0, dirDot)));
			const double prevSegmentWidth = segmentWidths[jointIndex - 1];
			const double nextSegmentWidth = segmentWidths[jointIndex];
			const double jointBodyWidth = (std::max)(prevSegmentWidth, nextSegmentWidth);

			if (isStraightContinuation) {
				if (std::fabs(prevSegmentWidth - nextSegmentWidth) <= 1e-6) {
					continue;
				}

				const double prevLength = std::hypot(points[jointIndex - 1].x - points[jointIndex].x,
					points[jointIndex - 1].y - points[jointIndex].y);
				const double nextLength = std::hypot(points[jointIndex + 1].x - points[jointIndex].x,
					points[jointIndex + 1].y - points[jointIndex].y);
				double halfTransitionLength = (std::max)(jointBodyWidth * kStraightTaperConnectorMinHalfLengthMultiplier,
					std::fabs(prevSegmentWidth - nextSegmentWidth) * kStraightTaperConnectorDiffHalfLengthMultiplier);
				const double prevMaxAllowed = (prevLength * 0.5) - 0.1;
				const double nextMaxAllowed = (nextLength * 0.5) - 0.1;
				if (prevMaxAllowed <= 0.1 || nextMaxAllowed <= 0.1) {
					continue;
				}
				if (halfTransitionLength > prevMaxAllowed) {
					halfTransitionLength = prevMaxAllowed;
				}
				if (halfTransitionLength > nextMaxAllowed) {
					halfTransitionLength = nextMaxAllowed;
				}
				if (halfTransitionLength <= 0.1) {
					continue;
				}

				trimAtEnd[jointIndex - 1] = (std::max)(trimAtEnd[jointIndex - 1], halfTransitionLength);
				trimAtStart[jointIndex] = (std::max)(trimAtStart[jointIndex], halfTransitionLength);

				ConnectorSpec connector;
				connector.sourceArt = path.art;
				connector.sourceId = sourceId;
				connector.layerName = path.layerName;
				connector.jointIndex = static_cast<int>(jointIndex);
				connector.joint = points[jointIndex];
				connector.prevTrimPoint = Add(points[jointIndex], prevDir, halfTransitionLength);
				connector.nextTrimPoint = Add(points[jointIndex], nextDir, halfTransitionLength);
				connector.prevDir = prevDir;
				connector.nextDir = nextDir;
				connector.prevWidth = prevSegmentWidth;
				connector.nextWidth = nextSegmentWidth;
				connector.prevTrimDistance = halfTransitionLength;
				connector.nextTrimDistance = halfTransitionLength;
				connector.trimDistance = halfTransitionLength;
				connector.style = "taper";
				connector.isStraightTaper = true;

				const int prevSegmentIndex = static_cast<int>(jointIndex - 1);
				if (prevSegmentIndex >= 0 && prevSegmentIndex < static_cast<int>(straightChainIndexBySegment.size())) {
					const int chainIndex = straightChainIndexBySegment[prevSegmentIndex];
					if (chainIndex >= 0 && chainIndex < static_cast<int>(straightChains.size())) {
						const StraightChainInfo& chain = straightChains[chainIndex];
						connector.chainHorizontal = chain.horizontal;
						connector.chainVertical = chain.vertical;
						connector.taperAlignment = chain.alignment;
						connector.taperReferenceWidth = chain.referenceWidth;
					}
				}
				if (connector.taperReferenceWidth < jointBodyWidth) {
					connector.taperReferenceWidth = jointBodyWidth;
				}
				if (connector.taperAlignment.empty()) {
					connector.taperAlignment = "center";
				}
				connectors.push_back(connector);
				continue;
			}

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
			double prevTrimDistance = trimDistance;
			double nextTrimDistance = trimDistance;
			if (jointBodyWidth > 0.0) {
				prevTrimDistance *= prevSegmentWidth / jointBodyWidth;
				nextTrimDistance *= nextSegmentWidth / jointBodyWidth;
			}

			const double prevMaxAllowed = (prevLength * 0.5) - 0.1;
			const double nextMaxAllowed = (nextLength * 0.5) - 0.1;
			if (prevMaxAllowed <= 0.1 || nextMaxAllowed <= 0.1) {
				continue;
			}
			if (prevTrimDistance > prevMaxAllowed) {
				prevTrimDistance = prevMaxAllowed;
			}
			if (nextTrimDistance > nextMaxAllowed) {
				nextTrimDistance = nextMaxAllowed;
			}
			if (prevTrimDistance <= 0.1 || nextTrimDistance <= 0.1) {
				continue;
			}

			trimAtEnd[jointIndex - 1] = (std::max)(trimAtEnd[jointIndex - 1], prevTrimDistance);
			trimAtStart[jointIndex] = (std::max)(trimAtStart[jointIndex], nextTrimDistance);

			ConnectorSpec connector;
			connector.sourceArt = path.art;
			connector.sourceId = sourceId;
			connector.layerName = path.layerName;
			connector.jointIndex = static_cast<int>(jointIndex);
			connector.joint = points[jointIndex];
			connector.prevTrimPoint = Add(points[jointIndex], prevDir, prevTrimDistance);
			connector.nextTrimPoint = Add(points[jointIndex], nextDir, nextTrimDistance);
			connector.prevDir = prevDir;
			connector.nextDir = nextDir;
			connector.prevWidth = prevSegmentWidth;
			connector.nextWidth = nextSegmentWidth;
			connector.prevTrimDistance = prevTrimDistance;
			connector.nextTrimDistance = nextTrimDistance;
			connector.trimDistance = (std::min)(prevTrimDistance, nextTrimDistance);
			connector.style = connectorStyle;
			connector.prevTaperAlignment = "center";
			connector.prevTaperReferenceWidth = prevSegmentWidth;
			const int prevSegmentIndex = static_cast<int>(jointIndex - 1);
			if (prevSegmentIndex >= 0 && prevSegmentIndex < static_cast<int>(straightChainIndexBySegment.size())) {
				const int prevChainIndex = straightChainIndexBySegment[prevSegmentIndex];
				if (prevChainIndex >= 0 && prevChainIndex < static_cast<int>(straightChains.size())) {
					const StraightChainInfo& prevChain = straightChains[prevChainIndex];
					connector.prevChainHorizontal = prevChain.horizontal;
					connector.prevChainVertical = prevChain.vertical;
					connector.prevTaperAlignment = prevChain.alignment;
					connector.prevTaperReferenceWidth = (std::max)(prevChain.referenceWidth, prevSegmentWidth);
				}
			}
			connector.nextTaperAlignment = "center";
			connector.nextTaperReferenceWidth = nextSegmentWidth;
			const int nextSegmentIndex = static_cast<int>(jointIndex);
			if (nextSegmentIndex >= 0 && nextSegmentIndex < static_cast<int>(straightChainIndexBySegment.size())) {
				const int nextChainIndex = straightChainIndexBySegment[nextSegmentIndex];
				if (nextChainIndex >= 0 && nextChainIndex < static_cast<int>(straightChains.size())) {
					const StraightChainInfo& nextChain = straightChains[nextChainIndex];
					connector.nextChainHorizontal = nextChain.horizontal;
					connector.nextChainVertical = nextChain.vertical;
					connector.nextTaperAlignment = nextChain.alignment;
					connector.nextTaperReferenceWidth = (std::max)(nextChain.referenceWidth, nextSegmentWidth);
				}
			}
			connectors.push_back(connector);
		}

		ResolveSegmentTrimRequests(points, trimAtStart, trimAtEnd);
		ApplyResolvedConnectorTrims(connectors, trimAtStart, trimAtEnd);

		AIArtHandle referenceArt = path.art;
		for (size_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
			if (IsSegmentThicknessOmitted(segmentIndex, segmentCount, omitStartSegmentThickness, omitEndSegmentThickness)) {
				continue;
			}

			const DuctworkPoint start = points[segmentIndex];
			const DuctworkPoint end = points[segmentIndex + 1];
			Vec2 dir;
			Vec2 normal;
			if (!BuildUnitDirection(start, end, dir, normal)) {
				continue;
			}
			const double segmentBodyWidth = segmentWidths[segmentIndex];
			std::string taperAlignment = "center";
			bool chainHorizontal = false;
			bool chainVertical = false;
			double taperReferenceWidth = segmentBodyWidth;
			if (segmentIndex < straightChainIndexBySegment.size()) {
				const int chainIndex = straightChainIndexBySegment[segmentIndex];
				if (chainIndex >= 0 && chainIndex < static_cast<int>(straightChains.size())) {
					const StraightChainInfo& chain = straightChains[chainIndex];
					taperAlignment = chain.alignment;
					chainHorizontal = chain.horizontal;
					chainVertical = chain.vertical;
					taperReferenceWidth = chain.referenceWidth;
				}
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
			if (!BuildAlignedBandPolygon(trimmedStart, trimmedEnd, segmentBodyWidth, taperReferenceWidth,
				taperAlignment, chainHorizontal, chainVertical, polygon)) {
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
			AIArtHandle connectorArt = nullptr;
			bool createdConnector = false;
			if (connectors[i].isStraightTaper) {
				std::vector<DuctworkPoint> polygon;
				if (BuildStraightTaperConnectorPolygon(connectors[i], polygon) &&
					CreateClosedPath(referenceArt, polygon, connectorArt) &&
					connectorArt) {
					createdConnector = true;
				}
			} else if (connectors[i].style == "round") {
				CornerPairing pairing;
				std::vector<AIPathSegment> segments;
				if (BuildCornerPairing(connectors[i], pairing) &&
					BuildRoundCornerBezierSegments(connectors[i], pairing, segments) &&
					CreateClosedPathSegments(referenceArt, segments, connectorArt) &&
					connectorArt) {
					createdConnector = true;
				}
			}
			if (!createdConnector) {
				std::vector<DuctworkPoint> polygon;
				if (!BuildConnectorPolygon(connectors[i], polygon)) {
					++stats.failed;
					continue;
				}
				if (!CreateClosedPath(referenceArt, polygon, connectorArt) || !connectorArt) {
					++stats.failed;
					continue;
				}
				createdConnector = true;
			}
			if (!createdConnector || !connectorArt) {
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

void DuctworkGeometry::UpdateSelectedEmoryFinalSegmentThicknessMetadata(const std::vector<DuctworkPath>& paths, bool enabled)
{
	if (!sAIArt || paths.empty()) {
		return;
	}

	std::vector<DuctworkPoint> registerAttachmentPoints;
	CollectRegisterAttachmentPoints(registerAttachmentPoints);

	std::vector<AIArtHandle> allLineArt;
	CollectAllLineLayerPaths(allLineArt);
	std::vector<DuctworkPath> allPaths;
	allPaths.reserve(allLineArt.size());
	for (size_t i = 0; i < allLineArt.size(); ++i) {
		DuctworkPath entry;
		if (!BuildProcessPathForArt(allLineArt[i], entry) ||
			!IsCenterlineCandidate(entry.art, entry.points, entry.closed, entry.layerName)) {
			continue;
		}
		allPaths.push_back(entry);
	}

	std::vector<DuctworkConnection> allConnections;
	DuctworkConnections::FindConnections(
		allPaths,
		2.0,
		3.0,
		15.0,
		10.0,
		true,
		allConnections);

	for (size_t i = 0; i < paths.size(); ++i) {
		const DuctworkPath& path = paths[i];
		if (!path.art || path.points.size() < 2 ||
			!DuctworkLayers::IsColorLayerName(path.layerName) ||
			!IsCenterlineCandidate(path.art, path.points, path.closed, path.layerName)) {
			continue;
		}

		bool omitStart = false;
		bool omitEnd = false;
		if (enabled && !registerAttachmentPoints.empty()) {
			int allPathIndex = -1;
			for (size_t pathIndex = 0; pathIndex < allPaths.size(); ++pathIndex) {
				if (allPaths[pathIndex].art == path.art) {
					allPathIndex = static_cast<int>(pathIndex);
					break;
				}
			}

			const int startEndpointIndex = 0;
			const int endEndpointIndex = static_cast<int>(path.points.size() - 1);
			const bool startConnected = (allPathIndex >= 0) ?
				EndpointHasAnyCenterlineConnection(allConnections, allPathIndex, startEndpointIndex) : false;
			const bool endConnected = (allPathIndex >= 0) ?
				EndpointHasAnyCenterlineConnection(allConnections, allPathIndex, endEndpointIndex) : false;

			omitStart = !startConnected && IsPointNearAny(path.points.front(), registerAttachmentPoints, 10.0);
			omitEnd = !endConnected && IsPointNearAny(path.points.back(), registerAttachmentPoints, 10.0);
		}

		WriteFinalSegmentThicknessMetadata(path.art, omitStart, omitEnd);
	}
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
		std::set<std::string> candidateSourceIds;
		CollectArtAssociatedSourceIds(art, candidateSourceIds);
		if (candidateSourceIds.empty()) {
			continue;
		}

		bool matchedSource = false;
		for (std::set<std::string>::const_iterator sourceIt = candidateSourceIds.begin();
			sourceIt != candidateSourceIds.end(); ++sourceIt) {
			if (ids.find(*sourceIt) != ids.end()) {
				matchedSource = true;
				break;
			}
		}
		if (!matchedSource) {
			continue;
		}

		double strokeWidth = 0.0;
		if ((GetMaxStyleStrokeWidth(art, strokeWidth) || GetSimpleStrokeWidth(art, strokeWidth)) && strokeWidth > 0.0) {
			for (std::set<std::string>::const_iterator sourceIt = candidateSourceIds.begin();
				sourceIt != candidateSourceIds.end(); ++sourceIt) {
				std::map<std::string, double>::iterator existing = strokeWidthBySourceId.find(*sourceIt);
				if (existing == strokeWidthBySourceId.end() || strokeWidth > existing->second) {
					strokeWidthBySourceId[*sourceIt] = strokeWidth;
				}
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
		GenerateEmoryNetworkConnectors(std::set<std::string>(affectedSourceIds.begin(), affectedSourceIds.end()), stats);
		ApplyFinalEmoryOrdering(affectedSourceIds);
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

	std::vector<AIArtHandle> selectedThermostatArts;
	CollectSelectedThermostatLineArts(selectedThermostatArts);
	std::set<std::string> strokeSourceIds;
	CollectSelectedEmorySourceIds(strokeSourceIds);

	double referenceStrokeWidth = 0.0;
	bool referenceStrokeWidthSet = false;
	bool mixedStrokeWidths = false;
	for (std::set<std::string>::const_iterator it = strokeSourceIds.begin(); it != strokeSourceIds.end(); ++it) {
		AIArtHandle sourceArt = nullptr;
		DuctworkPath sourcePath;
		if (!FindSourceArtForSourceId(*it, sourceArt, sourcePath) || !sourceArt) {
			continue;
		}

		double bodyWidth = 0.0;
		if (!ResolveSourceBodyWidth(sourceArt, *it, bodyWidth) || bodyWidth <= 0.0) {
			bodyWidth = kDefaultDuctWidth;
		}
		if (bodyWidth < kMinDuctWidth) {
			bodyWidth = kMinDuctWidth;
		}

		double strokeWidth = 0.0;
		if (!ResolveSourceStrokeWidth(sourceArt, *it, bodyWidth, strokeWidth) || strokeWidth <= 0.0) {
			continue;
		}

		if (!referenceStrokeWidthSet) {
			referenceStrokeWidth = strokeWidth;
			referenceStrokeWidthSet = true;
		} else if (std::fabs(strokeWidth - referenceStrokeWidth) > 1e-6) {
			mixedStrokeWidths = true;
		}
	}

	for (size_t i = 0; i < selectedThermostatArts.size(); ++i) {
		double strokeWidth = 0.0;
		if (!GetEffectiveStrokeWidth(selectedThermostatArts[i], strokeWidth) || strokeWidth <= 0.0) {
			continue;
		}

		if (!referenceStrokeWidthSet) {
			referenceStrokeWidth = strokeWidth;
			referenceStrokeWidthSet = true;
		} else if (std::fabs(strokeWidth - referenceStrokeWidth) > 1e-6) {
			mixedStrokeWidths = true;
		}
	}

	if (selectedBySource.empty()) {
		if (!selectedThermostatArts.empty() || !strokeSourceIds.empty()) {
			std::ostringstream out;
			out << "{\"ok\":true,\"available\":false,\"reason\":\"no-segment-selection\""
				<< ",\"runCount\":" << strokeSourceIds.size()
				<< ",\"selectedCount\":0"
				<< ",\"selectedThermostatCount\":" << selectedThermostatArts.size()
				<< ",\"mixedStrokeWidths\":" << (mixedStrokeWidths ? "true" : "false")
				<< ",\"referenceStrokeWidth\":" << (referenceStrokeWidthSet ? referenceStrokeWidth : 1.0)
				<< ",\"canApplyStroke\":" << (referenceStrokeWidthSet ? "true" : "false")
				<< ",\"canApplyWidth\":false"
				<< ",\"hasExplicitStart\":false"
				<< ",\"canClearStart\":false"
				<< ",\"centerlinesHidden\":false"
				<< "}";
			outJson = out.str();
			return true;
		}
		outJson = "{\"ok\":true,\"available\":false,\"reason\":\"no-segment-selection\",\"canApplyStroke\":false}";
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
			<< ",\"mixedStrokeWidths\":" << (mixedStrokeWidths ? "true" : "false")
			<< ",\"hasExplicitStart\":false"
			<< ",\"canClearStart\":false"
			<< ",\"centerlinesHidden\":false"
			<< ",\"selectedThermostatCount\":" << selectedThermostatArts.size()
			<< ",\"selectedSegmentIndex\":-1"
			<< ",\"selectedWidth\":0"
			<< ",\"referenceWidth\":" << (firstWidthSet ? firstWidth : kDefaultDuctWidth)
			<< ",\"referenceStrokeWidth\":" << (referenceStrokeWidthSet ? referenceStrokeWidth : 1.0)
			<< ",\"canApplyStroke\":" << (referenceStrokeWidthSet ? "true" : "false")
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
	const bool hasExplicitStart = HasExplicitStartSegmentIndex(sourceArt);
	if (!HasStoredSegmentWidths(sourceArt)) {
		ApplyDefaultStraightChainTapers(sourceArt, points, startSegmentIndex, segmentWidths);
	}
	std::vector<StraightChainInfo> straightChains;
	std::vector<int> straightChainIndexBySegment;
	CollectStraightChainInfos(sourceArt, points, segmentWidths, straightChains, straightChainIndexBySegment);
	bool taperAlignmentAvailable = false;
	std::string taperAlignment = "center";
	std::string taperOrientation;

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
		<< ",\"selectedThermostatCount\":" << selectedThermostatArts.size()
		<< ",\"segmentCount\":" << segmentCount
		<< ",\"startSegmentIndex\":" << startSegmentIndex
		<< ",\"hasExplicitStart\":" << (hasExplicitStart ? "true" : "false")
		<< ",\"centerlinesHidden\":" << (IsCenterlineHidden(sourceArt) ? "true" : "false")
		<< ",\"mixedWidths\":" << (mixedWidths ? "true" : "false")
		<< ",\"mixedStrokeWidths\":" << (mixedStrokeWidths ? "true" : "false")
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

	if (!selectedSegmentIndices.empty()) {
		int firstChainIndex = -1;
		bool sameChain = true;
		for (size_t i = 0; i < selectedSegmentIndices.size(); ++i) {
			const int segmentIndex = selectedSegmentIndices[i];
			int chainIndex = -1;
			if (segmentIndex >= 0 && segmentIndex < static_cast<int>(straightChainIndexBySegment.size())) {
				chainIndex = straightChainIndexBySegment[segmentIndex];
			}
			if (i == 0) {
				firstChainIndex = chainIndex;
			} else if (chainIndex != firstChainIndex) {
				sameChain = false;
				break;
			}
		}

		if (sameChain && firstChainIndex >= 0 && firstChainIndex < static_cast<int>(straightChains.size())) {
			const StraightChainInfo& chain = straightChains[firstChainIndex];
			if (chain.horizontal || chain.vertical) {
				taperAlignmentAvailable = true;
				taperAlignment = chain.alignment;
				taperOrientation = chain.horizontal ? "horizontal" : "vertical";
			}
		}
	}

	out << ",\"taperAlignmentAvailable\":" << (taperAlignmentAvailable ? "true" : "false");
	if (taperAlignmentAvailable) {
		out << ",\"taperAlignment\":\"" << JsonEscape(taperAlignment) << "\""
			<< ",\"taperOrientation\":\"" << JsonEscape(taperOrientation) << "\"";
	}

	if (selectedSegmentIndices.size() == 1) {
		const int selectedIndex = selectedSegmentIndices[0];
		const int cascadeDirection = DetermineCascadeDirection(segmentCount, startSegmentIndex, selectedIndex);
		out << ",\"selectedSegmentIndex\":" << selectedIndex
			<< ",\"selectedWidth\":" << segmentWidths[selectedIndex]
			<< ",\"referenceWidth\":" << segmentWidths[selectedIndex]
			<< ",\"referenceStrokeWidth\":" << (referenceStrokeWidthSet ? referenceStrokeWidth : 1.0)
			<< ",\"isStartSegment\":" << (selectedIndex == startSegmentIndex ? "true" : "false")
			<< ",\"canClearStart\":" << ((hasExplicitStart && selectedIndex == startSegmentIndex) ? "true" : "false")
			<< ",\"cascadeDirection\":\"" << (cascadeDirection == 0 ? "both" : (cascadeDirection < 0 ? "lower" : "higher")) << "\""
			<< ",\"canApplyStroke\":" << (referenceStrokeWidthSet ? "true" : "false")
			<< ",\"canApplyWidth\":true";
	} else {
		out << ",\"selectedSegmentIndex\":-1"
			<< ",\"selectedWidth\":0"
			<< ",\"referenceWidth\":" << (firstWidthSet ? firstWidth : defaultWidth)
			<< ",\"referenceStrokeWidth\":" << (referenceStrokeWidthSet ? referenceStrokeWidth : 1.0)
			<< ",\"isStartSegment\":false"
			<< ",\"canClearStart\":false"
			<< ",\"cascadeDirection\":\"multiple\""
			<< ",\"canApplyStroke\":" << (referenceStrokeWidthSet ? "true" : "false")
			<< ",\"canApplyWidth\":true";
		if (!mixedWidths && firstWidthSet) {
			out << ",\"sharedWidth\":" << firstWidth;
		}
	}

	out << "}";
	outJson = out.str();
	return true;
}

bool DuctworkGeometry::RevertSelectedEmoryToCenterlines(std::string& outMessage)
{
	outMessage = "Select generated Emory ductwork or its source centerline first.";
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

	std::set<std::string> sourceIdSet;
	std::vector<AIArtHandle> sourceSelection;
	size_t generatedSelectionCount = 0;

	for (size_t i = 0; i < selection.size(); ++i) {
		AIArtHandle art = selection[i];
		if (!art) {
			continue;
		}

		std::string sourceId;
		bool matched = false;

		if (IsGeneratedEmoryArtInternal(art)) {
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
				sourceId = ReadEmorySourceIdFromNote(art);
			}
			matched = !sourceId.empty();
			if (matched) {
				++generatedSelectionCount;
			}
		} else {
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
				sourceId = ReadEmorySourceIdFromNote(art);
			}
			if (!sourceId.empty()) {
				DuctworkPath path;
				matched = BuildProcessPathForArt(art, path) &&
					IsCenterlineCandidate(path.art, path.points, path.closed, path.layerName);
			}
		}

		if (!matched || sourceId.empty() || !sourceIdSet.insert(sourceId).second) {
			continue;
		}

		AIArtHandle sourceArt = nullptr;
		DuctworkPath sourcePath;
		if (FindSourceArtForSourceId(sourceId, sourceArt, sourcePath) && sourceArt) {
			sourceSelection.push_back(sourceArt);
		}
	}

	if (sourceIdSet.empty()) {
		return false;
	}

	std::vector<std::string> sourceIds(sourceIdSet.begin(), sourceIdSet.end());
	const size_t removed = DeleteGeneratedEmoryBodies(sourceIds);

	for (size_t i = 0; i < sourceSelection.size(); ++i) {
		SetCenterlineHidden(sourceSelection[i], false);
	}
	for (size_t i = 0; i < sourceSelection.size(); ++i) {
		UngroupAncestorGroupsRecursive(sourceSelection[i]);
	}
	if (!sourceSelection.empty()) {
		ClearSelectionInternal();
		SelectArtListInternal(sourceSelection);
	}

	std::ostringstream message;
	message << "Reverted " << sourceIds.size() << " Emory run";
	if (sourceIds.size() != 1) {
		message << "s";
	}
	message << " to centerlines. Removed " << removed << " generated piece";
	if (removed != 1) {
		message << "s";
	}
	if (generatedSelectionCount == 0) {
		message << ".";
	} else {
		message << " from " << generatedSelectionCount << " selected generated item";
		if (generatedSelectionCount != 1) {
			message << "s";
		}
		message << ".";
	}
	outMessage = message.str();
	return true;
}

bool DuctworkGeometry::SetSelectedEmoryTaperAlignment(const std::string& alignment, std::string& outMessage)
{
	outMessage = "Select one or more Emory duct segments from a horizontal or vertical taper chain first.";
	if (!sAIArt) {
		outMessage = "Illustrator SDK is not available.";
		return false;
	}

	const bool horizontalAlignment = (alignment == "top" || alignment == "center" || alignment == "bottom");
	const bool verticalAlignment = (alignment == "left" || alignment == "center" || alignment == "right");
	if (!horizontalAlignment && !verticalAlignment) {
		outMessage = "Unsupported taper alignment.";
		return false;
	}

	NormalizeDuplicateEmorySourceIds();

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

	std::vector<std::string> sourceIds;
	std::vector<DuctworkPath> regeneratePaths;
	size_t chainCount = 0;

	for (std::map<std::string, std::vector<int> >::iterator it = selectedBySource.begin(); it != selectedBySource.end(); ++it) {
		std::sort(it->second.begin(), it->second.end());
		it->second.erase(std::unique(it->second.begin(), it->second.end()), it->second.end());

		AIArtHandle sourceArt = nullptr;
		DuctworkPath sourcePath;
		if (!FindSourceArtForSourceId(it->first, sourceArt, sourcePath)) {
			outMessage = "Unable to find the source centerline for the selected taper chain.";
			return false;
		}

		std::vector<DuctworkPoint> points;
		SanitizePolyline(sourcePath.points, points);
		if (points.size() < 2) {
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
		ReadSegmentWidths(sourceArt, points.size() - 1, defaultWidth, widths);
		if (!HasStoredSegmentWidths(sourceArt)) {
			const int startSegmentIndex = ReadStartSegmentIndex(sourceArt, points.size() - 1);
			ApplyDefaultStraightChainTapers(sourceArt, points, startSegmentIndex, widths);
			WriteSegmentWidths(sourceArt, widths);
		}

		std::vector<StraightChainInfo> straightChains;
		std::vector<int> straightChainIndexBySegment;
		CollectStraightChainInfos(sourceArt, points, widths, straightChains, straightChainIndexBySegment);

		std::set<int> touchedChains;
		for (size_t i = 0; i < it->second.size(); ++i) {
			const int segmentIndex = it->second[i];
			if (segmentIndex < 0 || segmentIndex >= static_cast<int>(straightChainIndexBySegment.size())) {
				continue;
			}
			const int chainIndex = straightChainIndexBySegment[segmentIndex];
			if (chainIndex >= 0 && chainIndex < static_cast<int>(straightChains.size())) {
				touchedChains.insert(chainIndex);
			}
		}

		if (touchedChains.empty()) {
			outMessage = "The selected segment is not part of a taper chain.";
			return false;
		}

		for (std::set<int>::const_iterator chainIt = touchedChains.begin(); chainIt != touchedChains.end(); ++chainIt) {
			const StraightChainInfo& chain = straightChains[*chainIt];
			if (chain.horizontal) {
				if (!horizontalAlignment) {
					outMessage = "Use Top, Center, or Bottom for horizontal taper chains.";
					return false;
				}
			} else if (chain.vertical) {
				if (!verticalAlignment) {
					outMessage = "Use Left, Center, or Right for vertical taper chains.";
					return false;
				}
			} else {
				outMessage = "Alignment only applies to horizontal or vertical taper chains.";
				return false;
			}
			WriteTaperAlignment(sourceArt, chain.startSegmentIndex, chain.endSegmentIndex, alignment);
			++chainCount;
		}

		sourceIds.push_back(it->first);
		regeneratePaths.push_back(sourcePath);
	}

	if (sourceIds.empty() || regeneratePaths.empty()) {
		return false;
	}

	std::sort(sourceIds.begin(), sourceIds.end());
	sourceIds.erase(std::unique(sourceIds.begin(), sourceIds.end()), sourceIds.end());
	DeleteGeneratedEmoryBodies(sourceIds);
	EmoryBodyStats stats = GenerateEmoryBodies(regeneratePaths);
	SelectGeneratedSegmentsBySourceMap(selectedBySource);

	std::ostringstream message;
	message << "Updated taper alignment on " << chainCount << " chain";
	if (chainCount != 1) {
		message << "s";
	}
	message << ". Rebuilt " << stats.segmentsCreated << " segments and " << stats.connectorsCreated << " connectors.";
	outMessage = message.str();
	return true;
}

bool DuctworkGeometry::SetSelectedEmoryCenterlineVisibility(bool hidden, std::string& outMessage)
{
	outMessage = "Select Emory ductwork or its source centerline first.";
	if (!sAIArt) {
		outMessage = "Illustrator SDK is not available.";
		return false;
	}

	NormalizeDuplicateEmorySourceIds();

	std::set<std::string> sourceIds;
	if (!CollectSelectedEmorySourceIds(sourceIds)) {
		return false;
	}

	std::vector<AIArtHandle> sourceArts;
	for (std::set<std::string>::const_iterator it = sourceIds.begin(); it != sourceIds.end(); ++it) {
		AIArtHandle sourceArt = nullptr;
		DuctworkPath sourcePath;
		if (!FindSourceArtForSourceId(*it, sourceArt, sourcePath) || !sourceArt) {
			continue;
		}
		sourceArts.push_back(sourceArt);
	}

	if (sourceArts.empty()) {
		return false;
	}

	for (size_t i = 0; i < sourceArts.size(); ++i) {
		std::string sourceId;
		if (!EnsureEmorySourceId(sourceArts[i], sourceId) || sourceId.empty()) {
			continue;
		}
		if (hidden) {
			AIArtHandle group = nullptr;
			EnsureRunGroupForSource(sourceArts[i], sourceId, group);
		}
		SetCenterlineHidden(sourceArts[i], hidden);
	}

	std::vector<std::string> affectedSourceIds(sourceIds.begin(), sourceIds.end());
	ApplyFinalEmoryOrdering(affectedSourceIds);

	std::ostringstream message;
	message << (hidden ? "Hid " : "Showed ")
		<< sourceArts.size() << " centerline";
	if (sourceArts.size() != 1) {
		message << "s";
	}
	message << ".";
	outMessage = message.str();
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

	double defaultWidth = 0.0;
	if (!ResolveSourceBodyWidth(sourceArt, sourceId, defaultWidth) || defaultWidth <= 0.0) {
		defaultWidth = kDefaultDuctWidth;
	}
	if (defaultWidth < kMinDuctWidth) {
		defaultWidth = kMinDuctWidth;
	}

	std::vector<double> widths;
	ReadSegmentWidths(sourceArt, segmentCount, defaultWidth, widths);
	std::vector<double> retaperedWidths = widths;
	ApplyDefaultStraightChainTapers(sourceArt, points, selectedSegmentIndex, retaperedWidths);
	bool widthsChanged = false;
	if (retaperedWidths != widths) {
		widths = retaperedWidths;
		WriteSegmentWidths(sourceArt, widths);
		widthsChanged = true;
	}

	WriteStartSegmentIndex(sourceArt, selectedSegmentIndex);
	if (widthsChanged && selectedSegmentIndex >= 0 && selectedSegmentIndex < static_cast<int>(widths.size())) {
		WriteStoredSourceBodyWidth(sourceArt, widths[selectedSegmentIndex]);
		std::vector<std::string> sourceIds(1, sourceId);
		std::vector<DuctworkPath> regeneratePaths(1, sourcePath);
		DeleteGeneratedEmoryBodies(sourceIds);
		GenerateEmoryBodies(regeneratePaths);
	}
	SelectGeneratedSegmentBySourceIdAndIndex(sourceId, selectedSegmentIndex);

	std::ostringstream message;
	message << "Cascade start set to segment " << (selectedSegmentIndex + 1)
		<< " of " << segmentCount << ".";
	if (widthsChanged) {
		message << " Auto taper now flows from that start.";
	}
	outMessage = message.str();
	return true;
}

bool DuctworkGeometry::ClearSelectedEmoryStartSegment(std::string& outMessage)
{
	outMessage = "Select the marked Emory start segment first.";
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
			outMessage = "Select only one Emory duct segment to clear its start mark.";
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

	const int currentStartSegmentIndex = ReadStartSegmentIndex(sourceArt, segmentCount);
	if (!HasExplicitStartSegmentIndex(sourceArt) || selectedSegmentIndex != currentStartSegmentIndex) {
		outMessage = "The selected segment is not the marked start.";
		return false;
	}

	double defaultWidth = 0.0;
	if (!ResolveSourceBodyWidth(sourceArt, sourceId, defaultWidth) || defaultWidth <= 0.0) {
		defaultWidth = kDefaultDuctWidth;
	}
	if (defaultWidth < kMinDuctWidth) {
		defaultWidth = kMinDuctWidth;
	}

	std::vector<double> widths;
	ReadSegmentWidths(sourceArt, segmentCount, defaultWidth, widths);
	std::vector<double> retaperedWidths = widths;
	const int defaultStartSegmentIndex = 0;
	ApplyDefaultStraightChainTapers(sourceArt, points, defaultStartSegmentIndex, retaperedWidths);
	const bool widthsChanged = (retaperedWidths != widths);
	if (widthsChanged) {
		widths = retaperedWidths;
		WriteSegmentWidths(sourceArt, widths);
	}

	ClearStartSegmentIndex(sourceArt);
	if (widthsChanged && !widths.empty()) {
		WriteStoredSourceBodyWidth(sourceArt, widths[defaultStartSegmentIndex]);
		std::vector<std::string> sourceIds(1, sourceId);
		std::vector<DuctworkPath> regeneratePaths(1, sourcePath);
		DeleteGeneratedEmoryBodies(sourceIds);
		GenerateEmoryBodies(regeneratePaths);
	}

	SelectGeneratedSegmentBySourceIdAndIndex(sourceId, selectedSegmentIndex);

	std::ostringstream message;
	message << "Cleared the marked start. This run now defaults to segment 1 of " << segmentCount << ".";
	if (widthsChanged) {
		message << " Auto taper now flows from that default start.";
	}
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

bool DuctworkGeometry::ApplySelectedEmoryStrokeWidth(double newWidth, std::string& outMessage)
{
	outMessage = "Select Emory ductwork or thermostat lines first.";
	if (!sAIArt) {
		outMessage = "Illustrator SDK is not available.";
		return false;
	}

	NormalizeDuplicateEmorySourceIds();

	if (!std::isfinite(newWidth) || newWidth <= 0.0) {
		outMessage = "Stroke width must be greater than zero.";
		return false;
	}
	if (newWidth < 0.25) {
		newWidth = 0.25;
	}

	std::set<std::string> sourceIds;
	CollectSelectedEmorySourceIds(sourceIds);

	std::vector<AIArtHandle> selectedThermostatArts;
	CollectSelectedThermostatLineArts(selectedThermostatArts);

	if (sourceIds.empty() && selectedThermostatArts.empty()) {
		return false;
	}

	size_t updatedRuns = 0;
	if (!sourceIds.empty()) {
		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (std::set<std::string>::const_iterator it = sourceIds.begin(); it != sourceIds.end(); ++it) {
			AIArtHandle sourceArt = nullptr;
			DuctworkPath sourcePath;
			if (FindSourceArtForSourceId(*it, sourceArt, sourcePath) && sourceArt) {
				WriteStoredSourceStrokeWidth(sourceArt, newWidth);
				++updatedRuns;
			}
		}

		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || !IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			std::string artSourceId;
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, artSourceId) || artSourceId.empty()) {
				artSourceId = ReadEmorySourceIdFromNote(art);
			}
			if (artSourceId.empty() || sourceIds.find(artSourceId) == sourceIds.end()) {
				continue;
			}

			SetSimpleStrokeWidth(art, newWidth, true);
		}
	}

	size_t updatedThermostatLines = 0;
	for (size_t i = 0; i < selectedThermostatArts.size(); ++i) {
		if (SetSimpleStrokeWidth(selectedThermostatArts[i], newWidth, false)) {
			++updatedThermostatLines;
		}
	}

	if (updatedRuns == 0 && updatedThermostatLines == 0) {
		outMessage = "No selected Emory stroke widths were updated.";
		return false;
	}

	std::ostringstream message;
	message << "Updated stroke width to " << newWidth << " on ";
	bool wrotePart = false;
	if (updatedRuns > 0) {
		message << updatedRuns << " Emory run";
		if (updatedRuns != 1) {
			message << "s";
		}
		wrotePart = true;
	}
	if (updatedThermostatLines > 0) {
		if (wrotePart) {
			message << " and ";
		}
		message << updatedThermostatLines << " thermostat line";
		if (updatedThermostatLines != 1) {
			message << "s";
		}
	}
	message << ".";
	outMessage = message.str();
	return true;
}
