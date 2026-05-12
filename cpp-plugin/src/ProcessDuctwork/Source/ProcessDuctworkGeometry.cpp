#include "IllustratorSDK.h"
#include "ProcessDuctworkArt.h"
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
	const double kDefaultBranchInheritedWidthRatio = 0.75;
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
	const double kRevertGapMaxLength = 40.0;
	const double kRevertGapCollinearToleranceDeg = 7.0;
	const char* const kEmoryRoleKey = "MDUX_EmoryRole";
	const char* const kEmorySourceIdKey = "MDUX_EmorySourceId";
	const char* const kEmoryLinkedSourceIdsKey = "MDUX_EmoryLinkedSourceIds";
	const char* const kEmoryBodyWidthKey = "MDUX_EmoryBodyWidth";
	const char* const kEmorySourceBodyWidthKey = "MDUX_EmorySourceBodyWidth";
	const char* const kEmorySourceStrokeWidthKey = "MDUX_EmorySourceStrokeWidth";
	const char* const kEmorySourceStrokeExplicitKey = "MDUX_EmorySourceStrokeExplicit";
	const char* const kEmoryOriginalPathPointsKey = "MDUX_EmoryOriginalPathPoints";
	const char* const kEmorySegmentWidthsKey = "MDUX_EmorySegmentWidths";
	const char* const kEmoryStartSegmentIndexKey = "MDUX_EmoryStartSegmentIndex";
	const char* const kEmoryTaperAlignmentsKey = "MDUX_EmoryTaperAlignments";
	const char* const kEmoryCenterlinesHiddenKey = "MDUX_EmoryCenterlinesHidden";
	const char* const kEmoryCenterlineTailGuideKey = "MDUX_EmoryCenterlineTailGuide";
	const char* const kEmoryCenterlineTailGuideStartKey = "MDUX_EmoryCenterlineTailGuideStart";
	const char* const kEmoryCenterlineTailGuideEndKey = "MDUX_EmoryCenterlineTailGuideEnd";
	const char* const kEmoryCenterlineTailGuideCreatedKey = "MDUX_EmoryCenterlineTailGuideCreated";
	const char* const kEmoryBackupCenterlineKey = "MDUX_EmoryBackupCenterline";
	const char* const kEmoryOmitStartSegmentThicknessKey = "MDUX_EmoryOmitStartSegmentThickness";
	const char* const kEmoryOmitEndSegmentThicknessKey = "MDUX_EmoryOmitEndSegmentThickness";
	const char* const kEmoryOmittedSegmentIndicesKey = "MDUX_EmoryOmittedSegmentIndices";
	const char* const kEmorySegmentIndexKey = "MDUX_EmorySegmentIndex";
	const char* const kEmoryJointIndexKey = "MDUX_EmoryJointIndex";
	const char* const kEmoryConnectorStyleKey = "MDUX_EmoryConnectorStyle";
	const char* const kEmoryCornerStylesKey = "MDUX_EmoryCornerStyles";
	const char* const kEmoryTerminalStartStyleKey = "MDUX_EmoryTerminalStartStyle";
	const char* const kEmoryTerminalEndStyleKey = "MDUX_EmoryTerminalEndStyle";
	// Stored metadata value for generated Emory ductwork body art. Keep the literal stable for existing documents.
	const char* const kEmoryRoleSegment = "segment";
	const char* const kEmoryRoleConnector = "connector";
	// Stored metadata value for generated Emory wire art: the visible line left when a final has no ductwork body.
	const char* const kEmoryRoleGuide = "guide";
	const char* const kEmoryRoleCenterline = "centerline";
	const char* const kEmoryRoleRunGroup = "run-group";
	const char* const kEmoryGeneratedToken = "MD:EMORY_GENERATED";
	const char* const kEmoryCenterlineToken = "MD:EMORY_CENTERLINE";
	const char* const kEmoryBodyToken = "MD:EMORY_BODY";
	const char* const kEmorySourceIdPrefix = "MD:EMORY_SOURCE_ID=";

	void ReadOmittedSegmentIndices(AIArtHandle art, std::set<size_t>& outIndices);

	double gBranchInheritedWidthRatio = kDefaultBranchInheritedWidthRatio;
	std::set<std::string> gWidthApplyProtectedSourceIds;

	struct ScopedWidthApplyProtectedSourceIds
	{
		std::set<std::string> previous;

		ScopedWidthApplyProtectedSourceIds(const std::set<std::string>& sourceIds)
			: previous(gWidthApplyProtectedSourceIds)
		{
			gWidthApplyProtectedSourceIds = sourceIds;
		}

		~ScopedWidthApplyProtectedSourceIds()
		{
			gWidthApplyProtectedSourceIds = previous;
		}
	};

	double ClampBranchInheritedWidthRatio(double ratio)
	{
		if (!std::isfinite(ratio)) {
			return kDefaultBranchInheritedWidthRatio;
		}
		if (ratio < 0.05) {
			return 0.05;
		}
		if (ratio > 1.0) {
			return 1.0;
		}
		return ratio;
	}

	std::string DescribeEmoryGeneratedArtRole(const std::string& role)
	{
		if (role == kEmoryRoleSegment) {
			return "ductwork-body";
		}
		if (role == kEmoryRoleGuide) {
			return "wire";
		}
		if (role == kEmoryRoleConnector) {
			return "end-connector";
		}
		if (role == kEmoryRoleCenterline) {
			return "centerline";
		}
		return role;
	}

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
		bool hasExplicitStart = false;
		double defaultWidth = 0.0;
		double sourceStrokeWidth = 0.0;
		std::vector<double> originalWidths;
		std::vector<double> widths;
		std::vector<StraightChainInfo> straightChains;
		std::vector<int> straightChainIndexBySegment;
		bool hasStoredSegmentWidths = false;
		bool touched = false;
		bool selectedSeed = false;
	};

	void MarkWidthApplyProtectedSources(std::vector<EmorySourceState>& states)
	{
		if (gWidthApplyProtectedSourceIds.empty()) {
			return;
		}

		size_t protectedCount = 0;
		for (size_t i = 0; i < states.size(); ++i) {
			if (!states[i].sourceId.empty() &&
				gWidthApplyProtectedSourceIds.find(states[i].sourceId) != gWidthApplyProtectedSourceIds.end()) {
				states[i].selectedSeed = true;
				++protectedCount;
			}
		}

		if (protectedCount > 0) {
			DuctworkLog::WriteAlways("Emory width-apply protected selected source rebuild count=" + std::to_string(protectedCount));
		}
	}

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

	struct RevertGapEndpoint
	{
		size_t candidateIndex = 0;
		size_t pointIndex = 0;
		DuctworkPoint point{};
		DuctworkPoint neighbor{};
		std::string layerName;
	};

	struct RevertGapMergeCandidate
	{
		bool valid = false;
		size_t firstCandidateIndex = 0;
		size_t secondCandidateIndex = 0;
		bool reverseFirst = false;
		bool reverseSecond = false;
		double gapLength = 0.0;
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
	void CollectAllLineLayerPaths(std::vector<AIArtHandle>& outPaths);
	bool IsGeneratedEmoryArtInternal(AIArtHandle art);
	std::string ReadEmorySourceIdFromNote(AIArtHandle art);
	std::string ReadTerminalSegmentStyle(AIArtHandle sourceArt, bool atStart);
	void SanitizePolyline(const std::vector<DuctworkPoint>& input, std::vector<DuctworkPoint>& output);
	void SimplifyOpenPathCollinearPoints(std::vector<DuctworkPoint>& points);
	void UpdateEmoryTokens(AIArtHandle art, const std::string& role, const std::string& sourceId);
	void SetCenterlineHidden(AIArtHandle art, bool hidden);
	bool ComputeArtCenterPoint(AIArtHandle art, DuctworkPoint& outCenter);

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

		spec.fill = MakeRGBColor(126, 254, 130);
		spec.stroke = MakeRGBColor(0, 183, 19);
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
		return role == kEmoryRoleSegment || role == kEmoryRoleConnector || role == kEmoryRoleGuide;
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

		// Ignore/Ignored markers should override register attachment detection.
		// This keeps a formerly terminal register endpoint from staying "final"
		// after the user converts that register/endpoint to an ignore point.
		std::vector<DuctworkPoint> ignoredPoints;
		static const char* kIgnoreLayers[] = { "Ignore", "Ignored", "ignore", "ignored" };
		for (size_t layerIndex = 0; layerIndex < (sizeof(kIgnoreLayers) / sizeof(kIgnoreLayers[0])); ++layerIndex) {
			AILayerHandle layer = nullptr;
			const ai::UnicodeString layerName = ai::UnicodeString::FromUTF8(kIgnoreLayers[layerIndex]);
			if (sAILayer->GetLayerByTitle(&layer, layerName) != kNoErr || !layer) {
				continue;
			}

			AIArtHandle layerGroup = nullptr;
			if (sAIArt->GetFirstArtOfLayer(layer, &layerGroup) != kNoErr || !layerGroup) {
				continue;
			}
			CollectRegisterAttachmentPointsFromArt(layerGroup, ignoredPoints);
		}

		if (!ignoredPoints.empty() && !outPoints.empty()) {
			std::vector<DuctworkPoint> filteredPoints;
			filteredPoints.reserve(outPoints.size());
			const double tolerance = 10.0;
			const double toleranceSq = tolerance * tolerance;
			for (size_t pointIndex = 0; pointIndex < outPoints.size(); ++pointIndex) {
				bool ignored = false;
				for (size_t ignoredIndex = 0; ignoredIndex < ignoredPoints.size(); ++ignoredIndex) {
					const double dx = outPoints[pointIndex].x - ignoredPoints[ignoredIndex].x;
					const double dy = outPoints[pointIndex].y - ignoredPoints[ignoredIndex].y;
					if ((dx * dx + dy * dy) <= toleranceSq) {
						ignored = true;
						break;
					}
				}
				if (!ignored) {
					filteredPoints.push_back(outPoints[pointIndex]);
				}
			}
			outPoints.swap(filteredPoints);
		}
	}

	void CollectIgnoreAnchorPoints(std::vector<DuctworkPoint>& outPoints);

	void CollectUnitAttachmentPoints(std::vector<DuctworkPoint>& outPoints)
	{
		outPoints.clear();
		if (!sAILayer || !sAIArt) {
			return;
		}

		AILayerHandle layer = nullptr;
		const ai::UnicodeString layerName = ai::UnicodeString::FromUTF8("Units");
		if (sAILayer->GetLayerByTitle(&layer, layerName) != kNoErr || !layer) {
			return;
		}

		AIArtHandle layerGroup = nullptr;
		if (sAIArt->GetFirstArtOfLayer(layer, &layerGroup) != kNoErr || !layerGroup) {
			return;
		}
		CollectRegisterAttachmentPointsFromArt(layerGroup, outPoints);

		std::vector<DuctworkPoint> ignoredPoints;
		CollectIgnoreAnchorPoints(ignoredPoints);
		if (!ignoredPoints.empty() && !outPoints.empty()) {
			std::vector<DuctworkPoint> filteredPoints;
			filteredPoints.reserve(outPoints.size());
			const double tolerance = 10.0;
			const double toleranceSq = tolerance * tolerance;
			for (size_t pointIndex = 0; pointIndex < outPoints.size(); ++pointIndex) {
				bool ignored = false;
				for (size_t ignoredIndex = 0; ignoredIndex < ignoredPoints.size(); ++ignoredIndex) {
					const double dx = outPoints[pointIndex].x - ignoredPoints[ignoredIndex].x;
					const double dy = outPoints[pointIndex].y - ignoredPoints[ignoredIndex].y;
					if ((dx * dx + dy * dy) <= toleranceSq) {
						ignored = true;
						break;
					}
				}
				if (!ignored) {
					filteredPoints.push_back(outPoints[pointIndex]);
				}
			}
			outPoints.swap(filteredPoints);
		}
	}

	void CollectLayerAttachmentPoints(const char* layerTitle, std::vector<DuctworkPoint>& outPoints)
	{
		outPoints.clear();
		if (!layerTitle || !sAILayer || !sAIArt) {
			return;
		}

		AILayerHandle layer = nullptr;
		const ai::UnicodeString layerName = ai::UnicodeString::FromUTF8(layerTitle);
		if (sAILayer->GetLayerByTitle(&layer, layerName) != kNoErr || !layer) {
			return;
		}

		AIArtHandle layerGroup = nullptr;
		if (sAIArt->GetFirstArtOfLayer(layer, &layerGroup) != kNoErr || !layerGroup) {
			return;
		}
		CollectRegisterAttachmentPointsFromArt(layerGroup, outPoints);

		std::vector<DuctworkPoint> ignoredPoints;
		CollectIgnoreAnchorPoints(ignoredPoints);
		if (!ignoredPoints.empty() && !outPoints.empty()) {
			std::vector<DuctworkPoint> filteredPoints;
			filteredPoints.reserve(outPoints.size());
			const double tolerance = 10.0;
			const double toleranceSq = tolerance * tolerance;
			for (size_t pointIndex = 0; pointIndex < outPoints.size(); ++pointIndex) {
				bool ignored = false;
				for (size_t ignoredIndex = 0; ignoredIndex < ignoredPoints.size(); ++ignoredIndex) {
					const double dx = outPoints[pointIndex].x - ignoredPoints[ignoredIndex].x;
					const double dy = outPoints[pointIndex].y - ignoredPoints[ignoredIndex].y;
					if ((dx * dx + dy * dy) <= toleranceSq) {
						ignored = true;
						break;
					}
				}
				if (!ignored) {
					filteredPoints.push_back(outPoints[pointIndex]);
				}
			}
			outPoints.swap(filteredPoints);
		}
	}

	void CollectIgnoreAnchorPoints(std::vector<DuctworkPoint>& outPoints)
	{
		outPoints.clear();
		if (!sAILayer || !sAIArt) {
			return;
		}

		static const char* kIgnoreLayers[] = { "Ignore", "Ignored", "ignore", "ignored" };
		for (size_t layerIndex = 0; layerIndex < (sizeof(kIgnoreLayers) / sizeof(kIgnoreLayers[0])); ++layerIndex) {
			AILayerHandle layer = nullptr;
			const ai::UnicodeString layerName = ai::UnicodeString::FromUTF8(kIgnoreLayers[layerIndex]);
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

	AILayerHandle GetOrCreateAnchorLayer(const char* name)
	{
		if (!name || !sAILayer) {
			return nullptr;
		}

		AILayerHandle layer = DuctworkArt::FindLayerByTitle(name);
		if (layer) {
			return layer;
		}

		if (sAILayer->InsertLayer(nullptr, kPlaceAboveAll, &layer) != kNoErr || !layer) {
			return nullptr;
		}

		sAILayer->SetLayerTitle(layer, ai::UnicodeString::FromUTF8(name));
		return layer;
	}

	bool CreateIgnoreConnectionAnchor(const DuctworkPoint& point)
	{
		if (!sAIArt || !sAIPath) {
			return false;
		}

		AILayerHandle layer = GetOrCreateAnchorLayer("Ignored");
		if (!layer || !DuctworkArt::IsLayerChainEditableVisible(layer)) {
			return false;
		}

		AIArtHandle group = nullptr;
		if (sAIArt->GetFirstArtOfLayer(layer, &group) != kNoErr || !group) {
			return false;
		}

		AIArtHandle path = nullptr;
		if (sAIArt->NewArt(kPathArt, kPlaceInsideOnTop, group, &path) != kNoErr || !path) {
			return false;
		}

		if (sAIPath->SetPathSegmentCount(path, 1) != kNoErr) {
			return false;
		}

		AIPathSegment segment;
		segment.p.h = static_cast<AIReal>(point.x);
		segment.p.v = static_cast<AIReal>(point.y);
		segment.in = segment.p;
		segment.out = segment.p;
		segment.corner = true;
		if (sAIPath->SetPathSegments(path, 0, 1, &segment) != kNoErr) {
			return false;
		}

		if (sAIPathStyle) {
			AIPathStyle style;
			AIBoolean fillVisible = false;
			AIBoolean strokeVisible = false;
			if (sAIPathStyle->GetPathStyleEx(path, &style, &fillVisible, &strokeVisible) == kNoErr) {
				fillVisible = false;
				strokeVisible = false;
				sAIPathStyle->SetPathStyleEx(path, &style, fillVisible, strokeVisible);
			}
		}

		DuctworkNotes::SetNote(path, "MD:POINT_ROT=0");
		return true;
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

	bool IsSegmentThicknessOmitted(size_t segmentIndex,
		size_t segmentCount,
		bool omitStart,
		bool omitEnd,
		const std::set<size_t>& extraOmittedSegments)
	{
		if (extraOmittedSegments.find(segmentIndex) != extraOmittedSegments.end()) {
			return true;
		}
		return IsSegmentThicknessOmitted(segmentIndex, segmentCount, omitStart, omitEnd);
	}

	void CollectTerminalOmittedSegments(AIArtHandle art, size_t segmentCount, std::set<size_t>& outOmittedSegments)
	{
		outOmittedSegments.clear();
		if (!art || segmentCount == 0) {
			return;
		}

		ReadOmittedSegmentIndices(art, outOmittedSegments);
		if (outOmittedSegments.empty()) {
			if (ReadFinalSegmentThicknessFlag(art, kEmoryOmitStartSegmentThicknessKey)) {
				outOmittedSegments.insert(0);
			}
			if (ReadFinalSegmentThicknessFlag(art, kEmoryOmitEndSegmentThicknessKey)) {
				outOmittedSegments.insert(segmentCount - 1);
			}
		}
	}

	bool ResolveTerminalStyleTargetFromSelection(int selectedSegmentIndex,
		size_t canonicalSegmentCount,
		const std::set<size_t>& omittedSegments,
		bool allowStartTarget,
		bool allowEndTarget,
		bool& outAtStart,
		bool& outAtEnd)
	{
		outAtStart = false;
		outAtEnd = false;
		if (selectedSegmentIndex < 0 ||
			canonicalSegmentCount == 0 ||
			selectedSegmentIndex >= static_cast<int>(canonicalSegmentCount)) {
			return false;
		}

		const int lastSegmentIndex = static_cast<int>(canonicalSegmentCount - 1);

		// If a terminal segment is omitted as a guide/line-only tail, let the adjacent
		// visible thick segment control that terminal style.
		if (allowEndTarget &&
			omittedSegments.find(canonicalSegmentCount - 1) != omittedSegments.end() &&
			selectedSegmentIndex == lastSegmentIndex - 1) {
			outAtEnd = true;
			return true;
		}
		if (allowStartTarget &&
			omittedSegments.find(0) != omittedSegments.end() &&
			selectedSegmentIndex == 1) {
			outAtStart = true;
			return true;
		}

		if (allowStartTarget && selectedSegmentIndex == 0) {
			outAtStart = true;
			return true;
		}
		if (allowEndTarget && selectedSegmentIndex == lastSegmentIndex) {
			outAtEnd = true;
			return true;
		}
		return false;
	}

	bool ResolveTerminalStyleTargetFromJoint(int jointIndex,
		size_t canonicalSegmentCount,
		bool allowStartTarget,
		bool allowEndTarget,
		bool& outAtStart,
		bool& outAtEnd)
	{
		outAtStart = false;
		outAtEnd = false;
		if (jointIndex < 0 || canonicalSegmentCount < 2) {
			return false;
		}

		if (allowStartTarget && jointIndex == 1) {
			outAtStart = true;
			return true;
		}
		if (allowEndTarget && jointIndex == static_cast<int>(canonicalSegmentCount - 1)) {
			outAtEnd = true;
			return true;
		}
		return false;
	}

	bool SetOpenPathPoints(AIArtHandle art, const std::vector<DuctworkPoint>& points)
	{
		if (!art || !sAIPath || !sAIArt || points.size() < 2) {
			return false;
		}

		const ai::int16 count = static_cast<ai::int16>(points.size());
		if (sAIPath->SetPathSegmentCount(art, count) != kNoErr) {
			return false;
		}

		std::vector<AIPathSegment> segments(static_cast<size_t>(count));
		for (ai::int16 i = 0; i < count; ++i) {
			segments[static_cast<size_t>(i)].p.h = static_cast<AIReal>(points[static_cast<size_t>(i)].x);
			segments[static_cast<size_t>(i)].p.v = static_cast<AIReal>(points[static_cast<size_t>(i)].y);
			segments[static_cast<size_t>(i)].in = segments[static_cast<size_t>(i)].p;
			segments[static_cast<size_t>(i)].out = segments[static_cast<size_t>(i)].p;
			segments[static_cast<size_t>(i)].corner = true;
		}

		if (sAIPath->SetPathSegments(art, 0, count, &segments[0]) != kNoErr) {
			return false;
		}

		sAIPath->SetPathClosed(art, false);
		return true;
	}

	bool SetClosedPathPoints(AIArtHandle art, const std::vector<DuctworkPoint>& points)
	{
		if (!art || !sAIPath || !sAIArt || points.size() < 3) {
			return false;
		}

		const ai::int16 count = static_cast<ai::int16>(points.size());
		if (sAIPath->SetPathSegmentCount(art, count) != kNoErr) {
			return false;
		}

		std::vector<AIPathSegment> segments(static_cast<size_t>(count));
		for (ai::int16 i = 0; i < count; ++i) {
			segments[static_cast<size_t>(i)].p.h = static_cast<AIReal>(points[static_cast<size_t>(i)].x);
			segments[static_cast<size_t>(i)].p.v = static_cast<AIReal>(points[static_cast<size_t>(i)].y);
			segments[static_cast<size_t>(i)].in = segments[static_cast<size_t>(i)].p;
			segments[static_cast<size_t>(i)].out = segments[static_cast<size_t>(i)].p;
			segments[static_cast<size_t>(i)].corner = true;
		}

		if (sAIPath->SetPathSegments(art, 0, count, &segments[0]) != kNoErr) {
			return false;
		}

		sAIPath->SetPathClosed(art, true);
		return true;
	}

	std::string SerializeCenterlinePoints(const std::vector<DuctworkPoint>& points)
	{
		if (points.size() < 2) {
			return std::string();
		}

		std::ostringstream out;
		out.setf(std::ios::fixed);
		out.precision(6);
		for (size_t i = 0; i < points.size(); ++i) {
			if (i > 0) {
				out << ';';
			}
			out << points[i].x << ',' << points[i].y;
		}
		return out.str();
	}

	bool DeserializeCenterlinePoints(const std::string& serialized, std::vector<DuctworkPoint>& outPoints)
	{
		outPoints.clear();
		if (serialized.empty()) {
			return false;
		}

		size_t start = 0;
		while (start < serialized.size()) {
			size_t end = serialized.find(';', start);
			if (end == std::string::npos) {
				end = serialized.size();
			}
			const std::string token = serialized.substr(start, end - start);
			const size_t comma = token.find(',');
			if (comma == std::string::npos) {
				outPoints.clear();
				return false;
			}

			const std::string xText = token.substr(0, comma);
			const std::string yText = token.substr(comma + 1);
			char* xEnd = nullptr;
			char* yEnd = nullptr;
			const double x = std::strtod(xText.c_str(), &xEnd);
			const double y = std::strtod(yText.c_str(), &yEnd);
			if (!xEnd || *xEnd != '\0' || !yEnd || *yEnd != '\0') {
				outPoints.clear();
				return false;
			}

			DuctworkPoint point;
			point.x = x;
			point.y = y;
			outPoints.push_back(point);
			start = end + 1;
		}

		return outPoints.size() >= 2;
	}

	bool ReadStoredOriginalCenterlinePoints(AIArtHandle art, std::vector<DuctworkPoint>& outPoints)
	{
		outPoints.clear();
		std::string serialized;
		if (!DuctworkMetadata::GetString(art, kEmoryOriginalPathPointsKey, serialized) || serialized.empty()) {
			return false;
		}
		return DeserializeCenterlinePoints(serialized, outPoints);
	}

	bool EnsureStoredOriginalCenterlinePoints(AIArtHandle art)
	{
		std::vector<DuctworkPoint> storedPoints;
		if (ReadStoredOriginalCenterlinePoints(art, storedPoints)) {
			return true;
		}

		DuctworkPath path;
		if (!BuildProcessPathForArt(art, path) || path.closed || path.points.size() < 2) {
			return false;
		}

		const std::string serialized = SerializeCenterlinePoints(path.points);
		if (serialized.empty()) {
			return false;
		}

		DuctworkMetadata::SetString(art, kEmoryOriginalPathPointsKey, serialized);
		return true;
	}

	void ClearStoredOriginalCenterlinePoints(AIArtHandle art)
	{
		if (!art) {
			return;
		}
		DuctworkMetadata::RemoveKey(art, kEmoryOriginalPathPointsKey);
	}

	bool IsBackupCenterlineArt(AIArtHandle art)
	{
		if (!art) {
			return false;
		}
		double backupValue = 0.0;
		return DuctworkMetadata::GetDouble(art, kEmoryBackupCenterlineKey, backupValue) && backupValue > 0.5;
	}

	void SetBackupCenterlineArt(AIArtHandle art, bool isBackup)
	{
		if (!art) {
			return;
		}
		if (isBackup) {
			DuctworkMetadata::SetDouble(art, kEmoryBackupCenterlineKey, 1.0);
		} else {
			DuctworkMetadata::RemoveKey(art, kEmoryBackupCenterlineKey);
		}
	}

	int ClampStartSegmentIndex(int startIndex, size_t segmentCount)
	{
		if (segmentCount == 0) {
			return 0;
		}
		if (startIndex < 0) {
			startIndex = 0;
		}
		if (startIndex >= static_cast<int>(segmentCount)) {
			startIndex = static_cast<int>(segmentCount - 1);
		}
		return startIndex;
	}

	bool IsBlueOrOrangeRunLayer(const std::string& layerName)
	{
		return layerName == "Blue Ductwork" || layerName == "Orange Ductwork";
	}

	bool IsGreenOrLightOrangeRunLayer(const std::string& layerName)
	{
		return layerName == "Green Ductwork" ||
			layerName == "Light Green Ductwork" ||
			layerName == "Light Orange Ductwork";
	}

	void GetPairedUnitRunLayers(const std::string& layerName, std::vector<std::string>& outLayers)
	{
		outLayers.clear();
		if (layerName == "Blue Ductwork") {
			outLayers.push_back("Green Ductwork");
			outLayers.push_back("Light Green Ductwork");
			return;
		}
		if (layerName == "Orange Ductwork") {
			outLayers.push_back("Light Orange Ductwork");
			return;
		}
		if (layerName == "Green Ductwork" || layerName == "Light Green Ductwork") {
			outLayers.push_back("Blue Ductwork");
			return;
		}
		if (layerName == "Light Orange Ductwork") {
			outLayers.push_back("Orange Ductwork");
		}
	}

	bool EndpointNearAnyLayerEndpoint(const DuctworkPoint& point,
		const std::vector<std::string>& layerNames,
		AIArtHandle excludeArt,
		double tolerance)
	{
		if (layerNames.empty()) {
			return false;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		const double toleranceSq = tolerance * tolerance;
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || art == excludeArt || IsGeneratedEmoryArtInternal(art) || IsBackupCenterlineArt(art)) {
				continue;
			}

			DuctworkPath path;
			if (!BuildProcessPathForArt(art, path) ||
				!DuctworkGeometry::IsCenterlineCandidate(path.art, path.points, path.closed, path.layerName)) {
				continue;
			}

			bool layerMatches = false;
			for (size_t layerIndex = 0; layerIndex < layerNames.size(); ++layerIndex) {
				if (path.layerName == layerNames[layerIndex]) {
					layerMatches = true;
					break;
				}
			}
			if (!layerMatches) {
				continue;
			}

			std::vector<DuctworkPoint> candidatePoints;
			SanitizePolyline(path.points, candidatePoints);
			if (candidatePoints.size() < 2) {
				continue;
			}

			const double startDx = point.x - candidatePoints.front().x;
			const double startDy = point.y - candidatePoints.front().y;
			if ((startDx * startDx + startDy * startDy) <= toleranceSq) {
				return true;
			}

			const double endDx = point.x - candidatePoints.back().x;
			const double endDy = point.y - candidatePoints.back().y;
			if ((endDx * endDx + endDy * endDy) <= toleranceSq) {
				return true;
			}
		}
		return false;
	}

	bool TryResolveDirectionalDefaultStartSegmentIndex(AIArtHandle sourceArt,
		const DuctworkPath& path,
		const std::vector<DuctworkPoint>& points,
		size_t segmentCount,
		int& outStartIndex)
	{
		outStartIndex = 0;
		if (!sourceArt || segmentCount == 0 || points.size() < 2) {
			return false;
		}

		const int lastSegmentIndex = ClampStartSegmentIndex(static_cast<int>(segmentCount - 1), segmentCount);
		const DuctworkPoint& startPoint = points.front();
		const DuctworkPoint& endPoint = points.back();
		std::vector<std::string> pairedLayers;
		GetPairedUnitRunLayers(path.layerName, pairedLayers);
		const bool startNearPairedRun = EndpointNearAnyLayerEndpoint(startPoint, pairedLayers, sourceArt, 10.0);
		const bool endNearPairedRun = EndpointNearAnyLayerEndpoint(endPoint, pairedLayers, sourceArt, 10.0);

		if (IsBlueOrOrangeRunLayer(path.layerName)) {
			if (startNearPairedRun != endNearPairedRun) {
				outStartIndex = startNearPairedRun ? 0 : lastSegmentIndex;
				return true;
			}

			std::vector<DuctworkPoint> unitAttachmentPoints;
			CollectUnitAttachmentPoints(unitAttachmentPoints);
			const bool startNearUnit = IsPointNearAny(startPoint, unitAttachmentPoints, 10.0);
			const bool endNearUnit = IsPointNearAny(endPoint, unitAttachmentPoints, 10.0);
			if (startNearUnit != endNearUnit) {
				outStartIndex = startNearUnit ? 0 : lastSegmentIndex;
				return true;
			}
		}

		if (IsGreenOrLightOrangeRunLayer(path.layerName)) {
			if (startNearPairedRun != endNearPairedRun) {
				outStartIndex = startNearPairedRun ? lastSegmentIndex : 0;
				return true;
			}

			std::vector<DuctworkPoint> unitAttachmentPoints;
			CollectUnitAttachmentPoints(unitAttachmentPoints);
			const bool startNearUnit = IsPointNearAny(startPoint, unitAttachmentPoints, 10.0);
			const bool endNearUnit = IsPointNearAny(endPoint, unitAttachmentPoints, 10.0);
			if (startNearUnit != endNearUnit) {
				outStartIndex = startNearUnit ? lastSegmentIndex : 0;
				return true;
			}
		}

		return false;
	}

	bool TryResolveDirectionalStartForArt(AIArtHandle sourceArt, size_t segmentCount, int& outStartIndex)
	{
		outStartIndex = 0;
		if (!sourceArt || segmentCount == 0) {
			return false;
		}

		DuctworkPath path;
		if (!BuildProcessPathForArt(sourceArt, path) || path.closed || path.points.size() < 2) {
			return false;
		}

		std::vector<DuctworkPoint> points;
		SanitizePolyline(path.points, points);
		if (points.size() < 2) {
			return false;
		}

		return TryResolveDirectionalDefaultStartSegmentIndex(sourceArt,
			path,
			points,
			segmentCount,
			outStartIndex);
	}

	int ResolveDefaultStartSegmentIndex(AIArtHandle sourceArt, size_t segmentCount)
	{
		if (!sourceArt || segmentCount == 0) {
			return 0;
		}

		int directionalStartIndex = 0;
		if (TryResolveDirectionalStartForArt(sourceArt, segmentCount, directionalStartIndex)) {
			return directionalStartIndex;
		}

		return 0;
	}

	bool CollectBackupCenterlinesForSourceId(const std::string& sourceId, std::vector<AIArtHandle>& outArt)
	{
		outArt.clear();
		if (sourceId.empty()) {
			return false;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || IsGeneratedEmoryArtInternal(art) || !IsBackupCenterlineArt(art)) {
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
		return !outArt.empty();
	}

	bool SourceIdHasBackupCenterline(const std::string& sourceId)
	{
		std::vector<AIArtHandle> backups;
		return CollectBackupCenterlinesForSourceId(sourceId, backups) && !backups.empty();
	}

	bool GetPrimaryBackupCenterlineForSourceId(const std::string& sourceId,
		AIArtHandle& outArt,
		DuctworkPath& outPath)
	{
		outArt = nullptr;
		outPath = DuctworkPath();
		if (sourceId.empty()) {
			return false;
		}

		std::vector<AIArtHandle> backupArts;
		if (!CollectBackupCenterlinesForSourceId(sourceId, backupArts) || backupArts.empty()) {
			return false;
		}

		size_t bestPointCount = 0;
		for (size_t i = 0; i < backupArts.size(); ++i) {
			AIArtHandle candidate = backupArts[i];
			if (!candidate) {
				continue;
			}

			DuctworkPath candidatePath;
			if (!BuildProcessPathForArt(candidate, candidatePath) || candidatePath.closed) {
				continue;
			}

			std::vector<DuctworkPoint> points;
			SanitizePolyline(candidatePath.points, points);
			if (points.size() < 2) {
				continue;
			}

			if (!outArt || points.size() > bestPointCount) {
				outArt = candidate;
				outPath = candidatePath;
				outPath.points = points;
				bestPointCount = points.size();
			}
		}

		return outArt != nullptr && outPath.points.size() >= 2;
	}

	bool GetOriginalCenterlineSignature(AIArtHandle art,
		const DuctworkPath* optionalPath,
		std::string& outSignature)
	{
		outSignature.clear();
		if (!art) {
			return false;
		}

		std::string serialized;
		if (DuctworkMetadata::GetString(art, kEmoryOriginalPathPointsKey, serialized) && !serialized.empty()) {
			outSignature = serialized;
			return true;
		}

		DuctworkPath localPath;
		const DuctworkPath* path = optionalPath;
		if (!path) {
			if (!BuildProcessPathForArt(art, localPath) || localPath.closed || localPath.points.size() < 2) {
				return false;
			}
			path = &localPath;
		}

		if (!path || path->closed || path->points.size() < 2) {
			return false;
		}

		outSignature = SerializeCenterlinePoints(path->points);
		return !outSignature.empty();
	}

	void RepairVisibleFragmentSourceIdsFromBackups()
	{
		if (!sAIArt) {
			return;
		}

		struct BackupSignatureCandidate
		{
			std::string sourceId;
			std::string layerName;
		};

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);

		std::set<std::string> sourceIdsWithBackups;
		std::map<std::string, std::vector<BackupSignatureCandidate> > backupCandidatesBySignature;
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || !IsBackupCenterlineArt(art) || IsGeneratedEmoryArtInternal(art)) {
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

			sourceIdsWithBackups.insert(sourceId);

			std::string signature;
			if (!GetOriginalCenterlineSignature(art, &path, signature) || signature.empty()) {
				continue;
			}

			BackupSignatureCandidate candidate;
			candidate.sourceId = sourceId;
			candidate.layerName = path.layerName;
			backupCandidatesBySignature[signature].push_back(candidate);
		}

		if (backupCandidatesBySignature.empty()) {
			return;
		}

		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || IsGeneratedEmoryArtInternal(art) || IsBackupCenterlineArt(art)) {
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
			if (sourceIdsWithBackups.find(sourceId) != sourceIdsWithBackups.end()) {
				continue;
			}

			std::string signature;
			if (!GetOriginalCenterlineSignature(art, &path, signature) || signature.empty()) {
				continue;
			}

			std::map<std::string, std::vector<BackupSignatureCandidate> >::const_iterator signatureIt =
				backupCandidatesBySignature.find(signature);
			if (signatureIt == backupCandidatesBySignature.end() || signatureIt->second.empty()) {
				continue;
			}

			std::string replacementSourceId;
			for (size_t candidateIndex = 0; candidateIndex < signatureIt->second.size(); ++candidateIndex) {
				const BackupSignatureCandidate& candidate = signatureIt->second[candidateIndex];
				if (!replacementSourceId.empty() && replacementSourceId == candidate.sourceId) {
					continue;
				}
				if (!candidate.layerName.empty() && !path.layerName.empty() && candidate.layerName != path.layerName) {
					continue;
				}
				if (!replacementSourceId.empty() && replacementSourceId != candidate.sourceId) {
					replacementSourceId.clear();
					break;
				}
				replacementSourceId = candidate.sourceId;
			}

			if (replacementSourceId.empty() || replacementSourceId == sourceId) {
				continue;
			}

			DuctworkMetadata::SetString(art, kEmorySourceIdKey, replacementSourceId);
			DuctworkMetadata::SetString(art, kEmoryRoleKey, kEmoryRoleCenterline);
			UpdateEmoryTokens(art, kEmoryRoleCenterline, replacementSourceId);
		}
	}

	bool EnsureBackupCenterlineForArtInternal(AIArtHandle art)
	{
		if (!art || IsGeneratedEmoryArtInternal(art) || IsBackupCenterlineArt(art)) {
			return false;
		}

		std::string sourceId;
		if (!DuctworkGeometry::EnsureEmorySourceId(art, sourceId) || sourceId.empty()) {
			return false;
		}

		std::vector<AIArtHandle> existingBackups;
		if (CollectBackupCenterlinesForSourceId(sourceId, existingBackups) && !existingBackups.empty()) {
			for (size_t i = 0; i < existingBackups.size(); ++i) {
				SetCenterlineHidden(existingBackups[i], true);
			}
			return true;
		}

		AIArtHandle backup = nullptr;
		if (sAIArt->DuplicateArt(art, kPlaceBelow, art, &backup) != kNoErr || !backup) {
			return false;
		}

		DuctworkMetadata::SetString(backup, kEmorySourceIdKey, sourceId);
		DuctworkMetadata::SetString(backup, kEmoryRoleKey, kEmoryRoleCenterline);
		UpdateEmoryTokens(backup, kEmoryRoleCenterline, sourceId);
		SetBackupCenterlineArt(backup, true);
		SetCenterlineHidden(backup, true);
		return true;
	}

	bool AreGapEndpointsCollinear(const RevertGapEndpoint& a, const RevertGapEndpoint& b)
	{
		const double ax = a.point.x - a.neighbor.x;
		const double ay = a.point.y - a.neighbor.y;
		const double bx = b.point.x - b.neighbor.x;
		const double by = b.point.y - b.neighbor.y;
		const double aLen = std::sqrt(ax * ax + ay * ay);
		const double bLen = std::sqrt(bx * bx + by * by);
		if (aLen < 1e-6 || bLen < 1e-6) {
			return false;
		}

		const double dot = std::fabs((ax * bx + ay * by) / (aLen * bLen));
		const double cosTol = std::cos(kRevertGapCollinearToleranceDeg * (3.141592653589793 / 180.0));
		return dot >= cosTol;
	}

	void AppendRevertGapEndpoints(const EmorySourceIdCandidate& candidate,
		size_t candidateIndex,
		std::vector<RevertGapEndpoint>& outEndpoints)
	{
		if (!candidate.art || candidate.path.closed || candidate.path.points.size() < 2) {
			return;
		}

		RevertGapEndpoint startEndpoint;
		startEndpoint.candidateIndex = candidateIndex;
		startEndpoint.pointIndex = 0;
		startEndpoint.point = candidate.path.points.front();
		startEndpoint.neighbor = candidate.path.points[1];
		startEndpoint.layerName = candidate.path.layerName;
		outEndpoints.push_back(startEndpoint);

		RevertGapEndpoint endEndpoint;
		endEndpoint.candidateIndex = candidateIndex;
		endEndpoint.pointIndex = candidate.path.points.size() - 1;
		endEndpoint.point = candidate.path.points.back();
		endEndpoint.neighbor = candidate.path.points[candidate.path.points.size() - 2];
		endEndpoint.layerName = candidate.path.layerName;
		outEndpoints.push_back(endEndpoint);
	}

	bool CollectSourceCenterlineFragmentsForSourceId(const std::string& sourceId,
		std::vector<EmorySourceIdCandidate>& outFragments)
	{
		outFragments.clear();
		if (sourceId.empty()) {
			return false;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || IsGeneratedEmoryArtInternal(art) || IsBackupCenterlineArt(art)) {
				continue;
			}

			std::string artSourceId;
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, artSourceId) || artSourceId.empty()) {
				artSourceId = ReadEmorySourceIdFromNote(art);
			}
			if (artSourceId != sourceId) {
				continue;
			}

			EmorySourceIdCandidate fragment;
			fragment.art = art;
			fragment.oldSourceId = sourceId;
			if (!BuildProcessPathForArt(art, fragment.path) ||
				!DuctworkGeometry::IsCenterlineCandidate(fragment.path.art, fragment.path.points, fragment.path.closed, fragment.path.layerName)) {
				continue;
			}
			outFragments.push_back(fragment);
		}

		return !outFragments.empty();
	}

	void BuildRevertCenterlineIndex(std::map<std::string, std::vector<AIArtHandle> >& outBackupsBySourceId,
		std::map<std::string, std::vector<EmorySourceIdCandidate> >& outFragmentsBySourceId)
	{
		outBackupsBySourceId.clear();
		outFragmentsBySourceId.clear();

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			std::string sourceId;
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
				sourceId = ReadEmorySourceIdFromNote(art);
			}
			if (sourceId.empty()) {
				continue;
			}

			if (IsBackupCenterlineArt(art)) {
				outBackupsBySourceId[sourceId].push_back(art);
				continue;
			}

			EmorySourceIdCandidate fragment;
			fragment.art = art;
			fragment.oldSourceId = sourceId;
			if (!BuildProcessPathForArt(art, fragment.path) ||
				!DuctworkGeometry::IsCenterlineCandidate(fragment.path.art, fragment.path.points, fragment.path.closed, fragment.path.layerName)) {
				continue;
			}
			outFragmentsBySourceId[sourceId].push_back(fragment);
		}
	}

	bool FindBestRevertGapMergeCandidate(const std::vector<EmorySourceIdCandidate>& fragments,
		RevertGapMergeCandidate& outCandidate)
	{
		outCandidate = RevertGapMergeCandidate();
		if (fragments.size() < 2) {
			return false;
		}

		std::vector<RevertGapEndpoint> endpoints;
		endpoints.reserve(fragments.size() * 2);
		for (size_t i = 0; i < fragments.size(); ++i) {
			AppendRevertGapEndpoints(fragments[i], i, endpoints);
		}

		bool found = false;
		double bestGapLength = 0.0;
		for (size_t i = 0; i < endpoints.size(); ++i) {
			for (size_t j = i + 1; j < endpoints.size(); ++j) {
				if (endpoints[i].candidateIndex == endpoints[j].candidateIndex ||
					endpoints[i].layerName != endpoints[j].layerName) {
					continue;
				}

				const double gapLength = std::hypot(endpoints[i].point.x - endpoints[j].point.x,
					endpoints[i].point.y - endpoints[j].point.y);
				if (gapLength > kRevertGapMaxLength ||
					!AreGapEndpointsCollinear(endpoints[i], endpoints[j])) {
					continue;
				}

				if (!found || gapLength < bestGapLength) {
					found = true;
					bestGapLength = gapLength;
					outCandidate.valid = true;
					outCandidate.firstCandidateIndex = endpoints[i].candidateIndex;
					outCandidate.secondCandidateIndex = endpoints[j].candidateIndex;
					outCandidate.reverseFirst = (endpoints[i].pointIndex == 0);
					outCandidate.reverseSecond = (endpoints[j].pointIndex != 0);
					outCandidate.gapLength = gapLength;
				}
			}
		}

		return outCandidate.valid;
	}

	bool MergeRevertGapFragments(std::vector<EmorySourceIdCandidate>& ioFragments,
		const RevertGapMergeCandidate& candidate)
	{
		if (!candidate.valid ||
			candidate.firstCandidateIndex >= ioFragments.size() ||
			candidate.secondCandidateIndex >= ioFragments.size() ||
			candidate.firstCandidateIndex == candidate.secondCandidateIndex) {
			return false;
		}

		EmorySourceIdCandidate& first = ioFragments[candidate.firstCandidateIndex];
		EmorySourceIdCandidate& second = ioFragments[candidate.secondCandidateIndex];
		if (!first.art || !second.art) {
			return false;
		}

		std::vector<DuctworkPoint> firstPoints = first.path.points;
		std::vector<DuctworkPoint> secondPoints = second.path.points;
		if (candidate.reverseFirst) {
			std::reverse(firstPoints.begin(), firstPoints.end());
		}
		if (candidate.reverseSecond) {
			std::reverse(secondPoints.begin(), secondPoints.end());
		}
		if (firstPoints.size() < 2 || secondPoints.size() < 2) {
			return false;
		}

		firstPoints.pop_back();
		firstPoints.insert(firstPoints.end(), secondPoints.begin() + 1, secondPoints.end());
		SimplifyOpenPathCollinearPoints(firstPoints);
		if (firstPoints.size() < 2 || !SetOpenPathPoints(first.art, firstPoints)) {
			return false;
		}

		first.path.points = firstPoints;
		first.path.closed = false;
		first.path.layerName = first.path.layerName.empty() ? second.path.layerName : first.path.layerName;

		sAIArt->SetArtUserAttr(second.art, kArtLocked | kArtHidden, 0);
		sAIArt->DisposeArt(second.art);
		ioFragments.erase(ioFragments.begin() + static_cast<std::ptrdiff_t>(candidate.secondCandidateIndex));
		return true;
	}

	bool HealSourceCenterlineFragments(std::vector<EmorySourceIdCandidate>& fragments,
		std::vector<AIArtHandle>& outSourceArts)
	{
		outSourceArts.clear();
		if (fragments.empty()) {
			return false;
		}

		RevertGapMergeCandidate candidate;
		while (FindBestRevertGapMergeCandidate(fragments, candidate)) {
			if (!MergeRevertGapFragments(fragments, candidate)) {
				break;
			}
		}

		for (size_t i = 0; i < fragments.size(); ++i) {
			if (fragments[i].art) {
				outSourceArts.push_back(fragments[i].art);
			}
		}
		return !outSourceArts.empty();
	}

	bool HealSourceCenterlineFragmentsForSourceId(const std::string& sourceId,
		std::vector<AIArtHandle>& outSourceArts)
	{
		std::vector<EmorySourceIdCandidate> fragments;
		if (!CollectSourceCenterlineFragmentsForSourceId(sourceId, fragments)) {
			outSourceArts.clear();
			return false;
		}
		return HealSourceCenterlineFragments(fragments, outSourceArts);
	}

	bool RestoreStoredOriginalCenterlineFragments(const std::vector<EmorySourceIdCandidate>& fragments,
		std::vector<AIArtHandle>& outSourceArts)
	{
		outSourceArts.clear();
		if (fragments.empty()) {
			return false;
		}

		std::map<std::string, std::vector<size_t> > fragmentGroups;
		std::vector<size_t> passthroughIndices;
		for (size_t i = 0; i < fragments.size(); ++i) {
			std::string serialized;
			if (!DuctworkMetadata::GetString(fragments[i].art, kEmoryOriginalPathPointsKey, serialized) ||
				serialized.empty()) {
				passthroughIndices.push_back(i);
				continue;
			}
			fragmentGroups[serialized].push_back(i);
		}

		if (fragmentGroups.empty()) {
			return false;
		}

		for (std::map<std::string, std::vector<size_t> >::const_iterator groupIt = fragmentGroups.begin();
			groupIt != fragmentGroups.end();
			++groupIt) {
			std::vector<DuctworkPoint> originalPoints;
			if (!DeserializeCenterlinePoints(groupIt->first, originalPoints) || originalPoints.size() < 2) {
				return false;
			}

			const std::vector<size_t>& indices = groupIt->second;
			if (indices.empty()) {
				continue;
			}

			const size_t keeperIndex = indices.front();
			AIArtHandle keeper = fragments[keeperIndex].art;
			if (!keeper || !SetOpenPathPoints(keeper, originalPoints)) {
				return false;
			}
			ClearStoredOriginalCenterlinePoints(keeper);
			outSourceArts.push_back(keeper);

			for (size_t groupIndex = 1; groupIndex < indices.size(); ++groupIndex) {
				AIArtHandle duplicate = fragments[indices[groupIndex]].art;
				if (!duplicate) {
					continue;
				}
				ClearStoredOriginalCenterlinePoints(duplicate);
				sAIArt->SetArtUserAttr(duplicate, kArtLocked | kArtHidden, 0);
				sAIArt->DisposeArt(duplicate);
			}
		}

		for (size_t passthroughIndex = 0; passthroughIndex < passthroughIndices.size(); ++passthroughIndex) {
			AIArtHandle art = fragments[passthroughIndices[passthroughIndex]].art;
			if (art) {
				outSourceArts.push_back(art);
			}
		}

		return true;
	}

	bool RestoreStoredOriginalCenterlineForSourceId(const std::string& sourceId,
		std::vector<AIArtHandle>& outSourceArts)
	{
		std::vector<EmorySourceIdCandidate> fragments;
		if (!CollectSourceCenterlineFragmentsForSourceId(sourceId, fragments)) {
			outSourceArts.clear();
			return false;
		}
		return RestoreStoredOriginalCenterlineFragments(fragments, outSourceArts);
	}

	bool RestoreBackupCenterlinesFromFragments(const std::vector<AIArtHandle>& backups,
		const std::vector<EmorySourceIdCandidate>& workingFragments,
		std::vector<AIArtHandle>& outSourceArts)
	{
		outSourceArts.clear();
		if (backups.empty()) {
			return false;
		}

		AIArtHandle keeperBackup = nullptr;
		size_t keeperPointCount = 0;
		for (size_t i = 0; i < backups.size(); ++i) {
			AIArtHandle backup = backups[i];
			if (!backup) {
				continue;
			}

			std::vector<DuctworkPoint> points;
			bool closed = false;
			if (!DuctworkGeometry::GetPathPoints(backup, points, closed) || closed) {
				continue;
			}

			if (!keeperBackup || points.size() > keeperPointCount) {
				keeperBackup = backup;
				keeperPointCount = points.size();
			}
		}
		if (!keeperBackup) {
			keeperBackup = backups.front();
		}

		for (size_t i = 0; i < backups.size(); ++i) {
			AIArtHandle backup = backups[i];
			if (!backup) {
				continue;
			}

			if (backup == keeperBackup) {
				SetBackupCenterlineArt(backup, false);
				SetCenterlineHidden(backup, false);
				outSourceArts.push_back(backup);
			} else {
				sAIArt->SetArtUserAttr(backup, kArtLocked | kArtHidden, 0);
				sAIArt->DisposeArt(backup);
			}
		}

		for (size_t i = 0; i < workingFragments.size(); ++i) {
			AIArtHandle art = workingFragments[i].art;
			if (!art) {
				continue;
			}
			ClearStoredOriginalCenterlinePoints(art);
			sAIArt->SetArtUserAttr(art, kArtLocked | kArtHidden, 0);
			sAIArt->DisposeArt(art);
		}

		return !outSourceArts.empty();
	}

	bool RestoreBackupCenterlinesForSourceId(const std::string& sourceId,
		std::vector<AIArtHandle>& outSourceArts)
	{
		std::vector<AIArtHandle> backups;
		if (!CollectBackupCenterlinesForSourceId(sourceId, backups) || backups.empty()) {
			outSourceArts.clear();
			return false;
		}

		std::vector<EmorySourceIdCandidate> workingFragments;
		CollectSourceCenterlineFragmentsForSourceId(sourceId, workingFragments);
		return RestoreBackupCenterlinesFromFragments(backups, workingFragments, outSourceArts);
	}

	bool HealCenterlineFragmentArtSet(std::vector<AIArtHandle>& ioSourceArts)
	{
		std::vector<EmorySourceIdCandidate> fragments;
		std::set<AIArtHandle> seen;
		for (size_t i = 0; i < ioSourceArts.size(); ++i) {
			AIArtHandle art = ioSourceArts[i];
			if (!art || !seen.insert(art).second) {
				continue;
			}

			DuctworkPath path;
			if (!BuildProcessPathForArt(art, path) ||
				!DuctworkGeometry::IsCenterlineCandidate(path.art, path.points, path.closed, path.layerName)) {
				continue;
			}

			EmorySourceIdCandidate fragment;
			fragment.art = art;
			fragment.path = path;
			fragments.push_back(fragment);
		}

		if (fragments.size() < 2) {
			ioSourceArts.clear();
			for (size_t i = 0; i < fragments.size(); ++i) {
				if (fragments[i].art) {
					ioSourceArts.push_back(fragments[i].art);
				}
			}
			return !ioSourceArts.empty();
		}

		RevertGapMergeCandidate candidate;
		while (FindBestRevertGapMergeCandidate(fragments, candidate)) {
			if (!MergeRevertGapFragments(fragments, candidate)) {
				break;
			}
		}

		ioSourceArts.clear();
		for (size_t i = 0; i < fragments.size(); ++i) {
			if (fragments[i].art) {
				ioSourceArts.push_back(fragments[i].art);
			}
		}
		return !ioSourceArts.empty();
	}

	bool IsRedundantCollinearPoint(const DuctworkPoint& previous,
		const DuctworkPoint& current,
		const DuctworkPoint& next)
	{
		const double prevLength = std::hypot(current.x - previous.x, current.y - previous.y);
		const double nextLength = std::hypot(next.x - current.x, next.y - current.y);
		const double directLength = std::hypot(next.x - previous.x, next.y - previous.y);
		if (prevLength < 1e-6 || nextLength < 1e-6) {
			return true;
		}

		const double det = std::fabs((current.x - previous.x) * (next.y - current.y) -
			(current.y - previous.y) * (next.x - current.x));
		if (det > 0.01 * (prevLength + nextLength)) {
			return false;
		}

		return std::fabs((prevLength + nextLength) - directLength) <= 0.05;
	}

	void SimplifyOpenPathCollinearPoints(std::vector<DuctworkPoint>& points)
	{
		if (points.size() <= 2) {
			return;
		}

		bool changed = true;
		while (changed && points.size() > 2) {
			changed = false;
			std::vector<DuctworkPoint> simplified;
			simplified.reserve(points.size());
			simplified.push_back(points.front());
			for (size_t i = 1; i + 1 < points.size(); ++i) {
				if (IsRedundantCollinearPoint(simplified.back(), points[i], points[i + 1])) {
					changed = true;
					continue;
				}
				simplified.push_back(points[i]);
			}
			simplified.push_back(points.back());
			points.swap(simplified);
		}
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

	void CollectAllDocumentPaths(std::vector<AIArtHandle>& outPaths)
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
			AIArtHandle layerGroup = nullptr;
			if (sAIArt->GetFirstArtOfLayer(layer, &layerGroup) == kNoErr && layerGroup) {
				CollectAllPathsFromArt(layerGroup, outPaths);
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

	bool CreateOpenPath(AIArtHandle referenceArt, const std::vector<DuctworkPoint>& points, AIArtHandle& outPath)
	{
		outPath = nullptr;
		if (!referenceArt || points.size() < 2 || !sAIArt || !sAIPath) {
			return false;
		}

		AIArtHandle path = nullptr;
		if (sAIArt->NewArt(kPathArt, kPlaceAbove, referenceArt, &path) != kNoErr || !path) {
			return false;
		}
		if (!SetOpenPathPoints(path, points)) {
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
		tokens.erase(std::remove_if(tokens.begin(), tokens.end(),
			[](const std::string& token) {
				return token.find(kEmorySourceIdPrefix) == 0;
			}),
			tokens.end());
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

	void ClearAllEmoryMetadataInternal(AIArtHandle art)
	{
		if (!art) {
			return;
		}

		DuctworkMetadata::RemoveKey(art, kEmoryRoleKey);
		DuctworkMetadata::RemoveKey(art, kEmorySourceIdKey);
		DuctworkMetadata::RemoveKey(art, kEmoryLinkedSourceIdsKey);
		DuctworkMetadata::RemoveKey(art, kEmoryBodyWidthKey);
		DuctworkMetadata::RemoveKey(art, kEmorySourceBodyWidthKey);
		DuctworkMetadata::RemoveKey(art, kEmorySourceStrokeWidthKey);
		DuctworkMetadata::RemoveKey(art, kEmorySourceStrokeExplicitKey);
		DuctworkMetadata::RemoveKey(art, kEmoryOriginalPathPointsKey);
		DuctworkMetadata::RemoveKey(art, kEmorySegmentWidthsKey);
		DuctworkMetadata::RemoveKey(art, kEmoryStartSegmentIndexKey);
		DuctworkMetadata::RemoveKey(art, kEmoryTaperAlignmentsKey);
		DuctworkMetadata::RemoveKey(art, kEmoryCenterlinesHiddenKey);
		DuctworkMetadata::RemoveKey(art, kEmoryCenterlineTailGuideKey);
		DuctworkMetadata::RemoveKey(art, kEmoryCenterlineTailGuideStartKey);
		DuctworkMetadata::RemoveKey(art, kEmoryCenterlineTailGuideEndKey);
		DuctworkMetadata::RemoveKey(art, kEmoryCenterlineTailGuideCreatedKey);
		DuctworkMetadata::RemoveKey(art, kEmoryBackupCenterlineKey);
		DuctworkMetadata::RemoveKey(art, kEmoryOmitStartSegmentThicknessKey);
		DuctworkMetadata::RemoveKey(art, kEmoryOmitEndSegmentThicknessKey);
		DuctworkMetadata::RemoveKey(art, kEmoryOmittedSegmentIndicesKey);
		DuctworkMetadata::RemoveKey(art, kEmorySegmentIndexKey);
		DuctworkMetadata::RemoveKey(art, kEmoryJointIndexKey);
		DuctworkMetadata::RemoveKey(art, kEmoryConnectorStyleKey);
		DuctworkMetadata::RemoveKey(art, kEmoryCornerStylesKey);
		DuctworkMetadata::RemoveKey(art, kEmoryTerminalStartStyleKey);
		DuctworkMetadata::RemoveKey(art, kEmoryTerminalEndStyleKey);
		UpdateEmoryTokens(art, std::string(), std::string());

		if (sAIArt) {
			sAIArt->SetArtUserAttr(art, kArtHidden, 0);
		}
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
		double widthOverride,
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
		outArm.width = widthOverride > kMinDuctWidth ? widthOverride : state.widths[segmentIndex];
		if (!std::isfinite(outArm.width) || outArm.width < kMinDuctWidth) {
			outArm.width = kMinDuctWidth;
		}
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

	bool BuildNetworkConnectorArm(const EmorySourceState& state,
		int stateIndex,
		int segmentIndex,
		const DuctworkPoint& connectionPoint,
		const Vec2& outwardDir,
		double availableLength,
		double desiredLength,
		NetworkConnectorArm& outArm)
	{
		return BuildNetworkConnectorArm(state,
			stateIndex,
			segmentIndex,
			connectionPoint,
			outwardDir,
			availableLength,
			desiredLength,
			0.0,
			outArm);
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

	std::string ReadTerminalSegmentStyle(AIArtHandle sourceArt, bool atStart)
	{
		const char* key = atStart ? kEmoryTerminalStartStyleKey : kEmoryTerminalEndStyleKey;
		std::string style;
		if (!DuctworkMetadata::GetString(sourceArt, key, style) || style.empty()) {
			return "straight";
		}
		return style == "curved" ? "curved" : "straight";
	}

	void WriteTerminalSegmentStyle(AIArtHandle sourceArt, bool atStart, const std::string& style)
	{
		if (!sourceArt) {
			return;
		}
		const char* key = atStart ? kEmoryTerminalStartStyleKey : kEmoryTerminalEndStyleKey;
		if (style == "curved") {
			DuctworkMetadata::SetString(sourceArt, key, "curved");
		} else {
			DuctworkMetadata::RemoveKey(sourceArt, key);
		}
	}

	DuctworkPoint ToPoint(const AIRealPoint& point)
	{
		DuctworkPoint out;
		out.x = point.h;
		out.y = point.v;
		return out;
	}

	bool ApplyTerminalSegmentStyleToPathArt(AIArtHandle art, int segmentIndex, const std::string& style)
	{
		if (!art || !sAIPath) {
			return false;
		}

		ai::int16 segmentCount = 0;
		if (sAIPath->GetPathSegmentCount(art, &segmentCount) != kNoErr || segmentCount < 2) {
			return false;
		}

		const int lastSegmentIndex = static_cast<int>(segmentCount) - 2;
		if (segmentIndex != 0 && segmentIndex != lastSegmentIndex) {
			return false;
		}

		std::vector<AIPathSegment> segments(static_cast<size_t>(segmentCount));
		if (sAIPath->GetPathSegments(art, 0, segmentCount, &segments[0]) != kNoErr) {
			return false;
		}

	const bool curved = (style == "curved");
	const DuctworkPoint startPoint = ToPoint(segments[segmentIndex].p);
	const DuctworkPoint endPoint = ToPoint(segments[segmentIndex + 1].p);
	double handleLength = std::hypot(endPoint.x - startPoint.x, endPoint.y - startPoint.y) * 0.33;
	if (handleLength < 0.01) {
		handleLength = 0.0;
	}
	Vec2 segmentDir;
	Vec2 segmentNormal;
	const bool hasSegmentDirection = BuildUnitDirection(startPoint, endPoint, segmentDir, segmentNormal);

	if (segmentIndex == 0) {
		segments[0].in = segments[0].p;
		segments[0].out = segments[0].p;
		if (curved && handleLength > 0.0) {
			Vec2 nextDir;
			Vec2 nextNormal;
			const bool hasNextDirection = BuildUnitDirection(ToPoint(segments[1].p),
					segmentCount > 2 ? ToPoint(segments[2].p) : ToPoint(segments[1].p),
					nextDir,
					nextNormal);
			if (!hasNextDirection) {
				BuildUnitDirection(ToPoint(segments[0].p), ToPoint(segments[1].p), nextDir, nextNormal);
			}
			const bool needsOffAxisCurve = !hasNextDirection ||
				(hasSegmentDirection && Dot(segmentDir, nextDir) > 0.85 && std::fabs(Cross(segmentDir, nextDir)) < 0.15);
			if (needsOffAxisCurve && hasSegmentDirection) {
				const double axialHandle = handleLength * 0.6;
				const double lateralHandle = handleLength * 0.8;
				const DuctworkPoint startOut = Add(startPoint, segmentDir, axialHandle);
				const DuctworkPoint endIn = Add(endPoint, Scale(segmentDir, -1.0), axialHandle);
				const DuctworkPoint startOutOffset = Add(startOut, segmentNormal, lateralHandle);
				const DuctworkPoint endInOffset = Add(endIn, segmentNormal, lateralHandle);
				segments[0].out.h = static_cast<AIReal>(startOutOffset.x);
				segments[0].out.v = static_cast<AIReal>(startOutOffset.y);
				segments[1].in.h = static_cast<AIReal>(endInOffset.x);
				segments[1].in.v = static_cast<AIReal>(endInOffset.y);
			} else {
				segments[1].in.h = static_cast<AIReal>(segments[1].p.h - nextDir.x * handleLength);
				segments[1].in.v = static_cast<AIReal>(segments[1].p.v - nextDir.y * handleLength);
			}
			segments[0].corner = false;
			segments[1].corner = false;
		} else {
			segments[1].in = segments[1].p;
		}
		} else {
		segments[segmentCount - 1].in = segments[segmentCount - 1].p;
		segments[segmentCount - 1].out = segments[segmentCount - 1].p;
		if (curved && handleLength > 0.0) {
			Vec2 prevDir;
			Vec2 prevNormal;
			const bool hasPrevDirection = BuildUnitDirection(segmentIndex > 0 ? ToPoint(segments[segmentIndex - 1].p) : ToPoint(segments[segmentIndex].p),
					ToPoint(segments[segmentIndex].p),
					prevDir,
					prevNormal);
			if (!hasPrevDirection) {
				BuildUnitDirection(ToPoint(segments[segmentIndex].p), ToPoint(segments[segmentIndex + 1].p), prevDir, prevNormal);
			}
			const bool needsOffAxisCurve = !hasPrevDirection ||
				(hasSegmentDirection && Dot(segmentDir, prevDir) > 0.85 && std::fabs(Cross(segmentDir, prevDir)) < 0.15);
			if (needsOffAxisCurve && hasSegmentDirection) {
				const double axialHandle = handleLength * 0.6;
				const double lateralHandle = handleLength * 0.8;
				const DuctworkPoint startOut = Add(startPoint, segmentDir, axialHandle);
				const DuctworkPoint endIn = Add(endPoint, Scale(segmentDir, -1.0), axialHandle);
				const DuctworkPoint startOutOffset = Add(startOut, segmentNormal, lateralHandle);
				const DuctworkPoint endInOffset = Add(endIn, segmentNormal, lateralHandle);
				segments[segmentIndex].out.h = static_cast<AIReal>(startOutOffset.x);
				segments[segmentIndex].out.v = static_cast<AIReal>(startOutOffset.y);
				segments[segmentCount - 1].in.h = static_cast<AIReal>(endInOffset.x);
				segments[segmentCount - 1].in.v = static_cast<AIReal>(endInOffset.y);
			} else {
				segments[segmentIndex].out.h = static_cast<AIReal>(segments[segmentIndex].p.h + prevDir.x * handleLength);
				segments[segmentIndex].out.v = static_cast<AIReal>(segments[segmentIndex].p.v + prevDir.y * handleLength);
			}
			segments[segmentIndex].corner = false;
			segments[segmentCount - 1].corner = false;
		} else {
			segments[segmentIndex].out = segments[segmentIndex].p;
		}
		}

		return sAIPath->SetPathSegments(art, 0, segmentCount, &segments[0]) == kNoErr;
	}

	bool ApplyTerminalGuideStyleToGuideArt(AIArtHandle guideArt,
		const std::vector<DuctworkPoint>& sourcePoints,
		size_t sourceSegmentIndex,
		const std::string& style)
	{
		if (!guideArt || sourcePoints.size() < 2 || sourceSegmentIndex + 1 >= sourcePoints.size() || !sAIPath) {
			return false;
		}

		ai::int16 segmentCount = 0;
		if (sAIPath->GetPathSegmentCount(guideArt, &segmentCount) != kNoErr || segmentCount != 2) {
			return false;
		}

		std::vector<AIPathSegment> segments(static_cast<size_t>(segmentCount));
		if (sAIPath->GetPathSegments(guideArt, 0, segmentCount, &segments[0]) != kNoErr) {
			return false;
		}

		segments[0].in = segments[0].p;
		segments[0].out = segments[0].p;
		segments[1].in = segments[1].p;
		segments[1].out = segments[1].p;
		segments[0].corner = true;
		segments[1].corner = true;

		if (style != "curved") {
			return sAIPath->SetPathSegments(guideArt, 0, segmentCount, &segments[0]) == kNoErr;
		}

	const DuctworkPoint startPoint = sourcePoints[sourceSegmentIndex];
	const DuctworkPoint endPoint = sourcePoints[sourceSegmentIndex + 1];
	double handleLength = std::hypot(endPoint.x - startPoint.x, endPoint.y - startPoint.y) * 0.33;
	if (handleLength < 0.01) {
		handleLength = 0.0;
	}
	Vec2 segmentDir;
	Vec2 segmentNormal;
	const bool hasSegmentDirection = BuildUnitDirection(startPoint, endPoint, segmentDir, segmentNormal);

	segments[0].in = segments[0].p;
	segments[1].out = segments[1].p;

	if (sourceSegmentIndex == 0) {
		Vec2 nextDir;
		Vec2 nextNormal;
		const bool hasNextDirection = BuildUnitDirection(sourcePoints[1],
				sourcePoints.size() > 2 ? sourcePoints[2] : sourcePoints[1],
				nextDir,
				nextNormal);
		if (!hasNextDirection) {
			BuildUnitDirection(sourcePoints[0], sourcePoints[1], nextDir, nextNormal);
		}
		segments[0].out = segments[0].p;
		const bool needsOffAxisCurve = !hasNextDirection ||
			(hasSegmentDirection && Dot(segmentDir, nextDir) > 0.85 && std::fabs(Cross(segmentDir, nextDir)) < 0.15);
		if (needsOffAxisCurve && hasSegmentDirection) {
			const double axialHandle = handleLength * 0.6;
			const double lateralHandle = handleLength * 0.8;
			const DuctworkPoint startOut = Add(startPoint, segmentDir, axialHandle);
			const DuctworkPoint endIn = Add(endPoint, Scale(segmentDir, -1.0), axialHandle);
			const DuctworkPoint startOutOffset = Add(startOut, segmentNormal, lateralHandle);
			const DuctworkPoint endInOffset = Add(endIn, segmentNormal, lateralHandle);
			segments[0].out.h = static_cast<AIReal>(startOutOffset.x);
			segments[0].out.v = static_cast<AIReal>(startOutOffset.y);
			segments[1].in.h = static_cast<AIReal>(endInOffset.x);
			segments[1].in.v = static_cast<AIReal>(endInOffset.y);
		} else {
			segments[1].in.h = static_cast<AIReal>(segments[1].p.h - nextDir.x * handleLength);
			segments[1].in.v = static_cast<AIReal>(segments[1].p.v - nextDir.y * handleLength);
		}
	} else {
		Vec2 prevDir;
		Vec2 prevNormal;
		const bool hasPrevDirection = BuildUnitDirection(sourceSegmentIndex > 0 ? sourcePoints[sourceSegmentIndex - 1] : sourcePoints[sourceSegmentIndex],
				sourcePoints[sourceSegmentIndex],
				prevDir,
				prevNormal);
		if (!hasPrevDirection) {
			BuildUnitDirection(sourcePoints[sourceSegmentIndex], sourcePoints[sourceSegmentIndex + 1], prevDir, prevNormal);
		}
		const bool needsOffAxisCurve = !hasPrevDirection ||
			(hasSegmentDirection && Dot(segmentDir, prevDir) > 0.85 && std::fabs(Cross(segmentDir, prevDir)) < 0.15);
		if (needsOffAxisCurve && hasSegmentDirection) {
			const double axialHandle = handleLength * 0.6;
			const double lateralHandle = handleLength * 0.8;
			const DuctworkPoint startOut = Add(startPoint, segmentDir, axialHandle);
			const DuctworkPoint endIn = Add(endPoint, Scale(segmentDir, -1.0), axialHandle);
			const DuctworkPoint startOutOffset = Add(startOut, segmentNormal, lateralHandle);
			const DuctworkPoint endInOffset = Add(endIn, segmentNormal, lateralHandle);
			segments[0].out.h = static_cast<AIReal>(startOutOffset.x);
			segments[0].out.v = static_cast<AIReal>(startOutOffset.y);
			segments[1].in.h = static_cast<AIReal>(endInOffset.x);
			segments[1].in.v = static_cast<AIReal>(endInOffset.y);
		} else {
			segments[0].out.h = static_cast<AIReal>(segments[0].p.h + prevDir.x * handleLength);
			segments[0].out.v = static_cast<AIReal>(segments[0].p.v + prevDir.y * handleLength);
		}
		segments[1].in = segments[1].p;
	}

		segments[0].corner = false;
		segments[1].corner = false;
		return sAIPath->SetPathSegments(guideArt, 0, segmentCount, &segments[0]) == kNoErr;
	}

	bool ResolveTerminalCurveControlPoints(const std::vector<DuctworkPoint>& sourcePoints,
		size_t sourceSegmentIndex,
		const std::string& style,
		DuctworkPoint& outP0,
		DuctworkPoint& outP1,
		DuctworkPoint& outP2,
		DuctworkPoint& outP3)
	{
		if (sourcePoints.size() < 2 || sourceSegmentIndex + 1 >= sourcePoints.size()) {
			return false;
		}

		outP0 = sourcePoints[sourceSegmentIndex];
		outP1 = outP0;
		outP2 = sourcePoints[sourceSegmentIndex + 1];
		outP3 = outP2;

		if (style != "curved") {
			return true;
		}

		double handleLength = std::hypot(outP3.x - outP0.x, outP3.y - outP0.y) * 0.33;
		if (handleLength < 0.01) {
			handleLength = 0.0;
		}
		if (handleLength <= 0.0) {
			return true;
		}

		Vec2 segmentDir;
		Vec2 segmentNormal;
		const bool hasSegmentDirection = BuildUnitDirection(outP0, outP3, segmentDir, segmentNormal);

		if (sourceSegmentIndex == 0) {
			Vec2 nextDir;
			Vec2 nextNormal;
			const bool hasNextDirection = BuildUnitDirection(sourcePoints[1],
				sourcePoints.size() > 2 ? sourcePoints[2] : sourcePoints[1],
				nextDir,
				nextNormal);
			if (!hasNextDirection) {
				BuildUnitDirection(sourcePoints[0], sourcePoints[1], nextDir, nextNormal);
			}

			const bool needsOffAxisCurve = !hasNextDirection ||
				(hasSegmentDirection && Dot(segmentDir, nextDir) > 0.85 && std::fabs(Cross(segmentDir, nextDir)) < 0.15);
			if (needsOffAxisCurve && hasSegmentDirection) {
				const double axialHandle = handleLength * 0.6;
				const double lateralHandle = handleLength * 0.8;
				const DuctworkPoint startOut = Add(outP0, segmentDir, axialHandle);
				const DuctworkPoint endIn = Add(outP3, Scale(segmentDir, -1.0), axialHandle);
				outP1 = Add(startOut, segmentNormal, lateralHandle);
				outP2 = Add(endIn, segmentNormal, lateralHandle);
			} else {
				outP2 = Add(outP3, Scale(nextDir, -1.0), handleLength);
			}
			return true;
		}

		Vec2 prevDir;
		Vec2 prevNormal;
		const bool hasPrevDirection = BuildUnitDirection(sourceSegmentIndex > 0 ? sourcePoints[sourceSegmentIndex - 1] : sourcePoints[sourceSegmentIndex],
			sourcePoints[sourceSegmentIndex],
			prevDir,
			prevNormal);
		if (!hasPrevDirection) {
			BuildUnitDirection(sourcePoints[sourceSegmentIndex], sourcePoints[sourceSegmentIndex + 1], prevDir, prevNormal);
		}
		const bool needsOffAxisCurve = !hasPrevDirection ||
			(hasSegmentDirection && Dot(segmentDir, prevDir) > 0.85 && std::fabs(Cross(segmentDir, prevDir)) < 0.15);
		if (needsOffAxisCurve && hasSegmentDirection) {
			const double axialHandle = handleLength * 0.6;
			const double lateralHandle = handleLength * 0.8;
			const DuctworkPoint startOut = Add(outP0, segmentDir, axialHandle);
			const DuctworkPoint endIn = Add(outP3, Scale(segmentDir, -1.0), axialHandle);
			outP1 = Add(startOut, segmentNormal, lateralHandle);
			outP2 = Add(endIn, segmentNormal, lateralHandle);
		} else {
			outP1 = Add(outP0, prevDir, handleLength);
		}
		return true;
	}

	bool BuildTerminalBodyPolygon(const std::vector<DuctworkPoint>& sourcePoints,
		size_t sourceSegmentIndex,
		double fullWidth,
		double referenceWidth,
		const std::string& alignment,
		bool chainHorizontal,
		bool chainVertical,
		const std::string& style,
		std::vector<DuctworkPoint>& outPolygon)
	{
		outPolygon.clear();
		if (sourcePoints.size() < 2 || sourceSegmentIndex + 1 >= sourcePoints.size()) {
			return false;
		}

		if (style != "curved") {
			return BuildAlignedBandPolygon(sourcePoints[sourceSegmentIndex],
				sourcePoints[sourceSegmentIndex + 1],
				fullWidth,
				referenceWidth,
				alignment,
				chainHorizontal,
				chainVertical,
				outPolygon);
		}

		DuctworkPoint p0;
		DuctworkPoint p1;
		DuctworkPoint p2;
		DuctworkPoint p3;
		if (!ResolveTerminalCurveControlPoints(sourcePoints, sourceSegmentIndex, style, p0, p1, p2, p3)) {
			return false;
		}

		const int sampleCount = 12;
		std::vector<DuctworkPoint> primarySide;
		std::vector<DuctworkPoint> secondarySide;
		primarySide.reserve(static_cast<size_t>(sampleCount) + 1);
		secondarySide.reserve(static_cast<size_t>(sampleCount) + 1);

		Vec2 fallbackDir;
		Vec2 fallbackNormal;
		if (!BuildUnitDirection(p0, p3, fallbackDir, fallbackNormal)) {
			return false;
		}

		for (int sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex) {
			const double t = static_cast<double>(sampleIndex) / static_cast<double>(sampleCount);
			const DuctworkPoint center = EvaluateCubicBezier(p0, p1, p2, p3, t);
			Vec2 tangent = EvaluateCubicBezierTangent(p0, p1, p2, p3, t);
			Vec2 dir;
			if (!Normalize(tangent, dir)) {
				dir = fallbackDir;
			}
			Vec2 normal = PerpCCW(dir);

			double primaryOffset = 0.0;
			double secondaryOffset = 0.0;
			ResolveAlignedOffsets(dir,
				fullWidth,
				referenceWidth,
				alignment,
				chainHorizontal,
				chainVertical,
				primaryOffset,
				secondaryOffset);

			primarySide.push_back(Add(center, normal, primaryOffset));
			secondarySide.push_back(Add(center, normal, secondaryOffset));
		}

		for (size_t i = 0; i < primarySide.size(); ++i) {
			AppendIfDistinct(outPolygon, primarySide[i]);
		}
		for (size_t i = secondarySide.size(); i > 0; --i) {
			AppendIfDistinct(outPolygon, secondarySide[i - 1]);
		}
		return outPolygon.size() >= 4;
	}

	bool ApplyTerminalBodyStyleToSegmentArt(AIArtHandle segmentArt,
		const std::vector<DuctworkPoint>& sourcePoints,
		size_t sourceSegmentIndex,
		double fullWidth,
		double referenceWidth,
		const std::string& alignment,
		bool chainHorizontal,
		bool chainVertical,
		const std::string& style)
	{
		std::vector<DuctworkPoint> polygon;
		if (!BuildTerminalBodyPolygon(sourcePoints,
				sourceSegmentIndex,
				fullWidth,
				referenceWidth,
				alignment,
				chainHorizontal,
				chainVertical,
				style,
				polygon)) {
			return false;
		}

		return SetClosedPathPoints(segmentArt, polygon);
	}

	bool GuidePointsMatchTerminalSegment(const std::vector<DuctworkPoint>& guidePoints,
		const std::vector<DuctworkPoint>& sourcePoints,
		int sourceSegmentIndex,
		double tolerance)
	{
		if (guidePoints.size() < 2 ||
			sourcePoints.size() < 2 ||
			sourceSegmentIndex < 0 ||
			sourceSegmentIndex + 1 >= static_cast<int>(sourcePoints.size())) {
			return false;
		}

		const DuctworkPoint& sourceStart = sourcePoints[static_cast<size_t>(sourceSegmentIndex)];
		const DuctworkPoint& sourceEnd = sourcePoints[static_cast<size_t>(sourceSegmentIndex + 1)];
		const auto closeEnough = [tolerance](const DuctworkPoint& a, const DuctworkPoint& b) {
			return std::hypot(a.x - b.x, a.y - b.y) <= tolerance;
		};

		return (closeEnough(guidePoints[0], sourceStart) && closeEnough(guidePoints[1], sourceEnd)) ||
			(closeEnough(guidePoints[0], sourceEnd) && closeEnough(guidePoints[1], sourceStart));
	}

	bool PathSegmentMatchesReference(const std::vector<DuctworkPoint>& pathPoints,
		int pathSegmentIndex,
		const std::vector<DuctworkPoint>& sourcePoints,
		int sourceSegmentIndex,
		double tolerance)
	{
		if (pathPoints.size() < 2 ||
			pathSegmentIndex < 0 ||
			pathSegmentIndex + 1 >= static_cast<int>(pathPoints.size())) {
			return false;
		}

		std::vector<DuctworkPoint> segmentPoints;
		segmentPoints.push_back(pathPoints[static_cast<size_t>(pathSegmentIndex)]);
		segmentPoints.push_back(pathPoints[static_cast<size_t>(pathSegmentIndex + 1)]);
		return GuidePointsMatchTerminalSegment(segmentPoints, sourcePoints, sourceSegmentIndex, tolerance);
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

	void CollectConnectionParticipantSourceIds(const DuctworkConnection& connection,
		const std::vector<EmorySourceState>& states,
		std::set<std::string>& outSourceIds)
	{
		outSourceIds.clear();
		if (connection.a >= 0 && connection.a < static_cast<int>(states.size()) &&
			!states[connection.a].sourceId.empty()) {
			outSourceIds.insert(states[connection.a].sourceId);
		}
		if (connection.b >= 0 && connection.b < static_cast<int>(states.size()) &&
			!states[connection.b].sourceId.empty()) {
			outSourceIds.insert(states[connection.b].sourceId);
		}
	}

	bool ParticipantSourceIdsMatchConnector(const std::set<std::string>& participants,
		const std::set<std::string>& connectorSourceIds)
	{
		if (participants.empty()) {
			return false;
		}
		if (connectorSourceIds.empty()) {
			return true;
		}

		bool sharesSource = false;
		for (std::set<std::string>::const_iterator it = participants.begin(); it != participants.end(); ++it) {
			if (connectorSourceIds.find(*it) == connectorSourceIds.end()) {
				return false;
			}
			sharesSource = true;
		}
		return sharesSource;
	}

	bool ResolveNetworkConnectorIgnoreTarget(AIArtHandle connectorArt,
		const std::vector<EmorySourceState>& states,
		const std::vector<DuctworkConnection>& connections,
		std::set<std::string>& ioConnectorSourceIds,
		DuctworkPoint& outPoint,
		bool& outMatchedConnection)
	{
		outMatchedConnection = false;
		if (!connectorArt) {
			return false;
		}

		DuctworkPoint connectorCenter;
		if (!ComputeArtCenterPoint(connectorArt, connectorCenter)) {
			return false;
		}

		bool foundConnection = false;
		double bestDist2 = 0.0;
		DuctworkPoint bestPoint = connectorCenter;
		std::set<std::string> bestParticipantIds;

		for (size_t i = 0; i < connections.size(); ++i) {
			const DuctworkConnection& connection = connections[i];
			if (connection.type != kConnectionEndpointToSegment &&
				connection.type != kConnectionSegmentIntersection) {
				continue;
			}

			std::set<std::string> participantIds;
			CollectConnectionParticipantSourceIds(connection, states, participantIds);
			if (!ParticipantSourceIdsMatchConnector(participantIds, ioConnectorSourceIds)) {
				continue;
			}

			const double dist2 = DuctworkMath::Dist2(connectorCenter, connection.point);
			if (!foundConnection || dist2 < bestDist2) {
				foundConnection = true;
				bestDist2 = dist2;
				bestPoint = connection.point;
				bestParticipantIds = participantIds;
			}
		}

		if (foundConnection) {
			ioConnectorSourceIds.insert(bestParticipantIds.begin(), bestParticipantIds.end());
			outPoint = bestPoint;
			outMatchedConnection = true;
			return !ioConnectorSourceIds.empty();
		}

		if (ioConnectorSourceIds.empty()) {
			return false;
		}

		outPoint = connectorCenter;
		return true;
	}

	size_t CountSerializedSegmentWidths(const std::string& serialized)
	{
		if (serialized.empty()) {
			return 0;
		}

		size_t count = 0;
		std::stringstream ss(serialized);
		std::string token;
		while (std::getline(ss, token, ',')) {
			++count;
		}
		return count;
	}

	bool HasStoredSegmentWidths(AIArtHandle sourceArt, size_t expectedSegmentCount)
	{
		std::string serialized;
		if (!sourceArt ||
			!DuctworkMetadata::GetString(sourceArt, kEmorySegmentWidthsKey, serialized) ||
			serialized.empty()) {
			return false;
		}
		return CountSerializedSegmentWidths(serialized) == expectedSegmentCount;
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

	bool PathHasInternalVertexNear(const std::vector<DuctworkPoint>& points, const DuctworkPoint& point, double tolerance)
	{
		if (points.size() < 3) {
			return false;
		}
		const double toleranceSq = tolerance * tolerance;
		for (size_t i = 1; i + 1 < points.size(); ++i) {
			if (DuctworkMath::Dist2(points[i], point) <= toleranceSq) {
				return true;
			}
		}
		return false;
	}

	bool PointNearAnyEndpoint(const std::vector<DuctworkPoint>& points, const DuctworkPoint& point, double tolerance)
	{
		if (points.size() < 2) {
			return false;
		}
		const double toleranceSq = tolerance * tolerance;
		return DuctworkMath::Dist2(points.front(), point) <= toleranceSq ||
			DuctworkMath::Dist2(points.back(), point) <= toleranceSq;
	}

	bool PointOnAnySegmentInterior(const std::vector<DuctworkPoint>& points, const DuctworkPoint& point, double tolerance)
	{
		if (points.size() < 2) {
			return false;
		}
		const double toleranceSq = tolerance * tolerance;
		for (size_t segmentIndex = 0; segmentIndex + 1 < points.size(); ++segmentIndex) {
			double t = 0.0;
			const DuctworkPoint closest = DuctworkMath::ClosestPointOnSegment(points[segmentIndex], points[segmentIndex + 1], point, t);
			if (t > 0.0 && t < 1.0 && DuctworkMath::Dist2(closest, point) <= toleranceSq) {
				return true;
			}
		}
		return false;
	}

	bool IsSameLayerIntersectionMarkerVertex(AIArtHandle sourceArt,
		const std::vector<DuctworkPoint>& sourcePoints,
		size_t vertexIndex,
		const std::vector<AIArtHandle>& allLinePaths)
	{
		if (!sourceArt ||
			sourcePoints.size() < 3 ||
			vertexIndex == 0 ||
			vertexIndex + 1 >= sourcePoints.size()) {
			return false;
		}

		const std::string sourceLayerName = DuctworkGeometry::GetArtLayerName(sourceArt);
		if (!DuctworkLayers::IsColorLayerName(sourceLayerName)) {
			return false;
		}

		const DuctworkPoint& markerPoint = sourcePoints[vertexIndex];
		for (size_t i = 0; i < allLinePaths.size(); ++i) {
			AIArtHandle otherArt = allLinePaths[i];
			if (!otherArt ||
				otherArt == sourceArt ||
				IsGeneratedEmoryArtInternal(otherArt) ||
				IsBackupCenterlineArt(otherArt) ||
				DuctworkGeometry::GetArtLayerName(otherArt) != sourceLayerName) {
				continue;
			}

			std::vector<DuctworkPoint> rawOtherPoints;
			bool otherClosed = false;
			if (!DuctworkGeometry::GetPathPoints(otherArt, rawOtherPoints, otherClosed) ||
				otherClosed ||
				rawOtherPoints.size() < 2) {
				continue;
			}
			std::vector<DuctworkPoint> otherPoints;
			SanitizePolyline(rawOtherPoints, otherPoints);
			if (otherPoints.size() < 2 || PointNearAnyEndpoint(otherPoints, markerPoint, 10.0)) {
				continue;
			}

			if (PointOnAnySegmentInterior(otherPoints, markerPoint, 10.0)) {
				return true;
			}
		}
		return false;
	}

	void CollectSameLayerIntersectionMarkerVertices(AIArtHandle sourceArt,
		const std::vector<DuctworkPoint>& points,
		std::set<int>& outMarkerVertices)
	{
		outMarkerVertices.clear();
		if (!sourceArt || points.size() < 3) {
			return;
		}

		std::vector<AIArtHandle> allLinePaths;
		CollectAllLineLayerPaths(allLinePaths);
		if (allLinePaths.empty()) {
			return;
		}

		for (size_t vertexIndex = 1; vertexIndex + 1 < points.size(); ++vertexIndex) {
			if (IsSameLayerIntersectionMarkerVertex(sourceArt, points, vertexIndex, allLinePaths)) {
				outMarkerVertices.insert(static_cast<int>(vertexIndex));
			}
		}
	}

	bool NormalizeSameLayerIntersectionMarkerWidths(const std::set<int>& markerVertices,
		std::vector<double>& widths)
	{
		if (markerVertices.empty() || widths.empty()) {
			return false;
		}

		bool changed = false;
		for (std::set<int>::const_iterator it = markerVertices.begin(); it != markerVertices.end(); ++it) {
			const int vertexIndex = *it;
			const int prevSegmentIndex = vertexIndex - 1;
			const int nextSegmentIndex = vertexIndex;
			if (prevSegmentIndex < 0 ||
				nextSegmentIndex < 0 ||
				nextSegmentIndex >= static_cast<int>(widths.size())) {
				continue;
			}

			double mergedWidth = (std::max)(widths[static_cast<size_t>(prevSegmentIndex)],
				widths[static_cast<size_t>(nextSegmentIndex)]);
			if (!std::isfinite(mergedWidth) || mergedWidth < kMinDuctWidth) {
				mergedWidth = kMinDuctWidth;
			}

			if (std::fabs(widths[static_cast<size_t>(prevSegmentIndex)] - mergedWidth) > 1e-6) {
				widths[static_cast<size_t>(prevSegmentIndex)] = mergedWidth;
				changed = true;
			}
			if (std::fabs(widths[static_cast<size_t>(nextSegmentIndex)] - mergedWidth) > 1e-6) {
				widths[static_cast<size_t>(nextSegmentIndex)] = mergedWidth;
				changed = true;
			}
		}
		return changed;
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
		const std::set<int>& noTaperVertexIndices,
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
			const int crossedVertexIndex = direction > 0 ? segmentIndex : (segmentIndex + 1);
			if (noTaperVertexIndices.find(crossedVertexIndex) == noTaperVertexIndices.end()) {
				currentWidth *= kStraightTaperRatio;
			}
			if (currentWidth < kMinDuctWidth) {
				currentWidth = kMinDuctWidth;
			}
			widths[segmentIndex] = currentWidth;
		}
	}

	bool AreConsecutiveSegmentsCollinear(const std::vector<DuctworkPoint>& points, int firstSegmentIndex, int secondSegmentIndex)
	{
		Vec2 firstDir;
		Vec2 secondDir;
		if (!BuildSegmentDirection(points, firstSegmentIndex, firstDir) ||
			!BuildSegmentDirection(points, secondSegmentIndex, secondDir)) {
			return false;
		}
		return Dot(firstDir, secondDir) >= kCollinearThreshold;
	}

	void ApplyPolylineTaperContinuityFromAnchor(const std::vector<DuctworkPoint>& points,
		int startSegmentIndex,
		const std::set<int>& noTaperVertexIndices,
		std::vector<double>& widths)
	{
		if (widths.empty() || points.size() < 2 || widths.size() != points.size() - 1) {
			return;
		}

		int clampedStartSegmentIndex = startSegmentIndex;
		if (clampedStartSegmentIndex < 0) {
			clampedStartSegmentIndex = 0;
		}
		if (clampedStartSegmentIndex >= static_cast<int>(widths.size())) {
			clampedStartSegmentIndex = static_cast<int>(widths.size() - 1);
		}

		if (!std::isfinite(widths[clampedStartSegmentIndex]) || widths[clampedStartSegmentIndex] < kMinDuctWidth) {
			widths[clampedStartSegmentIndex] = kMinDuctWidth;
		}

		for (int segmentIndex = clampedStartSegmentIndex + 1;
			segmentIndex < static_cast<int>(widths.size());
			++segmentIndex) {
			const bool straightContinuation = AreConsecutiveSegmentsCollinear(points, segmentIndex - 1, segmentIndex);
			const bool suppressTaperAtMarker =
				straightContinuation &&
				noTaperVertexIndices.find(segmentIndex) != noTaperVertexIndices.end();
			double nextWidth = widths[segmentIndex - 1] *
				(straightContinuation && !suppressTaperAtMarker ? kStraightTaperRatio : 1.0);
			if (!std::isfinite(nextWidth) || nextWidth < kMinDuctWidth) {
				nextWidth = kMinDuctWidth;
			}
			widths[segmentIndex] = nextWidth;
		}

		for (int segmentIndex = clampedStartSegmentIndex - 1;
			segmentIndex >= 0;
			--segmentIndex) {
			const bool straightContinuation = AreConsecutiveSegmentsCollinear(points, segmentIndex, segmentIndex + 1);
			const bool suppressTaperAtMarker =
				straightContinuation &&
				noTaperVertexIndices.find(segmentIndex + 1) != noTaperVertexIndices.end();
			double nextWidth = widths[segmentIndex + 1] *
				(straightContinuation && !suppressTaperAtMarker ? kStraightTaperRatio : 1.0);
			if (!std::isfinite(nextWidth) || nextWidth < kMinDuctWidth) {
				nextWidth = kMinDuctWidth;
			}
			widths[segmentIndex] = nextWidth;
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
		std::set<int> noTaperVertexIndices;
		CollectSameLayerIntersectionMarkerVertices(sourceArt, points, noTaperVertexIndices);
		for (size_t chainIndex = 0; chainIndex < chains.size(); ++chainIndex) {
			const StraightChainInfo& chain = chains[chainIndex];
			if (chain.startSegmentIndex < 0 || chain.endSegmentIndex <= chain.startSegmentIndex ||
				chain.startSegmentIndex >= static_cast<int>(widths.size())) {
				continue;
			}

			if (clampedStartSegmentIndex < chain.startSegmentIndex) {
				ApplyStraightChainTaperFromAnchor(chain, seedWidths, chain.startSegmentIndex, 1, noTaperVertexIndices, widths);
				continue;
			}

			if (clampedStartSegmentIndex > chain.endSegmentIndex) {
				ApplyStraightChainTaperFromAnchor(chain, seedWidths, chain.endSegmentIndex, -1, noTaperVertexIndices, widths);
				continue;
			}

			ApplyStraightChainTaperFromAnchor(chain, seedWidths, clampedStartSegmentIndex, -1, noTaperVertexIndices, widths);
			ApplyStraightChainTaperFromAnchor(chain, seedWidths, clampedStartSegmentIndex, 1, noTaperVertexIndices, widths);
		}

		ApplyPolylineTaperContinuityFromAnchor(points, clampedStartSegmentIndex, noTaperVertexIndices, widths);
	}

	bool NormalizeBlueOrangeUnitEndpointTaper(const std::string& layerName,
		const std::vector<DuctworkPoint>& points,
		std::vector<double>& widths)
	{
		if (!IsBlueOrOrangeRunLayer(layerName) ||
			points.size() < 2 ||
			widths.empty() ||
			widths.size() != points.size() - 1) {
			return false;
		}

		std::vector<DuctworkPoint> unitAttachmentPoints;
		CollectUnitAttachmentPoints(unitAttachmentPoints);
		if (unitAttachmentPoints.empty()) {
			return false;
		}

		const bool unitAtStart = IsPointNearAny(points.front(), unitAttachmentPoints, 10.0);
		const bool unitAtEnd = IsPointNearAny(points.back(), unitAttachmentPoints, 10.0);
		if (unitAtStart == unitAtEnd) {
			return false;
		}

		const int rootSegmentIndex = unitAtStart ? 0 : static_cast<int>(widths.size() - 1);
		const int direction = unitAtStart ? 1 : -1;
		double currentWidth = widths[static_cast<size_t>(rootSegmentIndex)];
		if (!std::isfinite(currentWidth) || currentWidth < kMinDuctWidth) {
			currentWidth = kMinDuctWidth;
		}

		bool changed = false;
		if (!NearlyEqual(widths[static_cast<size_t>(rootSegmentIndex)], currentWidth, 0.001)) {
			widths[static_cast<size_t>(rootSegmentIndex)] = currentWidth;
			changed = true;
		}

		for (int segmentIndex = rootSegmentIndex + direction;
			segmentIndex >= 0 && segmentIndex < static_cast<int>(widths.size());
			segmentIndex += direction) {
			const double nextWidth = (std::max)(currentWidth * kStraightTaperRatio, kMinDuctWidth);
			const double existingWidth = widths[static_cast<size_t>(segmentIndex)];
			if (!std::isfinite(existingWidth) ||
				existingWidth > currentWidth + 0.001 ||
				NearlyEqual(existingWidth, currentWidth, 0.001)) {
				widths[static_cast<size_t>(segmentIndex)] = nextWidth;
				changed = true;
			}
			currentWidth = widths[static_cast<size_t>(segmentIndex)];
		}

		return changed;
	}

	bool ReadStoredSourceBodyWidth(AIArtHandle sourceArt, double& outWidth)
	{
		outWidth = 0.0;
		return sourceArt && DuctworkMetadata::GetDouble(sourceArt, kEmorySourceBodyWidthKey, outWidth) && outWidth > 0.0;
	}

	bool IsGuideLikeWidth(double width)
	{
		return std::fabs(width - kGuideStrokeWidth) <= 1e-6;
	}

	bool AreAllGuideLikeWidths(const std::vector<double>& widths)
	{
		if (widths.empty()) {
			return false;
		}
		for (size_t i = 0; i < widths.size(); ++i) {
			if (!IsGuideLikeWidth(widths[i])) {
				return false;
			}
		}
		return true;
	}

	size_t CountGuideLikeWidths(const std::vector<double>& widths)
	{
		size_t count = 0;
		for (size_t i = 0; i < widths.size(); ++i) {
			if (IsGuideLikeWidth(widths[i])) {
				++count;
			}
		}
		return count;
	}

	bool ReadStoredSourceBodyWidth(AIArtHandle sourceArt, double& outWidth);
	bool GetMaxStyleStrokeWidth(AIArtHandle art, double& outWidth);
	bool GetSimpleStrokeWidth(AIArtHandle path, double& outWidth);
	bool FindGeneratedBodyWidthForSourceId(const std::string& sourceId, double& outWidth);

	bool ResolveRecoveredGuideLikeWidth(AIArtHandle sourceArt,
		const std::string& sourceId,
		double fallbackWidth,
		double& outWidth)
	{
		outWidth = 0.0;

		AIArtHandle backupArt = nullptr;
		DuctworkPath backupPath;
		if (GetPrimaryBackupCenterlineForSourceId(sourceId, backupArt, backupPath) &&
			backupArt &&
			backupArt != sourceArt) {
			double backupWidth = 0.0;
			if (ReadStoredSourceBodyWidth(backupArt, backupWidth) &&
				backupWidth > 0.0 &&
				!IsGuideLikeWidth(backupWidth)) {
				outWidth = backupWidth;
				return true;
			}

			if ((GetMaxStyleStrokeWidth(backupArt, backupWidth) || GetSimpleStrokeWidth(backupArt, backupWidth)) &&
				backupWidth > 0.0 &&
				!IsGuideLikeWidth(backupWidth)) {
				outWidth = backupWidth;
				return true;
			}
		}

		double generatedWidth = 0.0;
		if (FindGeneratedBodyWidthForSourceId(sourceId, generatedWidth) &&
			generatedWidth > 0.0 &&
			!IsGuideLikeWidth(generatedWidth)) {
			outWidth = generatedWidth;
			return true;
		}

		if (fallbackWidth > 0.0 && !IsGuideLikeWidth(fallbackWidth)) {
			outWidth = fallbackWidth;
			return true;
		}

		outWidth = kDefaultDuctWidth;
		if (outWidth < kMinDuctWidth) {
			outWidth = kMinDuctWidth;
		}
		return true;
	}

	void WriteSegmentWidths(AIArtHandle sourceArt, const std::vector<double>& widths);

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

	bool ReadStoredSourceStrokeExplicit(AIArtHandle sourceArt)
	{
		double explicitValue = 0.0;
		return sourceArt &&
			DuctworkMetadata::GetDouble(sourceArt, kEmorySourceStrokeExplicitKey, explicitValue) &&
			explicitValue > 0.5;
	}

	void WriteStoredSourceStrokeExplicit(AIArtHandle sourceArt, bool explicitValue)
	{
		if (!sourceArt) {
			return;
		}
		if (explicitValue) {
			DuctworkMetadata::SetDouble(sourceArt, kEmorySourceStrokeExplicitKey, 1.0);
		} else {
			DuctworkMetadata::RemoveKey(sourceArt, kEmorySourceStrokeExplicitKey);
		}
	}

	void CollectCenterlineAndBackupArtsForSourceId(const std::string& sourceId, std::vector<AIArtHandle>& outArts)
	{
		outArts.clear();
		if (sourceId.empty()) {
			return;
		}

		std::set<AIArtHandle> seen;
		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			std::string artSourceId;
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, artSourceId) || artSourceId.empty()) {
				artSourceId = ReadEmorySourceIdFromNote(art);
			}
			if (artSourceId != sourceId || !seen.insert(art).second) {
				continue;
			}

			outArts.push_back(art);
		}
	}

	bool ResolveStoredSourceStrokeWidthForSourceId(const std::string& sourceId, double& outWidth)
	{
		outWidth = 0.0;
		if (sourceId.empty()) {
			return false;
		}

		std::vector<AIArtHandle> arts;
		CollectCenterlineAndBackupArtsForSourceId(sourceId, arts);
		bool found = false;
		for (size_t i = 0; i < arts.size(); ++i) {
			double width = 0.0;
			if (!ReadStoredSourceStrokeWidth(arts[i], width) || width <= 0.0) {
				continue;
			}
			if (!found || width < outWidth) {
				outWidth = width;
				found = true;
			}
		}
		if (found) {
			return true;
		}

		for (size_t i = 0; i < arts.size(); ++i) {
			double width = 0.0;
			if (!(GetMaxStyleStrokeWidth(arts[i], width) || GetSimpleStrokeWidth(arts[i], width)) || width <= 0.0) {
				continue;
			}
			if (std::fabs(width - kGuideStrokeWidth) <= 1e-6) {
				continue;
			}
			if (!found || width < outWidth) {
				outWidth = width;
				found = true;
			}
		}
		return found;
	}

	void WriteStoredSourceStrokeWidthForSourceId(const std::string& sourceId, double width)
	{
		if (sourceId.empty() || width <= 0.0) {
			return;
		}

		std::vector<AIArtHandle> arts;
		CollectCenterlineAndBackupArtsForSourceId(sourceId, arts);
		for (size_t i = 0; i < arts.size(); ++i) {
			WriteStoredSourceStrokeWidth(arts[i], width);
		}
	}

	bool HasExplicitStoredSourceStrokeForSourceId(const std::string& sourceId)
	{
		if (sourceId.empty()) {
			return false;
		}

		std::vector<AIArtHandle> arts;
		CollectCenterlineAndBackupArtsForSourceId(sourceId, arts);
		for (size_t i = 0; i < arts.size(); ++i) {
			if (ReadStoredSourceStrokeExplicit(arts[i])) {
				return true;
			}
		}
		return false;
	}

	void WriteStoredSourceStrokeExplicitForSourceId(const std::string& sourceId, bool explicitValue)
	{
		if (sourceId.empty()) {
			return;
		}

		std::vector<AIArtHandle> arts;
		CollectCenterlineAndBackupArtsForSourceId(sourceId, arts);
		for (size_t i = 0; i < arts.size(); ++i) {
			WriteStoredSourceStrokeExplicit(arts[i], explicitValue);
		}
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
		double storedWidth = 0.0;
		if (ReadStoredSourceBodyWidth(sourceArt, storedWidth) && !IsGuideLikeWidth(storedWidth)) {
			outWidth = storedWidth;
			return true;
		}

		double sourceStrokeWidth = 0.0;
		if (GetMaxStyleStrokeWidth(sourceArt, sourceStrokeWidth) || GetSimpleStrokeWidth(sourceArt, sourceStrokeWidth)) {
			if (sourceStrokeWidth > 0.0 && std::fabs(sourceStrokeWidth - kGuideStrokeWidth) > 1e-6) {
				outWidth = sourceStrokeWidth;
				WriteStoredSourceBodyWidth(sourceArt, outWidth);
				return true;
			}
		}

		if (FindGeneratedBodyWidthForSourceId(sourceId, outWidth)) {
			WriteStoredSourceBodyWidth(sourceArt, outWidth);
			return true;
		}

		if (ResolveRecoveredGuideLikeWidth(sourceArt, sourceId, storedWidth, outWidth) && outWidth > 0.0) {
			WriteStoredSourceBodyWidth(sourceArt, outWidth);
			return true;
		}

		if (sourceStrokeWidth > 0.0 && !IsGuideLikeWidth(sourceStrokeWidth)) {
			outWidth = sourceStrokeWidth;
			WriteStoredSourceBodyWidth(sourceArt, outWidth);
			return true;
		}
		return false;
	}

	bool ResolveSourceStrokeWidth(AIArtHandle sourceArt, const std::string& sourceId, double bodyWidth, double& outWidth)
	{
		outWidth = 0.0;
		double generatedStrokeWidth = 0.0;
		if (FindGeneratedStrokeWidthForSourceId(sourceId, generatedStrokeWidth) && generatedStrokeWidth > 0.0) {
			outWidth = generatedStrokeWidth;
			WriteStoredSourceStrokeWidthForSourceId(sourceId, outWidth);
			WriteStoredSourceStrokeExplicitForSourceId(sourceId, true);
			return true;
		}

		if (HasExplicitStoredSourceStrokeForSourceId(sourceId) &&
			ResolveStoredSourceStrokeWidthForSourceId(sourceId, outWidth)) {
			WriteStoredSourceStrokeWidthForSourceId(sourceId, outWidth);
			return true;
		}

		outWidth = ComputeBodyStrokeWidth(bodyWidth);
		if (outWidth > 0.0) {
			WriteStoredSourceStrokeWidthForSourceId(sourceId, outWidth);
			WriteStoredSourceStrokeExplicitForSourceId(sourceId, false);
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

	void NormalizeGuideLikeStoredWidths(AIArtHandle sourceArt,
		const std::string& sourceId,
		const std::vector<DuctworkPoint>& points,
		int startSegmentIndex,
		double& ioBodyWidth,
		std::vector<double>& ioWidths)
	{
		const size_t guideLikeWidthCount = CountGuideLikeWidths(ioWidths);
		const bool guideLikeBody = IsGuideLikeWidth(ioBodyWidth);
		const bool guideLikeWidths = (guideLikeWidthCount > 0);
		const bool allGuideLikeWidths = (guideLikeWidthCount == ioWidths.size());
		if (!sourceArt ||
			sourceId.empty() ||
			points.size() < 2 ||
			ioWidths.size() != points.size() - 1 ||
			(!guideLikeBody && !guideLikeWidths)) {
			return;
		}

		double recoveredWidth = 0.0;
		ResolveRecoveredGuideLikeWidth(sourceArt, sourceId, guideLikeBody ? 0.0 : ioBodyWidth, recoveredWidth);
		if (recoveredWidth < kMinDuctWidth) {
			recoveredWidth = kMinDuctWidth;
		}

		for (size_t i = 0; i < ioWidths.size(); ++i) {
			if (!IsGuideLikeWidth(ioWidths[i]) && ioWidths[i] > recoveredWidth) {
				recoveredWidth = ioWidths[i];
			}
		}

		if (guideLikeBody) {
			ioBodyWidth = recoveredWidth;
		}

		if (allGuideLikeWidths) {
			ioWidths.assign(points.size() - 1, recoveredWidth);
			ApplyDefaultStraightChainTapers(sourceArt, points, startSegmentIndex, ioWidths);
		} else {
			for (size_t i = 0; i < ioWidths.size(); ++i) {
				if (IsGuideLikeWidth(ioWidths[i])) {
					ioWidths[i] = recoveredWidth;
				}
				if (ioWidths[i] < kMinDuctWidth) {
					ioWidths[i] = kMinDuctWidth;
				}
			}
		}
		WriteStoredSourceBodyWidth(sourceArt, ioBodyWidth);
		WriteSegmentWidths(sourceArt, ioWidths);

		std::ostringstream logStream;
		logStream << "Emory width-normalize sourceId=" << sourceId
			<< " recoveredWidth=" << recoveredWidth
			<< " guideLikeBody=" << (guideLikeBody ? 1 : 0)
			<< " guideLikeWidths=" << guideLikeWidthCount
			<< " allGuideLikeWidths=" << (allGuideLikeWidths ? 1 : 0)
			<< " segmentCount=" << ioWidths.size()
			<< " startSegmentIndex=" << startSegmentIndex;
		DuctworkLog::Write(logStream.str());
	}

	void WriteSegmentWidths(AIArtHandle sourceArt, const std::vector<double>& widths)
	{
		if (!sourceArt) {
			return;
		}
		DuctworkMetadata::SetString(sourceArt, kEmorySegmentWidthsKey, SerializeSegmentWidths(widths));
	}

	std::string SerializeSegmentIndexSet(const std::set<size_t>& indices)
	{
		std::ostringstream out;
		bool first = true;
		for (std::set<size_t>::const_iterator it = indices.begin(); it != indices.end(); ++it) {
			if (!first) {
				out << ",";
			}
			first = false;
			out << *it;
		}
		return out.str();
	}

	std::string SerializePointForLog(const DuctworkPoint& point)
	{
		std::ostringstream out;
		out << "[" << point.x << "," << point.y << "]";
		return out.str();
	}

	void ReadOmittedSegmentIndices(AIArtHandle art, std::set<size_t>& outIndices)
	{
		outIndices.clear();
		if (!art) {
			return;
		}

		std::string serialized;
		if (!DuctworkMetadata::GetString(art, kEmoryOmittedSegmentIndicesKey, serialized) || serialized.empty()) {
			return;
		}

		std::stringstream ss(serialized);
		std::string token;
		while (std::getline(ss, token, ',')) {
			if (token.empty()) {
				continue;
			}
			const int value = std::atoi(token.c_str());
			if (value >= 0) {
				outIndices.insert(static_cast<size_t>(value));
			}
		}
	}

	void WriteOmittedSegmentIndices(AIArtHandle art, const std::set<size_t>& indices)
	{
		if (!art) {
			return;
		}
		if (indices.empty()) {
			DuctworkMetadata::RemoveKey(art, kEmoryOmittedSegmentIndicesKey);
			return;
		}
		DuctworkMetadata::SetString(art, kEmoryOmittedSegmentIndicesKey, SerializeSegmentIndexSet(indices));
	}

	int ReadStartSegmentIndex(AIArtHandle sourceArt, size_t segmentCount)
	{
		if (!sourceArt || segmentCount == 0) {
			return 0;
		}

		int directionalStartIndex = 0;
		if (TryResolveDirectionalStartForArt(sourceArt, segmentCount, directionalStartIndex)) {
			return directionalStartIndex;
		}

		double startValue = 0.0;
		if (!DuctworkMetadata::GetDouble(sourceArt, kEmoryStartSegmentIndexKey, startValue)) {
			return ResolveDefaultStartSegmentIndex(sourceArt, segmentCount);
		}

		return ClampStartSegmentIndex(static_cast<int>(startValue), segmentCount);
	}

	bool HasExplicitStartSegmentIndex(AIArtHandle sourceArt)
	{
		if (sourceArt) {
			DuctworkPath path;
			if (BuildProcessPathForArt(sourceArt, path) && path.points.size() >= 2 && !path.closed) {
				std::vector<DuctworkPoint> points;
				SanitizePolyline(path.points, points);
				if (points.size() >= 2) {
					int directionalStartIndex = 0;
					if (TryResolveDirectionalDefaultStartSegmentIndex(sourceArt,
						path,
						points,
						points.size() - 1,
						directionalStartIndex)) {
						return false;
					}
				}
			}
		}
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

	void SplitEmoryCenterlineMetadataInternal(AIArtHandle sourceArt,
		size_t splitSegmentIndex,
		AIArtHandle firstArt,
		AIArtHandle secondArt)
{
	if (!sourceArt || !firstArt || !secondArt) {
		return;
	}

	std::vector<DuctworkPoint> points;
	bool closed = false;
	if (!DuctworkGeometry::GetPathPoints(sourceArt, points, closed) || closed || points.size() < 2) {
		return;
	}

	const size_t segmentCount = points.size() - 1;
	if (segmentCount == 0 || splitSegmentIndex >= segmentCount) {
		return;
	}

	std::string sourceId;
	if (!DuctworkGeometry::EnsureEmorySourceId(sourceArt, sourceId)) {
		return;
	}

	double storedBodyWidth = 0.0;
	const bool hasStoredBodyWidth = ReadStoredSourceBodyWidth(sourceArt, storedBodyWidth) && storedBodyWidth > 0.0;
	const bool hasStoredWidths = HasStoredSegmentWidths(sourceArt, segmentCount);
	double generatedBodyWidth = 0.0;
	const bool hasGeneratedBodyWidth = FindGeneratedBodyWidthForSourceId(sourceId, generatedBodyWidth) && generatedBodyWidth > 0.0;
	const bool preserveWidthMetadata = hasStoredWidths || hasStoredBodyWidth || hasGeneratedBodyWidth;

	double defaultWidth = hasStoredBodyWidth ? storedBodyWidth :
		(hasGeneratedBodyWidth ? generatedBodyWidth : kDefaultDuctWidth);
	if (defaultWidth < kMinDuctWidth) {
		defaultWidth = kMinDuctWidth;
	}

	std::vector<double> widths;
	if (preserveWidthMetadata) {
		ReadSegmentWidths(sourceArt, segmentCount, defaultWidth, widths);
		if (!hasStoredWidths) {
			ApplyDefaultStraightChainTapers(sourceArt,
				points,
				ReadStartSegmentIndex(sourceArt, segmentCount),
				widths);
		}
		if (widths.empty()) {
			widths.assign(segmentCount, defaultWidth);
		}
	}

	const size_t firstSegmentCount = splitSegmentIndex + 1;
	const size_t secondSegmentCount = segmentCount - splitSegmentIndex;
	if (firstSegmentCount == 0 || secondSegmentCount == 0) {
		return;
	}

	std::vector<double> firstWidths;
	std::vector<double> secondWidths;
	if (preserveWidthMetadata) {
		if (firstSegmentCount > widths.size() || splitSegmentIndex > widths.size()) {
			return;
		}
		firstWidths.assign(widths.begin(), widths.begin() + static_cast<std::ptrdiff_t>(firstSegmentCount));
		secondWidths.assign(widths.begin() + static_cast<std::ptrdiff_t>(splitSegmentIndex), widths.end());
		if (secondWidths.size() > secondSegmentCount) {
			secondWidths.resize(secondSegmentCount);
		}

		WriteSegmentWidths(firstArt, firstWidths);
		WriteSegmentWidths(secondArt, secondWidths);
	} else {
		DuctworkMetadata::RemoveKey(firstArt, kEmorySegmentWidthsKey);
		DuctworkMetadata::RemoveKey(secondArt, kEmorySegmentWidthsKey);
	}

	const bool hasExplicitStart = HasExplicitStartSegmentIndex(sourceArt);
	const int sourceStartIndex = ReadStartSegmentIndex(sourceArt, segmentCount);
	int firstStartIndex = 0;
	int secondStartIndex = 0;
	if (sourceStartIndex <= static_cast<int>(splitSegmentIndex)) {
		firstStartIndex = (std::max)(0, (std::min)(sourceStartIndex, static_cast<int>(firstSegmentCount - 1)));
		secondStartIndex = 0;
	} else {
		firstStartIndex = static_cast<int>(firstSegmentCount - 1);
		secondStartIndex = sourceStartIndex - static_cast<int>(splitSegmentIndex);
		if (secondStartIndex < 0) {
			secondStartIndex = 0;
		}
		if (secondStartIndex >= static_cast<int>(secondSegmentCount)) {
			secondStartIndex = static_cast<int>(secondSegmentCount - 1);
		}
	}

	if (hasExplicitStart) {
		WriteStartSegmentIndex(firstArt, firstStartIndex);
		WriteStartSegmentIndex(secondArt, secondStartIndex);
	} else {
		ClearStartSegmentIndex(firstArt);
		ClearStartSegmentIndex(secondArt);
	}

	if (preserveWidthMetadata && !firstWidths.empty()) {
		const int storedIndex = hasExplicitStart ? firstStartIndex : 0;
		WriteStoredSourceBodyWidth(firstArt, firstWidths[(std::max)(0, (std::min)(storedIndex, static_cast<int>(firstWidths.size() - 1)))]);
	}
	if (preserveWidthMetadata && !secondWidths.empty()) {
		const int storedIndex = hasExplicitStart ? secondStartIndex : 0;
		WriteStoredSourceBodyWidth(secondArt, secondWidths[(std::max)(0, (std::min)(storedIndex, static_cast<int>(secondWidths.size() - 1)))]);
	} else if (!preserveWidthMetadata) {
		DuctworkMetadata::RemoveKey(firstArt, kEmorySourceBodyWidthKey);
		DuctworkMetadata::RemoveKey(secondArt, kEmorySourceBodyWidthKey);
	}

	double storedSourceStrokeWidth = 0.0;
	const bool hasStoredSourceStrokeWidth = ReadStoredSourceStrokeWidth(sourceArt, storedSourceStrokeWidth) && storedSourceStrokeWidth > 0.0;
	if (hasStoredSourceStrokeWidth) {
		WriteStoredSourceStrokeWidth(firstArt, storedSourceStrokeWidth);
		WriteStoredSourceStrokeWidth(secondArt, storedSourceStrokeWidth);
	} else {
		DuctworkMetadata::RemoveKey(firstArt, kEmorySourceStrokeWidthKey);
		DuctworkMetadata::RemoveKey(secondArt, kEmorySourceStrokeWidthKey);
	}

	const bool hasExplicitStoredStroke = ReadStoredSourceStrokeExplicit(sourceArt);
	WriteStoredSourceStrokeExplicit(firstArt, hasExplicitStoredStroke);
	WriteStoredSourceStrokeExplicit(secondArt, hasExplicitStoredStroke);

	if (ReadFinalSegmentThicknessFlag(sourceArt, kEmoryOmitStartSegmentThicknessKey)) {
		DuctworkMetadata::SetDouble(firstArt, kEmoryOmitStartSegmentThicknessKey, 1.0);
	} else {
		DuctworkMetadata::RemoveKey(firstArt, kEmoryOmitStartSegmentThicknessKey);
	}
	DuctworkMetadata::RemoveKey(secondArt, kEmoryOmitStartSegmentThicknessKey);

	if (ReadFinalSegmentThicknessFlag(sourceArt, kEmoryOmitEndSegmentThicknessKey)) {
		DuctworkMetadata::SetDouble(secondArt, kEmoryOmitEndSegmentThicknessKey, 1.0);
	} else {
		DuctworkMetadata::RemoveKey(secondArt, kEmoryOmitEndSegmentThicknessKey);
	}
	DuctworkMetadata::RemoveKey(firstArt, kEmoryOmitEndSegmentThicknessKey);
	WriteTerminalSegmentStyle(firstArt, true, ReadTerminalSegmentStyle(sourceArt, true));
	WriteTerminalSegmentStyle(firstArt, false, "straight");
	WriteTerminalSegmentStyle(secondArt, true, "straight");
	WriteTerminalSegmentStyle(secondArt, false, ReadTerminalSegmentStyle(sourceArt, false));

	std::set<size_t> omittedIndices;
	ReadOmittedSegmentIndices(sourceArt, omittedIndices);
	std::set<size_t> firstOmitted;
	std::set<size_t> secondOmitted;
	for (std::set<size_t>::const_iterator it = omittedIndices.begin(); it != omittedIndices.end(); ++it) {
		if (*it <= splitSegmentIndex) {
			firstOmitted.insert(*it);
		}
		if (*it >= splitSegmentIndex) {
			secondOmitted.insert(*it - splitSegmentIndex);
		}
	}
	WriteOmittedSegmentIndices(firstArt, firstOmitted);
	WriteOmittedSegmentIndices(secondArt, secondOmitted);
}

	double DistancePointToSegment(const DuctworkPoint& point, const DuctworkPoint& start, const DuctworkPoint& end)
	{
		const double dx = end.x - start.x;
		const double dy = end.y - start.y;
		const double lenSq = dx * dx + dy * dy;
		if (lenSq <= 1e-9) {
			return std::hypot(point.x - start.x, point.y - start.y);
		}

		double t = ((point.x - start.x) * dx + (point.y - start.y) * dy) / lenSq;
		if (t < 0.0) {
			t = 0.0;
		} else if (t > 1.0) {
			t = 1.0;
		}

		const double projX = start.x + dx * t;
		const double projY = start.y + dy * t;
		return std::hypot(point.x - projX, point.y - projY);
	}

	bool MapFragmentSegmentsToBackupIndices(const std::vector<DuctworkPoint>& fragmentPoints,
		const std::vector<DuctworkPoint>& backupPoints,
		std::vector<int>& outIndices)
	{
		outIndices.clear();
		if (fragmentPoints.size() < 2 || backupPoints.size() < 2) {
			return false;
		}

		const size_t fragmentSegmentCount = fragmentPoints.size() - 1;
		const size_t backupSegmentCount = backupPoints.size() - 1;
		outIndices.resize(fragmentSegmentCount, -1);

		for (size_t fragmentIndex = 0; fragmentIndex < fragmentSegmentCount; ++fragmentIndex) {
			Vec2 fragmentDir;
			Vec2 fragmentNormal;
			if (!BuildUnitDirection(fragmentPoints[fragmentIndex], fragmentPoints[fragmentIndex + 1], fragmentDir, fragmentNormal)) {
				return false;
			}

			DuctworkPoint midpoint;
			midpoint.x = (fragmentPoints[fragmentIndex].x + fragmentPoints[fragmentIndex + 1].x) * 0.5;
			midpoint.y = (fragmentPoints[fragmentIndex].y + fragmentPoints[fragmentIndex + 1].y) * 0.5;

			double bestScore = 0.0;
			bool bestScoreSet = false;
			int bestIndex = -1;
			for (size_t backupIndex = 0; backupIndex < backupSegmentCount; ++backupIndex) {
				Vec2 backupDir;
				Vec2 backupNormal;
				if (!BuildUnitDirection(backupPoints[backupIndex], backupPoints[backupIndex + 1], backupDir, backupNormal)) {
					continue;
				}

				const double alignment = std::fabs(fragmentDir.x * backupDir.x + fragmentDir.y * backupDir.y);
				if (alignment < 0.995) {
					continue;
				}

				const double distance = DistancePointToSegment(midpoint, backupPoints[backupIndex], backupPoints[backupIndex + 1]);
				const double score = distance + ((1.0 - alignment) * 1000.0);
				if (!bestScoreSet || score < bestScore) {
					bestScore = score;
					bestIndex = static_cast<int>(backupIndex);
					bestScoreSet = true;
				}
			}

			if (bestIndex < 0) {
				return false;
			}
			outIndices[fragmentIndex] = bestIndex;
		}

		for (size_t i = 1; i < outIndices.size(); ++i) {
			if (outIndices[i] < outIndices[i - 1]) {
				return false;
			}
			const int delta = outIndices[i] - outIndices[i - 1];
			if (delta != 0 && delta != 1) {
				return false;
			}
		}

		return true;
	}

	void LocalizeVisibleFragmentMetadataFromBackup(const std::set<std::string>& sourceIds, bool forceUpdate = false)
	{
		if (sourceIds.empty()) {
			return;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (std::set<std::string>::const_iterator sourceIt = sourceIds.begin(); sourceIt != sourceIds.end(); ++sourceIt) {
			if (sourceIt->empty()) {
				continue;
			}

			std::vector<AIArtHandle> backupArts;
			if (!CollectBackupCenterlinesForSourceId(*sourceIt, backupArts) || backupArts.empty()) {
				continue;
			}

			AIArtHandle backupArt = backupArts.front();
			std::vector<DuctworkPoint> backupPoints;
			bool backupClosed = false;
			if (!DuctworkGeometry::GetPathPoints(backupArt, backupPoints, backupClosed) || backupClosed || backupPoints.size() < 2) {
				continue;
			}

			const size_t backupSegmentCount = backupPoints.size() - 1;
			double defaultWidth = kDefaultDuctWidth;
			if (!ResolveSourceBodyWidth(backupArt, *sourceIt, defaultWidth) || defaultWidth <= 0.0) {
				defaultWidth = kDefaultDuctWidth;
			}
			if (defaultWidth < kMinDuctWidth) {
				defaultWidth = kMinDuctWidth;
			}

			std::vector<double> backupWidths;
			ReadSegmentWidths(backupArt, backupSegmentCount, defaultWidth, backupWidths);
			if (!HasStoredSegmentWidths(backupArt, backupSegmentCount)) {
				ApplyDefaultStraightChainTapers(backupArt,
					backupPoints,
					ReadStartSegmentIndex(backupArt, backupSegmentCount),
					backupWidths);
			}
			if (backupWidths.empty()) {
				continue;
			}

			const bool hasBackupExplicitStart = HasExplicitStartSegmentIndex(backupArt);
			const int backupStartIndex = ReadStartSegmentIndex(backupArt, backupSegmentCount);
			const bool omitStart = ReadFinalSegmentThicknessFlag(backupArt, kEmoryOmitStartSegmentThicknessKey);
			const bool omitEnd = ReadFinalSegmentThicknessFlag(backupArt, kEmoryOmitEndSegmentThicknessKey);

			for (size_t i = 0; i < allPaths.size(); ++i) {
				AIArtHandle art = allPaths[i];
				if (!art || IsGeneratedEmoryArtInternal(art) || IsBackupCenterlineArt(art)) {
					continue;
				}

				std::string artSourceId;
				if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, artSourceId) || artSourceId.empty()) {
					artSourceId = ReadEmorySourceIdFromNote(art);
				}
				if (artSourceId != *sourceIt) {
					continue;
				}

				std::vector<DuctworkPoint> fragmentPoints;
				bool fragmentClosed = false;
				if (!DuctworkGeometry::GetPathPoints(art, fragmentPoints, fragmentClosed) || fragmentClosed || fragmentPoints.size() < 2) {
					continue;
				}

				const size_t fragmentSegmentCount = fragmentPoints.size() - 1;
				std::string serializedWidths;
				const bool hasSerializedWidths =
					DuctworkMetadata::GetString(art, kEmorySegmentWidthsKey, serializedWidths) &&
					!serializedWidths.empty();
				const size_t serializedWidthCount = hasSerializedWidths ? CountSerializedSegmentWidths(serializedWidths) : 0;
				const bool fragmentHasExplicitStart = HasExplicitStartSegmentIndex(art);
				const int fragmentStartIndex = ReadStartSegmentIndex(art, fragmentSegmentCount);
				const bool needsMigration =
					forceUpdate ||
					(hasSerializedWidths && serializedWidthCount != fragmentSegmentCount) ||
					(fragmentHasExplicitStart && (fragmentStartIndex < 0 || fragmentStartIndex >= static_cast<int>(fragmentSegmentCount)));
				if (!needsMigration) {
					continue;
				}

				std::vector<int> backupIndices;
				if (!MapFragmentSegmentsToBackupIndices(fragmentPoints, backupPoints, backupIndices) || backupIndices.empty()) {
					continue;
				}

				std::vector<double> fragmentWidths;
				fragmentWidths.reserve(backupIndices.size());
				for (size_t index = 0; index < backupIndices.size(); ++index) {
					const int backupIndex = backupIndices[index];
					if (backupIndex < 0 || backupIndex >= static_cast<int>(backupWidths.size())) {
						fragmentWidths.push_back(defaultWidth);
					} else {
						fragmentWidths.push_back(backupWidths[backupIndex]);
					}
				}

				WriteSegmentWidths(art, fragmentWidths);

				int localStartIndex = 0;
				if (!backupIndices.empty()) {
					if (backupStartIndex <= backupIndices.front()) {
						localStartIndex = 0;
					} else if (backupStartIndex >= backupIndices.back()) {
						localStartIndex = static_cast<int>(backupIndices.size() - 1);
					} else {
						for (size_t index = 0; index < backupIndices.size(); ++index) {
							if (backupIndices[index] <= backupStartIndex) {
								localStartIndex = static_cast<int>(index);
							}
						}
					}
				}

				if (hasBackupExplicitStart) {
					WriteStartSegmentIndex(art, localStartIndex);
				} else {
					ClearStartSegmentIndex(art);
				}

				if (!fragmentWidths.empty()) {
					WriteStoredSourceBodyWidth(art, fragmentWidths[hasBackupExplicitStart ? localStartIndex : 0]);
				}

				if (omitStart && !backupIndices.empty() && backupIndices.front() == 0) {
					DuctworkMetadata::SetDouble(art, kEmoryOmitStartSegmentThicknessKey, 1.0);
				} else {
					DuctworkMetadata::RemoveKey(art, kEmoryOmitStartSegmentThicknessKey);
				}

				if (omitEnd && !backupIndices.empty() &&
					backupIndices.back() == static_cast<int>(backupSegmentCount - 1)) {
					DuctworkMetadata::SetDouble(art, kEmoryOmitEndSegmentThicknessKey, 1.0);
				} else {
					DuctworkMetadata::RemoveKey(art, kEmoryOmitEndSegmentThicknessKey);
				}
			}
		}
	}

	void ResolveFragmentOmitThicknessFlagsFromBackup(const std::string& sourceId,
		const std::vector<DuctworkPoint>& fragmentPoints,
		bool& ioOmitStart,
		bool& ioOmitEnd)
	{
		if (sourceId.empty() || fragmentPoints.size() < 2) {
			return;
		}

		std::vector<AIArtHandle> backupArts;
		if (!CollectBackupCenterlinesForSourceId(sourceId, backupArts) || backupArts.empty()) {
			return;
		}

		AIArtHandle backupArt = nullptr;
		size_t backupPointCount = 0;
		for (size_t i = 0; i < backupArts.size(); ++i) {
			AIArtHandle candidate = backupArts[i];
			if (!candidate) {
				continue;
			}

			std::vector<DuctworkPoint> candidatePoints;
			bool candidateClosed = false;
			if (!DuctworkGeometry::GetPathPoints(candidate, candidatePoints, candidateClosed) || candidateClosed) {
				continue;
			}
			if (!backupArt || candidatePoints.size() > backupPointCount) {
				backupArt = candidate;
				backupPointCount = candidatePoints.size();
			}
		}
		if (!backupArt) {
			return;
		}

		std::vector<DuctworkPoint> backupPoints;
		bool backupClosed = false;
		if (!DuctworkGeometry::GetPathPoints(backupArt, backupPoints, backupClosed) || backupClosed || backupPoints.size() < 2) {
			return;
		}

		const bool backupOmitStart = ReadFinalSegmentThicknessFlag(backupArt, kEmoryOmitStartSegmentThicknessKey);
		const bool backupOmitEnd = ReadFinalSegmentThicknessFlag(backupArt, kEmoryOmitEndSegmentThicknessKey);

		ioOmitStart = false;
		ioOmitEnd = false;

		const DuctworkPoint& fragmentStart = fragmentPoints.front();
		const DuctworkPoint& fragmentEnd = fragmentPoints.back();
		const DuctworkPoint& backupStart = backupPoints.front();
		const DuctworkPoint& backupEnd = backupPoints.back();
		const double endpointTolerance = 2.0;

		const auto nearPoint = [&](const DuctworkPoint& a, const DuctworkPoint& b) {
			return std::hypot(a.x - b.x, a.y - b.y) <= endpointTolerance;
		};

		if (backupOmitStart) {
			if (nearPoint(fragmentStart, backupStart)) {
				ioOmitStart = true;
			}
			if (nearPoint(fragmentEnd, backupStart)) {
				ioOmitEnd = true;
			}
		}
		if (backupOmitEnd) {
			if (nearPoint(fragmentEnd, backupEnd)) {
				ioOmitEnd = true;
			}
			if (nearPoint(fragmentStart, backupEnd)) {
				ioOmitStart = true;
			}
		}
	}

	void ResolveFragmentTerminalStylesFromBackup(const std::string& sourceId,
		const std::vector<DuctworkPoint>& fragmentPoints,
		std::string& ioStartStyle,
		std::string& ioEndStyle)
	{
		if (sourceId.empty() || fragmentPoints.size() < 2) {
			return;
		}

		std::vector<AIArtHandle> backupArts;
		if (!CollectBackupCenterlinesForSourceId(sourceId, backupArts) || backupArts.empty()) {
			return;
		}

		AIArtHandle backupArt = nullptr;
		size_t backupPointCount = 0;
		for (size_t i = 0; i < backupArts.size(); ++i) {
			AIArtHandle candidate = backupArts[i];
			if (!candidate) {
				continue;
			}

			std::vector<DuctworkPoint> candidatePoints;
			bool candidateClosed = false;
			if (!DuctworkGeometry::GetPathPoints(candidate, candidatePoints, candidateClosed) || candidateClosed) {
				continue;
			}
			if (!backupArt || candidatePoints.size() > backupPointCount) {
				backupArt = candidate;
				backupPointCount = candidatePoints.size();
			}
		}
		if (!backupArt) {
			return;
		}

		std::vector<DuctworkPoint> backupPoints;
		bool backupClosed = false;
		if (!DuctworkGeometry::GetPathPoints(backupArt, backupPoints, backupClosed) || backupClosed || backupPoints.size() < 2) {
			return;
		}

		const auto nearPoint = [](const DuctworkPoint& a, const DuctworkPoint& b) {
			return std::hypot(a.x - b.x, a.y - b.y) <= 2.0;
		};

		if (nearPoint(fragmentPoints.front(), backupPoints.front())) {
			ioStartStyle = ReadTerminalSegmentStyle(backupArt, true);
		}
		if (nearPoint(fragmentPoints.back(), backupPoints.back())) {
			ioEndStyle = ReadTerminalSegmentStyle(backupArt, false);
		}
	}

	bool ResolveFragmentOmittedSegmentsFromBackup(const std::string& sourceId,
		const std::vector<DuctworkPoint>& fragmentPoints,
		std::set<size_t>& outOmittedSegments)
	{
		outOmittedSegments.clear();
		if (sourceId.empty() || fragmentPoints.size() < 2) {
			return false;
		}

		std::vector<AIArtHandle> backupArts;
		if (!CollectBackupCenterlinesForSourceId(sourceId, backupArts) || backupArts.empty()) {
			return false;
		}

		AIArtHandle backupArt = nullptr;
		size_t backupPointCount = 0;
		for (size_t i = 0; i < backupArts.size(); ++i) {
			AIArtHandle candidate = backupArts[i];
			if (!candidate) {
				continue;
			}

			std::vector<DuctworkPoint> candidatePoints;
			bool candidateClosed = false;
			if (!DuctworkGeometry::GetPathPoints(candidate, candidatePoints, candidateClosed) || candidateClosed) {
				continue;
			}
			if (!backupArt || candidatePoints.size() > backupPointCount) {
				backupArt = candidate;
				backupPointCount = candidatePoints.size();
			}
		}
		if (!backupArt) {
			return false;
		}

		std::vector<DuctworkPoint> backupPoints;
		bool backupClosed = false;
		if (!DuctworkGeometry::GetPathPoints(backupArt, backupPoints, backupClosed) || backupClosed || backupPoints.size() < 2) {
			return false;
		}

		const bool backupOmitStart = ReadFinalSegmentThicknessFlag(backupArt, kEmoryOmitStartSegmentThicknessKey);
		const bool backupOmitEnd = ReadFinalSegmentThicknessFlag(backupArt, kEmoryOmitEndSegmentThicknessKey);
		std::set<size_t> backupOmittedIndices;
		ReadOmittedSegmentIndices(backupArt, backupOmittedIndices);
		if (backupOmittedIndices.empty()) {
			if (backupOmitStart) {
				backupOmittedIndices.insert(0);
			}
			if (backupOmitEnd && backupPoints.size() >= 2) {
				backupOmittedIndices.insert(backupPoints.size() - 2);
			}
		}

		std::vector<int> backupIndices;
		if (!MapFragmentSegmentsToBackupIndices(fragmentPoints, backupPoints, backupIndices) || backupIndices.empty()) {
			return false;
		}

		const auto nearPoint = [](const DuctworkPoint& a, const DuctworkPoint& b) {
			return std::hypot(a.x - b.x, a.y - b.y) <= 2.0;
		};
		const bool fragmentTouchesBackupStartAtStart = nearPoint(fragmentPoints.front(), backupPoints.front());
		const bool fragmentTouchesBackupStartAtEnd = nearPoint(fragmentPoints.back(), backupPoints.front());
		const bool fragmentTouchesBackupEndAtStart = nearPoint(fragmentPoints.front(), backupPoints.back());
		const bool fragmentTouchesBackupEndAtEnd = nearPoint(fragmentPoints.back(), backupPoints.back());
		const int backupLastSegmentIndex = static_cast<int>(backupPoints.size() - 2);
		const size_t localLastSegmentIndex = backupIndices.empty() ? 0 : (backupIndices.size() - 1);

		for (size_t localIndex = 0; localIndex < backupIndices.size(); ++localIndex) {
			const int backupIndex = backupIndices[localIndex];
			if (backupIndex < 0 ||
				backupOmittedIndices.find(static_cast<size_t>(backupIndex)) == backupOmittedIndices.end()) {
				continue;
			}

			if (backupIndex == 0 && backupOmitStart) {
				if ((fragmentTouchesBackupStartAtStart && localIndex == 0) ||
					(fragmentTouchesBackupStartAtEnd && localIndex == localLastSegmentIndex)) {
					outOmittedSegments.insert(localIndex);
				}
				continue;
			}

			if (backupIndex == backupLastSegmentIndex && backupOmitEnd) {
				if ((fragmentTouchesBackupEndAtEnd && localIndex == localLastSegmentIndex) ||
					(fragmentTouchesBackupEndAtStart && localIndex == 0)) {
					outOmittedSegments.insert(localIndex);
				}
				continue;
			}

			outOmittedSegments.insert(localIndex);
		}
		std::ostringstream logStream;
		logStream << "Emory omit-map sourceId=" << sourceId
			<< " fragmentSegments=" << fragmentPoints.size() - 1
			<< " backupOmitted=[" << SerializeSegmentIndexSet(backupOmittedIndices) << "]"
			<< " localOmitted=[" << SerializeSegmentIndexSet(outOmittedSegments) << "]"
			<< " fragmentStart=" << SerializePointForLog(fragmentPoints.front())
			<< " fragmentEnd=" << SerializePointForLog(fragmentPoints.back());
		DuctworkLog::Write(logStream.str());
		return true;
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
			if (!art || IsGeneratedEmoryArtInternal(art) || IsBackupCenterlineArt(art)) {
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
			if (!art || IsGeneratedEmoryArtInternal(art) || IsBackupCenterlineArt(art)) {
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

			std::vector<AIArtHandle> backups;
			if (CollectBackupCenterlinesForSourceId(groupIt->first, backups) && backups.size() == 1 && backups[0]) {
				DuctworkPath backupPath;
				std::string backupSignature;
				if (BuildProcessPathForArt(backups[0], backupPath) &&
					GetOriginalCenterlineSignature(backups[0], &backupPath, backupSignature) &&
					!backupSignature.empty()) {
					for (size_t candidateIndex = 0; candidateIndex < groupIt->second.size(); ++candidateIndex) {
						EmorySourceIdCandidate& candidate = groupIt->second[candidateIndex];
						std::string candidateSignature;
						if (GetOriginalCenterlineSignature(candidate.art, &candidate.path, candidateSignature) &&
							candidateSignature == backupSignature) {
							continue;
						}
						candidate.newSourceId = GenerateSourceId();
						anyReassigned = true;
					}
					continue;
				}
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
			if (!art || IsGeneratedEmoryArtInternal(art) || IsBackupCenterlineArt(art)) {
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
		if (!art) {
			return false;
		}
		double hiddenMetadata = 0.0;
		if (DuctworkMetadata::GetDouble(art, kEmoryCenterlinesHiddenKey, hiddenMetadata)) {
			return hiddenMetadata > 0.5;
		}
		if (!sAIArt) {
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
		const double connectorBodyWidth = (std::isfinite(trunkWidth) && trunkWidth > kMinDuctWidth)
			? trunkWidth
			: (std::max)(trunkWidth, branchWidth);
		const double branchConnectorWidth = (std::isfinite(connectorBodyWidth) && connectorBodyWidth > kMinDuctWidth)
			? (std::min)(branchWidth, connectorBodyWidth)
			: branchWidth;
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
			desiredLength, branchConnectorWidth, arm)) {
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

	double ResolveMaxSegmentWidth(const std::vector<double>& widths, double fallbackWidth)
	{
		double maxWidth = fallbackWidth;
		if (!std::isfinite(maxWidth) || maxWidth < kMinDuctWidth) {
			maxWidth = kMinDuctWidth;
		}
		for (size_t i = 0; i < widths.size(); ++i) {
			if (std::isfinite(widths[i]) && widths[i] > maxWidth) {
				maxWidth = widths[i];
			}
		}
		return maxWidth < kMinDuctWidth ? kMinDuctWidth : maxWidth;
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

	bool LayerListContains(const std::vector<std::string>& layerNames, const std::string& layerName)
	{
		for (size_t i = 0; i < layerNames.size(); ++i) {
			if (layerNames[i] == layerName) {
				return true;
			}
		}
		return false;
	}

	bool IsUnitPairTransitionToMain(const std::string& transitionLayer, const std::string& mainLayer)
	{
		if (!IsGreenOrLightOrangeRunLayer(transitionLayer) || !IsBlueOrOrangeRunLayer(mainLayer)) {
			return false;
		}

		std::vector<std::string> pairedLayers;
		GetPairedUnitRunLayers(transitionLayer, pairedLayers);
		return LayerListContains(pairedLayers, mainLayer);
	}

	bool ResolveUnitPairEndpointConnection(const DuctworkConnection& connection,
		const std::vector<EmorySourceState>& states,
		const std::vector<DuctworkPoint>& unitAttachmentPoints,
		int& outMainIndex,
		PathConnectionAttachment& outMainAttachment,
		int& outTransitionIndex,
		PathConnectionAttachment& outTransitionAttachment)
	{
		outMainIndex = -1;
		outTransitionIndex = -1;
		outMainAttachment = PathConnectionAttachment();
		outTransitionAttachment = PathConnectionAttachment();
		if (connection.type != kConnectionEndpointToEndpoint ||
			connection.a < 0 || connection.a >= static_cast<int>(states.size()) ||
			connection.b < 0 || connection.b >= static_cast<int>(states.size()) ||
			unitAttachmentPoints.empty() ||
			!IsPointNearAny(connection.point, unitAttachmentPoints, 10.0)) {
			return false;
		}

		const EmorySourceState& stateA = states[connection.a];
		const EmorySourceState& stateB = states[connection.b];
		if (IsUnitPairTransitionToMain(stateA.path.layerName, stateB.path.layerName)) {
			outTransitionIndex = connection.a;
			outMainIndex = connection.b;
		} else if (IsUnitPairTransitionToMain(stateB.path.layerName, stateA.path.layerName)) {
			outTransitionIndex = connection.b;
			outMainIndex = connection.a;
		} else {
			return false;
		}

		if (!DescribeConnectionForPath(connection, outMainIndex, states[outMainIndex], outMainAttachment) ||
			!DescribeConnectionForPath(connection, outTransitionIndex, states[outTransitionIndex], outTransitionAttachment) ||
			outMainAttachment.endpointSlot < 0 ||
			outTransitionAttachment.endpointSlot < 0 ||
			outMainAttachment.segmentIndex < 0 ||
			outTransitionAttachment.segmentIndex < 0) {
			return false;
		}

		return true;
	}

	bool SyncEndpointWidthFromState(const EmorySourceState& sourceState,
		const PathConnectionAttachment& sourceAttachment,
		EmorySourceState& targetState,
		const PathConnectionAttachment& targetAttachment)
	{
		if (sourceAttachment.segmentIndex < 0 ||
			sourceAttachment.segmentIndex >= static_cast<int>(sourceState.widths.size()) ||
			targetAttachment.segmentIndex < 0 ||
			targetAttachment.segmentIndex >= static_cast<int>(targetState.widths.size()) ||
			targetState.widths.size() != targetState.originalWidths.size()) {
			return false;
		}

		double matchedWidth = sourceState.widths[sourceAttachment.segmentIndex];
		if (!std::isfinite(matchedWidth) || matchedWidth < kMinDuctWidth) {
			matchedWidth = kMinDuctWidth;
		}

		std::vector<double> updatedWidths = targetState.widths;
		updatedWidths[targetAttachment.segmentIndex] = matchedWidth;
		const int direction = (targetAttachment.segmentIndex == 0) ? 1 : -1;
		ApplyCascadeFromAnchor(targetState.originalWidths, updatedWidths, targetAttachment.segmentIndex, direction);

		bool changed = targetState.widths.size() != updatedWidths.size();
		if (!changed) {
			for (size_t i = 0; i < updatedWidths.size(); ++i) {
				if (!NearlyEqual(targetState.widths[i], updatedWidths[i], 0.001)) {
					changed = true;
					break;
				}
			}
		}
		if (!changed) {
			return false;
		}

		targetState.widths = updatedWidths;
		targetState.originalWidths = updatedWidths;
		targetState.defaultWidth = ResolveMaxSegmentWidth(targetState.widths, matchedWidth);
		targetState.touched = true;
		return true;
	}

	bool ResolveEndpointAttachmentNearPoint(const EmorySourceState& state,
		const DuctworkPoint& point,
		double tolerance,
		PathConnectionAttachment& outAttachment)
	{
		outAttachment = PathConnectionAttachment();
		if (state.path.points.size() < 2 || state.segmentCount <= 0) {
			return false;
		}

		const double toleranceSq = tolerance * tolerance;
		bool found = false;
		double bestDistanceSq = toleranceSq;
		const DuctworkPoint& startPoint = state.path.points.front();
		const double startDx = startPoint.x - point.x;
		const double startDy = startPoint.y - point.y;
		const double startDistanceSq = (startDx * startDx) + (startDy * startDy);
		if (startDistanceSq <= bestDistanceSq) {
			outAttachment.endpointSlot = 0;
			outAttachment.segmentIndex = 0;
			bestDistanceSq = startDistanceSq;
			found = true;
		}

		const DuctworkPoint& endPoint = state.path.points.back();
		const double endDx = endPoint.x - point.x;
		const double endDy = endPoint.y - point.y;
		const double endDistanceSq = (endDx * endDx) + (endDy * endDy);
		if (endDistanceSq <= bestDistanceSq) {
			outAttachment.endpointSlot = 1;
			outAttachment.segmentIndex = state.segmentCount - 1;
			found = true;
		}

		return found;
	}

	size_t ApplyDirectUnitPairEndpointWidthSync(std::vector<EmorySourceState>& states,
		const std::set<std::string>& affectedSourceIds,
		const std::vector<DuctworkPoint>& unitAttachmentPoints)
	{
		if (states.size() < 2 || affectedSourceIds.empty() || unitAttachmentPoints.empty()) {
			return 0;
		}

		size_t changedCount = 0;
		for (size_t transitionIndex = 0; transitionIndex < states.size(); ++transitionIndex) {
			EmorySourceState& transitionState = states[transitionIndex];
			if (!IsGreenOrLightOrangeRunLayer(transitionState.path.layerName)) {
				continue;
			}

			for (size_t mainIndex = 0; mainIndex < states.size(); ++mainIndex) {
				if (mainIndex == transitionIndex) {
					continue;
				}

				EmorySourceState& mainState = states[mainIndex];
				if (!IsUnitPairTransitionToMain(transitionState.path.layerName, mainState.path.layerName)) {
					continue;
				}
				const bool mainAffected = affectedSourceIds.find(mainState.sourceId) != affectedSourceIds.end();
				const bool transitionAffected = affectedSourceIds.find(transitionState.sourceId) != affectedSourceIds.end();
				if (!mainAffected && !transitionAffected) {
					continue;
				}

				for (size_t pointIndex = 0; pointIndex < unitAttachmentPoints.size(); ++pointIndex) {
					PathConnectionAttachment mainAttachment;
					PathConnectionAttachment transitionAttachment;
					if (!ResolveEndpointAttachmentNearPoint(mainState, unitAttachmentPoints[pointIndex], 10.0, mainAttachment) ||
						!ResolveEndpointAttachmentNearPoint(transitionState, unitAttachmentPoints[pointIndex], 10.0, transitionAttachment)) {
						continue;
					}

					if (mainAffected && !transitionState.selectedSeed) {
						const double sourceWidth = (mainAttachment.segmentIndex >= 0 &&
							mainAttachment.segmentIndex < static_cast<int>(mainState.widths.size()))
							? mainState.widths[mainAttachment.segmentIndex]
							: 0.0;
						if (SyncEndpointWidthFromState(mainState, mainAttachment, transitionState, transitionAttachment)) {
							++changedCount;
							std::ostringstream logStream;
							logStream << "Emory direct unit-pair width sync initial direction=main-to-transition transition="
								<< transitionState.sourceId
								<< " main=" << mainState.sourceId
								<< " point=[" << unitAttachmentPoints[pointIndex].x << "," << unitAttachmentPoints[pointIndex].y << "]"
								<< " sourceWidth=" << sourceWidth
								<< " transitionSegment=" << transitionAttachment.segmentIndex;
							DuctworkLog::Write(logStream.str());
						}
					} else if (transitionAffected && !mainState.selectedSeed) {
						const double sourceWidth = (transitionAttachment.segmentIndex >= 0 &&
							transitionAttachment.segmentIndex < static_cast<int>(transitionState.widths.size()))
							? transitionState.widths[transitionAttachment.segmentIndex]
							: 0.0;
						if (SyncEndpointWidthFromState(transitionState, transitionAttachment, mainState, mainAttachment)) {
							++changedCount;
							std::ostringstream logStream;
							logStream << "Emory direct unit-pair width sync initial direction=transition-to-main transition="
								<< transitionState.sourceId
								<< " main=" << mainState.sourceId
								<< " point=[" << unitAttachmentPoints[pointIndex].x << "," << unitAttachmentPoints[pointIndex].y << "]"
								<< " sourceWidth=" << sourceWidth
								<< " mainSegment=" << mainAttachment.segmentIndex;
							DuctworkLog::Write(logStream.str());
						}
					}
				}
			}
		}

		return changedCount;
	}

	bool CascadeDirectUnitPairEndpointWidthsFromState(std::vector<EmorySourceState>& states,
		int currentIndex,
		const std::vector<DuctworkPoint>& unitAttachmentPoints,
		std::vector<int>& queue)
	{
		if (currentIndex < 0 ||
			currentIndex >= static_cast<int>(states.size()) ||
			unitAttachmentPoints.empty()) {
			return false;
		}

		bool changed = false;
		for (size_t neighborIndex = 0; neighborIndex < states.size(); ++neighborIndex) {
			if (static_cast<int>(neighborIndex) == currentIndex ||
				states[neighborIndex].selectedSeed) {
				continue;
			}

			if (!IsUnitPairTransitionToMain(states[currentIndex].path.layerName, states[neighborIndex].path.layerName) &&
				!IsUnitPairTransitionToMain(states[neighborIndex].path.layerName, states[currentIndex].path.layerName)) {
				continue;
			}

			for (size_t pointIndex = 0; pointIndex < unitAttachmentPoints.size(); ++pointIndex) {
				PathConnectionAttachment currentAttachment;
				PathConnectionAttachment neighborAttachment;
				if (!ResolveEndpointAttachmentNearPoint(states[currentIndex], unitAttachmentPoints[pointIndex], 10.0, currentAttachment) ||
					!ResolveEndpointAttachmentNearPoint(states[neighborIndex], unitAttachmentPoints[pointIndex], 10.0, neighborAttachment)) {
					continue;
				}

				const double sourceWidth = (currentAttachment.segmentIndex >= 0 &&
					currentAttachment.segmentIndex < static_cast<int>(states[currentIndex].widths.size()))
					? states[currentIndex].widths[currentAttachment.segmentIndex]
					: 0.0;
				if (SyncEndpointWidthFromState(states[currentIndex],
					currentAttachment,
					states[neighborIndex],
					neighborAttachment)) {
					queue.push_back(static_cast<int>(neighborIndex));
					changed = true;
					std::ostringstream logStream;
					logStream << "Emory direct unit-pair width sync cascade source="
						<< states[currentIndex].sourceId
						<< " target=" << states[neighborIndex].sourceId
						<< " point=[" << unitAttachmentPoints[pointIndex].x << "," << unitAttachmentPoints[pointIndex].y << "]"
						<< " sourceWidth=" << sourceWidth
						<< " targetSegment=" << neighborAttachment.segmentIndex;
					DuctworkLog::Write(logStream.str());
				}
			}
		}

		return changed;
	}

	bool CollectEmorySourceStates(std::vector<EmorySourceState>& outStates, std::map<std::string, int>& outIndexBySourceId)
	{
		outStates.clear();
		outIndexBySourceId.clear();

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || IsGeneratedEmoryArtInternal(art) || IsBackupCenterlineArt(art)) {
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

			AIArtHandle backupArt = nullptr;
			DuctworkPath backupPath;
			std::vector<int> backupIndices;
			if (GetPrimaryBackupCenterlineForSourceId(sourceId, backupArt, backupPath) &&
				MapFragmentSegmentsToBackupIndices(points, backupPath.points, backupIndices) &&
				!backupIndices.empty()) {
				const bool hasBackupExplicitStart = HasExplicitStartSegmentIndex(backupArt);
				const int backupSegmentCount = static_cast<int>(backupPath.points.size() > 1 ? backupPath.points.size() - 1 : 0);
				const int backupStartIndex = ReadStartSegmentIndex(backupArt, static_cast<size_t>(backupSegmentCount));
				int localStartIndex = 0;
				if (backupStartIndex <= backupIndices.front()) {
					localStartIndex = 0;
				} else if (backupStartIndex >= backupIndices.back()) {
					localStartIndex = static_cast<int>(backupIndices.size() - 1);
				} else {
					for (size_t index = 0; index < backupIndices.size(); ++index) {
						if (backupIndices[index] <= backupStartIndex) {
							localStartIndex = static_cast<int>(index);
						}
					}
				}
				state.startSegmentIndex = localStartIndex;
				state.hasExplicitStart = hasBackupExplicitStart;
			} else {
				state.startSegmentIndex = ReadStartSegmentIndex(art, static_cast<size_t>(segmentCount));
				state.hasExplicitStart = HasExplicitStartSegmentIndex(art);
			}
			ReadDuctRole(art, state.ductRole);

			if (!ResolveSourceBodyWidth(art, sourceId, state.defaultWidth) || state.defaultWidth <= 0.0) {
				state.defaultWidth = kDefaultDuctWidth;
			}
			if (state.defaultWidth < kMinDuctWidth) {
				state.defaultWidth = kMinDuctWidth;
			}
			ResolveSourceStrokeWidth(art, sourceId, state.defaultWidth, state.sourceStrokeWidth);

			state.hasStoredSegmentWidths = HasStoredSegmentWidths(art, static_cast<size_t>(segmentCount));
			ReadSegmentWidths(art, static_cast<size_t>(segmentCount), state.defaultWidth, state.widths);
			NormalizeGuideLikeStoredWidths(art, sourceId, points, state.startSegmentIndex, state.defaultWidth, state.widths);
			if (!state.hasStoredSegmentWidths) {
				ApplyDefaultStraightChainTapers(art, points, state.startSegmentIndex, state.widths);
			}
			state.originalWidths = state.widths;
			CollectStraightChainInfos(art, points, state.widths, state.straightChains, state.straightChainIndexBySegment);

			outIndexBySourceId[sourceId] = static_cast<int>(outStates.size());
			outStates.push_back(state);
		}

		return !outStates.empty();
	}

	bool SyncAffectedSourceWidthsToBackups(const std::set<std::string>& affectedSourceIds,
		const std::vector<EmorySourceState>& states)
	{
		bool updatedAny = false;
		for (std::set<std::string>::const_iterator sourceIt = affectedSourceIds.begin(); sourceIt != affectedSourceIds.end(); ++sourceIt) {
			AIArtHandle backupArt = nullptr;
			DuctworkPath backupPath;
			if (!GetPrimaryBackupCenterlineForSourceId(*sourceIt, backupArt, backupPath) ||
				!backupArt ||
				backupPath.points.size() < 2) {
				continue;
			}

			const size_t backupSegmentCount = backupPath.points.size() - 1;
			double defaultWidth = 0.0;
			if (!ResolveSourceBodyWidth(backupArt, *sourceIt, defaultWidth) || defaultWidth <= 0.0) {
				defaultWidth = kDefaultDuctWidth;
			}
			if (defaultWidth < kMinDuctWidth) {
				defaultWidth = kMinDuctWidth;
			}

			std::vector<double> backupWidths;
			ReadSegmentWidths(backupArt, backupSegmentCount, defaultWidth, backupWidths);
			if (!HasStoredSegmentWidths(backupArt, backupSegmentCount)) {
				ApplyDefaultStraightChainTapers(backupArt,
					backupPath.points,
					ReadStartSegmentIndex(backupArt, backupSegmentCount),
					backupWidths);
			}

			bool wroteSource = false;
			for (size_t stateIndex = 0; stateIndex < states.size(); ++stateIndex) {
				const EmorySourceState& state = states[stateIndex];
				if (state.sourceId != *sourceIt || state.widths.empty() || state.path.points.size() < 2) {
					continue;
				}

				std::vector<DuctworkPoint> fragmentPoints;
				SanitizePolyline(state.path.points, fragmentPoints);
				std::vector<int> backupIndices;
				if (!MapFragmentSegmentsToBackupIndices(fragmentPoints, backupPath.points, backupIndices) || backupIndices.empty()) {
					continue;
				}

				const size_t count = (std::min)(backupIndices.size(), state.widths.size());
				for (size_t localIndex = 0; localIndex < count; ++localIndex) {
					const int backupIndex = backupIndices[localIndex];
					if (backupIndex < 0 || backupIndex >= static_cast<int>(backupWidths.size())) {
						continue;
					}
					backupWidths[backupIndex] = state.widths[localIndex];
					wroteSource = true;
				}
			}

			if (!wroteSource) {
				continue;
			}

			WriteSegmentWidths(backupArt, backupWidths);
			int storedIndex = ReadStartSegmentIndex(backupArt, backupSegmentCount);
			if (storedIndex < 0 || storedIndex >= static_cast<int>(backupWidths.size())) {
				storedIndex = 0;
			}
			if (!backupWidths.empty()) {
				WriteStoredSourceBodyWidth(backupArt, backupWidths[storedIndex]);
			}
			updatedAny = true;
		}

		return updatedAny;
	}

	bool FindBestStateIndexForGeneratedArt(AIArtHandle generatedArt,
		const std::string& role,
		const std::string& sourceId,
		const std::vector<EmorySourceState>& states,
		int& outStateIndex)
	{
		outStateIndex = -1;
		if (!generatedArt || sourceId.empty()) {
			return false;
		}

		double bestScore = 0.0;
		bool bestScoreSet = false;
		for (size_t i = 0; i < states.size(); ++i) {
			if (states[i].sourceId != sourceId) {
				continue;
			}
			double score = 0.0;
			if (!ComputeGeneratedArtCandidateScore(generatedArt, role, states[i].path, score)) {
				continue;
			}
			if (!bestScoreSet || score < bestScore) {
				bestScore = score;
				outStateIndex = static_cast<int>(i);
				bestScoreSet = true;
			}
		}
		return outStateIndex >= 0;
	}

	bool FindBestStateIndexForGeneratedArtLoose(AIArtHandle generatedArt,
		const std::string& role,
		const std::vector<EmorySourceState>& states,
		int& outStateIndex)
	{
		outStateIndex = -1;
		if (!generatedArt) {
			return false;
		}

		double bestScore = 0.0;
		bool bestScoreSet = false;
		for (size_t i = 0; i < states.size(); ++i) {
			double score = 0.0;
			if (!ComputeGeneratedArtCandidateScore(generatedArt, role, states[i].path, score)) {
				continue;
			}
			if (!bestScoreSet || score < bestScore) {
				bestScore = score;
				outStateIndex = static_cast<int>(i);
				bestScoreSet = true;
			}
		}
		return outStateIndex >= 0;
	}

	bool SelectGeneratedSegmentsByStateMap(const std::vector<EmorySourceState>& states,
		const std::map<int, std::vector<int> >& selectedByState)
	{
		if (!sAIArt || selectedByState.empty()) {
			return false;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);

		std::vector<AIArtHandle> matches;
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || !IsGeneratedEmoryArtInternal(art)) {
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

			int bestStateIndex = -1;
			if (!FindBestStateIndexForGeneratedArt(art, role, artSourceId, states, bestStateIndex)) {
				continue;
			}

			std::map<int, std::vector<int> >::const_iterator selectedIt = selectedByState.find(bestStateIndex);
			if (selectedIt == selectedByState.end()) {
				continue;
			}

			const std::vector<int>& wantedIndices = selectedIt->second;
			if (std::find(wantedIndices.begin(), wantedIndices.end(), segmentIndex) != wantedIndices.end()) {
				matches.push_back(art);
			}
		}

		if (matches.empty()) {
			return false;
		}

		ClearSelectionInternal();
		SelectArtListInternal(matches);
		return true;
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

		std::vector<DuctworkPoint> ignorePoints;
		CollectIgnoreAnchorPoints(ignorePoints);
		if (ignorePoints.empty() || outConnections.empty()) {
			return;
		}

		const size_t beforeCount = outConnections.size();
		const double ignoreTolerance = 10.0;
		std::vector<DuctworkConnection> filteredConnections;
		filteredConnections.reserve(outConnections.size());
		for (size_t i = 0; i < outConnections.size(); ++i) {
			if (IsPointNearAny(outConnections[i].point, ignorePoints, ignoreTolerance)) {
				continue;
			}
			filteredConnections.push_back(outConnections[i]);
		}

		if (filteredConnections.size() != beforeCount) {
			std::ostringstream logStream;
			logStream << "Emory ignore-connection-filter removed=" << (beforeCount - filteredConnections.size())
				<< " kept=" << filteredConnections.size();
			DuctworkLog::Write(logStream.str());
		}

		outConnections.swap(filteredConnections);
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

	int CountEndpointToSegmentBranchChildren(int pathIndex, const std::vector<DuctworkConnection>& connections)
	{
		int count = 0;
		for (size_t i = 0; i < connections.size(); ++i) {
			int trunkIndex = -1;
			int branchIndex = -1;
			if (IsEndpointToSegmentBranchConnection(connections[i], trunkIndex, branchIndex) &&
				trunkIndex == pathIndex) {
				++count;
			}
		}
		return count;
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

	bool AllowDistributionTrunkTerminalRegister(size_t pointCount,
		bool distributionTrunk,
		bool registerAtEndpoint)
	{
		return distributionTrunk && pointCount >= 3 && registerAtEndpoint;
	}

	bool SelectionCoversEverySegment(const EmorySourceState& state, const std::vector<int>& selectedSegmentIndices)
	{
		if (state.segmentCount <= 0 || selectedSegmentIndices.empty()) {
			return false;
		}

		std::vector<bool> selected(static_cast<size_t>(state.segmentCount), false);
		int validCount = 0;
		for (size_t i = 0; i < selectedSegmentIndices.size(); ++i) {
			const int segmentIndex = selectedSegmentIndices[i];
			if (segmentIndex < 0 || segmentIndex >= state.segmentCount || selected[static_cast<size_t>(segmentIndex)]) {
				continue;
			}
			selected[static_cast<size_t>(segmentIndex)] = true;
			++validCount;
		}

		return validCount == state.segmentCount;
	}

	void ApplyProportionalWidthScaleToPathState(EmorySourceState& state, double scale)
	{
		if (state.widths.empty() || !std::isfinite(scale) || scale <= 0.0) {
			return;
		}

		for (size_t i = 0; i < state.widths.size(); ++i) {
			double scaledWidth = state.widths[i] * scale;
			if (!std::isfinite(scaledWidth) || scaledWidth < kMinDuctWidth) {
				scaledWidth = kMinDuctWidth;
			}
			state.widths[i] = scaledWidth;
		}

		if (std::isfinite(state.defaultWidth) && state.defaultWidth > 0.0) {
			state.defaultWidth = (std::max)(kMinDuctWidth, state.defaultWidth * scale);
		}
		state.touched = true;
		state.selectedSeed = true;
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

	void ApplyUniformWidthToPathState(EmorySourceState& state, const std::vector<int>& selectedSegmentIndices, double newWidth)
	{
		if (selectedSegmentIndices.empty() || state.segmentCount <= 0) {
			return;
		}

		if (SelectionCoversEverySegment(state, selectedSegmentIndices)) {
			int anchorSegment = state.startSegmentIndex;
			if (anchorSegment < 0 || anchorSegment >= state.segmentCount) {
				anchorSegment = 0;
			}
			std::vector<int> anchorOnly(1, anchorSegment);
			ApplySelectedWidthToPathState(state, anchorOnly, newWidth);
			return;
		}

		ApplySelectedWidthToPathState(state, selectedSegmentIndices, newWidth);
	}

	bool SelectionContainsOnlyTerminalWidthControls(const EmorySourceState& state, const std::vector<int>& selectedSegmentIndices)
	{
		if (!state.art || state.segmentCount <= 0 || selectedSegmentIndices.empty()) {
			return false;
		}

		std::set<size_t> omittedSegments;
		CollectTerminalOmittedSegments(state.art, static_cast<size_t>(state.segmentCount), omittedSegments);
		std::vector<int> canonicalSelectedIndices = selectedSegmentIndices;
		size_t canonicalSegmentCount = static_cast<size_t>(state.segmentCount);

		AIArtHandle backupArt = nullptr;
		DuctworkPath backupPath;
		if (GetPrimaryBackupCenterlineForSourceId(state.sourceId, backupArt, backupPath) &&
			backupPath.points.size() >= 2) {
			const size_t backupSegmentCount = backupPath.points.size() - 1;
			std::set<size_t> backupOmittedSegments;
			CollectTerminalOmittedSegments(backupArt, backupSegmentCount, backupOmittedSegments);
			if (!backupOmittedSegments.empty()) {
				std::vector<DuctworkPoint> fragmentPoints;
				SanitizePolyline(state.path.points, fragmentPoints);
				std::vector<int> backupIndices;
				if (MapFragmentSegmentsToBackupIndices(fragmentPoints, backupPath.points, backupIndices)) {
					std::vector<int> mappedSelectedIndices;
					for (size_t i = 0; i < selectedSegmentIndices.size(); ++i) {
						const int localIndex = selectedSegmentIndices[i];
						if (localIndex < 0 || localIndex >= static_cast<int>(backupIndices.size()) || backupIndices[localIndex] < 0) {
							return false;
						}
						mappedSelectedIndices.push_back(backupIndices[localIndex]);
					}
					canonicalSelectedIndices = mappedSelectedIndices;
					canonicalSegmentCount = backupSegmentCount;
					omittedSegments = backupOmittedSegments;
				}
			}
		}

		if (omittedSegments.empty()) {
			return false;
		}

		for (size_t i = 0; i < canonicalSelectedIndices.size(); ++i) {
			const int segmentIndex = canonicalSelectedIndices[i];
			if (segmentIndex < 0 || segmentIndex >= static_cast<int>(canonicalSegmentCount)) {
				return false;
			}
			if (omittedSegments.find(static_cast<size_t>(segmentIndex)) != omittedSegments.end()) {
				continue;
			}

			bool atStart = false;
			bool atEnd = false;
			if (!ResolveTerminalStyleTargetFromSelection(segmentIndex,
				canonicalSegmentCount,
				omittedSegments,
				true,
				true,
				atStart,
				atEnd) ||
				(!atStart && !atEnd)) {
				return false;
			}
		}

		return true;
	}

	void CollectEffectiveTerminalOmittedSegmentsForState(const EmorySourceState& state, std::set<size_t>& outOmittedSegments)
	{
		outOmittedSegments.clear();
		if (!state.art || state.segmentCount <= 0) {
			return;
		}

		CollectTerminalOmittedSegments(state.art, static_cast<size_t>(state.segmentCount), outOmittedSegments);

		std::vector<DuctworkPoint> fragmentPoints;
		SanitizePolyline(state.path.points, fragmentPoints);
		if (fragmentPoints.size() < 2) {
			return;
		}

		std::set<size_t> backupOmittedSegments;
		if (ResolveFragmentOmittedSegmentsFromBackup(state.sourceId, fragmentPoints, backupOmittedSegments)) {
			outOmittedSegments.insert(backupOmittedSegments.begin(), backupOmittedSegments.end());
		}
	}

	bool ResolveTerminalWidthControlSegmentIndex(const EmorySourceState& state,
		int selectedSegmentIndex,
		int& outControlSegmentIndex)
	{
		outControlSegmentIndex = -1;
		if (state.segmentCount <= 0 ||
			selectedSegmentIndex < 0 ||
			selectedSegmentIndex >= state.segmentCount) {
			return false;
		}

		std::set<size_t> omittedSegments;
		CollectEffectiveTerminalOmittedSegmentsForState(state, omittedSegments);

		const int lastSegmentIndex = state.segmentCount - 1;
		const bool startOmitted = omittedSegments.find(0) != omittedSegments.end();
		const bool endOmitted = omittedSegments.find(static_cast<size_t>(lastSegmentIndex)) != omittedSegments.end();

		if (startOmitted) {
			const int startControlIndex = (state.segmentCount > 1) ? 1 : 0;
			if (selectedSegmentIndex == 0 || selectedSegmentIndex == startControlIndex) {
				outControlSegmentIndex = startControlIndex;
				return true;
			}
		} else if (selectedSegmentIndex == 0) {
			outControlSegmentIndex = 0;
			return true;
		}

		if (endOmitted) {
			const int endControlIndex = (state.segmentCount > 1) ? (lastSegmentIndex - 1) : lastSegmentIndex;
			if (selectedSegmentIndex == lastSegmentIndex || selectedSegmentIndex == endControlIndex) {
				outControlSegmentIndex = endControlIndex;
				return true;
			}
		} else if (selectedSegmentIndex == lastSegmentIndex) {
			outControlSegmentIndex = lastSegmentIndex;
			return true;
		}

		return false;
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

	bool PropagateWidthToBranchSegmentState(const EmorySourceState& upstreamState,
		int upstreamSegmentIndex,
		EmorySourceState& branchState,
		int branchSegmentIndex)
	{
		if (upstreamSegmentIndex < 0 ||
			upstreamSegmentIndex >= static_cast<int>(upstreamState.widths.size()) ||
			branchSegmentIndex < 0 ||
			branchSegmentIndex >= static_cast<int>(branchState.widths.size()) ||
			branchState.widths.size() != branchState.originalWidths.size()) {
			return false;
		}

		double upstreamOriginalWidth = upstreamState.defaultWidth;
		if (upstreamSegmentIndex < static_cast<int>(upstreamState.originalWidths.size()) &&
			upstreamState.originalWidths[upstreamSegmentIndex] > kPointEpsilon) {
			upstreamOriginalWidth = upstreamState.originalWidths[upstreamSegmentIndex];
		}

		double branchOriginalWidth = branchState.defaultWidth;
		if (branchSegmentIndex < static_cast<int>(branchState.originalWidths.size()) &&
			branchState.originalWidths[branchSegmentIndex] > kPointEpsilon) {
			branchOriginalWidth = branchState.originalWidths[branchSegmentIndex];
		}

		double ratio = 1.0;
		if (upstreamOriginalWidth > kPointEpsilon) {
			ratio = branchOriginalWidth / upstreamOriginalWidth;
		}

		double branchWidth = upstreamState.widths[upstreamSegmentIndex] * ratio;
		if (!std::isfinite(branchWidth) || branchWidth < kMinDuctWidth) {
			branchWidth = kMinDuctWidth;
		}

		const bool changed = !NearlyEqual(branchState.widths[branchSegmentIndex], branchWidth, 0.001);
		branchState.widths[branchSegmentIndex] = branchWidth;
		ApplyCascadeFromAnchor(branchState.originalWidths, branchState.widths, branchSegmentIndex, -1);
		ApplyCascadeFromAnchor(branchState.originalWidths, branchState.widths, branchSegmentIndex, 1);
		branchState.touched = true;
		return changed;
	}

	bool ApplyInheritedWidthToBranchState(const EmorySourceState& trunkState,
		const PathConnectionAttachment& trunkAttachment,
		EmorySourceState& branchState,
		const PathConnectionAttachment& branchAttachment)
	{
		if (trunkAttachment.segmentIndex < 0 ||
			trunkAttachment.segmentIndex >= static_cast<int>(trunkState.widths.size()) ||
			branchAttachment.endpointSlot < 0 ||
			branchState.segmentCount <= 0 ||
			branchState.widths.empty()) {
			return false;
		}

		const int branchRootSegmentIndex = (branchAttachment.endpointSlot == 0) ? 0 : (branchState.segmentCount - 1);
		if (branchRootSegmentIndex < 0 || branchRootSegmentIndex >= static_cast<int>(branchState.widths.size())) {
			return false;
		}

		double inheritedRootWidth = trunkState.widths[trunkAttachment.segmentIndex] * gBranchInheritedWidthRatio;
		if (!std::isfinite(inheritedRootWidth) || inheritedRootWidth < kMinDuctWidth) {
			inheritedRootWidth = kMinDuctWidth;
		}
		int inheritedStartSegmentIndex = branchRootSegmentIndex;
		bool persistInheritedStartSegment = true;
		int directionalStartSegmentIndex = 0;
		if (TryResolveDirectionalDefaultStartSegmentIndex(branchState.art,
			branchState.path,
			branchState.path.points,
			branchState.segmentCount,
			directionalStartSegmentIndex)) {
			inheritedStartSegmentIndex = directionalStartSegmentIndex;
			persistInheritedStartSegment = false;
		}
		std::vector<double> inheritedWidths(branchState.widths.size(), inheritedRootWidth);
		ApplyDefaultStraightChainTapers(branchState.art,
			branchState.path.points,
			inheritedStartSegmentIndex,
			inheritedWidths);
		if (branchRootSegmentIndex >= 0 && branchRootSegmentIndex < static_cast<int>(inheritedWidths.size())) {
			inheritedWidths[branchRootSegmentIndex] = inheritedRootWidth;
		}

		bool changed = branchState.startSegmentIndex != inheritedStartSegmentIndex ||
			branchState.hasExplicitStart != persistInheritedStartSegment ||
			branchState.widths.size() != inheritedWidths.size();
		if (!changed) {
			for (size_t i = 0; i < inheritedWidths.size(); ++i) {
				if (!NearlyEqual(branchState.widths[i], inheritedWidths[i], 0.001)) {
					changed = true;
					break;
				}
			}
		}
		if (!changed) {
			return false;
		}

		branchState.widths = inheritedWidths;
		branchState.originalWidths = inheritedWidths;
		branchState.defaultWidth = ResolveMaxSegmentWidth(branchState.widths, inheritedRootWidth);
		branchState.startSegmentIndex = inheritedStartSegmentIndex;
		branchState.hasExplicitStart = persistInheritedStartSegment;
		branchState.touched = true;
		return true;
	}

	bool SourceStateHasUnitEndpoint(const EmorySourceState& state, const std::vector<DuctworkPoint>& unitAttachmentPoints)
	{
		return state.path.points.size() >= 2 &&
			!unitAttachmentPoints.empty() &&
			(IsPointNearAny(state.path.points.front(), unitAttachmentPoints, 10.0) ||
				IsPointNearAny(state.path.points.back(), unitAttachmentPoints, 10.0));
	}

	bool StateEndpointSlotNearAny(const EmorySourceState& state,
		int endpointSlot,
		const std::vector<DuctworkPoint>& points,
		double tolerance)
	{
		if (state.path.points.size() < 2 || points.empty()) {
			return false;
		}
		if (endpointSlot == 0) {
			return IsPointNearAny(state.path.points.front(), points, tolerance);
		}
		if (endpointSlot == 1) {
			return IsPointNearAny(state.path.points.back(), points, tolerance);
		}
		return false;
	}

	bool ResolveUnitFeederEndpointToSegmentConnection(const DuctworkConnection& connection,
		const std::vector<EmorySourceState>& states,
		const std::vector<DuctworkPoint>& unitAttachmentPoints,
		int& outFeederIndex,
		PathConnectionAttachment& outFeederAttachment,
		int& outDownstreamIndex,
		PathConnectionAttachment& outDownstreamAttachment)
	{
		outFeederIndex = -1;
		outDownstreamIndex = -1;
		outFeederAttachment = PathConnectionAttachment();
		outDownstreamAttachment = PathConnectionAttachment();
		if (unitAttachmentPoints.empty()) {
			return false;
		}

		int trunkIndex = -1;
		int branchIndex = -1;
		if (!IsEndpointToSegmentBranchConnection(connection, trunkIndex, branchIndex) ||
			trunkIndex < 0 || trunkIndex >= static_cast<int>(states.size()) ||
			branchIndex < 0 || branchIndex >= static_cast<int>(states.size())) {
			return false;
		}

		const EmorySourceState& feederState = states[branchIndex];
		const EmorySourceState& downstreamState = states[trunkIndex];
		if (feederState.path.layerName != downstreamState.path.layerName ||
			!IsBlueOrOrangeRunLayer(feederState.path.layerName)) {
			return false;
		}

		PathConnectionAttachment feederAttachment;
		PathConnectionAttachment downstreamAttachment;
		if (!DescribeConnectionForPath(connection, branchIndex, feederState, feederAttachment) ||
			!DescribeConnectionForPath(connection, trunkIndex, downstreamState, downstreamAttachment) ||
			feederAttachment.endpointSlot < 0 ||
			feederAttachment.segmentIndex < 0 ||
			downstreamAttachment.segmentIndex < 0) {
			return false;
		}

		const int unitEndpointSlot = (feederAttachment.endpointSlot == 0) ? 1 : 0;
		if (!StateEndpointSlotNearAny(feederState, unitEndpointSlot, unitAttachmentPoints, 10.0)) {
			return false;
		}

		outFeederIndex = branchIndex;
		outFeederAttachment = feederAttachment;
		outDownstreamIndex = trunkIndex;
		outDownstreamAttachment = downstreamAttachment;
		return true;
	}

	bool ResolveUnitTrunkSegmentIntersection(const DuctworkConnection& connection,
		const std::vector<EmorySourceState>& states,
		const std::vector<DuctworkPoint>& unitAttachmentPoints,
		int& outTrunkIndex,
		int& outBranchIndex)
	{
		outTrunkIndex = -1;
		outBranchIndex = -1;
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

		const bool aHasUnit = SourceStateHasUnitEndpoint(stateA, unitAttachmentPoints);
		const bool bHasUnit = SourceStateHasUnitEndpoint(stateB, unitAttachmentPoints);
		if (aHasUnit == bHasUnit) {
			return false;
		}

		outTrunkIndex = aHasUnit ? connection.a : connection.b;
		outBranchIndex = aHasUnit ? connection.b : connection.a;
		return true;
	}

	int CountRegisterEndpointAttachments(const EmorySourceState& state, const std::vector<DuctworkPoint>& registerAttachmentPoints)
	{
		if (state.path.points.size() < 2 || registerAttachmentPoints.empty()) {
			return 0;
		}

		int count = 0;
		if (IsPointNearAny(state.path.points.front(), registerAttachmentPoints, 10.0)) {
			++count;
		}
		if (IsPointNearAny(state.path.points.back(), registerAttachmentPoints, 10.0)) {
			++count;
		}
		return count;
	}

	double ComputeStatePathLength(const EmorySourceState& state)
	{
		double length = 0.0;
		for (size_t i = 0; i + 1 < state.path.points.size(); ++i) {
			length += DuctworkMath::Dist(state.path.points[i], state.path.points[i + 1]);
		}
		return length;
	}

	bool ResolveSameLayerSegmentIntersectionBranch(const DuctworkConnection& connection,
		const std::vector<EmorySourceState>& states,
		const std::vector<DuctworkConnection>& connections,
		const std::vector<DuctworkPoint>& unitAttachmentPoints,
		const std::vector<DuctworkPoint>& registerAttachmentPoints,
		int& outTrunkIndex,
		int& outBranchIndex)
	{
		outTrunkIndex = -1;
		outBranchIndex = -1;
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

		if (PathHasInternalVertexNear(stateA.path.points, connection.point, 10.0) ||
			PathHasInternalVertexNear(stateB.path.points, connection.point, 10.0)) {
			return false;
		}

		if (ResolveUnitTrunkSegmentIntersection(connection, states, unitAttachmentPoints, outTrunkIndex, outBranchIndex)) {
			return true;
		}

		const bool aIsBranchRole = IsBranchRole(stateA.ductRole);
		const bool bIsBranchRole = IsBranchRole(stateB.ductRole);
		if (aIsBranchRole != bIsBranchRole) {
			outTrunkIndex = aIsBranchRole ? connection.b : connection.a;
			outBranchIndex = aIsBranchRole ? connection.a : connection.b;
			return true;
		}

		const bool aIsEndpointBranchChild = PathIsBranchChild(connection.a, connections);
		const bool bIsEndpointBranchChild = PathIsBranchChild(connection.b, connections);
		if (aIsEndpointBranchChild != bIsEndpointBranchChild) {
			outTrunkIndex = aIsEndpointBranchChild ? connection.b : connection.a;
			outBranchIndex = aIsEndpointBranchChild ? connection.a : connection.b;
			return true;
		}

		const int aChildCount = CountEndpointToSegmentBranchChildren(connection.a, connections);
		const int bChildCount = CountEndpointToSegmentBranchChildren(connection.b, connections);
		if ((aChildCount > 0) != (bChildCount > 0)) {
			outTrunkIndex = (aChildCount > 0) ? connection.a : connection.b;
			outBranchIndex = (aChildCount > 0) ? connection.b : connection.a;
			return true;
		}

		const int aRegisterEndpoints = CountRegisterEndpointAttachments(stateA, registerAttachmentPoints);
		const int bRegisterEndpoints = CountRegisterEndpointAttachments(stateB, registerAttachmentPoints);
		if (aRegisterEndpoints != bRegisterEndpoints) {
			outTrunkIndex = (aRegisterEndpoints > bRegisterEndpoints) ? connection.b : connection.a;
			outBranchIndex = (aRegisterEndpoints > bRegisterEndpoints) ? connection.a : connection.b;
			return true;
		}

		const double widthA = stateA.widths[connection.segA];
		const double widthB = stateB.widths[connection.segB];
		if (std::isfinite(widthA) &&
			std::isfinite(widthB) &&
			std::fabs(widthA - widthB) > 0.001) {
			outTrunkIndex = (widthA >= widthB) ? connection.a : connection.b;
			outBranchIndex = (widthA >= widthB) ? connection.b : connection.a;
			return true;
		}

		const double lengthA = ComputeStatePathLength(stateA);
		const double lengthB = ComputeStatePathLength(stateB);
		if (std::fabs(lengthA - lengthB) > 0.001) {
			outTrunkIndex = (lengthA >= lengthB) ? connection.a : connection.b;
			outBranchIndex = (lengthA >= lengthB) ? connection.b : connection.a;
			return true;
		}

		return false;
	}

	bool ApplyContinuationWidthFromUnitFeeder(const EmorySourceState& feederState,
		const PathConnectionAttachment& feederAttachment,
		EmorySourceState& downstreamState,
		const PathConnectionAttachment& downstreamAttachment)
	{
		if (feederAttachment.segmentIndex < 0 ||
			feederAttachment.segmentIndex >= static_cast<int>(feederState.widths.size()) ||
			downstreamAttachment.segmentIndex < 0 ||
			downstreamAttachment.segmentIndex >= static_cast<int>(downstreamState.widths.size())) {
			return false;
		}

		double rootWidth = feederState.widths[feederAttachment.segmentIndex];
		if (!std::isfinite(rootWidth) || rootWidth < kMinDuctWidth) {
			rootWidth = kMinDuctWidth;
		}

		std::vector<double> inheritedWidths(downstreamState.widths.size(), rootWidth);
		ApplyDefaultStraightChainTapers(downstreamState.art,
			downstreamState.path.points,
			downstreamAttachment.segmentIndex,
			inheritedWidths);
		inheritedWidths[downstreamAttachment.segmentIndex] = rootWidth;

		bool changed = downstreamState.startSegmentIndex != downstreamAttachment.segmentIndex ||
			!downstreamState.hasExplicitStart ||
			downstreamState.widths.size() != inheritedWidths.size();
		if (!changed) {
			for (size_t i = 0; i < inheritedWidths.size(); ++i) {
				if (!NearlyEqual(downstreamState.widths[i], inheritedWidths[i], 0.001)) {
					changed = true;
					break;
				}
			}
		}
		if (!changed) {
			return false;
		}

		downstreamState.widths = inheritedWidths;
		downstreamState.originalWidths = inheritedWidths;
		downstreamState.defaultWidth = ResolveMaxSegmentWidth(downstreamState.widths, rootWidth);
		downstreamState.startSegmentIndex = downstreamAttachment.segmentIndex;
		downstreamState.hasExplicitStart = true;
		downstreamState.touched = true;
		{
			std::ostringstream logStream;
			logStream << "Emory unit-feeder downstream root applied feeder="
				<< feederState.sourceId
				<< " downstream=" << downstreamState.sourceId
				<< " rootSegment=" << downstreamAttachment.segmentIndex
				<< " rootWidth=" << rootWidth;
			DuctworkLog::Write(logStream.str());
		}
		return true;
	}

	bool ApplyInheritedWidthToBranchSegmentState(const EmorySourceState& trunkState,
		int trunkSegmentIndex,
		EmorySourceState& branchState,
		int branchSegmentIndex)
	{
		if (trunkSegmentIndex < 0 ||
			trunkSegmentIndex >= static_cast<int>(trunkState.widths.size()) ||
			branchSegmentIndex < 0 ||
			branchSegmentIndex >= static_cast<int>(branchState.widths.size())) {
			return false;
		}

		double inheritedRootWidth = trunkState.widths[trunkSegmentIndex] * gBranchInheritedWidthRatio;
		if (!std::isfinite(inheritedRootWidth) || inheritedRootWidth < kMinDuctWidth) {
			inheritedRootWidth = kMinDuctWidth;
		}
		int inheritedStartSegmentIndex = branchSegmentIndex;
		bool persistInheritedStartSegment = true;
		int directionalStartSegmentIndex = 0;
		if (TryResolveDirectionalDefaultStartSegmentIndex(branchState.art,
			branchState.path,
			branchState.path.points,
			branchState.segmentCount,
			directionalStartSegmentIndex)) {
			inheritedStartSegmentIndex = directionalStartSegmentIndex;
			persistInheritedStartSegment = false;
		}
		std::vector<double> inheritedWidths(branchState.widths.size(), inheritedRootWidth);
		ApplyDefaultStraightChainTapers(branchState.art,
			branchState.path.points,
			inheritedStartSegmentIndex,
			inheritedWidths);
		if (branchSegmentIndex >= 0 && branchSegmentIndex < static_cast<int>(inheritedWidths.size())) {
			inheritedWidths[branchSegmentIndex] = inheritedRootWidth;
		}

		bool changed = branchState.startSegmentIndex != inheritedStartSegmentIndex ||
			branchState.hasExplicitStart != persistInheritedStartSegment ||
			branchState.widths.size() != inheritedWidths.size();
		if (!changed) {
			for (size_t i = 0; i < inheritedWidths.size(); ++i) {
				if (!NearlyEqual(branchState.widths[i], inheritedWidths[i], 0.001)) {
					changed = true;
					break;
				}
			}
		}
		if (!changed) {
			return false;
		}

		branchState.widths = inheritedWidths;
		branchState.originalWidths = inheritedWidths;
		branchState.defaultWidth = ResolveMaxSegmentWidth(branchState.widths, inheritedRootWidth);
		branchState.startSegmentIndex = inheritedStartSegmentIndex;
		branchState.hasExplicitStart = persistInheritedStartSegment;
		branchState.touched = true;
		return true;
	}

	size_t ApplyInheritedBranchWidths(std::vector<EmorySourceState>& states, const std::set<std::string>& affectedSourceIds)
	{
		if (states.size() < 2 || affectedSourceIds.empty()) {
			return 0;
		}

		std::vector<DuctworkConnection> connections;
		CollectEmoryNetworkConnections(states, connections);

		std::vector<DuctworkPoint> unitAttachmentPoints;
		CollectUnitAttachmentPoints(unitAttachmentPoints);
		std::vector<DuctworkPoint> registerAttachmentPoints;
		CollectRegisterAttachmentPoints(registerAttachmentPoints);

		size_t directUnitPairSyncs = 0;
		const size_t maxPasses = states.size() + 1;
		for (size_t pass = 0; pass < maxPasses; ++pass) {
			bool changedThisPass = false;
			for (size_t connectionIndex = 0; connectionIndex < connections.size(); ++connectionIndex) {
				int unitPairMainIndex = -1;
				int unitPairTransitionIndex = -1;
				PathConnectionAttachment unitPairMainAttachment;
				PathConnectionAttachment unitPairTransitionAttachment;
				if (ResolveUnitPairEndpointConnection(connections[connectionIndex],
					states,
					unitAttachmentPoints,
					unitPairMainIndex,
					unitPairMainAttachment,
					unitPairTransitionIndex,
					unitPairTransitionAttachment)) {
					const bool mainAffected = unitPairMainIndex >= 0 &&
						unitPairMainIndex < static_cast<int>(states.size()) &&
						affectedSourceIds.find(states[unitPairMainIndex].sourceId) != affectedSourceIds.end();
					const bool transitionAffected = unitPairTransitionIndex >= 0 &&
						unitPairTransitionIndex < static_cast<int>(states.size()) &&
						affectedSourceIds.find(states[unitPairTransitionIndex].sourceId) != affectedSourceIds.end();
					if (mainAffected &&
						!states[unitPairTransitionIndex].selectedSeed &&
						SyncEndpointWidthFromState(states[unitPairMainIndex],
							unitPairMainAttachment,
							states[unitPairTransitionIndex],
							unitPairTransitionAttachment)) {
						changedThisPass = true;
						++directUnitPairSyncs;
					} else if (transitionAffected &&
						!states[unitPairMainIndex].selectedSeed &&
						SyncEndpointWidthFromState(states[unitPairTransitionIndex],
							unitPairTransitionAttachment,
							states[unitPairMainIndex],
							unitPairMainAttachment)) {
						changedThisPass = true;
						++directUnitPairSyncs;
					}
					continue;
				}

				int trunkIndex = -1;
				int branchIndex = -1;
				int unitFeederIndex = -1;
				int unitFeederDownstreamIndex = -1;
				PathConnectionAttachment unitFeederAttachment;
				PathConnectionAttachment unitFeederDownstreamAttachment;
				if (ResolveUnitFeederEndpointToSegmentConnection(connections[connectionIndex],
					states,
					unitAttachmentPoints,
					unitFeederIndex,
					unitFeederAttachment,
					unitFeederDownstreamIndex,
					unitFeederDownstreamAttachment)) {
					const bool feederAffected = unitFeederIndex >= 0 &&
						unitFeederIndex < static_cast<int>(states.size()) &&
						affectedSourceIds.find(states[unitFeederIndex].sourceId) != affectedSourceIds.end();
					if (feederAffected &&
						unitFeederDownstreamIndex >= 0 &&
						unitFeederDownstreamIndex < static_cast<int>(states.size()) &&
						!states[unitFeederDownstreamIndex].selectedSeed &&
						ApplyContinuationWidthFromUnitFeeder(states[unitFeederIndex],
							unitFeederAttachment,
							states[unitFeederDownstreamIndex],
							unitFeederDownstreamAttachment)) {
						changedThisPass = true;
					}
					continue;
				}

				if (IsEndpointToSegmentBranchConnection(connections[connectionIndex], trunkIndex, branchIndex)) {
					if (trunkIndex < 0 || trunkIndex >= static_cast<int>(states.size()) ||
						branchIndex < 0 || branchIndex >= static_cast<int>(states.size()) ||
						states[branchIndex].selectedSeed ||
						affectedSourceIds.find(states[branchIndex].sourceId) == affectedSourceIds.end()) {
						continue;
					}

					PathConnectionAttachment trunkAttachment;
					PathConnectionAttachment branchAttachment;
					if (!DescribeConnectionForPath(connections[connectionIndex], trunkIndex, states[trunkIndex], trunkAttachment) ||
						!DescribeConnectionForPath(connections[connectionIndex], branchIndex, states[branchIndex], branchAttachment) ||
						branchAttachment.endpointSlot < 0) {
						continue;
					}

					if (ApplyInheritedWidthToBranchState(states[trunkIndex], trunkAttachment, states[branchIndex], branchAttachment)) {
						changedThisPass = true;
					}
					continue;
				}

				if (!ResolveSameLayerSegmentIntersectionBranch(connections[connectionIndex],
					states,
					connections,
					unitAttachmentPoints,
					registerAttachmentPoints,
					trunkIndex,
					branchIndex) ||
					trunkIndex < 0 || trunkIndex >= static_cast<int>(states.size()) ||
					branchIndex < 0 || branchIndex >= static_cast<int>(states.size()) ||
					states[branchIndex].selectedSeed ||
					affectedSourceIds.find(states[branchIndex].sourceId) == affectedSourceIds.end()) {
					continue;
				}

				const int trunkSegmentIndex = (trunkIndex == connections[connectionIndex].a)
					? connections[connectionIndex].segA
					: connections[connectionIndex].segB;
				const int branchSegmentIndex = (branchIndex == connections[connectionIndex].a)
					? connections[connectionIndex].segA
					: connections[connectionIndex].segB;

				if (ApplyInheritedWidthToBranchSegmentState(states[trunkIndex],
					trunkSegmentIndex,
					states[branchIndex],
					branchSegmentIndex)) {
					changedThisPass = true;
				}
			}
			size_t directSyncsThisPass = ApplyDirectUnitPairEndpointWidthSync(states, affectedSourceIds, unitAttachmentPoints);
			if (directSyncsThisPass > 0) {
				directUnitPairSyncs += directSyncsThisPass;
				changedThisPass = true;
			}
			if (!changedThisPass) {
				break;
			}
			if (connections.empty()) {
				break;
			}
		}

		size_t touchedCount = 0;
		for (size_t i = 0; i < states.size(); ++i) {
			if (states[i].touched && affectedSourceIds.find(states[i].sourceId) != affectedSourceIds.end()) {
				++touchedCount;
			}
		}
		if (touchedCount > 0) {
			std::ostringstream logStream;
			logStream << "Emory branch width inheritance applied sources=" << touchedCount
				<< " connections=" << connections.size()
				<< " unitPairSyncs=" << directUnitPairSyncs
				<< " factor=" << gBranchInheritedWidthRatio;
			DuctworkLog::Write(logStream.str());
		}
		return touchedCount;
	}

	void PersistInheritedBranchWidths(const std::vector<EmorySourceState>& states, const std::set<std::string>& affectedSourceIds)
	{
		if (states.empty() || affectedSourceIds.empty()) {
			return;
		}

		std::set<std::string> persistedSourceIds;
		for (size_t i = 0; i < states.size(); ++i) {
			const EmorySourceState& state = states[i];
			if (!state.touched ||
				!state.art ||
				state.widths.empty() ||
				affectedSourceIds.find(state.sourceId) == affectedSourceIds.end()) {
				continue;
			}

			WriteSegmentWidths(state.art, state.widths);
			if (state.hasExplicitStart) {
				WriteStartSegmentIndex(state.art, state.startSegmentIndex);
			} else {
				ClearStartSegmentIndex(state.art);
			}
			WriteStoredSourceBodyWidth(state.art, ResolveMaxSegmentWidth(state.widths, state.defaultWidth));
			persistedSourceIds.insert(state.sourceId);
		}

		if (persistedSourceIds.empty()) {
			return;
		}
		if (SyncAffectedSourceWidthsToBackups(persistedSourceIds, states)) {
			LocalizeVisibleFragmentMetadataFromBackup(persistedSourceIds, true);
		}
	}

	bool RefreshBranchRootWidthFromParent(const EmorySourceState& trunkState,
		int trunkSegmentIndex,
		EmorySourceState& branchState,
		int branchRootSegmentIndex)
	{
		if (trunkSegmentIndex < 0 ||
			trunkSegmentIndex >= static_cast<int>(trunkState.widths.size()) ||
			branchRootSegmentIndex < 0 ||
			branchRootSegmentIndex >= static_cast<int>(branchState.widths.size())) {
			return false;
		}

		double inheritedRootWidth = trunkState.widths[trunkSegmentIndex] * gBranchInheritedWidthRatio;
		if (!std::isfinite(inheritedRootWidth) || inheritedRootWidth < kMinDuctWidth) {
			inheritedRootWidth = kMinDuctWidth;
		}

		const bool changed = !NearlyEqual(branchState.widths[branchRootSegmentIndex], inheritedRootWidth, 0.001);
		branchState.widths[branchRootSegmentIndex] = inheritedRootWidth;
		branchState.defaultWidth = ResolveMaxSegmentWidth(branchState.widths, inheritedRootWidth);
		branchState.touched = true;
		return changed;
	}

	size_t RefreshAffectedBranchRootWidthsFromParents(std::vector<EmorySourceState>& states, const std::set<std::string>& affectedSourceIds)
	{
		if (states.size() < 2 || affectedSourceIds.empty()) {
			return 0;
		}

		std::vector<DuctworkConnection> connections;
		CollectEmoryNetworkConnections(states, connections);

		std::vector<DuctworkPoint> unitAttachmentPoints;
		CollectUnitAttachmentPoints(unitAttachmentPoints);
		std::vector<DuctworkPoint> registerAttachmentPoints;
		CollectRegisterAttachmentPoints(registerAttachmentPoints);

		size_t refreshedCount = 0;
		for (size_t connectionIndex = 0; connectionIndex < connections.size(); ++connectionIndex) {
			int trunkIndex = -1;
			int branchIndex = -1;
			int unitFeederIndex = -1;
			int unitFeederDownstreamIndex = -1;
			PathConnectionAttachment unitFeederAttachment;
			PathConnectionAttachment unitFeederDownstreamAttachment;
			if (ResolveUnitFeederEndpointToSegmentConnection(connections[connectionIndex],
				states,
				unitAttachmentPoints,
				unitFeederIndex,
				unitFeederAttachment,
				unitFeederDownstreamIndex,
				unitFeederDownstreamAttachment)) {
				if (unitFeederIndex >= 0 &&
					unitFeederIndex < static_cast<int>(states.size()) &&
					affectedSourceIds.find(states[unitFeederIndex].sourceId) != affectedSourceIds.end() &&
					unitFeederDownstreamIndex >= 0 &&
					unitFeederDownstreamIndex < static_cast<int>(states.size()) &&
					ApplyContinuationWidthFromUnitFeeder(states[unitFeederIndex],
						unitFeederAttachment,
						states[unitFeederDownstreamIndex],
						unitFeederDownstreamAttachment)) {
					++refreshedCount;
				}
				continue;
			}

			if (IsEndpointToSegmentBranchConnection(connections[connectionIndex], trunkIndex, branchIndex)) {
				if (trunkIndex < 0 || trunkIndex >= static_cast<int>(states.size()) ||
					branchIndex < 0 || branchIndex >= static_cast<int>(states.size()) ||
					affectedSourceIds.find(states[branchIndex].sourceId) == affectedSourceIds.end()) {
					continue;
				}

				PathConnectionAttachment trunkAttachment;
				PathConnectionAttachment branchAttachment;
				if (!DescribeConnectionForPath(connections[connectionIndex], trunkIndex, states[trunkIndex], trunkAttachment) ||
					!DescribeConnectionForPath(connections[connectionIndex], branchIndex, states[branchIndex], branchAttachment) ||
					branchAttachment.endpointSlot < 0 ||
					trunkAttachment.segmentIndex < 0) {
					continue;
				}

				const int branchRootSegmentIndex = (branchAttachment.endpointSlot == 0) ? 0 : (states[branchIndex].segmentCount - 1);
				if (RefreshBranchRootWidthFromParent(states[trunkIndex],
					trunkAttachment.segmentIndex,
					states[branchIndex],
					branchRootSegmentIndex)) {
					++refreshedCount;
				}
				continue;
			}

			if (!ResolveSameLayerSegmentIntersectionBranch(connections[connectionIndex],
				states,
				connections,
				unitAttachmentPoints,
				registerAttachmentPoints,
				trunkIndex,
				branchIndex) ||
				trunkIndex < 0 || trunkIndex >= static_cast<int>(states.size()) ||
				branchIndex < 0 || branchIndex >= static_cast<int>(states.size()) ||
				affectedSourceIds.find(states[branchIndex].sourceId) == affectedSourceIds.end()) {
				continue;
			}

			const int trunkSegmentIndex = (trunkIndex == connections[connectionIndex].a)
				? connections[connectionIndex].segA
				: connections[connectionIndex].segB;
			const int branchSegmentIndex = (branchIndex == connections[connectionIndex].a)
				? connections[connectionIndex].segA
				: connections[connectionIndex].segB;
			if (RefreshBranchRootWidthFromParent(states[trunkIndex],
				trunkSegmentIndex,
				states[branchIndex],
				branchSegmentIndex)) {
				++refreshedCount;
			}
		}

		if (refreshedCount > 0) {
			std::ostringstream logStream;
			logStream << "Emory width-apply refreshed selected branch roots from parents count="
				<< refreshedCount
				<< " connections=" << connections.size()
				<< " factor=" << gBranchInheritedWidthRatio;
			DuctworkLog::WriteAlways(logStream.str());
		}
		return refreshedCount;
	}

	void CascadeConnectedBranchWidths(std::vector<EmorySourceState>& states, const std::vector<DuctworkConnection>& connections)
	{
		std::vector<int> queue;
		std::vector<DuctworkPoint> unitAttachmentPoints;
		CollectUnitAttachmentPoints(unitAttachmentPoints);
		std::vector<DuctworkPoint> registerAttachmentPoints;
		CollectRegisterAttachmentPoints(registerAttachmentPoints);

		for (size_t i = 0; i < states.size(); ++i) {
			if (states[i].touched) {
				queue.push_back(static_cast<int>(i));
			}
		}

		for (size_t queueIndex = 0; queueIndex < queue.size(); ++queueIndex) {
			const int currentIndex = queue[queueIndex];
			CascadeDirectUnitPairEndpointWidthsFromState(states, currentIndex, unitAttachmentPoints, queue);
			for (size_t connectionIndex = 0; connectionIndex < connections.size(); ++connectionIndex) {
				const DuctworkConnection& connection = connections[connectionIndex];
				int unitFeederIndex = -1;
				int unitFeederDownstreamIndex = -1;
				PathConnectionAttachment unitFeederAttachment;
				PathConnectionAttachment unitFeederDownstreamAttachment;
				if (ResolveUnitFeederEndpointToSegmentConnection(connection,
					states,
					unitAttachmentPoints,
					unitFeederIndex,
					unitFeederAttachment,
					unitFeederDownstreamIndex,
					unitFeederDownstreamAttachment)) {
					if (currentIndex == unitFeederIndex &&
						unitFeederDownstreamIndex >= 0 &&
						unitFeederDownstreamIndex < static_cast<int>(states.size()) &&
						!states[unitFeederDownstreamIndex].selectedSeed &&
						ApplyContinuationWidthFromUnitFeeder(states[unitFeederIndex],
							unitFeederAttachment,
							states[unitFeederDownstreamIndex],
							unitFeederDownstreamAttachment)) {
						queue.push_back(unitFeederDownstreamIndex);
					}
					continue;
				}

				if (connection.type == kConnectionSegmentIntersection) {
					int trunkIndex = -1;
					int branchIndex = -1;
					if (!ResolveSameLayerSegmentIntersectionBranch(connection,
						states,
						connections,
						unitAttachmentPoints,
						registerAttachmentPoints,
						trunkIndex,
						branchIndex) ||
						currentIndex != trunkIndex ||
						branchIndex < 0 || branchIndex >= static_cast<int>(states.size()) ||
						states[branchIndex].selectedSeed) {
						continue;
					}

					const int trunkSegmentIndex = (trunkIndex == connection.a) ? connection.segA : connection.segB;
					const int branchSegmentIndex = (branchIndex == connection.a) ? connection.segA : connection.segB;
					if (PropagateWidthToBranchSegmentState(states[trunkIndex],
						trunkSegmentIndex,
						states[branchIndex],
						branchSegmentIndex)) {
						queue.push_back(branchIndex);
					}
					continue;
				}

				if (connection.type == kConnectionEndpointToEndpoint) {
					int unitPairMainIndex = -1;
					int unitPairTransitionIndex = -1;
					PathConnectionAttachment unitPairMainAttachment;
					PathConnectionAttachment unitPairTransitionAttachment;
					if (ResolveUnitPairEndpointConnection(connection,
						states,
						unitAttachmentPoints,
						unitPairMainIndex,
						unitPairMainAttachment,
						unitPairTransitionIndex,
						unitPairTransitionAttachment)) {
						int targetIndex = -1;
						const PathConnectionAttachment* sourceAttachment = nullptr;
						const PathConnectionAttachment* targetAttachment = nullptr;
						if (currentIndex == unitPairMainIndex) {
							targetIndex = unitPairTransitionIndex;
							sourceAttachment = &unitPairMainAttachment;
							targetAttachment = &unitPairTransitionAttachment;
						} else if (currentIndex == unitPairTransitionIndex) {
							targetIndex = unitPairMainIndex;
							sourceAttachment = &unitPairTransitionAttachment;
							targetAttachment = &unitPairMainAttachment;
						}

						if (targetIndex >= 0 &&
							targetIndex < static_cast<int>(states.size()) &&
							!states[targetIndex].selectedSeed &&
							sourceAttachment &&
							targetAttachment &&
							SyncEndpointWidthFromState(states[currentIndex],
								*sourceAttachment,
								states[targetIndex],
								*targetAttachment)) {
							queue.push_back(targetIndex);
						}
						continue;
					}

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

	void PromoteNetworkConnectorsInsideRunGroups(const std::vector<std::string>& affectedSourceIds)
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

		size_t promotedCount = 0;
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle connectorArt = allPaths[i];
			if (!connectorArt || !IsNetworkConnectorArt(connectorArt)) {
				continue;
			}

			std::set<std::string> connectorSourceIds;
			CollectArtAssociatedSourceIds(connectorArt, connectorSourceIds);
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

			std::string primarySourceId;
			if (!DuctworkMetadata::GetString(connectorArt, kEmorySourceIdKey, primarySourceId) || primarySourceId.empty()) {
				primarySourceId = ReadEmorySourceIdFromNote(connectorArt);
			}
			if (primarySourceId.empty() && !connectorSourceIds.empty()) {
				primarySourceId = *connectorSourceIds.begin();
			}
			if (primarySourceId.empty()) {
				continue;
			}

			AIArtHandle sourceArt = nullptr;
			DuctworkPath sourcePath;
			AIArtHandle group = nullptr;
			if (FindSourceArtForSourceId(primarySourceId, sourceArt, sourcePath) &&
				sourceArt &&
				GetExistingRunGroupForSource(sourceArt, primarySourceId, group) &&
				group &&
				sAIArt->ReorderArt(connectorArt, kPlaceInsideOnTop, group) == kNoErr) {
				++promotedCount;
			}
		}

		if (promotedCount > 0) {
			std::ostringstream logStream;
			logStream << "Emory network connector group-promotion count=" << promotedCount;
			DuctworkLog::Write(logStream.str());
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

		std::map<std::string, std::vector<AIArtHandle> > associatedBodiesBySource;
		std::map<std::string, std::vector<AIArtHandle> > associatedGuidesBySource;
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || !IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			std::string role;
			DuctworkMetadata::GetString(art, kEmoryRoleKey, role);
			const bool isGuide = (role == kEmoryRoleGuide);

			std::set<std::string> associatedIds;
			CollectArtAssociatedSourceIds(art, associatedIds);
			for (std::set<std::string>::const_iterator it = associatedIds.begin(); it != associatedIds.end(); ++it) {
				if (affected.find(*it) != affected.end()) {
					if (isGuide) {
						associatedGuidesBySource[*it].push_back(art);
					} else {
						associatedBodiesBySource[*it].push_back(art);
					}
				}
			}
		}

		for (std::set<std::string>::const_iterator it = affected.begin(); it != affected.end(); ++it) {
			std::map<std::string, std::vector<AIArtHandle> >::const_iterator bodiesIt = associatedBodiesBySource.find(*it);
			if (bodiesIt == associatedBodiesBySource.end() || bodiesIt->second.empty()) {
				continue;
			}

			AIArtHandle lowestBodyArt = nullptr;
			if (!FindLowestArtHandle(bodiesIt->second, lowestBodyArt) || !lowestBodyArt) {
				continue;
			}

			std::vector<EmorySourceIdCandidate> fragments;
			if (CollectSourceCenterlineFragmentsForSourceId(*it, fragments)) {
				for (size_t fragmentIndex = 0; fragmentIndex < fragments.size(); ++fragmentIndex) {
					AIArtHandle fragmentArt = fragments[fragmentIndex].art;
					if (!fragmentArt || fragmentArt == lowestBodyArt) {
						continue;
					}
					sAIArt->ReorderArt(fragmentArt, kPlaceBelow, lowestBodyArt);
				}
			}

			std::map<std::string, std::vector<AIArtHandle> >::const_iterator guidesIt = associatedGuidesBySource.find(*it);
			if (guidesIt != associatedGuidesBySource.end()) {
				for (size_t guideIndex = 0; guideIndex < guidesIt->second.size(); ++guideIndex) {
					AIArtHandle guideArt = guidesIt->second[guideIndex];
					if (!guideArt || guideArt == lowestBodyArt) {
						continue;
					}
					sAIArt->ReorderArt(guideArt, kPlaceBelow, lowestBodyArt);
				}
			}
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
		PromoteNetworkConnectorsInsideRunGroups(affectedSourceIds);
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

	bool CanMergeSameLayerMarkerBodySegments(size_t leftSegmentIndex,
		const std::vector<DuctworkPoint>& points,
		const std::vector<double>& segmentWidths,
		const std::vector<double>& trimAtStart,
		const std::vector<double>& trimAtEnd,
		const std::vector<int>& straightChainIndexBySegment,
		const std::set<int>& markerVertices)
	{
		const size_t rightSegmentIndex = leftSegmentIndex + 1;
		const size_t segmentCount = points.size() > 1 ? points.size() - 1 : 0;
		if (rightSegmentIndex >= segmentCount ||
			rightSegmentIndex >= segmentWidths.size() ||
			rightSegmentIndex >= trimAtStart.size() ||
			leftSegmentIndex >= trimAtEnd.size()) {
			return false;
		}

		const int markerVertexIndex = static_cast<int>(rightSegmentIndex);
		if (markerVertices.find(markerVertexIndex) == markerVertices.end()) {
			return false;
		}
		if (trimAtEnd[leftSegmentIndex] > 0.001 || trimAtStart[rightSegmentIndex] > 0.001) {
			return false;
		}
		if (std::fabs(segmentWidths[leftSegmentIndex] - segmentWidths[rightSegmentIndex]) > 0.001) {
			return false;
		}
		if (!AreConsecutiveSegmentsCollinear(points,
				static_cast<int>(leftSegmentIndex),
				static_cast<int>(rightSegmentIndex))) {
			return false;
		}

		int leftChainIndex = -1;
		int rightChainIndex = -1;
		if (leftSegmentIndex < straightChainIndexBySegment.size()) {
			leftChainIndex = straightChainIndexBySegment[leftSegmentIndex];
		}
		if (rightSegmentIndex < straightChainIndexBySegment.size()) {
			rightChainIndex = straightChainIndexBySegment[rightSegmentIndex];
		}
		if (leftChainIndex >= 0 && rightChainIndex >= 0 && leftChainIndex != rightChainIndex) {
			return false;
		}

		return true;
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
		WriteStoredSourceStrokeWidthForSourceId(sourceId, sourceStrokeWidth);

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
		const int startSegmentIndex = ReadStartSegmentIndex(path.art, segmentCount);
		const bool hasExplicitStartSegmentIndex = HasExplicitStartSegmentIndex(path.art);
		std::vector<double> segmentWidths;
		ReadSegmentWidths(path.art, segmentCount, bodyWidth, segmentWidths);
		NormalizeGuideLikeStoredWidths(path.art,
			sourceId,
			points,
			startSegmentIndex,
			bodyWidth,
			segmentWidths);
		std::set<int> sameLayerMarkerVertices;
		CollectSameLayerIntersectionMarkerVertices(path.art, points, sameLayerMarkerVertices);
		bool normalizedUnitEndpointTaper = false;
		bool normalizedSameLayerMarkerWidths = false;
		if (!HasStoredSegmentWidths(path.art, segmentCount)) {
			ApplyDefaultStraightChainTapers(path.art, points, startSegmentIndex, segmentWidths);
			WriteSegmentWidths(path.art, segmentWidths);
		}
		if (NormalizeBlueOrangeUnitEndpointTaper(path.layerName, points, segmentWidths)) {
			normalizedUnitEndpointTaper = true;
			WriteSegmentWidths(path.art, segmentWidths);
			WriteStoredSourceBodyWidth(path.art, ResolveMaxSegmentWidth(segmentWidths, bodyWidth));
		}
		if (NormalizeSameLayerIntersectionMarkerWidths(sameLayerMarkerVertices, segmentWidths)) {
			normalizedSameLayerMarkerWidths = true;
			WriteSegmentWidths(path.art, segmentWidths);
			WriteStoredSourceBodyWidth(path.art, ResolveMaxSegmentWidth(segmentWidths, bodyWidth));
		}
		bool omitStartSegmentThickness = ReadFinalSegmentThicknessFlag(path.art, kEmoryOmitStartSegmentThicknessKey);
		bool omitEndSegmentThickness = ReadFinalSegmentThicknessFlag(path.art, kEmoryOmitEndSegmentThicknessKey);
		std::string omitStartTerminalStyle = ReadTerminalSegmentStyle(path.art, true);
		std::string omitEndTerminalStyle = ReadTerminalSegmentStyle(path.art, false);
		std::set<size_t> effectiveOmittedSegments;
		ReadOmittedSegmentIndices(path.art, effectiveOmittedSegments);
		const bool hasLocalSegmentOmitMap = !effectiveOmittedSegments.empty();
		ResolveFragmentOmitThicknessFlagsFromBackup(sourceId, points, omitStartSegmentThickness, omitEndSegmentThickness);
		ResolveFragmentTerminalStylesFromBackup(sourceId, points, omitStartTerminalStyle, omitEndTerminalStyle);
		std::set<size_t> backupOmittedSegments;
		const bool hasBackupSegmentOmitMap = ResolveFragmentOmittedSegmentsFromBackup(sourceId, points, backupOmittedSegments);
		effectiveOmittedSegments.insert(backupOmittedSegments.begin(), backupOmittedSegments.end());
		if (hasBackupSegmentOmitMap) {
			omitStartSegmentThickness = false;
			omitEndSegmentThickness = false;
		}
		{
			std::ostringstream logStream;
			logStream << "Emory path sourceId=" << sourceId
				<< " layer=" << path.layerName
				<< " segments=" << segmentCount
				<< " startSegment=" << startSegmentIndex
				<< " explicitStart=" << (hasExplicitStartSegmentIndex ? 1 : 0)
				<< " bodyWidth=" << bodyWidth
				<< " strokeWidth=" << sourceStrokeWidth
				<< " omitStart=" << (omitStartSegmentThickness ? 1 : 0)
				<< " omitEnd=" << (omitEndSegmentThickness ? 1 : 0)
				<< " localMap=" << (hasLocalSegmentOmitMap ? 1 : 0)
				<< " backupMap=" << (hasBackupSegmentOmitMap ? 1 : 0)
				<< " omittedSegments=[" << SerializeSegmentIndexSet(effectiveOmittedSegments) << "]"
				<< " widths=[" << SerializeSegmentWidths(segmentWidths) << "]"
				<< " unitEndpointTaperNormalized=" << (normalizedUnitEndpointTaper ? 1 : 0)
				<< " sameLayerMarkerCount=" << sameLayerMarkerVertices.size()
				<< " sameLayerMarkerWidthsNormalized=" << (normalizedSameLayerMarkerWidths ? 1 : 0)
				<< " start=" << SerializePointForLog(points.front())
				<< " end=" << SerializePointForLog(points.back());
			DuctworkLog::Write(logStream.str());
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
			const bool prevOmitted = IsSegmentThicknessOmitted(prevJointSegmentIndex,
					segmentCount,
					omitStartSegmentThickness,
					omitEndSegmentThickness,
					effectiveOmittedSegments);
			const bool nextOmitted = IsSegmentThicknessOmitted(nextJointSegmentIndex,
					segmentCount,
					omitStartSegmentThickness,
					omitEndSegmentThickness,
					effectiveOmittedSegments);
			if (prevOmitted || nextOmitted) {
				std::ostringstream logStream;
				logStream << "Emory joint-skip sourceId=" << sourceId
					<< " jointIndex=" << jointIndex
					<< " prevSegment=" << prevJointSegmentIndex
					<< " nextSegment=" << nextJointSegmentIndex
					<< " prevOmitted=" << (prevOmitted ? 1 : 0)
					<< " nextOmitted=" << (nextOmitted ? 1 : 0);
				DuctworkLog::Write(logStream.str());
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
			if (isStraightContinuation &&
				sameLayerMarkerVertices.find(static_cast<int>(jointIndex)) != sameLayerMarkerVertices.end()) {
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
		size_t segmentIndex = 0;
		while (segmentIndex < segmentCount) {
			size_t spanStartSegmentIndex = segmentIndex;
			size_t spanEndSegmentIndex = segmentIndex;
			if (IsSegmentThicknessOmitted(segmentIndex,
					segmentCount,
					omitStartSegmentThickness,
					omitEndSegmentThickness,
					effectiveOmittedSegments)) {
				std::ostringstream logStream;
				logStream << "Emory segment-skip sourceId=" << sourceId
					<< " segmentIndex=" << segmentIndex
					<< " start=" << SerializePointForLog(points[segmentIndex])
					<< " end=" << SerializePointForLog(points[segmentIndex + 1]);
				DuctworkLog::Write(logStream.str());
				++segmentIndex;
				continue;
			}

			while (spanEndSegmentIndex + 1 < segmentCount &&
				!IsSegmentThicknessOmitted(spanEndSegmentIndex + 1,
					segmentCount,
					omitStartSegmentThickness,
					omitEndSegmentThickness,
					effectiveOmittedSegments) &&
				CanMergeSameLayerMarkerBodySegments(spanEndSegmentIndex,
					points,
					segmentWidths,
					trimAtStart,
					trimAtEnd,
					straightChainIndexBySegment,
					sameLayerMarkerVertices)) {
				++spanEndSegmentIndex;
			}

			const size_t nextSegmentIndex = spanEndSegmentIndex + 1;
			const DuctworkPoint start = points[spanStartSegmentIndex];
			const DuctworkPoint end = points[spanEndSegmentIndex + 1];
			Vec2 dir;
			Vec2 normal;
			if (!BuildUnitDirection(start, end, dir, normal)) {
				segmentIndex = nextSegmentIndex;
				continue;
			}
			const double segmentBodyWidth = segmentWidths[spanStartSegmentIndex];
			std::string taperAlignment = "center";
			bool chainHorizontal = false;
			bool chainVertical = false;
			double taperReferenceWidth = segmentBodyWidth;
			if (spanStartSegmentIndex < straightChainIndexBySegment.size()) {
				const int chainIndex = straightChainIndexBySegment[spanStartSegmentIndex];
				if (chainIndex >= 0 && chainIndex < static_cast<int>(straightChains.size())) {
					const StraightChainInfo& chain = straightChains[chainIndex];
					taperAlignment = chain.alignment;
					chainHorizontal = chain.horizontal;
					chainVertical = chain.vertical;
					taperReferenceWidth = chain.referenceWidth;
				}
			}

			const double length = std::hypot(end.x - start.x, end.y - start.y);
			double trimStart = trimAtStart[spanStartSegmentIndex];
			double trimEnd = trimAtEnd[spanEndSegmentIndex];
			const double maxUsable = length - 0.1;
			if (maxUsable <= 0.1) {
				segmentIndex = nextSegmentIndex;
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
				segmentIndex = nextSegmentIndex;
				continue;
			}

			std::vector<DuctworkPoint> polygon;
			if (!BuildAlignedBandPolygon(trimmedStart, trimmedEnd, segmentBodyWidth, taperReferenceWidth,
				taperAlignment, chainHorizontal, chainVertical, polygon)) {
				segmentIndex = nextSegmentIndex;
				continue;
			}

			AIArtHandle segmentArt = nullptr;
			if (!CreateClosedPath(referenceArt, polygon, segmentArt) || !segmentArt) {
				++stats.failed;
				segmentIndex = nextSegmentIndex;
				continue;
			}
			if (!ApplyFilledPathStyle(segmentArt, colors, sourceStrokeWidth)) {
				sAIArt->DisposeArt(segmentArt);
				++stats.failed;
				segmentIndex = nextSegmentIndex;
				continue;
			}

			TagGeneratedArt(segmentArt, kEmoryRoleSegment, sourceId, segmentBodyWidth,
				static_cast<int>(spanStartSegmentIndex), -1, std::string());
			if (spanEndSegmentIndex > spanStartSegmentIndex) {
				std::ostringstream logStream;
				logStream << "Emory marker-span sourceId=" << sourceId
					<< " startSegment=" << spanStartSegmentIndex
					<< " endSegment=" << spanEndSegmentIndex
					<< " width=" << segmentBodyWidth
					<< " start=" << SerializePointForLog(start)
					<< " end=" << SerializePointForLog(end);
				DuctworkLog::Write(logStream.str());
			}
			referenceArt = segmentArt;
			++stats.segmentsCreated;
			++stats.created;
			segmentIndex = nextSegmentIndex;
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

		if (IsCenterlineHidden(path.art)) {
			auto createOmittedGuide = [&](size_t segmentIndex, const std::string& terminalStyle) {
				if (segmentIndex >= segmentCount) {
					return;
				}
				std::vector<DuctworkPoint> guidePoints;
				guidePoints.push_back(points[segmentIndex]);
				guidePoints.push_back(points[segmentIndex + 1]);
				AIArtHandle guideArt = nullptr;
				if (!CreateOpenPath(referenceArt, guidePoints, guideArt) || !guideArt) {
					++stats.failed;
					return;
				}
				if (!ApplyGuideStyleInternal(guideArt, colors)) {
					sAIArt->DisposeArt(guideArt);
					++stats.failed;
					return;
				}
				if (!ApplyTerminalGuideStyleToGuideArt(guideArt, points, segmentIndex, terminalStyle)) {
					sAIArt->DisposeArt(guideArt);
					++stats.failed;
					return;
				}
				TagGeneratedArt(guideArt, kEmoryRoleGuide, sourceId, 0.0,
					static_cast<int>(segmentIndex), -1, std::string());
				referenceArt = guideArt;
				++stats.guidesStyled;
				++stats.created;
			};

			const bool effectiveOmitStart = effectiveOmittedSegments.find(0) != effectiveOmittedSegments.end();
			const bool effectiveOmitEnd = segmentCount > 0 &&
				effectiveOmittedSegments.find(segmentCount - 1) != effectiveOmittedSegments.end();

			if (effectiveOmitStart) {
				createOmittedGuide(0, omitStartTerminalStyle);
			}
			if (effectiveOmitEnd && segmentCount > 0) {
				createOmittedGuide(segmentCount - 1, omitEndTerminalStyle);
			}
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

void DuctworkGeometry::SetEmoryBranchTaperReductionPercent(double reductionPercent)
{
	if (!std::isfinite(reductionPercent)) {
		reductionPercent = 25.0;
	}
	if (reductionPercent < 0.0) {
		reductionPercent = 0.0;
	}
	if (reductionPercent > 95.0) {
		reductionPercent = 95.0;
	}
	gBranchInheritedWidthRatio = ClampBranchInheritedWidthRatio((100.0 - reductionPercent) / 100.0);
}

bool DuctworkGeometry::IsCenterlineCandidate(AIArtHandle art,
	const std::vector<DuctworkPoint>& points,
	bool closed,
	const std::string& layerName)
{
	if (!art || closed || points.size() < 2) {
		return false;
	}
	// Emory source centerlines are only the ductwork color layers.
	// Thermostat lines share the broader line-layer bucket but should
	// never participate in centerline hide/show, revert, or source-id
	// collection for Emory duct runs.
	if (!DuctworkLayers::IsColorLayerName(layerName)) {
		return false;
	}
	if (IsGeneratedEmoryArtInternal(art)) {
		return false;
	}
	if (IsBackupCenterlineArt(art)) {
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

bool DuctworkGeometry::CopyEmoryCenterlineIdentity(AIArtHandle sourceArt, AIArtHandle targetArt)
{
	if (!sourceArt || !targetArt) {
		return false;
	}

	std::string sourceId;
	if (!EnsureEmorySourceId(sourceArt, sourceId) || sourceId.empty()) {
		return false;
	}

	EnsureStoredOriginalCenterlinePoints(sourceArt);
	std::string originalPointsSerialized;
	if (DuctworkMetadata::GetString(sourceArt, kEmoryOriginalPathPointsKey, originalPointsSerialized) &&
		!originalPointsSerialized.empty()) {
		DuctworkMetadata::SetString(targetArt, kEmoryOriginalPathPointsKey, originalPointsSerialized);
	}
	std::string terminalStyle;
	if (DuctworkMetadata::GetString(sourceArt, kEmoryTerminalStartStyleKey, terminalStyle) && !terminalStyle.empty()) {
		DuctworkMetadata::SetString(targetArt, kEmoryTerminalStartStyleKey, terminalStyle);
	} else {
		DuctworkMetadata::RemoveKey(targetArt, kEmoryTerminalStartStyleKey);
	}
	if (DuctworkMetadata::GetString(sourceArt, kEmoryTerminalEndStyleKey, terminalStyle) && !terminalStyle.empty()) {
		DuctworkMetadata::SetString(targetArt, kEmoryTerminalEndStyleKey, terminalStyle);
	} else {
		DuctworkMetadata::RemoveKey(targetArt, kEmoryTerminalEndStyleKey);
	}

	DuctworkMetadata::SetString(targetArt, kEmorySourceIdKey, sourceId);
	DuctworkMetadata::SetString(targetArt, kEmoryRoleKey, kEmoryRoleCenterline);
	UpdateEmoryTokens(targetArt, kEmoryRoleCenterline, sourceId);
	return true;
}

void DuctworkGeometry::SplitEmoryCenterlineMetadata(AIArtHandle sourceArt,
	size_t splitSegmentIndex,
	AIArtHandle firstArt,
	AIArtHandle secondArt)
{
	SplitEmoryCenterlineMetadataInternal(sourceArt, splitSegmentIndex, firstArt, secondArt);
}

void DuctworkGeometry::EnsureEmoryBackupCenterlines(const std::vector<AIArtHandle>& selection)
{
	if (!sAIArt) {
		return;
	}

	for (size_t i = 0; i < selection.size(); ++i) {
		if (selection[i]) {
			EnsureBackupCenterlineForArtInternal(selection[i]);
		}
	}
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
	std::vector<DuctworkPoint> rectangularRegisterAttachmentPoints;
	CollectLayerAttachmentPoints("Rectangular Registers", rectangularRegisterAttachmentPoints);
	std::vector<DuctworkPoint> unitAttachmentPoints;
	CollectUnitAttachmentPoints(unitAttachmentPoints);

	std::vector<AIArtHandle> allLineArt;
	CollectAllLineLayerPaths(allLineArt);
	std::vector<DuctworkPath> allPaths;
	std::map<AIArtHandle, int> artIndexByHandle;
	allPaths.reserve(allLineArt.size());
	for (size_t i = 0; i < allLineArt.size(); ++i) {
		if (IsBackupCenterlineArt(allLineArt[i])) {
			continue;
		}
		DuctworkPath entry;
		if (!BuildProcessPathForArt(allLineArt[i], entry) ||
			!IsCenterlineCandidate(entry.art, entry.points, entry.closed, entry.layerName)) {
			continue;
		}
		artIndexByHandle[entry.art] = static_cast<int>(allPaths.size());
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
	std::vector<DuctworkConnection> rawAllConnections = allConnections;
	{
		std::vector<DuctworkPoint> ignorePoints;
		CollectIgnoreAnchorPoints(ignorePoints);
		if (!ignorePoints.empty() && !allConnections.empty()) {
			std::vector<DuctworkConnection> filteredConnections;
			filteredConnections.reserve(allConnections.size());
			for (size_t connectionIndex = 0; connectionIndex < allConnections.size(); ++connectionIndex) {
				if (IsPointNearAny(allConnections[connectionIndex].point, ignorePoints, 10.0)) {
					continue;
				}
				filteredConnections.push_back(allConnections[connectionIndex]);
			}
			allConnections.swap(filteredConnections);
		}
	}

	const auto nearPoint = [](const DuctworkPoint& a, const DuctworkPoint& b) {
		return std::hypot(a.x - b.x, a.y - b.y) <= 2.0;
	};

	const auto endpointConnected = [&](AIArtHandle art, int endpointIndex) {
		std::map<AIArtHandle, int>::const_iterator it = artIndexByHandle.find(art);
		if (it == artIndexByHandle.end()) {
			return false;
		}
		return EndpointHasAnyCenterlineConnection(allConnections, it->second, endpointIndex);
	};

	const auto pathActsAsTrunk = [&](AIArtHandle art) {
		std::map<AIArtHandle, int>::const_iterator it = artIndexByHandle.find(art);
		if (it == artIndexByHandle.end()) {
			return false;
		}
		const int pathIndex = it->second;
		for (size_t connectionIndex = 0; connectionIndex < allConnections.size(); ++connectionIndex) {
			int trunkIndex = -1;
			int branchIndex = -1;
			if (IsEndpointToSegmentBranchConnection(allConnections[connectionIndex], trunkIndex, branchIndex) &&
				trunkIndex == pathIndex) {
				return true;
			}
		}
		return false;
	};

	const auto pathHasSegmentIntersection = [&](AIArtHandle art) {
		std::map<AIArtHandle, int>::const_iterator it = artIndexByHandle.find(art);
		if (it == artIndexByHandle.end()) {
			return false;
		}
		const int pathIndex = it->second;
		for (size_t connectionIndex = 0; connectionIndex < allConnections.size(); ++connectionIndex) {
			const DuctworkConnection& connection = allConnections[connectionIndex];
			if (connection.type == kConnectionSegmentIntersection &&
				(connection.a == pathIndex || connection.b == pathIndex)) {
				return true;
			}
		}
		return false;
	};

	const auto pathHasSameColorSegmentIntersection = [&](AIArtHandle art) {
		std::map<AIArtHandle, int>::const_iterator it = artIndexByHandle.find(art);
		if (it == artIndexByHandle.end()) {
			return false;
		}
		const int pathIndex = it->second;
		if (pathIndex < 0 || pathIndex >= static_cast<int>(allPaths.size())) {
			return false;
		}
		const std::string layerName = allPaths[static_cast<size_t>(pathIndex)].layerName;
		for (size_t connectionIndex = 0; connectionIndex < allConnections.size(); ++connectionIndex) {
			const DuctworkConnection& connection = allConnections[connectionIndex];
			if (connection.type != kConnectionSegmentIntersection) {
				continue;
			}
			int otherIndex = -1;
			if (connection.a == pathIndex) {
				otherIndex = connection.b;
			} else if (connection.b == pathIndex) {
				otherIndex = connection.a;
			}
			if (otherIndex < 0 || otherIndex >= static_cast<int>(allPaths.size())) {
				continue;
			}
			if (allPaths[static_cast<size_t>(otherIndex)].layerName == layerName) {
				return true;
			}
		}
		return false;
	};

	const auto pathActsAsUnitEndpointBranchTrunk = [&](AIArtHandle art) {
		std::map<AIArtHandle, int>::const_iterator it = artIndexByHandle.find(art);
		if (it == artIndexByHandle.end()) {
			return false;
		}
		const int pathIndex = it->second;
		if (pathIndex < 0 || pathIndex >= static_cast<int>(allPaths.size())) {
			return false;
		}

		const DuctworkPath& trunkPath = allPaths[static_cast<size_t>(pathIndex)];
		for (size_t connectionIndex = 0; connectionIndex < rawAllConnections.size(); ++connectionIndex) {
			const DuctworkConnection& connection = rawAllConnections[connectionIndex];
			int trunkIndex = -1;
			int branchIndex = -1;
			if (!IsEndpointToSegmentBranchConnection(connection, trunkIndex, branchIndex) ||
				trunkIndex != pathIndex ||
				branchIndex < 0 ||
				branchIndex >= static_cast<int>(allPaths.size())) {
				continue;
			}

			const DuctworkPath& branchPath = allPaths[static_cast<size_t>(branchIndex)];
			if (branchPath.closed ||
				trunkPath.closed ||
				branchPath.points.size() < 2 ||
				trunkPath.points.size() < 2 ||
				branchPath.layerName != trunkPath.layerName ||
				!DuctworkLayers::IsColorLayerName(branchPath.layerName)) {
				continue;
			}

			const int branchEndpointIndex = (connection.a == branchIndex) ?
				connection.endpointA :
				connection.endpointB;
			int branchEndpointSlot = -1;
			if (!GetEndpointSlotForPath(branchPath, branchEndpointIndex, branchEndpointSlot)) {
				continue;
			}

			const DuctworkPoint& unitEndpoint = (branchEndpointSlot == 0) ?
				branchPath.points.back() :
				branchPath.points.front();
			if (IsPointNearAny(unitEndpoint, unitAttachmentPoints, 10.0)) {
				return true;
			}
		}
		return false;
	};

	std::set<std::string> processedSourceIds;
	for (size_t i = 0; i < paths.size(); ++i) {
		const DuctworkPath& path = paths[i];
		if (!path.art || path.points.size() < 2 ||
			!DuctworkLayers::IsColorLayerName(path.layerName) ||
			!IsCenterlineCandidate(path.art, path.points, path.closed, path.layerName)) {
			continue;
		}

		std::string sourceId;
		if (!EnsureEmorySourceId(path.art, sourceId) || sourceId.empty() ||
			!processedSourceIds.insert(sourceId).second) {
			continue;
		}

		AIArtHandle metadataArt = path.art;
		std::vector<DuctworkPoint> sourcePoints = path.points;
		bool hasBackup = false;

		std::vector<AIArtHandle> backupArts;
		if (CollectBackupCenterlinesForSourceId(sourceId, backupArts) && !backupArts.empty()) {
			size_t bestPointCount = 0;
			for (size_t backupIndex = 0; backupIndex < backupArts.size(); ++backupIndex) {
				AIArtHandle backupArt = backupArts[backupIndex];
				if (!backupArt) {
					continue;
				}

				std::vector<DuctworkPoint> backupPoints;
				bool backupClosed = false;
				if (!GetPathPoints(backupArt, backupPoints, backupClosed) || backupClosed || backupPoints.size() < 2) {
					continue;
				}

				if (!metadataArt || backupPoints.size() > bestPointCount) {
					metadataArt = backupArt;
					sourcePoints = backupPoints;
					bestPointCount = backupPoints.size();
					hasBackup = true;
				}
			}
		}

		bool omitStart = false;
		bool omitEnd = false;
		std::set<size_t> omittedSegmentIndices;
		bool sourceActsAsTrunkConnection = false;
		bool sourceHasSegmentIntersection = false;
		bool sourceHasSameColorSegmentIntersection = false;
		bool sourceHasUnitEndpointBranchTrunk = false;
		bool protectedFromOmittingAllSegments = false;

		if (enabled && sourcePoints.size() >= 2) {
			const bool registerAtStart = IsPointNearAny(sourcePoints.front(), registerAttachmentPoints, 10.0);
			const bool registerAtEnd = IsPointNearAny(sourcePoints.back(), registerAttachmentPoints, 10.0);
			const bool rectangularRegisterAtStart = IsPointNearAny(sourcePoints.front(), rectangularRegisterAttachmentPoints, 10.0);
			const bool rectangularRegisterAtEnd = IsPointNearAny(sourcePoints.back(), rectangularRegisterAttachmentPoints, 10.0);
			const bool unitAtStart = IsPointNearAny(sourcePoints.front(), unitAttachmentPoints, 10.0);
			const bool unitAtEnd = IsPointNearAny(sourcePoints.back(), unitAttachmentPoints, 10.0);

			for (size_t pathIndex = 0; pathIndex < paths.size(); ++pathIndex) {
				const DuctworkPath& candidate = paths[pathIndex];
				if (!candidate.art || candidate.points.size() < 2) {
					continue;
				}

				std::string candidateSourceId;
				if (!EnsureEmorySourceId(candidate.art, candidateSourceId) || candidateSourceId != sourceId) {
					continue;
				}

				if (pathActsAsTrunk(candidate.art)) {
					sourceActsAsTrunkConnection = true;
				}
				if (pathHasSegmentIntersection(candidate.art)) {
					sourceHasSegmentIntersection = true;
				}
				if (pathHasSameColorSegmentIntersection(candidate.art)) {
					sourceHasSameColorSegmentIntersection = true;
				}
				if (pathActsAsUnitEndpointBranchTrunk(candidate.art)) {
					sourceHasUnitEndpointBranchTrunk = true;
				}

				const bool startConnected = endpointConnected(candidate.art, 0);
				const bool endConnected = endpointConnected(candidate.art, static_cast<int>(candidate.points.size() - 1));

				if (registerAtStart) {
					if (!startConnected && nearPoint(candidate.points.front(), sourcePoints.front())) {
						omitStart = true;
					}
					if (!endConnected && nearPoint(candidate.points.back(), sourcePoints.front())) {
						omitStart = true;
					}
				}
				if (registerAtEnd) {
					if (!startConnected && nearPoint(candidate.points.front(), sourcePoints.back())) {
						omitEnd = true;
					}
					if (!endConnected && nearPoint(candidate.points.back(), sourcePoints.back())) {
						omitEnd = true;
					}
				}
			}

			const bool preserveTerminalOmitForDoubleRegisterSegmentIntersection =
				!sourceHasUnitEndpointBranchTrunk &&
				sourceHasSameColorSegmentIntersection &&
				registerAtStart &&
				registerAtEnd;
			if (preserveTerminalOmitForDoubleRegisterSegmentIntersection) {
				// Same-color crossovers with registers on both ends should keep the through
				// run thick except for the true register-connected terminal segments.
				omitStart = true;
				omitEnd = true;
				omittedSegmentIndices.clear();
				if (sourcePoints.size() >= 2) {
					omittedSegmentIndices.insert(0);
					omittedSegmentIndices.insert(sourcePoints.size() - 2);
				}
			} else {
				// Paths acting as trunks for endpoint-to-segment branches, or participating
				// in true segment intersections, should keep full ductwork even if a part
				// marker or ignored endpoint sits at one of their ends.
				if (sourceActsAsTrunkConnection || sourceHasSegmentIntersection) {
					omittedSegmentIndices.clear();
					const bool preserveStartTerminal =
						(omitStart && registerAtStart && (unitAtEnd || rectangularRegisterAtStart)) ||
						(omitStart &&
							AllowDistributionTrunkTerminalRegister(sourcePoints.size(),
								sourceActsAsTrunkConnection,
								registerAtStart) &&
							!unitAtStart &&
							!unitAtEnd);
					const bool preserveEndTerminal =
						(omitEnd && registerAtEnd && (unitAtStart || rectangularRegisterAtEnd)) ||
						(omitEnd &&
							AllowDistributionTrunkTerminalRegister(sourcePoints.size(),
								sourceActsAsTrunkConnection,
								registerAtEnd) &&
							!unitAtStart &&
							!unitAtEnd);
					omitStart = preserveStartTerminal;
					omitEnd = preserveEndTerminal;
					if (omitStart) {
						omittedSegmentIndices.insert(0);
					}
					if (omitEnd && sourcePoints.size() >= 2) {
						omittedSegmentIndices.insert(sourcePoints.size() - 2);
					}
				} else {
					if (omitStart) {
						omittedSegmentIndices.insert(0);
					}
					if (omitEnd && sourcePoints.size() >= 2) {
						omittedSegmentIndices.insert(sourcePoints.size() - 2);
					}
				}
			}

			const size_t sourceSegmentCount = sourcePoints.size() - 1;
			if (sourceSegmentCount > 0 && omittedSegmentIndices.size() >= sourceSegmentCount) {
				// A short run can have every segment classified as a terminal segment.
				// Keep the body rather than turning a valid register-to-register run
				// into registers and no Emory ductwork.
				omitStart = false;
				omitEnd = false;
				omittedSegmentIndices.clear();
				protectedFromOmittingAllSegments = true;
			}
		}

		WriteFinalSegmentThicknessMetadata(metadataArt, omitStart, omitEnd);
		WriteOmittedSegmentIndices(metadataArt, omittedSegmentIndices);
		{
			std::ostringstream logStream;
			logStream << "Emory final-thickness sourceId=" << sourceId
				<< " metadataArtIsBackup=" << (hasBackup ? 1 : 0)
				<< " registerAtStart=" << (enabled && IsPointNearAny(sourcePoints.front(), registerAttachmentPoints, 10.0) ? 1 : 0)
				<< " registerAtEnd=" << (enabled && IsPointNearAny(sourcePoints.back(), registerAttachmentPoints, 10.0) ? 1 : 0)
				<< " rectangularRegisterAtStart=" << (enabled && IsPointNearAny(sourcePoints.front(), rectangularRegisterAttachmentPoints, 10.0) ? 1 : 0)
				<< " rectangularRegisterAtEnd=" << (enabled && IsPointNearAny(sourcePoints.back(), rectangularRegisterAttachmentPoints, 10.0) ? 1 : 0)
				<< " unitAtStart=" << (enabled && IsPointNearAny(sourcePoints.front(), unitAttachmentPoints, 10.0) ? 1 : 0)
				<< " unitAtEnd=" << (enabled && IsPointNearAny(sourcePoints.back(), unitAttachmentPoints, 10.0) ? 1 : 0)
				<< " trunkConnection=" << (sourceActsAsTrunkConnection ? 1 : 0)
				<< " segmentIntersection=" << (sourceHasSegmentIntersection ? 1 : 0)
				<< " sameColorSegmentIntersection=" << (sourceHasSameColorSegmentIntersection ? 1 : 0)
				<< " unitEndpointBranchTrunk=" << (sourceHasUnitEndpointBranchTrunk ? 1 : 0)
				<< " protectedFromOmittingAllSegments=" << (protectedFromOmittingAllSegments ? 1 : 0)
				<< " omitStart=" << (omitStart ? 1 : 0)
				<< " omitEnd=" << (omitEnd ? 1 : 0)
				<< " omittedSegments=[" << SerializeSegmentIndexSet(omittedSegmentIndices) << "]"
				<< " start=" << SerializePointForLog(sourcePoints.front())
				<< " end=" << SerializePointForLog(sourcePoints.back());
			DuctworkLog::Write(logStream.str());
		}

		if (hasBackup) {
			for (size_t pathIndex = 0; pathIndex < paths.size(); ++pathIndex) {
				const DuctworkPath& candidate = paths[pathIndex];
				if (!candidate.art) {
					continue;
				}
				std::string candidateSourceId;
				if (!EnsureEmorySourceId(candidate.art, candidateSourceId) || candidateSourceId != sourceId) {
					continue;
				}

				DuctworkMetadata::RemoveKey(candidate.art, kEmoryOmitStartSegmentThicknessKey);
				DuctworkMetadata::RemoveKey(candidate.art, kEmoryOmitEndSegmentThicknessKey);
				DuctworkMetadata::RemoveKey(candidate.art, kEmoryOmittedSegmentIndicesKey);
			}
		} else {
			WriteFinalSegmentThicknessMetadata(path.art, omitStart, omitEnd);
			WriteOmittedSegmentIndices(path.art, omittedSegmentIndices);
		}
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
		if (it->second > 0.0) {
			WriteStoredSourceStrokeWidthForSourceId(it->first, it->second);
			WriteStoredSourceStrokeExplicitForSourceId(it->first, true);
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
	}

	if (!affectedSourceIds.empty()) {
		std::sort(affectedSourceIds.begin(), affectedSourceIds.end());
		affectedSourceIds.erase(std::unique(affectedSourceIds.begin(), affectedSourceIds.end()), affectedSourceIds.end());
		std::set<std::string> affectedSourceIdSet(affectedSourceIds.begin(), affectedSourceIds.end());

		std::vector<EmorySourceState> sourceStates;
		std::map<std::string, int> stateIndexBySourceId;
		if (CollectEmorySourceStates(sourceStates, stateIndexBySourceId)) {
			MarkWidthApplyProtectedSources(sourceStates);
		}
		if (!sourceStates.empty() &&
			ApplyInheritedBranchWidths(sourceStates, affectedSourceIdSet) > 0) {
			PersistInheritedBranchWidths(sourceStates, affectedSourceIdSet);
		}
	}

	for (size_t i = 0; i < paths.size(); ++i) {
		GenerateEmoryForPath(paths[i], stats);
	}

	if (!affectedSourceIds.empty()) {
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

bool DuctworkGeometry::MarkSelectedEmoryConnectorSeparate(std::string& outMessage)
{
	outMessage = "Select one or more generated Emory tee/cross connectors first.";
	if (!sAIArt || !sAILayer || !sAIPath) {
		outMessage = "Illustrator SDK is not available.";
		return false;
	}

	NormalizeDuplicateEmorySourceIds();

	std::vector<AIArtHandle> selection;
	DuctworkSelection::CollectSelectedPaths(selection);
	if (selection.empty()) {
		return false;
	}

	std::vector<EmorySourceState> states;
	std::map<std::string, int> stateIndexBySourceId;
	if (!CollectEmorySourceStates(states, stateIndexBySourceId) || states.size() < 2) {
		outMessage = "No Emory source runs were found to separate.";
		return false;
	}

	std::vector<DuctworkConnection> connections;
	CollectEmoryNetworkConnections(states, connections);

	struct SeparateTarget
	{
		DuctworkPoint point;
		std::set<std::string> sourceIds;
		bool matchedConnection = false;
	};

	std::vector<SeparateTarget> targets;
	std::set<std::string> affectedSourceIds;
	size_t selectedConnectorCount = 0;
	size_t fallbackPointCount = 0;

	for (size_t i = 0; i < selection.size(); ++i) {
		AIArtHandle connectorArt = selection[i];
		if (!connectorArt || !IsNetworkConnectorArt(connectorArt)) {
			continue;
		}
		++selectedConnectorCount;

		std::set<std::string> connectorSourceIds;
		CollectArtAssociatedSourceIds(connectorArt, connectorSourceIds);

		DuctworkPoint ignorePoint;
		bool matchedConnection = false;
		if (!ResolveNetworkConnectorIgnoreTarget(connectorArt,
			states,
			connections,
			connectorSourceIds,
			ignorePoint,
			matchedConnection) ||
			connectorSourceIds.empty()) {
			continue;
		}

		if (!matchedConnection) {
			++fallbackPointCount;
		}

		bool duplicateTarget = false;
		for (size_t targetIndex = 0; targetIndex < targets.size(); ++targetIndex) {
			if (DuctworkMath::Dist2(targets[targetIndex].point, ignorePoint) <= 16.0) {
				targets[targetIndex].sourceIds.insert(connectorSourceIds.begin(), connectorSourceIds.end());
				duplicateTarget = true;
				break;
			}
		}
		if (duplicateTarget) {
			affectedSourceIds.insert(connectorSourceIds.begin(), connectorSourceIds.end());
			continue;
		}

		SeparateTarget target;
		target.point = ignorePoint;
		target.sourceIds = connectorSourceIds;
		target.matchedConnection = matchedConnection;
		targets.push_back(target);
		affectedSourceIds.insert(connectorSourceIds.begin(), connectorSourceIds.end());
	}

	if (targets.empty() || affectedSourceIds.empty()) {
		if (selectedConnectorCount == 0) {
			return false;
		}
		outMessage = "Unable to resolve the selected Emory connector intersection.";
		return false;
	}

	std::vector<DuctworkPoint> existingIgnorePoints;
	CollectIgnoreAnchorPoints(existingIgnorePoints);

	size_t anchorsCreated = 0;
	size_t alreadyMarked = 0;
	size_t anchorsFailed = 0;
	for (size_t i = 0; i < targets.size(); ++i) {
		if (IsPointNearAny(targets[i].point, existingIgnorePoints, 4.0)) {
			++alreadyMarked;
			continue;
		}

		if (CreateIgnoreConnectionAnchor(targets[i].point)) {
			existingIgnorePoints.push_back(targets[i].point);
			++anchorsCreated;
		} else {
			++anchorsFailed;
		}
	}

	if (anchorsCreated == 0 && alreadyMarked == 0) {
		outMessage = "Unable to create the separation marker. Check that the Ignored layer is visible and unlocked.";
		return false;
	}

	std::vector<std::string> sourceIds;
	std::vector<DuctworkPath> regeneratePaths;
	for (size_t i = 0; i < states.size(); ++i) {
		if (states[i].sourceId.empty() || affectedSourceIds.find(states[i].sourceId) == affectedSourceIds.end()) {
			continue;
		}
		sourceIds.push_back(states[i].sourceId);
		regeneratePaths.push_back(states[i].path);
	}

	if (sourceIds.empty() || regeneratePaths.empty()) {
		outMessage = "Marked the separation point, but could not find the source runs to rebuild.";
		return false;
	}

	std::sort(sourceIds.begin(), sourceIds.end());
	sourceIds.erase(std::unique(sourceIds.begin(), sourceIds.end()), sourceIds.end());
	DeleteGeneratedEmoryBodies(sourceIds);
	EmoryBodyStats stats = GenerateEmoryBodies(regeneratePaths);

	{
		std::ostringstream logStream;
		logStream << "Emory separate-network-connectors selected=" << selectedConnectorCount
			<< " targets=" << targets.size()
			<< " anchorsCreated=" << anchorsCreated
			<< " alreadyMarked=" << alreadyMarked
			<< " anchorsFailed=" << anchorsFailed
			<< " fallbackPoints=" << fallbackPointCount
			<< " rebuiltSources=" << sourceIds.size();
		for (size_t i = 0; i < targets.size(); ++i) {
			logStream << " target" << i << "=" << SerializePointForLog(targets[i].point)
				<< " matched=" << (targets[i].matchedConnection ? 1 : 0);
		}
		DuctworkLog::Write(logStream.str());
	}

	std::ostringstream message;
	message << "Marked " << (anchorsCreated + alreadyMarked) << " connector intersection";
	if ((anchorsCreated + alreadyMarked) != 1) {
		message << "s";
	}
	message << " as separate runs. Rebuilt " << stats.segmentsCreated
		<< " segments and " << stats.connectorsCreated << " connectors.";
	if (anchorsFailed > 0) {
		message << " " << anchorsFailed << " marker";
		if (anchorsFailed != 1) {
			message << "s";
		}
		message << " could not be created.";
	}
	outMessage = message.str();
	return true;
}

bool ApplySelectedEmoryTerminalSegmentStyleInternal(const std::string& requestedStyle,
	bool useRequestedStyle,
	std::string& outMessage)
{
	outMessage = "Select one or more Emory visible final guides, terminal segments, or end connectors first.";
	if (!sAIArt) {
		outMessage = "Illustrator SDK is not available.";
		return false;
	}

	NormalizeDuplicateEmorySourceIds();
	std::vector<DuctworkPoint> registerAttachmentPoints;
	CollectRegisterAttachmentPoints(registerAttachmentPoints);

	std::vector<AIArtHandle> selection;
	DuctworkSelection::CollectSelectedPaths(selection);
	if (selection.empty()) {
		DuctworkLog::Write("Emory terminal-toggle selection-empty");
		return false;
	}

	std::vector<EmorySourceState> states;
	std::map<std::string, int> stateIndexBySourceId;
	CollectEmorySourceStates(states, stateIndexBySourceId);
	std::vector<DuctworkConnection> terminalConnections;
	CollectEmoryNetworkConnections(states, terminalConnections);

	struct TerminalToggleTarget
	{
		std::string sourceId;
		AIArtHandle sourceArt = nullptr;
		DuctworkPath sourcePath;
		std::vector<DuctworkPoint> sourcePoints;
		AIArtHandle canonicalArt = nullptr;
		std::vector<DuctworkPoint> canonicalPoints;
		int sourceTerminalSegmentIndex = -1;
		bool atStart = false;
		bool omittedTerminal = false;
		bool resolvedFromBackupOnly = false;
	};

	struct GeneratedGuideCandidate
	{
		AIArtHandle art = nullptr;
		std::string sourceId;
		int segmentIndex = -1;
		std::vector<DuctworkPoint> points;
	};

	std::vector<TerminalToggleTarget> targets;
	std::set<std::string> targetKeys;

	for (size_t i = 0; i < selection.size(); ++i) {
		AIArtHandle art = selection[i];
		if (!art) {
			continue;
		}

		std::string role;
		if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) ||
			(role != kEmoryRoleSegment &&
			 role != kEmoryRoleGuide &&
			 role != kEmoryRoleConnector)) {
			continue;
		}

		std::string sourceId;
		if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
			sourceId = ReadEmorySourceIdFromNote(art);
		}
		if (sourceId.empty()) {
			continue;
		}

		int selectedSegmentIndex = -1;
		int selectedJointIndex = -1;
		if (role == kEmoryRoleConnector) {
			if (!ReadGeneratedJointIndex(art, selectedJointIndex)) {
				std::ostringstream logStream;
				logStream << "Emory terminal-toggle connector-missing-joint sourceId=" << sourceId;
				DuctworkLog::Write(logStream.str());
				continue;
			}
		} else {
			if (!ReadGeneratedSegmentIndex(art, selectedSegmentIndex)) {
				std::ostringstream logStream;
				logStream << "Emory terminal-toggle missing-segment-index role=" << role
					<< " sourceId=" << sourceId;
				DuctworkLog::Write(logStream.str());
				continue;
			}
		}

		AIArtHandle sourceArt = nullptr;
		DuctworkPath sourcePath;
		AIArtHandle backupArt = nullptr;
		DuctworkPath backupPath;
		const bool hasBackupCenterline = GetPrimaryBackupCenterlineForSourceId(sourceId, backupArt, backupPath);
		bool resolvedFromBackupOnly = false;
		const std::string scoringRole = (role == kEmoryRoleGuide) ? kEmoryRoleSegment : role;
		int selectedStateIndex = -1;
		if (!states.empty() &&
			(FindBestStateIndexForGeneratedArt(art, scoringRole, sourceId, states, selectedStateIndex) ||
			 FindBestStateIndexForGeneratedArtLoose(art, scoringRole, states, selectedStateIndex)) &&
			selectedStateIndex >= 0 &&
			selectedStateIndex < static_cast<int>(states.size())) {
			sourceArt = states[selectedStateIndex].art;
			sourcePath = states[selectedStateIndex].path;
		} else if (!FindSourceArtForSourceId(sourceId, sourceArt, sourcePath)) {
			if (hasBackupCenterline && backupArt && backupPath.points.size() >= 2) {
				sourceArt = backupArt;
				sourcePath = backupPath;
				resolvedFromBackupOnly = true;
				std::ostringstream logStream;
				logStream << "Emory terminal-toggle backup-source-fallback sourceId=" << sourceId
					<< " role=" << role;
				DuctworkLog::Write(logStream.str());
			} else {
				std::ostringstream logStream;
				logStream << "Emory terminal-toggle missing-source sourceId=" << sourceId
					<< " role=" << role;
				DuctworkLog::Write(logStream.str());
				continue;
			}
		}

		std::vector<DuctworkPoint> points;
		SanitizePolyline(sourcePath.points, points);
		AIArtHandle canonicalArt = sourceArt;
		std::vector<DuctworkPoint> canonicalPoints = points;
		int canonicalSelectedSegmentIndex = selectedSegmentIndex;
		int canonicalSelectedJointIndex = selectedJointIndex;

		if (hasBackupCenterline) {
			canonicalArt = backupArt;
			canonicalPoints = backupPath.points;
			if (!resolvedFromBackupOnly && role != kEmoryRoleConnector) {
				std::vector<int> backupIndices;
				if (MapFragmentSegmentsToBackupIndices(points, backupPath.points, backupIndices) &&
					selectedSegmentIndex >= 0 &&
					selectedSegmentIndex < static_cast<int>(backupIndices.size()) &&
					backupIndices[selectedSegmentIndex] >= 0) {
					canonicalSelectedSegmentIndex = backupIndices[selectedSegmentIndex];
				}
			}
		}

		if (resolvedFromBackupOnly &&
			role != kEmoryRoleConnector &&
			canonicalPoints.size() >= 2) {
			DuctworkPath selectedArtPath;
			if (BuildProcessPathForArt(art, selectedArtPath) && !selectedArtPath.closed) {
				std::vector<DuctworkPoint> selectedArtPoints;
				SanitizePolyline(selectedArtPath.points, selectedArtPoints);
				const int canonicalEndSegmentIndex = static_cast<int>(canonicalPoints.size()) - 2;
				if (selectedArtPoints.size() >= 2) {
					if (GuidePointsMatchTerminalSegment(selectedArtPoints, canonicalPoints, 0, 4.0)) {
						canonicalSelectedSegmentIndex = 0;
					} else if (canonicalEndSegmentIndex >= 0 &&
						GuidePointsMatchTerminalSegment(selectedArtPoints,
							canonicalPoints,
							canonicalEndSegmentIndex,
							4.0)) {
						canonicalSelectedSegmentIndex = canonicalEndSegmentIndex;
					}
				}
			}
		}

		const size_t canonicalSegmentCount = canonicalPoints.size() > 1 ? (canonicalPoints.size() - 1) : 0;
		if (canonicalSegmentCount == 0) {
			std::ostringstream logStream;
			logStream << "Emory terminal-toggle canonical-empty sourceId=" << sourceId;
			DuctworkLog::Write(logStream.str());
			continue;
		}

		std::set<size_t> omittedSegments;
		CollectTerminalOmittedSegments(canonicalArt, canonicalSegmentCount, omittedSegments);
		int sourceStateIndex = -1;
		std::map<std::string, int>::const_iterator sourceStateIt = stateIndexBySourceId.find(sourceId);
		if (sourceStateIt != stateIndexBySourceId.end()) {
			sourceStateIndex = sourceStateIt->second;
		}
		const bool distributionTrunk =
			sourceStateIndex >= 0 &&
			CountEndpointToSegmentBranchChildren(sourceStateIndex, terminalConnections) >= 2;
		const bool omittedStart = omittedSegments.find(0) != omittedSegments.end();
		const bool omittedEnd = omittedSegments.find(canonicalSegmentCount - 1) != omittedSegments.end();
		const bool registerAtStart = !canonicalPoints.empty() &&
			IsPointNearAny(canonicalPoints.front(), registerAttachmentPoints, 10.0);
		const bool registerAtEnd = !canonicalPoints.empty() &&
			IsPointNearAny(canonicalPoints.back(), registerAttachmentPoints, 10.0);
		const bool allowStartTarget = omittedStart;
		const bool allowEndTarget = omittedEnd;
		bool addStart = false;
		bool addEnd = false;
		if (role == kEmoryRoleConnector) {
			ResolveTerminalStyleTargetFromJoint(canonicalSelectedJointIndex,
				canonicalSegmentCount,
				allowStartTarget,
				allowEndTarget,
				addStart,
				addEnd);
		} else {
			ResolveTerminalStyleTargetFromSelection(canonicalSelectedSegmentIndex,
				canonicalSegmentCount,
				omittedSegments,
				allowStartTarget,
				allowEndTarget,
				addStart,
				addEnd);
		}

		{
			std::ostringstream logStream;
			logStream << "Emory terminal-toggle candidate role=" << role
				<< " sourceId=" << sourceId
				<< " localSegmentIndex=" << selectedSegmentIndex
				<< " localJointIndex=" << selectedJointIndex
				<< " canonicalSegmentIndex=" << canonicalSelectedSegmentIndex
				<< " canonicalJointIndex=" << canonicalSelectedJointIndex
				<< " registerAtStart=" << (registerAtStart ? 1 : 0)
				<< " registerAtEnd=" << (registerAtEnd ? 1 : 0)
				<< " distributionTrunk=" << (distributionTrunk ? 1 : 0)
				<< " addStart=" << (addStart ? 1 : 0)
				<< " addEnd=" << (addEnd ? 1 : 0)
				<< " omittedSegments=[" << SerializeSegmentIndexSet(omittedSegments) << "]";
			DuctworkLog::Write(logStream.str());
		}

		if (addStart) {
			const std::string key = sourceId + "#start";
			if (targetKeys.insert(key).second) {
				TerminalToggleTarget target;
				target.sourceId = sourceId;
				target.sourceArt = sourceArt;
				target.sourcePath = sourcePath;
				target.sourcePoints = points;
				target.canonicalArt = canonicalArt;
				target.canonicalPoints = canonicalPoints;
				target.sourceTerminalSegmentIndex = points.size() > 1 ? 0 : -1;
				target.atStart = true;
				target.omittedTerminal = omittedStart;
				target.resolvedFromBackupOnly = resolvedFromBackupOnly;
				targets.push_back(target);
			}
		}
		if (addEnd) {
			const std::string key = sourceId + "#end";
			if (targetKeys.insert(key).second) {
				TerminalToggleTarget target;
				target.sourceId = sourceId;
				target.sourceArt = sourceArt;
				target.sourcePath = sourcePath;
				target.sourcePoints = points;
				target.canonicalArt = canonicalArt;
				target.canonicalPoints = canonicalPoints;
				target.sourceTerminalSegmentIndex = points.size() > 1 ? static_cast<int>(points.size() - 2) : -1;
				target.atStart = false;
				target.omittedTerminal = omittedEnd;
				target.resolvedFromBackupOnly = resolvedFromBackupOnly;
				targets.push_back(target);
			}
		}
	}

	if (targets.empty()) {
		DuctworkLog::Write("Emory terminal-toggle no-valid-selection");
		return false;
	}

	bool allCurved = true;
	for (size_t i = 0; i < targets.size(); ++i) {
		if (ReadTerminalSegmentStyle(targets[i].canonicalArt, targets[i].atStart) != "curved") {
			allCurved = false;
		}
	}
	std::string nextStyle = allCurved ? "straight" : "curved";
	if (useRequestedStyle) {
		if (requestedStyle != "curved" && requestedStyle != "straight") {
			outMessage = "Invalid final segment style target.";
			return false;
		}
		nextStyle = requestedStyle;
	}
	std::set<std::string> affectedSourceIds;
	std::vector<GeneratedGuideCandidate> guideCandidates;
	{
		std::vector<AIArtHandle> allPaths;
		CollectAllDocumentPaths(allPaths);
		guideCandidates.reserve(allPaths.size());
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || !IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			std::string role;
			if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) ||
				role != kEmoryRoleGuide) {
				continue;
			}

			std::string sourceId;
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
				sourceId = ReadEmorySourceIdFromNote(art);
			}
			if (sourceId.empty()) {
				continue;
			}

			int segmentIndex = -1;
			if (!ReadGeneratedSegmentIndex(art, segmentIndex)) {
				continue;
			}

			if (role == kEmoryRoleGuide) {
				DuctworkPath guidePath;
				if (!BuildProcessPathForArt(art, guidePath)) {
					continue;
				}

				GeneratedGuideCandidate candidate;
				candidate.art = art;
				candidate.sourceId = sourceId;
				candidate.segmentIndex = segmentIndex;
				SanitizePolyline(guidePath.points, candidate.points);
				guideCandidates.push_back(candidate);
			}
		}
	}
	size_t sourcePathUpdates = 0;
	size_t visibleGuideUpdates = 0;
	size_t createdVisibleGuides = 0;
	std::set<AIArtHandle> updatedGuideArts;

	for (size_t i = 0; i < targets.size(); ++i) {
		TerminalToggleTarget& target = targets[i];
		const size_t canonicalSegmentCount = target.canonicalPoints.size() > 1 ? (target.canonicalPoints.size() - 1) : 0;
		if (canonicalSegmentCount == 0) {
			continue;
		}
		const int terminalCanonicalIndex = target.atStart ? 0 : static_cast<int>(canonicalSegmentCount - 1);
		affectedSourceIds.insert(target.sourceId);
		const EmorySourceState* displayState = nullptr;
		std::map<std::string, int>::const_iterator displayStateIt = stateIndexBySourceId.find(target.sourceId);
		if (displayStateIt != stateIndexBySourceId.end() &&
			displayStateIt->second >= 0 &&
			displayStateIt->second < static_cast<int>(states.size())) {
			displayState = &states[displayStateIt->second];
		}
		std::vector<AIArtHandle> sourceArts;
		CollectCenterlineAndBackupArtsForSourceId(target.sourceId, sourceArts);
		if (sourceArts.empty()) {
			if (target.canonicalArt) {
				sourceArts.push_back(target.canonicalArt);
			}
			if (target.sourceArt && target.sourceArt != target.canonicalArt) {
				sourceArts.push_back(target.sourceArt);
			}
		}
		std::set<AIArtHandle> updatedArts;
		for (size_t artIndex = 0; artIndex < sourceArts.size(); ++artIndex) {
			AIArtHandle sourceTargetArt = sourceArts[artIndex];
			if (!sourceTargetArt || !updatedArts.insert(sourceTargetArt).second) {
				continue;
			}
			WriteTerminalSegmentStyle(sourceTargetArt, target.atStart, nextStyle);

			int targetSegmentIndexForArt = -1;
			if (sourceTargetArt == target.canonicalArt) {
				targetSegmentIndexForArt = terminalCanonicalIndex;
			} else {
				DuctworkPath artPath;
				if (BuildProcessPathForArt(sourceTargetArt, artPath)) {
					std::vector<DuctworkPoint> artPoints;
					SanitizePolyline(artPath.points, artPoints);
					const int artSegmentCount = static_cast<int>(artPoints.size()) - 1;
					if (artSegmentCount > 0) {
						if (PathSegmentMatchesReference(artPoints,
								0,
								target.canonicalPoints,
								terminalCanonicalIndex,
								4.0)) {
							targetSegmentIndexForArt = 0;
						} else if (PathSegmentMatchesReference(artPoints,
								artSegmentCount - 1,
								target.canonicalPoints,
								terminalCanonicalIndex,
								4.0)) {
							targetSegmentIndexForArt = artSegmentCount - 1;
						}
					}
				}
			}

			if (targetSegmentIndexForArt >= 0 &&
				ApplyTerminalSegmentStyleToPathArt(sourceTargetArt, targetSegmentIndexForArt, nextStyle)) {
				++sourcePathUpdates;
			}
		}

		bool matchedVisibleGuideForTarget = false;
		for (size_t guideIndex = 0; guideIndex < guideCandidates.size(); ++guideIndex) {
			GeneratedGuideCandidate& candidate = guideCandidates[guideIndex];
			if (candidate.sourceId != target.sourceId ||
				!candidate.art ||
				!updatedGuideArts.insert(candidate.art).second) {
				continue;
			}

			const bool segmentMatches = candidate.segmentIndex == terminalCanonicalIndex;
			const bool canonicalGeometryMatches = GuidePointsMatchTerminalSegment(candidate.points,
				target.canonicalPoints,
				terminalCanonicalIndex,
				4.0);
			const bool localSegmentMatches =
				target.sourceTerminalSegmentIndex >= 0 &&
				candidate.segmentIndex == target.sourceTerminalSegmentIndex;
			const bool localGeometryMatches =
				target.sourceTerminalSegmentIndex >= 0 &&
				GuidePointsMatchTerminalSegment(candidate.points,
					target.sourcePoints,
					target.sourceTerminalSegmentIndex,
					4.0);
			if (!segmentMatches && !canonicalGeometryMatches && !localSegmentMatches && !localGeometryMatches) {
				updatedGuideArts.erase(candidate.art);
				continue;
			}
			matchedVisibleGuideForTarget = true;

			if (ApplyTerminalGuideStyleToGuideArt(candidate.art,
				(target.sourceTerminalSegmentIndex >= 0 ? target.sourcePoints : target.canonicalPoints),
				static_cast<size_t>(target.sourceTerminalSegmentIndex >= 0 ? target.sourceTerminalSegmentIndex : terminalCanonicalIndex),
				nextStyle)) {
				++visibleGuideUpdates;
			}
		}

		if (!matchedVisibleGuideForTarget &&
			target.omittedTerminal &&
			target.sourceArt &&
			target.sourceTerminalSegmentIndex >= 0 &&
			target.sourceTerminalSegmentIndex + 1 < static_cast<int>(target.sourcePoints.size())) {
			AIArtHandle referenceArt = target.sourceArt;
			if (displayState) {
				ResolveReferenceArtForSourceId(*displayState, referenceArt);
			}

			std::vector<DuctworkPoint> guidePoints;
			guidePoints.push_back(target.sourcePoints[static_cast<size_t>(target.sourceTerminalSegmentIndex)]);
			guidePoints.push_back(target.sourcePoints[static_cast<size_t>(target.sourceTerminalSegmentIndex + 1)]);

			AIArtHandle guideArt = nullptr;
			if (CreateOpenPath(referenceArt, guidePoints, guideArt) &&
				guideArt &&
				ApplyGuideStyleInternal(guideArt, GetEmoryColorSpec(target.sourcePath.layerName)) &&
				ApplyTerminalGuideStyleToGuideArt(guideArt,
					target.sourcePoints,
					static_cast<size_t>(target.sourceTerminalSegmentIndex),
					nextStyle)) {
				TagGeneratedArt(guideArt, kEmoryRoleGuide, target.sourceId, 0.0,
					target.sourceTerminalSegmentIndex, -1, std::string());
				++visibleGuideUpdates;
				++createdVisibleGuides;
			} else if (guideArt) {
				sAIArt->DisposeArt(guideArt);
			}
		}

		{
			std::ostringstream logStream;
			logStream << "Emory terminal-toggle apply sourceId=" << target.sourceId
				<< " targetCanonicalIndex=" << terminalCanonicalIndex
				<< " terminalAtStart=" << (target.atStart ? 1 : 0)
				<< " backupOnly=" << (target.resolvedFromBackupOnly ? 1 : 0)
				<< " nextStyle=" << nextStyle;
			DuctworkLog::Write(logStream.str());
		}
	}

	if (createdVisibleGuides > 0) {
		std::vector<std::string> orderingSourceIds(affectedSourceIds.begin(), affectedSourceIds.end());
		ApplyFinalEmoryOrdering(orderingSourceIds);
	}

	{
		std::ostringstream logStream;
		logStream << "Emory terminal-toggle complete sources=" << affectedSourceIds.size()
			<< " targets=" << targets.size()
			<< " nextStyle=" << nextStyle
			<< " sourcePathUpdates=" << sourcePathUpdates
			<< " guideCandidates=" << guideCandidates.size()
			<< " visibleGuideUpdates=" << visibleGuideUpdates
			<< " createdVisibleGuides=" << createdVisibleGuides
			<< " regenerated=0";
		DuctworkLog::Write(logStream.str());
	}

	std::ostringstream message;
	message << "Updated " << targets.size() << " selected final ";
	if (targets.size() == 1) {
		message << "segment";
	} else {
		message << "segments";
	}
	message << " to " << nextStyle << ".";
	if (sourcePathUpdates > 0) {
		message << " Updated " << sourcePathUpdates << " source path terminal";
		if (sourcePathUpdates != 1) {
			message << "s";
		}
		message << ".";
	}
	if (visibleGuideUpdates > 0) {
		message << " Restyled " << visibleGuideUpdates << " visible final guide";
		if (visibleGuideUpdates != 1) {
			message << "s";
		}
		message << ".";
	}
	outMessage = message.str();
	return true;
}

bool DuctworkGeometry::ToggleSelectedEmoryTerminalSegmentStyle(std::string& outMessage)
{
	return ApplySelectedEmoryTerminalSegmentStyleInternal(std::string(), false, outMessage);
}

bool DuctworkGeometry::SetSelectedEmoryTerminalSegmentStyle(const std::string& targetStyle, std::string& outMessage)
{
	return ApplySelectedEmoryTerminalSegmentStyleInternal(targetStyle == "curved" ? "curved" : "straight", true, outMessage);
}

bool DuctworkGeometry::GetSelectedEmorySegmentState(std::string& outJson)
{
	outJson = "{\"ok\":true,\"available\":false,\"reason\":\"no-selection\"}";
	if (!sAIArt) {
		outJson = "{\"ok\":false,\"available\":false,\"reason\":\"sdk-unavailable\"}";
		return false;
	}

	std::vector<AIArtHandle> selection;
	DuctworkSelection::CollectSelectedPaths(selection);
	if (selection.empty()) {
		return true;
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

	std::map<std::string, std::vector<int> > selectedBySource;
	for (size_t i = 0; i < selection.size(); ++i) {
		AIArtHandle art = selection[i];
		if (!art) {
			continue;
		}

		std::string role;
		if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) ||
			role != kEmoryRoleSegment) {
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
				<< ",\"canToggleTerminalStyle\":false"
				<< ",\"terminalStyle\":\"straight\""
				<< "}";
			outJson = out.str();
			return true;
		}
		outJson = "{\"ok\":true,\"available\":false,\"reason\":\"no-segment-selection\",\"canApplyWidth\":false,\"canApplyStroke\":false}";
		return true;
	}

	struct SelectedSourceInfo
	{
		AIArtHandle art = nullptr;
		DuctworkPath path;
	};

	std::set<std::string> requestedSourceIds;
	for (std::map<std::string, std::vector<int> >::const_iterator it = selectedBySource.begin();
		it != selectedBySource.end();
		++it) {
		if (!it->first.empty()) {
			requestedSourceIds.insert(it->first);
		}
	}

	std::map<std::string, SelectedSourceInfo> sourceInfoById;
	std::vector<AIArtHandle> allPaths;
	CollectAllLineLayerPaths(allPaths);
	for (size_t i = 0; i < allPaths.size(); ++i) {
		AIArtHandle art = allPaths[i];
		if (!art || IsGeneratedEmoryArtInternal(art) || IsBackupCenterlineArt(art)) {
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

		if (requestedSourceIds.find(sourceId) == requestedSourceIds.end() ||
			sourceInfoById.find(sourceId) != sourceInfoById.end()) {
			continue;
		}

		SelectedSourceInfo info;
		info.art = art;
		info.path = path;
		sourceInfoById[sourceId] = info;

		if (sourceInfoById.size() == requestedSourceIds.size()) {
			break;
		}
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

			std::map<std::string, SelectedSourceInfo>::const_iterator sourceInfoIt = sourceInfoById.find(it->first);
			if (sourceInfoIt == sourceInfoById.end() || !sourceInfoIt->second.art) {
				continue;
			}
			AIArtHandle sourceArt = sourceInfoIt->second.art;
			const DuctworkPath& sourcePath = sourceInfoIt->second.path;

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
			if (!HasStoredSegmentWidths(sourceArt, segmentCount)) {
				ApplyDefaultStraightChainTapers(sourceArt,
					points,
					ReadStartSegmentIndex(sourceArt, segmentCount),
					widths);
			}
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
			<< ",\"canToggleTerminalStyle\":false"
			<< ",\"terminalStyle\":\"straight\""
			<< ",\"taperAlignmentAvailable\":false"
			<< ",\"cascadeDirection\":\"multiple-runs\""
			<< "}";
		outJson = out.str();
		return true;
	}

	const std::string sourceId = selectedBySource.begin()->first;
	std::vector<int>& selectedSegmentIndices = selectedBySource.begin()->second;
	std::sort(selectedSegmentIndices.begin(), selectedSegmentIndices.end());
	selectedSegmentIndices.erase(std::unique(selectedSegmentIndices.begin(), selectedSegmentIndices.end()), selectedSegmentIndices.end());

	std::map<std::string, SelectedSourceInfo>::const_iterator sourceInfoIt = sourceInfoById.find(sourceId);
	if (sourceInfoIt == sourceInfoById.end() || !sourceInfoIt->second.art) {
		outJson = "{\"ok\":true,\"available\":false,\"reason\":\"source-not-found\"}";
		return true;
	}
	AIArtHandle sourceArt = sourceInfoIt->second.art;
	DuctworkPath sourcePath = sourceInfoIt->second.path;

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
	if (!HasStoredSegmentWidths(sourceArt, segmentCount)) {
		ApplyDefaultStraightChainTapers(sourceArt, points, startSegmentIndex, segmentWidths);
	}

	std::vector<int> validSelectedSegmentIndices;
	validSelectedSegmentIndices.reserve(selectedSegmentIndices.size());
	for (size_t i = 0; i < selectedSegmentIndices.size(); ++i) {
		const int segmentIndex = selectedSegmentIndices[i];
		if (segmentIndex >= 0 && segmentIndex < static_cast<int>(segmentWidths.size())) {
			validSelectedSegmentIndices.push_back(segmentIndex);
		}
	}
	if (validSelectedSegmentIndices.empty()) {
		outJson = "{\"ok\":true,\"available\":false,\"reason\":\"segment-not-found\"}";
		return true;
	}
	selectedSegmentIndices.swap(validSelectedSegmentIndices);

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
	out << "]"
		<< ",\"taperAlignmentAvailable\":false"
		<< ",\"canToggleTerminalStyle\":false"
		<< ",\"terminalStyle\":\"straight\"";

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

bool DuctworkGeometry::SelectSelectedEmoryFinalSegments(std::string& outMessage)
{
	outMessage = "Select generated Emory ductwork first.";
	if (!sAIArt) {
		outMessage = "Illustrator SDK is not available.";
		return false;
	}

	RepairVisibleFragmentSourceIdsFromBackups();
	NormalizeDuplicateEmorySourceIds();

	std::vector<AIArtHandle> selection;
	DuctworkSelection::CollectSelectedPaths(selection);
	if (selection.empty()) {
		return false;
	}

	std::vector<EmorySourceState> states;
	std::map<std::string, int> stateIndexBySourceId;
	if (!CollectEmorySourceStates(states, stateIndexBySourceId)) {
		outMessage = "Unable to read Emory source centerlines.";
		return false;
	}

	std::map<int, std::vector<int> > selectedFinalIndicesByState;
	size_t consideredGeneratedPieces = 0;

	for (size_t i = 0; i < selection.size(); ++i) {
		AIArtHandle art = selection[i];
		if (!art || !IsGeneratedEmoryArtInternal(art)) {
			continue;
		}

		std::string role;
		if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) ||
			(role != kEmoryRoleSegment && role != kEmoryRoleGuide)) {
			continue;
		}

		std::string sourceId;
		if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
			sourceId = ReadEmorySourceIdFromNote(art);
		}
		if (sourceId.empty()) {
			continue;
		}

		int segmentIndex = -1;
		if (!ReadGeneratedSegmentIndex(art, segmentIndex)) {
			continue;
		}

		const std::string scoringRole = (role == kEmoryRoleGuide) ? kEmoryRoleSegment : role;
		int stateIndex = -1;
		if (!FindBestStateIndexForGeneratedArt(art, scoringRole, sourceId, states, stateIndex) &&
			!FindBestStateIndexForGeneratedArtLoose(art, scoringRole, states, stateIndex)) {
			continue;
		}
		if (stateIndex < 0 || stateIndex >= static_cast<int>(states.size())) {
			continue;
		}

		int controlSegmentIndex = -1;
		if (!ResolveTerminalWidthControlSegmentIndex(states[stateIndex], segmentIndex, controlSegmentIndex)) {
			continue;
		}
		if (controlSegmentIndex < 0 || controlSegmentIndex >= states[stateIndex].segmentCount) {
			continue;
		}

		selectedFinalIndicesByState[stateIndex].push_back(controlSegmentIndex);
		++consideredGeneratedPieces;
	}

	size_t finalSegmentCount = 0;
	for (std::map<int, std::vector<int> >::iterator it = selectedFinalIndicesByState.begin();
		it != selectedFinalIndicesByState.end();
		++it) {
		std::vector<int>& indices = it->second;
		std::sort(indices.begin(), indices.end());
		indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
		finalSegmentCount += indices.size();
	}

	if (finalSegmentCount == 0) {
		outMessage = "No final Emory segments were found in the current selection.";
		return false;
	}

	if (!SelectGeneratedSegmentsByStateMap(states, selectedFinalIndicesByState)) {
		outMessage = "Found final Emory segments, but couldn't select the generated pieces.";
		return false;
	}

	std::ostringstream message;
	message << "Isolated " << finalSegmentCount << " final segment";
	if (finalSegmentCount != 1) {
		message << "s";
	}
	if (consideredGeneratedPieces > finalSegmentCount) {
		message << " from the mixed selection";
	}
	message << ".";
	outMessage = message.str();
	return true;
}

namespace
{
	bool ResetOpenPathHandlesToCorners(AIArtHandle art)
	{
		if (!art || !sAIPath) {
			return false;
		}

		ai::int16 segmentCount = 0;
		if (sAIPath->GetPathSegmentCount(art, &segmentCount) != kNoErr || segmentCount < 1) {
			return false;
		}

		std::vector<AIPathSegment> segments(static_cast<size_t>(segmentCount));
		if (sAIPath->GetPathSegments(art, 0, segmentCount, &segments[0]) != kNoErr) {
			return false;
		}

		for (ai::int16 i = 0; i < segmentCount; ++i) {
			segments[i].in = segments[i].p;
			segments[i].out = segments[i].p;
			segments[i].corner = true;
		}
		return sAIPath->SetPathSegments(art, 0, segmentCount, &segments[0]) == kNoErr;
	}

	size_t TurnFinalCurvesOffForSourceIds(const std::vector<std::string>& sourceIds)
	{
		size_t updatedCount = 0;
		std::set<AIArtHandle> updatedArts;
		for (size_t i = 0; i < sourceIds.size(); ++i) {
			if (sourceIds[i].empty()) {
				continue;
			}

			std::vector<AIArtHandle> arts;
			CollectCenterlineAndBackupArtsForSourceId(sourceIds[i], arts);
			for (size_t artIndex = 0; artIndex < arts.size(); ++artIndex) {
				AIArtHandle art = arts[artIndex];
				if (!art || !updatedArts.insert(art).second) {
					continue;
				}
				WriteTerminalSegmentStyle(art, true, "straight");
				WriteTerminalSegmentStyle(art, false, "straight");
				if (ResetOpenPathHandlesToCorners(art)) {
					++updatedCount;
				}
			}
		}
		return updatedCount;
	}
}

bool DuctworkGeometry::RevertSelectedEmoryToCenterlines(std::string& outMessage)
{
	outMessage = "Select generated Emory ductwork or its source centerline first.";
	if (!sAIArt) {
		outMessage = "Illustrator SDK is not available.";
		return false;
	}

	RepairVisibleFragmentSourceIdsFromBackups();

	std::map<std::string, std::vector<AIArtHandle> > backupsBySourceId;
	std::map<std::string, std::vector<EmorySourceIdCandidate> > fragmentsBySourceId;
	BuildRevertCenterlineIndex(backupsBySourceId, fragmentsBySourceId);

	std::vector<AIArtHandle> selection;
	DuctworkSelection::CollectSelectedPaths(selection);
	if (selection.empty()) {
		return false;
	}

	std::set<std::string> sourceIdSet;
	std::set<std::string> deleteSourceIdSet;
	size_t generatedSelectionCount = 0;

	for (size_t i = 0; i < selection.size(); ++i) {
		AIArtHandle art = selection[i];
		if (!art) {
			continue;
		}

		std::string sourceId;
		bool matched = false;

		if (IsGeneratedEmoryArtInternal(art)) {
			std::set<std::string> associatedSourceIds;
			CollectArtAssociatedSourceIds(art, associatedSourceIds);
			bool resolvedRestoreSource = false;
			for (std::set<std::string>::const_iterator sourceIt = associatedSourceIds.begin();
				sourceIt != associatedSourceIds.end();
				++sourceIt) {
				if (!sourceIt->empty()) {
					deleteSourceIdSet.insert(*sourceIt);
				}
			}
			for (std::set<std::string>::const_iterator sourceIt = associatedSourceIds.begin();
				sourceIt != associatedSourceIds.end();
				++sourceIt) {
				if (sourceIt->empty()) {
					continue;
				}
				if (backupsBySourceId.find(*sourceIt) != backupsBySourceId.end() ||
					fragmentsBySourceId.find(*sourceIt) != fragmentsBySourceId.end()) {
					sourceIdSet.insert(*sourceIt);
					resolvedRestoreSource = true;
				}
			}
			if (!resolvedRestoreSource) {
				std::string role;
				if (DuctworkMetadata::GetString(art, kEmoryRoleKey, role) && IsGeneratedRole(role)) {
					std::vector<EmorySourceState> states;
					std::map<std::string, int> stateIndexBySourceId;
					if (CollectEmorySourceStates(states, stateIndexBySourceId)) {
						int bestStateIndex = -1;
						if (FindBestStateIndexForGeneratedArtLoose(art, role, states, bestStateIndex) &&
							bestStateIndex >= 0 &&
							bestStateIndex < static_cast<int>(states.size()) &&
							!states[bestStateIndex].sourceId.empty()) {
							sourceIdSet.insert(states[bestStateIndex].sourceId);
							resolvedRestoreSource = true;
						}
					}
				}
			}
			matched = !associatedSourceIds.empty() || resolvedRestoreSource;
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

		if (!matched) {
			continue;
		}
		if (IsGeneratedEmoryArtInternal(art)) {
			continue;
		}
		if (sourceId.empty() || !sourceIdSet.insert(sourceId).second) {
			continue;
		}
		deleteSourceIdSet.insert(sourceId);

	}

	if (sourceIdSet.empty()) {
		return false;
	}

	std::vector<std::string> sourceIds(sourceIdSet.begin(), sourceIdSet.end());
	deleteSourceIdSet.insert(sourceIdSet.begin(), sourceIdSet.end());
	std::vector<std::string> deleteSourceIds(deleteSourceIdSet.begin(), deleteSourceIdSet.end());
	const size_t finalCurvesStraightened = TurnFinalCurvesOffForSourceIds(sourceIds);
	{
		std::ostringstream logStream;
		logStream << "Emory revert-start sourceIds=";
		for (size_t i = 0; i < sourceIds.size(); ++i) {
			if (i > 0) {
				logStream << ",";
			}
			logStream << sourceIds[i];
		}
		logStream << " deleteSourceIds=";
		for (size_t i = 0; i < deleteSourceIds.size(); ++i) {
			if (i > 0) {
				logStream << ",";
			}
			logStream << deleteSourceIds[i];
		}
		logStream << " generatedSelectionCount=" << generatedSelectionCount;
		logStream << " finalCurvesStraightened=" << finalCurvesStraightened;
		DuctworkLog::Write(logStream.str());
	}
	const size_t removed = DeleteGeneratedEmoryBodies(deleteSourceIds);

	std::vector<AIArtHandle> sourceSelection;
	for (size_t sourceIndex = 0; sourceIndex < sourceIds.size(); ++sourceIndex) {
		std::vector<AIArtHandle> healedSourceArts;

		std::map<std::string, std::vector<AIArtHandle> >::const_iterator backupIt =
			backupsBySourceId.find(sourceIds[sourceIndex]);
		if (backupIt != backupsBySourceId.end() &&
			RestoreBackupCenterlinesFromFragments(backupIt->second, fragmentsBySourceId[sourceIds[sourceIndex]], healedSourceArts)) {
			std::ostringstream logStream;
			logStream << "Emory revert-restore-backup sourceId=" << sourceIds[sourceIndex]
				<< " restoredArts=" << healedSourceArts.size();
			DuctworkLog::Write(logStream.str());
			for (size_t artIndex = 0; artIndex < healedSourceArts.size(); ++artIndex) {
				if (healedSourceArts[artIndex]) {
					sourceSelection.push_back(healedSourceArts[artIndex]);
				}
			}
			continue;
		}

		std::map<std::string, std::vector<EmorySourceIdCandidate> >::iterator fragmentIt =
			fragmentsBySourceId.find(sourceIds[sourceIndex]);
		if (fragmentIt == fragmentsBySourceId.end()) {
			DuctworkLog::Write("Emory revert-restore-failed sourceId=" + sourceIds[sourceIndex]);
			continue;
		}

		if (!RestoreStoredOriginalCenterlineFragments(fragmentIt->second, healedSourceArts)) {
			if (!HealSourceCenterlineFragments(fragmentIt->second, healedSourceArts)) {
				DuctworkLog::Write("Emory revert-restore-failed sourceId=" + sourceIds[sourceIndex]);
				continue;
			}
			std::ostringstream logStream;
			logStream << "Emory revert-heal-fallback sourceId=" << sourceIds[sourceIndex]
				<< " healedArts=" << healedSourceArts.size();
			DuctworkLog::Write(logStream.str());
		} else {
			std::ostringstream logStream;
			logStream << "Emory revert-restore-original sourceId=" << sourceIds[sourceIndex]
				<< " restoredArts=" << healedSourceArts.size();
			DuctworkLog::Write(logStream.str());
		}

		for (size_t artIndex = 0; artIndex < healedSourceArts.size(); ++artIndex) {
			if (healedSourceArts[artIndex]) {
				sourceSelection.push_back(healedSourceArts[artIndex]);
			}
		}
	}
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
	{
		std::ostringstream logStream;
		logStream << "Emory revert-complete sourceCount=" << sourceIds.size()
			<< " removedGenerated=" << removed
			<< " selectedCenterlines=" << sourceSelection.size();
		DuctworkLog::Write(logStream.str());
	}
	outMessage = message.str();
	return true;
}

bool DuctworkGeometry::SelectSelectedEmoryCenterlines(std::string& outMessage)
{
	outMessage = "Select Emory ductwork or its source centerline first.";
	if (!sAIArt) {
		outMessage = "Illustrator SDK is not available.";
		return false;
	}

	std::set<std::string> sourceIds;
	if (!CollectSelectedEmorySourceIds(sourceIds) || sourceIds.empty()) {
		return false;
	}

	std::vector<AIArtHandle> centerlineSelection;
	std::set<AIArtHandle> seenCenterlines;
	size_t shownCenterlines = 0;

	for (std::set<std::string>::const_iterator it = sourceIds.begin(); it != sourceIds.end(); ++it) {
		std::vector<EmorySourceIdCandidate> fragments;
		if (!CollectSourceCenterlineFragmentsForSourceId(*it, fragments) || fragments.empty()) {
			AIArtHandle sourceArt = nullptr;
			DuctworkPath sourcePath;
			if (!FindSourceArtForSourceId(*it, sourceArt, sourcePath) || !sourceArt) {
				continue;
			}
			if (IsCenterlineHidden(sourceArt)) {
				SetCenterlineHidden(sourceArt, false);
				++shownCenterlines;
			}
			if (seenCenterlines.insert(sourceArt).second) {
				centerlineSelection.push_back(sourceArt);
			}
			continue;
		}

		for (size_t fragmentIndex = 0; fragmentIndex < fragments.size(); ++fragmentIndex) {
			AIArtHandle sourceArt = fragments[fragmentIndex].art;
			if (!sourceArt) {
				continue;
			}
			if (IsCenterlineHidden(sourceArt)) {
				SetCenterlineHidden(sourceArt, false);
				++shownCenterlines;
			}
			if (seenCenterlines.insert(sourceArt).second) {
				centerlineSelection.push_back(sourceArt);
			}
		}
	}

	if (centerlineSelection.empty()) {
		return false;
	}

	ClearSelectionInternal();
	SelectArtListInternal(centerlineSelection);

	std::ostringstream message;
	message << "Selected " << centerlineSelection.size() << " centerline";
	if (centerlineSelection.size() != 1) {
		message << "s";
	}
	message << " from " << sourceIds.size() << " Emory run";
	if (sourceIds.size() != 1) {
		message << "s";
	}
	if (shownCenterlines > 0) {
		message << " and showed " << shownCenterlines << " hidden centerline";
		if (shownCenterlines != 1) {
			message << "s";
		}
	}
	message << ".";
	outMessage = message.str();
	return true;
}

bool DuctworkGeometry::PurgeSelectedEmoryState(std::string& outMessage)
{
	outMessage = "Select one or more Emory centerlines or generated Emory ductwork pieces first.";
	if (!sAIArt) {
		outMessage = "Illustrator SDK is not available.";
		return false;
	}

	std::vector<AIArtHandle> selection;
	DuctworkSelection::CollectSelectedPaths(selection);
	if (selection.empty()) {
		return false;
	}

	std::set<std::string> sourceIds;
	std::vector<AIArtHandle> directCenterlineSelection;
	std::set<AIArtHandle> seenCenterlines;

	for (size_t i = 0; i < selection.size(); ++i) {
		AIArtHandle art = selection[i];
		if (!art) {
			continue;
		}

		if (IsGeneratedEmoryArtInternal(art)) {
			std::set<std::string> associatedSourceIds;
			CollectArtAssociatedSourceIds(art, associatedSourceIds);
			sourceIds.insert(associatedSourceIds.begin(), associatedSourceIds.end());
			continue;
		}

		DuctworkPath path;
		if (!BuildProcessPathForArt(art, path) ||
			!DuctworkGeometry::IsCenterlineCandidate(path.art, path.points, path.closed, path.layerName)) {
			continue;
		}

		if (seenCenterlines.insert(art).second) {
			directCenterlineSelection.push_back(art);
		}

		std::string sourceId;
		if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
			sourceId = ReadEmorySourceIdFromNote(art);
		}
		if (!sourceId.empty()) {
			sourceIds.insert(sourceId);
		}
	}

	if (sourceIds.empty() && directCenterlineSelection.empty()) {
		return false;
	}

	std::map<std::string, std::vector<AIArtHandle> > backupsBySourceId;
	std::map<std::string, std::vector<EmorySourceIdCandidate> > fragmentsBySourceId;
	BuildRevertCenterlineIndex(backupsBySourceId, fragmentsBySourceId);

	std::vector<std::string> sourceIdList(sourceIds.begin(), sourceIds.end());
	const size_t removedGenerated = DeleteGeneratedEmoryBodies(sourceIdList);

	size_t removedBackups = 0;
	size_t clearedCenterlines = 0;
	std::vector<AIArtHandle> centerlineSelection;
	std::set<AIArtHandle> selectedOutput;

	for (std::set<std::string>::const_iterator sourceIt = sourceIds.begin();
		sourceIt != sourceIds.end();
		++sourceIt) {
		std::map<std::string, std::vector<AIArtHandle> >::iterator backupIt = backupsBySourceId.find(*sourceIt);
		if (backupIt != backupsBySourceId.end()) {
			for (size_t i = 0; i < backupIt->second.size(); ++i) {
				AIArtHandle backupArt = backupIt->second[i];
				if (!backupArt) {
					continue;
				}
				ClearAllEmoryMetadataInternal(backupArt);
				sAIArt->SetArtUserAttr(backupArt, kArtLocked | kArtHidden, 0);
				if (sAIArt->DisposeArt(backupArt) == kNoErr) {
					++removedBackups;
				}
			}
		}

		std::map<std::string, std::vector<EmorySourceIdCandidate> >::iterator fragmentIt =
			fragmentsBySourceId.find(*sourceIt);
		if (fragmentIt == fragmentsBySourceId.end()) {
			continue;
		}
		for (size_t i = 0; i < fragmentIt->second.size(); ++i) {
			AIArtHandle sourceArt = fragmentIt->second[i].art;
			if (!sourceArt) {
				continue;
			}
			ClearAllEmoryMetadataInternal(sourceArt);
			UngroupAncestorGroupsRecursive(sourceArt);
			if (selectedOutput.insert(sourceArt).second) {
				centerlineSelection.push_back(sourceArt);
			}
			++clearedCenterlines;
		}
	}

	for (size_t i = 0; i < directCenterlineSelection.size(); ++i) {
		AIArtHandle sourceArt = directCenterlineSelection[i];
		if (!sourceArt) {
			continue;
		}
		ClearAllEmoryMetadataInternal(sourceArt);
		UngroupAncestorGroupsRecursive(sourceArt);
		if (selectedOutput.insert(sourceArt).second) {
			centerlineSelection.push_back(sourceArt);
		}
		++clearedCenterlines;
	}

	if (!centerlineSelection.empty()) {
		ClearSelectionInternal();
		SelectArtListInternal(centerlineSelection);
	}

	std::ostringstream message;
	message << "Purged Emory state from " << clearedCenterlines << " centerline";
	if (clearedCenterlines != 1) {
		message << "s";
	}
	message << ", removed " << removedGenerated << " generated piece";
	if (removedGenerated != 1) {
		message << "s";
	}
	message << " and " << removedBackups << " hidden backup";
	if (removedBackups != 1) {
		message << "s";
	}
	message << ".";
	outMessage = message.str();
	return clearedCenterlines > 0 || removedGenerated > 0 || removedBackups > 0;
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
		if (!HasStoredSegmentWidths(sourceArt, points.size() - 1)) {
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

namespace
{
	struct CenterlineTailGuideCandidate
	{
		AIArtHandle art = nullptr;
		std::string sourceId;
		int segmentIndex = -1;
		bool includesStart = false;
		bool includesEnd = false;
		bool createdForHiddenCenterline = false;
		std::vector<DuctworkPoint> points;
	};

	double PointDistanceSquared(const DuctworkPoint& a, const DuctworkPoint& b)
	{
		const double dx = a.x - b.x;
		const double dy = a.y - b.y;
		return (dx * dx) + (dy * dy);
	}

	void ClearCenterlineTailGuideMetadata(AIArtHandle art)
	{
		if (!art) {
			return;
		}
		DuctworkMetadata::RemoveKey(art, kEmoryCenterlineTailGuideKey);
		DuctworkMetadata::RemoveKey(art, kEmoryCenterlineTailGuideStartKey);
		DuctworkMetadata::RemoveKey(art, kEmoryCenterlineTailGuideEndKey);
		DuctworkMetadata::RemoveKey(art, kEmoryCenterlineTailGuideCreatedKey);
	}

	void MarkCenterlineTailGuide(AIArtHandle art, bool includesStart, bool includesEnd, bool createdForHiddenCenterline)
	{
		if (!art) {
			return;
		}
		DuctworkMetadata::SetDouble(art, kEmoryCenterlineTailGuideKey, 1.0);
		DuctworkMetadata::SetDouble(art, kEmoryCenterlineTailGuideStartKey, includesStart ? 1.0 : 0.0);
		DuctworkMetadata::SetDouble(art, kEmoryCenterlineTailGuideEndKey, includesEnd ? 1.0 : 0.0);
		DuctworkMetadata::SetDouble(art, kEmoryCenterlineTailGuideCreatedKey, createdForHiddenCenterline ? 1.0 : 0.0);
	}

	bool ReadGeneratedArtSourceId(AIArtHandle art, std::string& outSourceId)
	{
		outSourceId.clear();
		if (!art) {
			return false;
		}
		if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, outSourceId) || outSourceId.empty()) {
			outSourceId = ReadEmorySourceIdFromNote(art);
		}
		return !outSourceId.empty();
	}

	void AppendUniqueArt(AIArtHandle art, std::vector<AIArtHandle>& outArts, std::set<AIArtHandle>& seenArts)
	{
		if (art && seenArts.insert(art).second) {
			outArts.push_back(art);
		}
	}

	void CollectSourceCenterlineArtsByMetadata(const std::set<std::string>& sourceIds,
		std::vector<AIArtHandle>& outArts,
		std::set<AIArtHandle>& seenArts)
	{
		if (sourceIds.empty()) {
			return;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllDocumentPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || IsGeneratedEmoryArtInternal(art) || IsBackupCenterlineArt(art)) {
				continue;
			}

			const std::string layerName = DuctworkGeometry::GetArtLayerName(art);
			if (!DuctworkLayers::IsColorLayerName(layerName)) {
				continue;
			}

			std::string sourceId;
			if (!DuctworkMetadata::GetString(art, kEmorySourceIdKey, sourceId) || sourceId.empty()) {
				sourceId = ReadEmorySourceIdFromNote(art);
			}
			if (sourceIds.find(sourceId) == sourceIds.end()) {
				continue;
			}

			AppendUniqueArt(art, outArts, seenArts);
		}
	}

	bool ReadCenterlineTailGuideCandidate(AIArtHandle art,
		const std::set<std::string>& sourceIds,
		CenterlineTailGuideCandidate& outCandidate)
	{
		outCandidate = CenterlineTailGuideCandidate();
		if (!art || !IsGeneratedEmoryArtInternal(art)) {
			return false;
		}

		double tailValue = 0.0;
		if (!DuctworkMetadata::GetDouble(art, kEmoryCenterlineTailGuideKey, tailValue) || tailValue <= 0.5) {
			return false;
		}

		std::string role;
		if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) || role != kEmoryRoleGuide) {
			return false;
		}

		std::string sourceId;
		if (!ReadGeneratedArtSourceId(art, sourceId)) {
			return false;
		}
		if (!sourceIds.empty() && sourceIds.find(sourceId) == sourceIds.end()) {
			return false;
		}

		int segmentIndex = -1;
		if (!ReadGeneratedSegmentIndex(art, segmentIndex)) {
			return false;
		}

		DuctworkPath guidePath;
		if (!BuildProcessPathForArt(art, guidePath)) {
			return false;
		}
		std::vector<DuctworkPoint> points;
		SanitizePolyline(guidePath.points, points);
		if (points.size() < 2) {
			return false;
		}

		double startValue = 0.0;
		double endValue = 0.0;
		double createdValue = 0.0;
		DuctworkMetadata::GetDouble(art, kEmoryCenterlineTailGuideStartKey, startValue);
		DuctworkMetadata::GetDouble(art, kEmoryCenterlineTailGuideEndKey, endValue);
		DuctworkMetadata::GetDouble(art, kEmoryCenterlineTailGuideCreatedKey, createdValue);

		outCandidate.art = art;
		outCandidate.sourceId = sourceId;
		outCandidate.segmentIndex = segmentIndex;
		outCandidate.includesStart = startValue > 0.5;
		outCandidate.includesEnd = endValue > 0.5;
		outCandidate.createdForHiddenCenterline = createdValue > 0.5;
		outCandidate.points = points;
		return outCandidate.includesStart || outCandidate.includesEnd;
	}

	void CollectCenterlineTailGuides(const std::set<std::string>& sourceIds,
		std::vector<CenterlineTailGuideCandidate>& outGuides)
	{
		outGuides.clear();
		std::vector<AIArtHandle> allPaths;
		CollectAllDocumentPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			CenterlineTailGuideCandidate candidate;
			if (ReadCenterlineTailGuideCandidate(allPaths[i], sourceIds, candidate)) {
				outGuides.push_back(candidate);
			}
		}
	}

	size_t RemoveCreatedCenterlineTailGuidesAndClearTags(const std::set<std::string>& sourceIds)
	{
		if (!sAIArt) {
			return 0;
		}

		std::vector<CenterlineTailGuideCandidate> guides;
		CollectCenterlineTailGuides(sourceIds, guides);
		size_t removedCount = 0;
		for (size_t i = 0; i < guides.size(); ++i) {
			AIArtHandle art = guides[i].art;
			if (!art) {
				continue;
			}
			if (guides[i].createdForHiddenCenterline) {
				sAIArt->SetArtUserAttr(art, kArtLocked | kArtHidden, 0);
				if (sAIArt->DisposeArt(art) == kNoErr) {
					++removedCount;
				}
			} else {
				ClearCenterlineTailGuideMetadata(art);
			}
		}
		return removedCount;
	}

	bool FindMatchingGeneratedGuideForTail(const std::string& sourceId,
		const std::vector<DuctworkPoint>& sourcePoints,
		int segmentIndex,
		AIArtHandle& outGuideArt)
	{
		outGuideArt = nullptr;
		if (sourceId.empty() ||
			sourcePoints.size() < 2 ||
			segmentIndex < 0 ||
			segmentIndex + 1 >= static_cast<int>(sourcePoints.size())) {
			return false;
		}

		std::vector<AIArtHandle> allPaths;
		CollectAllDocumentPaths(allPaths);
		for (size_t i = 0; i < allPaths.size(); ++i) {
			AIArtHandle art = allPaths[i];
			if (!art || !IsGeneratedEmoryArtInternal(art)) {
				continue;
			}

			std::string role;
			if (!DuctworkMetadata::GetString(art, kEmoryRoleKey, role) || role != kEmoryRoleGuide) {
				continue;
			}

			std::string artSourceId;
			if (!ReadGeneratedArtSourceId(art, artSourceId) || artSourceId != sourceId) {
				continue;
			}

			int artSegmentIndex = -1;
			const bool segmentMatches = ReadGeneratedSegmentIndex(art, artSegmentIndex) &&
				artSegmentIndex == segmentIndex;

			DuctworkPath guidePath;
			std::vector<DuctworkPoint> guidePoints;
			const bool geometryMatches = BuildProcessPathForArt(art, guidePath) &&
				(SanitizePolyline(guidePath.points, guidePoints), GuidePointsMatchTerminalSegment(guidePoints, sourcePoints, segmentIndex, 4.0));

			if (segmentMatches || geometryMatches) {
				outGuideArt = art;
				return true;
			}
		}
		return false;
	}

	bool MovePathTerminalEndpointPreservingHandles(AIArtHandle art, bool atStart, const DuctworkPoint& newPoint)
	{
		if (!art || !sAIPath) {
			return false;
		}

		ai::int16 segmentCount = 0;
		if (sAIPath->GetPathSegmentCount(art, &segmentCount) != kNoErr || segmentCount < 2) {
			return false;
		}

		std::vector<AIPathSegment> segments(static_cast<size_t>(segmentCount));
		if (sAIPath->GetPathSegments(art, 0, segmentCount, &segments[0]) != kNoErr) {
			return false;
		}

		const ai::int16 endpointIndex = atStart ? 0 : static_cast<ai::int16>(segmentCount - 1);
		const AIReal oldH = segments[endpointIndex].p.h;
		const AIReal oldV = segments[endpointIndex].p.v;
		const AIReal newH = static_cast<AIReal>(newPoint.x);
		const AIReal newV = static_cast<AIReal>(newPoint.y);
		const AIReal deltaH = newH - oldH;
		const AIReal deltaV = newV - oldV;

		if (std::fabs(static_cast<double>(deltaH)) < 0.001 &&
			std::fabs(static_cast<double>(deltaV)) < 0.001) {
			return true;
		}

		segments[endpointIndex].p.h = newH;
		segments[endpointIndex].p.v = newV;
		segments[endpointIndex].in.h += deltaH;
		segments[endpointIndex].in.v += deltaV;
		segments[endpointIndex].out.h += deltaH;
		segments[endpointIndex].out.v += deltaV;

		if (sAIPath->SetPathSegments(art, 0, segmentCount, &segments[0]) != kNoErr) {
			return false;
		}

		const int terminalSegmentIndex = atStart ? 0 : static_cast<int>(segmentCount) - 2;
		ApplyTerminalSegmentStyleToPathArt(art, terminalSegmentIndex, ReadTerminalSegmentStyle(art, atStart));
		return true;
	}

	bool ResolveSourceArtForTailEndpoint(const std::vector<EmorySourceIdCandidate>& fragments,
		const CenterlineTailGuideCandidate& tail,
		bool atStart,
		AIArtHandle& outArt)
	{
		outArt = nullptr;
		if (fragments.empty() || tail.points.size() < 2) {
			return false;
		}

		const DuctworkPoint guideNeighbor = atStart ? tail.points[1] : tail.points[tail.points.size() - 2];
		int bestIndex = -1;
		double bestScore = 0.0;
		bool bestScoreSet = false;
		for (size_t i = 0; i < fragments.size(); ++i) {
			const EmorySourceIdCandidate& fragment = fragments[i];
			if (!fragment.art) {
				continue;
			}

			std::vector<DuctworkPoint> points;
			SanitizePolyline(fragment.path.points, points);
			if (points.size() < 2) {
				continue;
			}

			const int expectedSegmentIndex = atStart ? 0 : static_cast<int>(points.size() - 2);
			if (tail.segmentIndex >= 0 &&
				tail.segmentIndex != expectedSegmentIndex &&
				fragments.size() > 1) {
				continue;
			}

			const DuctworkPoint sourceNeighbor = atStart ? points[1] : points[points.size() - 2];
			const double score = PointDistanceSquared(guideNeighbor, sourceNeighbor);
			if (!bestScoreSet || score < bestScore) {
				bestIndex = static_cast<int>(i);
				bestScore = score;
				bestScoreSet = true;
			}
		}

		if (bestIndex < 0 && fragments.size() == 1 && fragments[0].art) {
			bestIndex = 0;
			bestScore = 0.0;
			bestScoreSet = true;
		}
		if (bestIndex < 0 || !bestScoreSet) {
			return false;
		}
		if (bestScore > 25.0 &&
			fragments.size() > 1 &&
			!(tail.includesStart && tail.includesEnd)) {
			return false;
		}

		outArt = fragments[static_cast<size_t>(bestIndex)].art;
		return outArt != nullptr;
	}

	void UpdateStoredOriginalTerminalEndpointForFragments(const std::vector<EmorySourceIdCandidate>& fragments,
		bool atStart,
		const DuctworkPoint& newPoint)
	{
		std::set<AIArtHandle> updatedArts;
		for (size_t i = 0; i < fragments.size(); ++i) {
			AIArtHandle art = fragments[i].art;
			if (!art || !updatedArts.insert(art).second) {
				continue;
			}

			std::string serialized;
			if (!DuctworkMetadata::GetString(art, kEmoryOriginalPathPointsKey, serialized) || serialized.empty()) {
				continue;
			}

			std::vector<DuctworkPoint> originalPoints;
			if (!DeserializeCenterlinePoints(serialized, originalPoints) || originalPoints.size() < 2) {
				continue;
			}

			if (atStart) {
				originalPoints.front() = newPoint;
			} else {
				originalPoints.back() = newPoint;
			}

			const std::string updated = SerializeCenterlinePoints(originalPoints);
			if (!updated.empty()) {
				DuctworkMetadata::SetString(art, kEmoryOriginalPathPointsKey, updated);
			}
		}
	}

	size_t SyncSourceEndpointsFromCenterlineTailGuides(
		const std::map<std::string, std::vector<EmorySourceIdCandidate> >& fragmentsBySourceId,
		const std::set<std::string>& sourceIds)
	{
		std::vector<CenterlineTailGuideCandidate> guides;
		CollectCenterlineTailGuides(sourceIds, guides);
		size_t syncCount = 0;
		for (size_t i = 0; i < guides.size(); ++i) {
			const CenterlineTailGuideCandidate& guide = guides[i];
			std::map<std::string, std::vector<EmorySourceIdCandidate> >::const_iterator fragmentsIt =
				fragmentsBySourceId.find(guide.sourceId);
			if (fragmentsIt == fragmentsBySourceId.end() || fragmentsIt->second.empty()) {
				continue;
			}

			if (guide.includesStart && guide.points.size() >= 2) {
				AIArtHandle sourceArt = nullptr;
				if (ResolveSourceArtForTailEndpoint(fragmentsIt->second, guide, true, sourceArt) &&
					MovePathTerminalEndpointPreservingHandles(sourceArt, true, guide.points.front())) {
					UpdateStoredOriginalTerminalEndpointForFragments(fragmentsIt->second, true, guide.points.front());
					++syncCount;
				}
			}
			if (guide.includesEnd && guide.points.size() >= 2) {
				AIArtHandle sourceArt = nullptr;
				if (ResolveSourceArtForTailEndpoint(fragmentsIt->second, guide, false, sourceArt) &&
					MovePathTerminalEndpointPreservingHandles(sourceArt, false, guide.points.back())) {
					UpdateStoredOriginalTerminalEndpointForFragments(fragmentsIt->second, false, guide.points.back());
					++syncCount;
				}
			}
		}
		return syncCount;
	}

	bool EnsureCenterlineTailGuideForSegment(const std::string& sourceId,
		const EmorySourceIdCandidate& fragment,
		const std::vector<DuctworkPoint>& points,
		int segmentIndex,
		bool includesStart,
		bool includesEnd,
		size_t& ioCreatedCount,
		size_t& ioExistingCount)
	{
		if (sourceId.empty() ||
			!fragment.art ||
			points.size() < 2 ||
			segmentIndex < 0 ||
			segmentIndex + 1 >= static_cast<int>(points.size()) ||
			(!includesStart && !includesEnd)) {
			return false;
		}

		std::string terminalStyle = ReadTerminalSegmentStyle(fragment.art, includesStart ? true : false);
		if (includesStart && includesEnd && terminalStyle != "curved") {
			terminalStyle = ReadTerminalSegmentStyle(fragment.art, false);
		}

		AIArtHandle existingGuide = nullptr;
		if (FindMatchingGeneratedGuideForTail(sourceId, points, segmentIndex, existingGuide) && existingGuide) {
			sAIArt->SetArtUserAttr(existingGuide, kArtLocked | kArtHidden, 0);
			ApplyGuideStyleInternal(existingGuide, GetEmoryColorSpec(fragment.path.layerName));
			ApplyTerminalGuideStyleToGuideArt(existingGuide, points, static_cast<size_t>(segmentIndex), terminalStyle);
			MarkCenterlineTailGuide(existingGuide, includesStart, includesEnd, false);
			++ioExistingCount;
			return true;
		}

		std::vector<DuctworkPoint> guidePoints;
		guidePoints.push_back(points[static_cast<size_t>(segmentIndex)]);
		guidePoints.push_back(points[static_cast<size_t>(segmentIndex + 1)]);

		AIArtHandle guideArt = nullptr;
		if (!CreateOpenPath(fragment.art, guidePoints, guideArt) || !guideArt) {
			return false;
		}
		if (!ApplyGuideStyleInternal(guideArt, GetEmoryColorSpec(fragment.path.layerName)) ||
			!ApplyTerminalGuideStyleToGuideArt(guideArt, points, static_cast<size_t>(segmentIndex), terminalStyle)) {
			sAIArt->DisposeArt(guideArt);
			return false;
		}

		TagGeneratedArt(guideArt, kEmoryRoleGuide, sourceId, 0.0, segmentIndex, -1, std::string());
		MarkCenterlineTailGuide(guideArt, includesStart, includesEnd, true);
		++ioCreatedCount;
		return true;
	}

	size_t EnsureCenterlineTailGuidesForHiddenCenterlines(
		const std::map<std::string, std::vector<EmorySourceIdCandidate> >& fragmentsBySourceId,
		const std::vector<DuctworkPoint>& registerAttachmentPoints,
		size_t& outExistingCount)
	{
		outExistingCount = 0;
		size_t createdCount = 0;
		for (std::map<std::string, std::vector<EmorySourceIdCandidate> >::const_iterator sourceIt = fragmentsBySourceId.begin();
			sourceIt != fragmentsBySourceId.end();
			++sourceIt) {
			for (size_t fragmentIndex = 0; fragmentIndex < sourceIt->second.size(); ++fragmentIndex) {
				const EmorySourceIdCandidate& fragment = sourceIt->second[fragmentIndex];
				std::vector<DuctworkPoint> points;
				SanitizePolyline(fragment.path.points, points);
				if (points.size() < 2) {
					continue;
				}

				const bool registerAtStart = IsPointNearAny(points.front(), registerAttachmentPoints, 10.0);
				const bool registerAtEnd = IsPointNearAny(points.back(), registerAttachmentPoints, 10.0);
				const int lastSegmentIndex = static_cast<int>(points.size() - 2);
				if (registerAtStart) {
					EnsureCenterlineTailGuideForSegment(sourceIt->first,
						fragment,
						points,
						0,
						true,
						registerAtEnd && lastSegmentIndex == 0,
						createdCount,
						outExistingCount);
				}
				if (registerAtEnd && !(registerAtStart && lastSegmentIndex == 0)) {
					EnsureCenterlineTailGuideForSegment(sourceIt->first,
						fragment,
						points,
						lastSegmentIndex,
						false,
						true,
						createdCount,
						outExistingCount);
				}
			}
		}
		return createdCount;
	}
}

bool DuctworkGeometry::SetSelectedEmoryCenterlineVisibility(bool hidden, std::string& outMessage)
{
	outMessage = "Select Emory ductwork or its source centerline first.";
	if (!sAIArt) {
		outMessage = "Illustrator SDK is not available.";
		return false;
	}

	std::set<std::string> sourceIds;
	if (!CollectSelectedEmorySourceIds(sourceIds)) {
		return false;
	}

	std::vector<AIArtHandle> sourceArts;
	std::set<AIArtHandle> seenSourceArts;
	std::map<std::string, std::vector<EmorySourceIdCandidate> > fragmentsBySourceId;
	for (std::set<std::string>::const_iterator it = sourceIds.begin(); it != sourceIds.end(); ++it) {
		std::vector<EmorySourceIdCandidate> fragments;
		if (!CollectSourceCenterlineFragmentsForSourceId(*it, fragments) || fragments.empty()) {
			continue;
		}

		if (hidden && fragments.size() == 1) {
			AIArtHandle group = nullptr;
			EnsureRunGroupForSource(fragments[0].art, *it, group);
		}

		fragmentsBySourceId[*it] = fragments;
		for (size_t fragmentIndex = 0; fragmentIndex < fragments.size(); ++fragmentIndex) {
			AppendUniqueArt(fragments[fragmentIndex].art, sourceArts, seenSourceArts);
		}
	}

	if (!hidden) {
		CollectSourceCenterlineArtsByMetadata(sourceIds, sourceArts, seenSourceArts);
	}

	if (sourceArts.empty()) {
		return false;
	}

	size_t tailGuidesCreated = 0;
	size_t tailGuidesExisting = 0;
	size_t tailGuidesRemoved = 0;
	size_t tailEndpointsSynced = 0;
	if (hidden) {
		RemoveCreatedCenterlineTailGuidesAndClearTags(sourceIds);
		std::vector<DuctworkPoint> registerAttachmentPoints;
		CollectRegisterAttachmentPoints(registerAttachmentPoints);
		tailGuidesCreated = EnsureCenterlineTailGuidesForHiddenCenterlines(fragmentsBySourceId,
			registerAttachmentPoints,
			tailGuidesExisting);
	} else {
		for (size_t i = 0; i < sourceArts.size(); ++i) {
			SetCenterlineHidden(sourceArts[i], false);
		}

		fragmentsBySourceId.clear();
		for (std::set<std::string>::const_iterator it = sourceIds.begin(); it != sourceIds.end(); ++it) {
			std::vector<EmorySourceIdCandidate> fragments;
			if (CollectSourceCenterlineFragmentsForSourceId(*it, fragments) && !fragments.empty()) {
				fragmentsBySourceId[*it] = fragments;
			}
		}
		tailEndpointsSynced = SyncSourceEndpointsFromCenterlineTailGuides(fragmentsBySourceId, sourceIds);
		tailGuidesRemoved = RemoveCreatedCenterlineTailGuidesAndClearTags(sourceIds);
	}

	for (size_t i = 0; i < sourceArts.size(); ++i) {
		if (hidden) {
			SetCenterlineHidden(sourceArts[i], true);
		}
	}

	std::vector<std::string> affectedSourceIds(sourceIds.begin(), sourceIds.end());
	ApplyFinalEmoryOrdering(affectedSourceIds);

	{
		std::ostringstream logStream;
		logStream << "Emory centerline visibility hidden=" << (hidden ? 1 : 0)
			<< " sources=" << sourceIds.size()
			<< " centerlines=" << sourceArts.size()
			<< " tailCreated=" << tailGuidesCreated
			<< " tailExisting=" << tailGuidesExisting
			<< " tailSynced=" << tailEndpointsSynced
			<< " tailRemoved=" << tailGuidesRemoved;
		DuctworkLog::Write(logStream.str());
	}

	std::ostringstream message;
	message << (hidden ? "Hid " : "Showed ")
		<< sourceArts.size() << " centerline";
	if (sourceArts.size() != 1) {
		message << "s";
	}
	message << ".";
	if (hidden && (tailGuidesCreated + tailGuidesExisting) > 0) {
		const size_t tailCount = tailGuidesCreated + tailGuidesExisting;
		message << " Left " << tailCount << " register tail";
		if (tailCount != 1) {
			message << "s";
		}
		message << " visible.";
	} else if (!hidden) {
		if (tailEndpointsSynced > 0) {
			message << " Synced " << tailEndpointsSynced << " edited tail endpoint";
			if (tailEndpointsSynced != 1) {
				message << "s";
			}
			message << " back to the source line.";
		}
		if (tailGuidesRemoved > 0) {
			message << " Removed " << tailGuidesRemoved << " temporary tail";
			if (tailGuidesRemoved != 1) {
				message << "s";
			}
			message << ".";
		}
	}
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
	std::string selectedSegmentKey;
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

		std::ostringstream segmentKeyStream;
		segmentKeyStream << artSourceId << "#" << segmentIndex;
		const std::string segmentKey = segmentKeyStream.str();
		if (!selectedSegmentKey.empty() && segmentKey != selectedSegmentKey) {
			outMessage = "Select only one Emory duct segment to mark the cascade start.";
			return false;
		}

		selectedSegment = art;
		sourceId = artSourceId;
		selectedSegmentIndex = segmentIndex;
		selectedSegmentKey = segmentKey;
	}

	if (!selectedSegment || sourceId.empty() || selectedSegmentIndex < 0) {
		return false;
	}

	std::vector<EmorySourceState> states;
	std::map<std::string, int> stateIndexBySourceId;
	CollectEmorySourceStates(states, stateIndexBySourceId);

	AIArtHandle sourceArt = nullptr;
	DuctworkPath sourcePath;
	int selectedStateIndex = -1;
	if (!states.empty() &&
		(FindBestStateIndexForGeneratedArt(selectedSegment, kEmoryRoleSegment, sourceId, states, selectedStateIndex) ||
		 FindBestStateIndexForGeneratedArtLoose(selectedSegment, kEmoryRoleSegment, states, selectedStateIndex)) &&
		selectedStateIndex >= 0 &&
		selectedStateIndex < static_cast<int>(states.size())) {
		sourceArt = states[selectedStateIndex].art;
		sourcePath = states[selectedStateIndex].path;
	} else if (!FindSourceArtForSourceId(sourceId, sourceArt, sourcePath)) {
		outMessage = "Unable to find the source centerline for the selected duct segment.";
		return false;
	}

	std::vector<DuctworkPoint> points;
	SanitizePolyline(sourcePath.points, points);
	AIArtHandle canonicalArt = sourceArt;
	DuctworkPath canonicalPath = sourcePath;
	std::vector<DuctworkPoint> canonicalPoints = points;
	int canonicalSelectedSegmentIndex = selectedSegmentIndex;

	AIArtHandle backupArt = nullptr;
	DuctworkPath backupPath;
	if (GetPrimaryBackupCenterlineForSourceId(sourceId, backupArt, backupPath)) {
		std::vector<int> backupIndices;
		if (MapFragmentSegmentsToBackupIndices(points, backupPath.points, backupIndices) &&
			selectedSegmentIndex >= 0 &&
			selectedSegmentIndex < static_cast<int>(backupIndices.size()) &&
			backupIndices[selectedSegmentIndex] >= 0) {
			canonicalArt = backupArt;
			canonicalPath = backupPath;
			canonicalPoints = backupPath.points;
			canonicalSelectedSegmentIndex = backupIndices[selectedSegmentIndex];
		}
	}

	const size_t segmentCount = canonicalPoints.size() > 1 ? (canonicalPoints.size() - 1) : 0;
	if (segmentCount == 0) {
		outMessage = "The selected run has no segments.";
		return false;
	}

	if (canonicalSelectedSegmentIndex < 0 || canonicalSelectedSegmentIndex >= static_cast<int>(segmentCount)) {
		outMessage = "The selected segment is out of range for this run.";
		return false;
	}

	double defaultWidth = 0.0;
	if (!ResolveSourceBodyWidth(canonicalArt, sourceId, defaultWidth) || defaultWidth <= 0.0) {
		defaultWidth = kDefaultDuctWidth;
	}
	if (defaultWidth < kMinDuctWidth) {
		defaultWidth = kMinDuctWidth;
	}

	std::vector<double> widths;
	ReadSegmentWidths(canonicalArt, segmentCount, defaultWidth, widths);
	std::vector<double> retaperedWidths = widths;
	ApplyDefaultStraightChainTapers(canonicalArt, canonicalPoints, canonicalSelectedSegmentIndex, retaperedWidths);
	bool widthsChanged = false;
	if (retaperedWidths != widths) {
		widths = retaperedWidths;
		WriteSegmentWidths(canonicalArt, widths);
		widthsChanged = true;
	}

	WriteStartSegmentIndex(canonicalArt, canonicalSelectedSegmentIndex);

	std::set<std::string> sourceIdsToLocalize;
	sourceIdsToLocalize.insert(sourceId);
	if (canonicalArt == backupArt) {
		LocalizeVisibleFragmentMetadataFromBackup(sourceIdsToLocalize, true);
	}

	if (widthsChanged && canonicalSelectedSegmentIndex >= 0 && canonicalSelectedSegmentIndex < static_cast<int>(widths.size())) {
		WriteStoredSourceBodyWidth(canonicalArt, widths[canonicalSelectedSegmentIndex]);
		std::vector<DuctworkPath> regeneratePaths;
		std::vector<EmorySourceState> regenerateStates;
		std::map<std::string, int> regenerateIndexBySourceId;
		if (CollectEmorySourceStates(regenerateStates, regenerateIndexBySourceId)) {
			for (size_t i = 0; i < regenerateStates.size(); ++i) {
				if (regenerateStates[i].sourceId == sourceId) {
					regeneratePaths.push_back(regenerateStates[i].path);
				}
			}
		}
		if (regeneratePaths.empty()) {
			regeneratePaths.push_back(sourcePath);
		}
		std::vector<std::string> sourceIds(1, sourceId);
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
	std::string selectedSegmentKey;
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

		std::ostringstream segmentKeyStream;
		segmentKeyStream << artSourceId << "#" << segmentIndex;
		const std::string segmentKey = segmentKeyStream.str();
		if (!selectedSegmentKey.empty() && segmentKey != selectedSegmentKey) {
			outMessage = "Select only one Emory duct segment to clear its start mark.";
			return false;
		}

		selectedSegment = art;
		sourceId = artSourceId;
		selectedSegmentIndex = segmentIndex;
		selectedSegmentKey = segmentKey;
	}

	if (!selectedSegment || sourceId.empty() || selectedSegmentIndex < 0) {
		return false;
	}

	std::vector<EmorySourceState> states;
	std::map<std::string, int> stateIndexBySourceId;
	CollectEmorySourceStates(states, stateIndexBySourceId);

	AIArtHandle sourceArt = nullptr;
	DuctworkPath sourcePath;
	int selectedStateIndex = -1;
	if (!states.empty() &&
		(FindBestStateIndexForGeneratedArt(selectedSegment, kEmoryRoleSegment, sourceId, states, selectedStateIndex) ||
		 FindBestStateIndexForGeneratedArtLoose(selectedSegment, kEmoryRoleSegment, states, selectedStateIndex)) &&
		selectedStateIndex >= 0 &&
		selectedStateIndex < static_cast<int>(states.size())) {
		sourceArt = states[selectedStateIndex].art;
		sourcePath = states[selectedStateIndex].path;
	} else if (!FindSourceArtForSourceId(sourceId, sourceArt, sourcePath)) {
		outMessage = "Unable to find the source centerline for the selected duct segment.";
		return false;
	}

	std::vector<DuctworkPoint> points;
	SanitizePolyline(sourcePath.points, points);
	AIArtHandle canonicalArt = sourceArt;
	DuctworkPath canonicalPath = sourcePath;
	std::vector<DuctworkPoint> canonicalPoints = points;
	int canonicalSelectedSegmentIndex = selectedSegmentIndex;

	AIArtHandle backupArt = nullptr;
	DuctworkPath backupPath;
	if (GetPrimaryBackupCenterlineForSourceId(sourceId, backupArt, backupPath)) {
		std::vector<int> backupIndices;
		if (MapFragmentSegmentsToBackupIndices(points, backupPath.points, backupIndices) &&
			selectedSegmentIndex >= 0 &&
			selectedSegmentIndex < static_cast<int>(backupIndices.size()) &&
			backupIndices[selectedSegmentIndex] >= 0) {
			canonicalArt = backupArt;
			canonicalPath = backupPath;
			canonicalPoints = backupPath.points;
			canonicalSelectedSegmentIndex = backupIndices[selectedSegmentIndex];
		}
	}

	const size_t segmentCount = canonicalPoints.size() > 1 ? (canonicalPoints.size() - 1) : 0;
	if (segmentCount == 0) {
		outMessage = "The selected run has no segments.";
		return false;
	}

	const int currentStartSegmentIndex = ReadStartSegmentIndex(canonicalArt, segmentCount);
	if (!HasExplicitStartSegmentIndex(canonicalArt) || canonicalSelectedSegmentIndex != currentStartSegmentIndex) {
		outMessage = "The selected segment is not the marked start.";
		return false;
	}

	double defaultWidth = 0.0;
	if (!ResolveSourceBodyWidth(canonicalArt, sourceId, defaultWidth) || defaultWidth <= 0.0) {
		defaultWidth = kDefaultDuctWidth;
	}
	if (defaultWidth < kMinDuctWidth) {
		defaultWidth = kMinDuctWidth;
	}

	std::vector<double> widths;
	ReadSegmentWidths(canonicalArt, segmentCount, defaultWidth, widths);
	std::vector<double> retaperedWidths = widths;
	const int defaultStartSegmentIndex = ResolveDefaultStartSegmentIndex(canonicalArt, segmentCount);
	ApplyDefaultStraightChainTapers(canonicalArt, canonicalPoints, defaultStartSegmentIndex, retaperedWidths);
	const bool widthsChanged = (retaperedWidths != widths);
	if (widthsChanged) {
		widths = retaperedWidths;
		WriteSegmentWidths(canonicalArt, widths);
	}

	ClearStartSegmentIndex(canonicalArt);
	std::set<std::string> sourceIdsToLocalize;
	sourceIdsToLocalize.insert(sourceId);
	if (canonicalArt == backupArt) {
		LocalizeVisibleFragmentMetadataFromBackup(sourceIdsToLocalize, true);
	}
	if (widthsChanged && !widths.empty()) {
		WriteStoredSourceBodyWidth(canonicalArt, widths[defaultStartSegmentIndex]);
		std::vector<DuctworkPath> regeneratePaths;
		std::vector<EmorySourceState> regenerateStates;
		std::map<std::string, int> regenerateIndexBySourceId;
		if (CollectEmorySourceStates(regenerateStates, regenerateIndexBySourceId)) {
			for (size_t i = 0; i < regenerateStates.size(); ++i) {
				if (regenerateStates[i].sourceId == sourceId) {
					regeneratePaths.push_back(regenerateStates[i].path);
				}
			}
		}
		if (regeneratePaths.empty()) {
			regeneratePaths.push_back(sourcePath);
		}
		std::vector<std::string> sourceIds(1, sourceId);
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
	auto widthApplyStart = std::chrono::steady_clock::now();
	outMessage = "Select one or more Emory duct segments first.";
	if (!sAIArt) {
		outMessage = "Illustrator SDK is not available.";
		return false;
	}

	// Repair/normalize are needed to ensure consistent source IDs
	RepairVisibleFragmentSourceIdsFromBackups();
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

	struct SelectedGeneratedSegment
	{
		AIArtHandle art = nullptr;
		std::string sourceId;
		int segmentIndex = -1;
	};

	std::vector<SelectedGeneratedSegment> selectedSegments;
	std::set<std::string> deleteSourceIds;
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

		SelectedGeneratedSegment selected;
		selected.art = art;
		selected.sourceId = artSourceId;
		selected.segmentIndex = segmentIndex;
		selectedSegments.push_back(selected);
		deleteSourceIds.insert(artSourceId);
	}

	if (selectedSegments.empty()) {
		return false;
	}

	// PERF: Removed verbose per-segment logging

	{
		std::set<std::string> selectedSourceIdsToLocalize;
		for (size_t i = 0; i < selectedSegments.size(); ++i) {
			if (!selectedSegments[i].sourceId.empty()) {
				selectedSourceIdsToLocalize.insert(selectedSegments[i].sourceId);
			}
		}
		LocalizeVisibleFragmentMetadataFromBackup(selectedSourceIdsToLocalize);
	}

	std::vector<EmorySourceState> states;
	std::map<std::string, int> stateIndexBySourceId;
	if (!CollectEmorySourceStates(states, stateIndexBySourceId)) {
		outMessage = "Unable to read the Emory source centerlines for the selected ductwork.";
		return false;
	}

	{
		auto collectMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - widthApplyStart).count();
		DuctworkLog::WriteAlways("[PERF] Width-apply: collect+localize=" + std::to_string(collectMs) + "ms states=" + std::to_string(states.size()));
	}

	std::map<int, std::vector<int> > selectedIndicesByState;
	std::vector<std::string> sourceIds;
	std::vector<DuctworkPath> regeneratePaths;
	size_t totalSelectedCount = 0;

	for (size_t i = 0; i < selectedSegments.size(); ++i) {
		int stateIndex = -1;
		if (!FindBestStateIndexForGeneratedArt(selectedSegments[i].art,
			kEmoryRoleSegment,
			selectedSegments[i].sourceId,
			states,
			stateIndex) &&
			!FindBestStateIndexForGeneratedArtLoose(selectedSegments[i].art,
				kEmoryRoleSegment,
				states,
				stateIndex)) {
			outMessage = "Unable to find the source centerline for one of the selected duct runs.";
			return false;
		}

		selectedIndicesByState[stateIndex].push_back(selectedSegments[i].segmentIndex);
	}

	// PERF: Removed verbose mapped-selection logging

	for (std::map<int, std::vector<int> >::iterator it = selectedIndicesByState.begin(); it != selectedIndicesByState.end(); ++it) {
		std::vector<int>& selectedSegmentIndices = it->second;
		std::sort(selectedSegmentIndices.begin(), selectedSegmentIndices.end());
		selectedSegmentIndices.erase(std::unique(selectedSegmentIndices.begin(), selectedSegmentIndices.end()), selectedSegmentIndices.end());
		totalSelectedCount += selectedSegmentIndices.size();

		EmorySourceState& state = states[it->first];
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
	}

	bool selectedRunsAreFullyCovered = !selectedIndicesByState.empty();
	for (std::map<int, std::vector<int> >::const_iterator it = selectedIndicesByState.begin(); it != selectedIndicesByState.end(); ++it) {
		if (it->first < 0 ||
			it->first >= static_cast<int>(states.size()) ||
			!SelectionCoversEverySegment(states[it->first], it->second)) {
			selectedRunsAreFullyCovered = false;
			break;
		}
	}

	double fullSelectionReferenceWidth = 0.0;
	if (selectedRunsAreFullyCovered) {
		std::map<std::string, std::vector<int> > selectedIndicesBySourceId;
		for (size_t i = 0; i < selectedSegments.size(); ++i) {
			if (!selectedSegments[i].sourceId.empty()) {
				selectedIndicesBySourceId[selectedSegments[i].sourceId].push_back(selectedSegments[i].segmentIndex);
			}
		}

		for (std::map<std::string, std::vector<int> >::iterator sourceIt = selectedIndicesBySourceId.begin();
			sourceIt != selectedIndicesBySourceId.end() && fullSelectionReferenceWidth <= 0.0;
			++sourceIt) {
			std::vector<int>& selectedForSource = sourceIt->second;
			std::sort(selectedForSource.begin(), selectedForSource.end());
			selectedForSource.erase(std::unique(selectedForSource.begin(), selectedForSource.end()), selectedForSource.end());

			int stateIndex = -1;
			std::map<std::string, int>::const_iterator indexIt = stateIndexBySourceId.find(sourceIt->first);
			if (indexIt != stateIndexBySourceId.end()) {
				stateIndex = indexIt->second;
			}
			if (stateIndex < 0 || stateIndex >= static_cast<int>(states.size()) ||
				selectedIndicesByState.find(stateIndex) == selectedIndicesByState.end()) {
				for (std::map<int, std::vector<int> >::const_iterator stateIt = selectedIndicesByState.begin();
					stateIt != selectedIndicesByState.end();
					++stateIt) {
					if (stateIt->first >= 0 &&
						stateIt->first < static_cast<int>(states.size()) &&
						states[stateIt->first].sourceId == sourceIt->first) {
						stateIndex = stateIt->first;
						break;
					}
				}
			}
			if (stateIndex < 0 || stateIndex >= static_cast<int>(states.size())) {
				continue;
			}

			const EmorySourceState& state = states[stateIndex];
			for (size_t i = 0; i < selectedForSource.size(); ++i) {
				const int segmentIndex = selectedForSource[i];
				if (segmentIndex < 0 || segmentIndex >= static_cast<int>(state.widths.size())) {
					continue;
				}
				const double candidateWidth = state.widths[segmentIndex];
				if (std::isfinite(candidateWidth) && candidateWidth > kPointEpsilon) {
					fullSelectionReferenceWidth = candidateWidth;
					break;
				}
			}
		}
	}

	double fullSelectionScale = 0.0;
	if (selectedRunsAreFullyCovered && fullSelectionReferenceWidth > kPointEpsilon) {
		fullSelectionScale = newWidth / fullSelectionReferenceWidth;
		if (!std::isfinite(fullSelectionScale) || fullSelectionScale <= 0.0) {
			fullSelectionScale = 0.0;
		}
	}

	double mixedSelectionReferenceWidth = 0.0;
	bool mixedSelectionReferenceSet = false;
	bool selectedWidthsAreMixed = false;
	for (std::map<int, std::vector<int> >::const_iterator it = selectedIndicesByState.begin(); it != selectedIndicesByState.end(); ++it) {
		if (it->first < 0 || it->first >= static_cast<int>(states.size())) {
			continue;
		}

		const EmorySourceState& state = states[it->first];
		for (size_t i = 0; i < it->second.size(); ++i) {
			const int segmentIndex = it->second[i];
			if (segmentIndex < 0 || segmentIndex >= static_cast<int>(state.widths.size())) {
				continue;
			}

			const double width = state.widths[segmentIndex];
			if (!std::isfinite(width) || width <= kPointEpsilon) {
				continue;
			}
			if (!mixedSelectionReferenceSet) {
				mixedSelectionReferenceWidth = width;
				mixedSelectionReferenceSet = true;
			} else if (std::fabs(width - mixedSelectionReferenceWidth) > 1e-6) {
				selectedWidthsAreMixed = true;
			}
		}
	}

	const bool forceUniformMixedSelection = selectedIndicesByState.size() > 1 && selectedWidthsAreMixed;
	if (forceUniformMixedSelection) {
		fullSelectionScale = 0.0;
	}

	bool terminalOnlyWidthSelection = !selectedIndicesByState.empty();
	for (std::map<int, std::vector<int> >::const_iterator it = selectedIndicesByState.begin(); it != selectedIndicesByState.end(); ++it) {
		if (it->first < 0 ||
			it->first >= static_cast<int>(states.size()) ||
			!SelectionContainsOnlyTerminalWidthControls(states[it->first], it->second)) {
			terminalOnlyWidthSelection = false;
			break;
		}
	}

	{
		std::ostringstream logStream;
		logStream << "Width-apply selection selected=" << totalSelectedCount
			<< " states=" << selectedIndicesByState.size()
			<< " fullCovered=" << (selectedRunsAreFullyCovered ? 1 : 0)
			<< " mixedWidths=" << (selectedWidthsAreMixed ? 1 : 0)
			<< " uniformMixed=" << (forceUniformMixedSelection ? 1 : 0)
			<< " terminalOnly=" << (terminalOnlyWidthSelection ? 1 : 0)
			<< " proportionalScale=" << fullSelectionScale;
		DuctworkLog::WriteAlways(logStream.str());
	}

	// PERF: Removed verbose before/after cascade logging
	for (std::map<int, std::vector<int> >::const_iterator it = selectedIndicesByState.begin(); it != selectedIndicesByState.end(); ++it) {
		if (fullSelectionScale > 0.0) {
			ApplyProportionalWidthScaleToPathState(states[it->first], fullSelectionScale);
		} else if (forceUniformMixedSelection) {
			ApplyUniformWidthToPathState(states[it->first], it->second, newWidth);
		} else {
			ApplySelectedWidthToPathState(states[it->first], it->second, newWidth);
		}
	}

	std::set<std::string> selectedSourceIds;
	for (std::map<int, std::vector<int> >::const_iterator it = selectedIndicesByState.begin(); it != selectedIndicesByState.end(); ++it) {
		if (it->first >= 0 &&
			it->first < static_cast<int>(states.size()) &&
			!states[it->first].sourceId.empty()) {
			selectedSourceIds.insert(states[it->first].sourceId);
		}
	}

	std::vector<DuctworkConnection> widthConnections;
	if (terminalOnlyWidthSelection) {
		DuctworkLog::WriteAlways("Emory width-apply terminal-only selection: branch cascade skipped, parent roots refreshed");
		RefreshAffectedBranchRootWidthsFromParents(states, selectedSourceIds);
	} else {
		CollectEmoryNetworkConnections(states, widthConnections);
		CascadeConnectedBranchWidths(states, widthConnections);
	}

	std::set<std::string> affectedSourceIds;
	for (size_t i = 0; i < states.size(); ++i) {
		if (states[i].touched && !states[i].sourceId.empty()) {
			affectedSourceIds.insert(states[i].sourceId);
			deleteSourceIds.insert(states[i].sourceId);
		}
	}

	if (affectedSourceIds.empty()) {
		outMessage = "No Emory runs were updated from the current selection.";
		return false;
	}

	// Preserve the current visible stroke for each affected run before rebuild.
	// Width changes should not silently normalize only some runs back to the
	// computed default stroke width.
	for (size_t i = 0; i < states.size(); ++i) {
		const EmorySourceState& state = states[i];
		if (affectedSourceIds.find(state.sourceId) == affectedSourceIds.end() ||
			state.sourceId.empty() ||
			state.sourceStrokeWidth <= 0.0) {
			continue;
		}
		WriteStoredSourceStrokeWidthForSourceId(state.sourceId, state.sourceStrokeWidth);
		WriteStoredSourceStrokeExplicitForSourceId(state.sourceId, true);
	}

	std::set<std::string> backupAffectedSourceIds;
	for (std::set<std::string>::const_iterator sourceIt = affectedSourceIds.begin(); sourceIt != affectedSourceIds.end(); ++sourceIt) {
		if (SourceIdHasBackupCenterline(*sourceIt)) {
			backupAffectedSourceIds.insert(*sourceIt);
		}
	}

	if (!backupAffectedSourceIds.empty()) {
		SyncAffectedSourceWidthsToBackups(backupAffectedSourceIds, states);
		LocalizeVisibleFragmentMetadataFromBackup(backupAffectedSourceIds, true);
	}

	for (size_t i = 0; i < states.size(); ++i) {
		EmorySourceState& state = states[i];
		if (affectedSourceIds.find(state.sourceId) == affectedSourceIds.end() || !state.art || state.widths.empty()) {
			continue;
		}

		if (state.touched && backupAffectedSourceIds.find(state.sourceId) == backupAffectedSourceIds.end()) {
			WriteSegmentWidths(state.art, state.widths);
			if (state.hasExplicitStart) {
				WriteStartSegmentIndex(state.art, state.startSegmentIndex);
			} else {
				ClearStartSegmentIndex(state.art);
			}
			int storedIndex = state.startSegmentIndex;
			if (storedIndex < 0 || storedIndex >= static_cast<int>(state.widths.size())) {
				storedIndex = 0;
			}
			WriteStoredSourceBodyWidth(state.art, state.widths[storedIndex]);
		}
	}

	// PERF: Reuse the existing states with updated widths instead of calling
	// CollectEmorySourceStates a second time (was a full document scan).
	// Re-map the selected segments using the already-collected states.
	std::map<int, std::vector<int> > finalSelectedIndicesByState = selectedIndicesByState;

	for (size_t i = 0; i < states.size(); ++i) {
		EmorySourceState& state = states[i];
		if (affectedSourceIds.find(state.sourceId) == affectedSourceIds.end() || !state.art || state.widths.empty()) {
			continue;
		}
		sourceIds.push_back(state.sourceId);
		regeneratePaths.push_back(state.path);
	}

	if (sourceIds.empty() || regeneratePaths.empty()) {
		outMessage = "No Emory runs were updated from the current selection.";
		return false;
	}

	std::sort(sourceIds.begin(), sourceIds.end());
	sourceIds.erase(std::unique(sourceIds.begin(), sourceIds.end()), sourceIds.end());
	std::vector<std::string> deleteSourceIdList(deleteSourceIds.begin(), deleteSourceIds.end());

	auto t0 = std::chrono::steady_clock::now();
	DeleteGeneratedEmoryBodies(deleteSourceIdList);
	auto t1 = std::chrono::steady_clock::now();
	EmoryBodyStats stats;
	{
		ScopedWidthApplyProtectedSourceIds protectSelectedSources(selectedSourceIds);
		stats = GenerateEmoryBodies(regeneratePaths);
	}
	auto t2 = std::chrono::steady_clock::now();
	SelectGeneratedSegmentsByStateMap(states, finalSelectedIndicesByState);
	auto t3 = std::chrono::steady_clock::now();

	{
		const auto deleteMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
		const auto generateMs = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
		const auto selectMs = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
		std::ostringstream perf;
		perf << "[PERF] Width-apply: delete=" << deleteMs << "ms generate=" << generateMs
			<< "ms select=" << selectMs << "ms paths=" << regeneratePaths.size()
			<< " segments=" << stats.segmentsCreated << " connectors=" << stats.connectorsCreated;
		DuctworkLog::Write(perf.str());
	}

	std::ostringstream message;
	message << "Updated " << totalSelectedCount << " segment";
	if (totalSelectedCount != 1) {
		message << "s";
	}
	message << " to width " << newWidth;
	message
		<< ". Rebuilt " << stats.segmentsCreated << " segments and "
		<< stats.connectorsCreated << " connectors.";
	outMessage = message.str();
	return true;
}

bool DuctworkGeometry::ApplySelectedEmoryStrokeWidth(double newWidth, std::string& outMessage, bool includeThermostatLines)
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
	if (includeThermostatLines) {
		CollectSelectedThermostatLineArts(selectedThermostatArts);
	}

	if (sourceIds.empty() && selectedThermostatArts.empty()) {
		return false;
	}

	size_t updatedRuns = 0;
	if (!sourceIds.empty()) {
		std::vector<AIArtHandle> allPaths;
		CollectAllLineLayerPaths(allPaths);
		for (std::set<std::string>::const_iterator it = sourceIds.begin(); it != sourceIds.end(); ++it) {
			WriteStoredSourceStrokeWidthForSourceId(*it, newWidth);
			WriteStoredSourceStrokeExplicitForSourceId(*it, true);
			++updatedRuns;
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
