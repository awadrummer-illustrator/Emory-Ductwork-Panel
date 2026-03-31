#include "IllustratorSDK.h"
#include "ProcessDuctworkOrtho.h"
#include "ProcessDuctworkConnections.h"
#include "ProcessDuctworkLayers.h"
#include "ProcessDuctworkMath.h"
#include "ProcessDuctworkNotes.h"
#include "ProcessDuctworkMetadata.h"
#include "ProcessDuctworkSuites.h"

#include <array>
#include <cmath>

namespace
{
	bool IsUnitPairLayerName(const std::string& a, const std::string& b)
	{
		if (a == b) {
			return false;
		}
		if ((a == "Green Ductwork" && b == "Blue Ductwork") ||
			(a == "Blue Ductwork" && b == "Green Ductwork")) {
			return true;
		}
		if ((a == "Light Green Ductwork" && b == "Blue Ductwork") ||
			(a == "Blue Ductwork" && b == "Light Green Ductwork")) {
			return true;
		}
		if ((a == "Orange Ductwork" && b == "Light Orange Ductwork") ||
			(a == "Light Orange Ductwork" && b == "Orange Ductwork")) {
			return true;
		}
		if ((a == "Thermostat Lines" && b == "Blue Ductwork") ||
			(a == "Blue Ductwork" && b == "Thermostat Lines")) {
			return true;
		}
		if ((a == "Thermostat Lines" && b == "Green Ductwork") ||
			(a == "Green Ductwork" && b == "Thermostat Lines")) {
			return true;
		}
		if ((a == "Thermostat Lines" && b == "Light Green Ductwork") ||
			(a == "Light Green Ductwork" && b == "Thermostat Lines")) {
			return true;
		}
		return false;
	}

	static bool HasNoOrthoTag(AIArtHandle art)
	{
		std::string note = DuctworkNotes::GetNote(art);
		if (note.empty()) {
			return false;
		}
		return note.find("MD:NO_ORTHO") != std::string::npos;
	}

	static double DegreesToRadians(double degrees)
	{
		return degrees * 3.14159265358979323846 / 180.0;
	}

	static double NormalizeOrientation(double angle)
	{
		angle = std::fmod(angle, 180.0);
		if (angle < 0.0) {
			angle += 180.0;
		}
		return angle;
	}

	static double AngularDistance(double a, double b)
	{
		const double diff = std::fabs(a - b);
		return (diff < 90.0) ? diff : (180.0 - diff);
	}

	static DuctworkPoint ClosestPointOnSegment(const DuctworkPoint& a, const DuctworkPoint& b, const DuctworkPoint& p)
	{
		double t = 0.0;
		return DuctworkMath::ClosestPointOnSegment(a, b, p, t);
	}
}

OrthoResult DuctworkOrtho::ApplyToPaths(std::vector<DuctworkPath>& paths, double snapThresholdDegrees,
	bool hasRotationOverride, double rotationOverrideDegrees,
	const std::vector<DuctworkConnection>& preConnections,
	bool skipAllBranchSegments,
	bool skipFinalRegisterSegment)
{
	OrthoResult result = {};
	if (!sAIPath) {
		return result;
	}

	const double baseAngle = hasRotationOverride ? rotationOverrideDegrees : 0.0;
	const double snapThreshold = hasRotationOverride ? 180.0 : snapThresholdDegrees;
	const double gridAnglesRaw[4] = { baseAngle, baseAngle + 90.0, baseAngle + 45.0, baseAngle - 45.0 };
	double gridOrientations[4] = {};
	for (int i = 0; i < 4; ++i) {
		gridOrientations[i] = NormalizeOrientation(gridAnglesRaw[i]);
	}

	std::vector<bool> touched(paths.size(), false);
	std::vector<bool> eligible(paths.size(), false);
	std::vector<std::array<bool, 2>> endpointConnected(paths.size(), { false, false });
	std::vector<std::array<bool, 2>> endpointConnectedEndpoint(paths.size(), { false, false });

	if (skipFinalRegisterSegment || skipAllBranchSegments) {
		for (size_t i = 0; i < preConnections.size(); ++i) {
			const DuctworkConnection& conn = preConnections[i];
			if (conn.a >= 0 && conn.a < static_cast<int>(paths.size()) && conn.endpointA >= 0) {
				const size_t endIdx = paths[conn.a].points.size() > 0 ? paths[conn.a].points.size() - 1 : 0;
				if (conn.endpointA == 0) {
					endpointConnected[conn.a][0] = true;
					if (conn.type == kConnectionEndpointToEndpoint) {
						endpointConnectedEndpoint[conn.a][0] = true;
					}
				} else if (conn.endpointA == static_cast<int>(endIdx)) {
					endpointConnected[conn.a][1] = true;
					if (conn.type == kConnectionEndpointToEndpoint) {
						endpointConnectedEndpoint[conn.a][1] = true;
					}
				}
			}
			if (conn.b >= 0 && conn.b < static_cast<int>(paths.size()) && conn.endpointB >= 0) {
				const size_t endIdx = paths[conn.b].points.size() > 0 ? paths[conn.b].points.size() - 1 : 0;
				if (conn.endpointB == 0) {
					endpointConnected[conn.b][0] = true;
					if (conn.type == kConnectionEndpointToEndpoint) {
						endpointConnectedEndpoint[conn.b][0] = true;
					}
				} else if (conn.endpointB == static_cast<int>(endIdx)) {
					endpointConnected[conn.b][1] = true;
					if (conn.type == kConnectionEndpointToEndpoint) {
						endpointConnectedEndpoint[conn.b][1] = true;
					}
				}
			}
		}
	}

	// PRE-ORTHO: Merge cross-layer endpoint-to-endpoint connections FIRST
	// to establish junction points. Ortho then works outward from these fixed points.
	// Track which path endpoints are cross-layer junctions so ortho processes outward.
	std::vector<int> junctionEndpoint(paths.size(), -1); // -1=none, 0=first pt, 1=last pt
	for (size_t i = 0; i < preConnections.size(); ++i) {
		const DuctworkConnection& conn = preConnections[i];
		if (conn.type != kConnectionEndpointToEndpoint) continue;
		if (conn.a < 0 || conn.b < 0 ||
			conn.a >= static_cast<int>(paths.size()) ||
			conn.b >= static_cast<int>(paths.size())) continue;
		if (conn.endpointA < 0 || conn.endpointB < 0) continue;
		const std::string& layerA = paths[conn.a].layerName;
		const std::string& layerB = paths[conn.b].layerName;
		if (layerA == layerB) continue; // same-layer handled post-ortho

		DuctworkPoint& aPt = paths[conn.a].points[static_cast<size_t>(conn.endpointA)];
		DuctworkPoint& bPt = paths[conn.b].points[static_cast<size_t>(conn.endpointB)];
		DuctworkPoint merged = {};
		merged.x = (aPt.x + bPt.x) * 0.5;
		merged.y = (aPt.y + bPt.y) * 0.5;
		aPt = merged;
		bPt = merged;
		touched[conn.a] = true;
		touched[conn.b] = true;

		// Record which endpoint is the junction for each path
		const size_t endIdxA = paths[conn.a].points.size() > 0 ? paths[conn.a].points.size() - 1 : 0;
		const size_t endIdxB = paths[conn.b].points.size() > 0 ? paths[conn.b].points.size() - 1 : 0;
		junctionEndpoint[conn.a] = (conn.endpointA == 0) ? 0 : 1;
		junctionEndpoint[conn.b] = (conn.endpointB == 0) ? 0 : 1;
	}

	for (size_t i = 0; i < paths.size(); ++i) {
		AIArtHandle art = paths[i].art;
		if (!art) {
			continue;
		}
		if (!DuctworkLayers::IsOrthoEligibleLineLayerName(paths[i].layerName)) {
			continue;
		}
		if (HasNoOrthoTag(art)) {
			continue;
		}

		if (paths[i].points.size() < 2) {
			continue;
		}

		eligible[i] = true;
		bool pathIsBranch = false;
		bool roleFromMeta = false;
		if (skipAllBranchSegments || skipFinalRegisterSegment) {
			std::string ductRole;
			if (DuctworkMetadata::GetString(art, "ductRole", ductRole)) {
				if (ductRole == "branch") {
					pathIsBranch = true;
					roleFromMeta = true;
				} else if (ductRole == "trunk") {
					pathIsBranch = false;
					roleFromMeta = true;
				}
			}
			if (!roleFromMeta) {
				const bool connected0 = endpointConnectedEndpoint[i][0];
				const bool connected1 = endpointConnectedEndpoint[i][1];
				pathIsBranch = !connected0 && !connected1;
			}
		}
		bool skipBranch = false;
		if (skipAllBranchSegments) {
			skipBranch = pathIsBranch;
		}
		const bool closed = paths[i].closed;
		const size_t count = paths[i].points.size();
		const size_t limit = closed ? count : (count - 1);

		// If junction is at the LAST point, process segments in reverse so the
		// junction stays fixed and ortho propagates outward from it.
		const bool reverseOrtho = (!closed && junctionEndpoint[i] == 1);

		for (size_t step = 0; step < limit; ++step) {
			if (skipBranch) {
				continue;
			}
			// Forward: segIndex 0,1,2,...  Reverse: segIndex limit-1, limit-2, ...
			const size_t segIndex = reverseOrtho ? (limit - 1 - step) : step;

			if (skipFinalRegisterSegment && !closed && pathIsBranch) {
				const bool isFirstSegment = (segIndex == 0);
				const bool isLastSegment = (segIndex + 1 == count - 1);
				if ((isFirstSegment && !endpointConnected[i][0]) ||
					(isLastSegment && !endpointConnected[i][1])) {
					continue;
				}
			}

			size_t anchorIndex, moveIndex;
			if (reverseOrtho) {
				// Reverse: keep the far end (segIndex+1) fixed, move segIndex
				anchorIndex = (segIndex + 1) % count;
				moveIndex = segIndex;
			} else {
				// Forward: keep segIndex fixed, move segIndex+1
				anchorIndex = segIndex;
				moveIndex = (segIndex + 1) % count;
			}
			const DuctworkPoint& p1 = paths[i].points[anchorIndex];
			const DuctworkPoint& p2 = paths[i].points[moveIndex];

			const double dx = p2.x - p1.x;
			const double dyLocal = (p2.y - p1.y);
			const double length = std::sqrt(dx * dx + dyLocal * dyLocal);
			if (length < 1e-3) {
				continue;
			}

			const double angle = std::atan2(dyLocal, dx) * 180.0 / 3.14159265358979323846;
			const double orientation = NormalizeOrientation(angle);
			double minDist = 180.0;
			int closestIdx = 0;
			for (int g = 0; g < 4; ++g) {
				const double dist = AngularDistance(orientation, gridOrientations[g]);
				if (dist < minDist) {
					minDist = dist;
					closestIdx = g;
				}
			}
			if (minDist > snapThreshold) {
				continue;
			}

			const double targetAngle = gridAnglesRaw[closestIdx];
			const double newRad = DegreesToRadians(targetAngle);
			double unitX = std::cos(newRad);
			double unitY = std::sin(newRad);
			const double dot = dx * unitX + dyLocal * unitY;
			if (dot < 0.0) {
				unitX = -unitX;
				unitY = -unitY;
			}

			const double newX = p1.x + unitX * length;
			const double newY = p1.y + unitY * length;

			paths[i].points[moveIndex].x = newX;
			paths[i].points[moveIndex].y = newY;
			++result.segmentsSnapped;
			touched[i] = true;
		}
	}

	// POST-ORTHO: Merge same-layer endpoint-to-endpoint connections
	for (size_t i = 0; i < preConnections.size(); ++i) {
		const DuctworkConnection& conn = preConnections[i];
		if (conn.a < 0 || conn.b < 0 ||
			conn.a >= static_cast<int>(paths.size()) ||
			conn.b >= static_cast<int>(paths.size())) {
			continue;
		}
		if (!eligible[conn.a] || !eligible[conn.b]) {
			continue;
		}

		if (conn.type == kConnectionEndpointToEndpoint) {
			if (conn.endpointA < 0 || conn.endpointB < 0) {
				continue;
			}
			// Skip cross-layer (already handled pre-ortho)
			const std::string& layerA = paths[conn.a].layerName;
			const std::string& layerB = paths[conn.b].layerName;
			if (layerA != layerB) {
				continue;
			}

			// Same-layer: average the endpoints
			const DuctworkPoint& aPt = paths[conn.a].points[static_cast<size_t>(conn.endpointA)];
			const DuctworkPoint& bPt = paths[conn.b].points[static_cast<size_t>(conn.endpointB)];
			DuctworkPoint merged = {};
			merged.x = (aPt.x + bPt.x) * 0.5;
			merged.y = (aPt.y + bPt.y) * 0.5;
			paths[conn.a].points[static_cast<size_t>(conn.endpointA)] = merged;
			paths[conn.b].points[static_cast<size_t>(conn.endpointB)] = merged;
			touched[conn.a] = true;
			touched[conn.b] = true;
			continue;
		}

		if (conn.type == kConnectionEndpointToSegment) {
			int endpointPath = -1;
			int endpointIndex = -1;
			int segmentPath = -1;
			int segmentIndex = -1;
			if (conn.endpointA >= 0) {
				endpointPath = conn.a;
				endpointIndex = conn.endpointA;
				segmentPath = conn.b;
				segmentIndex = conn.segB;
			} else if (conn.endpointB >= 0) {
				endpointPath = conn.b;
				endpointIndex = conn.endpointB;
				segmentPath = conn.a;
				segmentIndex = conn.segA;
			}
			if (endpointPath < 0 || segmentPath < 0 || segmentIndex < 0) {
				continue;
			}
			std::vector<DuctworkPoint>& segmentPoints = paths[segmentPath].points;
			if (segmentIndex + 1 >= static_cast<int>(segmentPoints.size())) {
				continue;
			}
			const DuctworkPoint& segA = segmentPoints[static_cast<size_t>(segmentIndex)];
			const DuctworkPoint& segB = segmentPoints[static_cast<size_t>(segmentIndex + 1)];
			const DuctworkPoint& endpoint = paths[endpointPath].points[static_cast<size_t>(endpointIndex)];
			const DuctworkPoint projected = ClosestPointOnSegment(segA, segB, endpoint);
			paths[endpointPath].points[static_cast<size_t>(endpointIndex)] = projected;
			touched[endpointPath] = true;
		}
	}

	for (size_t i = 0; i < paths.size(); ++i) {
		if (!touched[i]) {
			continue;
		}
		AIArtHandle art = paths[i].art;
		if (!art) {
			continue;
		}
		const size_t pointCount = paths[i].points.size();
		if (pointCount < 2) {
			continue;
		}
		ai::int16 count = 0;
		if (sAIPath->GetPathSegmentCount(art, &count) || count != static_cast<ai::int16>(pointCount)) {
			continue;
		}
		std::vector<AIPathSegment> segments(static_cast<size_t>(count));
		if (sAIPath->GetPathSegments(art, 0, count, &segments[0])) {
			continue;
		}
		for (ai::int16 segIndex = 0; segIndex < count; ++segIndex) {
			const DuctworkPoint& pt = paths[i].points[static_cast<size_t>(segIndex)];
			segments[segIndex].p.h = static_cast<AIReal>(pt.x);
			segments[segIndex].p.v = static_cast<AIReal>(pt.y);
			segments[segIndex].in = segments[segIndex].p;
			segments[segIndex].out = segments[segIndex].p;
			segments[segIndex].corner = true;
		}
		if (!sAIPath->SetPathSegments(art, 0, count, &segments[0])) {
			++result.pathsTouched;
		}
	}

	return result;
}
