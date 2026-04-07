#include "IllustratorSDK.h"
#include "ProcessDuctworkOrtho.h"
#include "ProcessDuctworkConnections.h"
#include "ProcessDuctworkLayers.h"
#include "ProcessDuctworkMath.h"
#include "ProcessDuctworkNotes.h"
#include "ProcessDuctworkMetadata.h"
#include "ProcessDuctworkSuites.h"

#include <array>
#include <cstdint>
#include <cmath>
#include <set>

namespace
{
	struct PathBounds
	{
		double minX = 0.0;
		double minY = 0.0;
		double maxX = 0.0;
		double maxY = 0.0;
		bool valid = false;
	};

	struct UnsafeConnectionKey
	{
		uintptr_t artA = 0;
		uintptr_t artB = 0;
		int type = 0;

		bool operator<(const UnsafeConnectionKey& other) const
		{
			if (artA != other.artA) return artA < other.artA;
			if (artB != other.artB) return artB < other.artB;
			return type < other.type;
		}
	};

	struct UnsafeConnectionSummary
	{
		int count = 0;
		std::set<UnsafeConnectionKey> keys;
	};

	PathBounds ComputePathBounds(const DuctworkPath& path)
	{
		PathBounds bounds;
		if (path.points.empty()) {
			return bounds;
		}

		bounds.minX = bounds.maxX = path.points.front().x;
		bounds.minY = bounds.maxY = path.points.front().y;
		bounds.valid = true;
		for (size_t i = 1; i < path.points.size(); ++i) {
			const DuctworkPoint& point = path.points[i];
			if (point.x < bounds.minX) bounds.minX = point.x;
			if (point.x > bounds.maxX) bounds.maxX = point.x;
			if (point.y < bounds.minY) bounds.minY = point.y;
			if (point.y > bounds.maxY) bounds.maxY = point.y;
		}
		return bounds;
	}

	PathBounds ComputeCombinedBounds(const std::vector<DuctworkPath>& paths)
	{
		PathBounds combined;
		for (size_t i = 0; i < paths.size(); ++i) {
			const PathBounds bounds = ComputePathBounds(paths[i]);
			if (!bounds.valid) {
				continue;
			}
			if (!combined.valid) {
				combined = bounds;
				continue;
			}
			if (bounds.minX < combined.minX) combined.minX = bounds.minX;
			if (bounds.minY < combined.minY) combined.minY = bounds.minY;
			if (bounds.maxX > combined.maxX) combined.maxX = bounds.maxX;
			if (bounds.maxY > combined.maxY) combined.maxY = bounds.maxY;
		}
		return combined;
	}

	bool BoundsOverlap(const PathBounds& a, const PathBounds& b, double padding)
	{
		if (!a.valid || !b.valid) {
			return false;
		}
		return !(a.maxX + padding < b.minX ||
			b.maxX + padding < a.minX ||
			a.maxY + padding < b.minY ||
			b.maxY + padding < a.minY);
	}

	bool SegmentsOverlapCollinear(const DuctworkPoint& a1, const DuctworkPoint& a2,
		const DuctworkPoint& b1, const DuctworkPoint& b2,
		double distTolerance, double angleToleranceDeg)
	{
		const double ax = a2.x - a1.x;
		const double ay = a2.y - a1.y;
		const double bx = b2.x - b1.x;
		const double by = b2.y - b1.y;
		const double aLen = std::sqrt(ax * ax + ay * ay);
		const double bLen = std::sqrt(bx * bx + by * by);
		if (aLen < 1e-6 || bLen < 1e-6) {
			return false;
		}

		const double dot = (ax * bx + ay * by) / (aLen * bLen);
		const double cosTol = std::cos(angleToleranceDeg * (3.14159265358979323846 / 180.0));
		if (std::fabs(dot) < cosTol) {
			return false;
		}

		auto distancePointToLine = [](const DuctworkPoint& p, const DuctworkPoint& a, const DuctworkPoint& b) {
			double t = 0.0;
			const DuctworkPoint closest = DuctworkMath::ClosestPointOnSegment(a, b, p, t);
			return std::sqrt(DuctworkMath::Dist2(p, closest));
		};

		if (distancePointToLine(b1, a1, a2) > distTolerance &&
			distancePointToLine(b2, a1, a2) > distTolerance) {
			return false;
		}

		const double dirX = ax / aLen;
		const double dirY = ay / aLen;
		const double aStart = 0.0;
		const double aEnd = aLen;
		const double bStart = (b1.x - a1.x) * dirX + (b1.y - a1.y) * dirY;
		const double bEnd = (b2.x - a1.x) * dirX + (b2.y - a1.y) * dirY;
		const double bMin = (bStart < bEnd) ? bStart : bEnd;
		const double bMax = (bStart > bEnd) ? bStart : bEnd;
		const double overlapStart = (aStart > bMin) ? aStart : bMin;
		const double overlapEnd = (aEnd < bMax) ? aEnd : bMax;
		return overlapEnd >= overlapStart;
	}

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

	static bool AreAdjacentSegments(size_t aSeg, size_t bSeg, size_t segmentCount, bool closed)
	{
		if (aSeg == bSeg) {
			return true;
		}
		if ((aSeg + 1 == bSeg) || (bSeg + 1 == aSeg)) {
			return true;
		}
		if (closed && segmentCount > 2) {
			if ((aSeg == 0 && bSeg + 1 == segmentCount) ||
				(bSeg == 0 && aSeg + 1 == segmentCount)) {
				return true;
			}
		}
		return false;
	}

	static bool PathHasNonAdjacentSelfIntersection(const DuctworkPath& path)
	{
		const size_t pointCount = path.points.size();
		if (pointCount < 4) {
			return false;
		}

		const size_t segmentCount = path.closed ? pointCount : (pointCount - 1);
		const double overlapDistTolerance = 3.0;
		const double overlapAngleTolerance = 5.0;
		for (size_t aSeg = 0; aSeg < segmentCount; ++aSeg) {
			const DuctworkPoint& a1 = path.points[aSeg];
			const DuctworkPoint& a2 = path.points[(aSeg + 1) % pointCount];
			for (size_t bSeg = aSeg + 1; bSeg < segmentCount; ++bSeg) {
				if (AreAdjacentSegments(aSeg, bSeg, segmentCount, path.closed)) {
					continue;
				}

				const DuctworkPoint& b1 = path.points[bSeg];
				const DuctworkPoint& b2 = path.points[(bSeg + 1) % pointCount];
				if (SegmentsOverlapCollinear(a1, a2, b1, b2, overlapDistTolerance, overlapAngleTolerance)) {
					return true;
				}

				DuctworkPoint intersection;
				if (DuctworkMath::SegmentIntersection(a1, a2, b1, b2, intersection)) {
					return true;
				}
			}
		}

		return false;
	}

	static UnsafeConnectionSummary SummarizeUnsafeConnectionsWithContext(const std::vector<DuctworkPath>& selectedPaths,
		const std::vector<DuctworkPath>& documentContextPaths)
	{
		UnsafeConnectionSummary summary;
		if (selectedPaths.empty()) {
			return summary;
		}

		const double kContextPadding = 20.0;
		const size_t selectedCount = selectedPaths.size();
		const PathBounds selectedBounds = ComputeCombinedBounds(selectedPaths);
		std::set<std::string> selectedLayers;
		for (size_t i = 0; i < selectedPaths.size(); ++i) {
			if (!selectedPaths[i].layerName.empty()) {
				selectedLayers.insert(selectedPaths[i].layerName);
			}
		}

		std::vector<DuctworkPath> combinedPaths;
		combinedPaths.reserve(selectedPaths.size() + documentContextPaths.size());
		combinedPaths.insert(combinedPaths.end(), selectedPaths.begin(), selectedPaths.end());
		for (size_t i = 0; i < documentContextPaths.size(); ++i) {
			const DuctworkPath& contextPath = documentContextPaths[i];
			if (selectedLayers.find(contextPath.layerName) == selectedLayers.end()) {
				continue;
			}
			if (!BoundsOverlap(selectedBounds, ComputePathBounds(contextPath), kContextPadding)) {
				continue;
			}
			combinedPaths.push_back(contextPath);
		}

		std::vector<DuctworkConnection> connections;
		DuctworkConnections::FindConnections(
			combinedPaths,
			2.0,
			3.0,
			15.0,
			10.0,
			true,
			connections);

		for (size_t i = 0; i < connections.size(); ++i) {
			const DuctworkConnection& connection = connections[i];
			const bool touchesSelection =
				(connection.a >= 0 && connection.a < static_cast<int>(selectedCount)) ||
				(connection.b >= 0 && connection.b < static_cast<int>(selectedCount));
			if (!touchesSelection) {
				continue;
			}
			if (connection.type == kConnectionEndpointToSegment ||
				connection.type == kConnectionSegmentIntersection) {
				++summary.count;
				uintptr_t artA = 0;
				uintptr_t artB = 0;
				if (connection.a >= 0 && connection.a < static_cast<int>(combinedPaths.size())) {
					artA = reinterpret_cast<uintptr_t>(combinedPaths[static_cast<size_t>(connection.a)].art);
				}
				if (connection.b >= 0 && connection.b < static_cast<int>(combinedPaths.size())) {
					artB = reinterpret_cast<uintptr_t>(combinedPaths[static_cast<size_t>(connection.b)].art);
				}
				if (artB < artA) {
					const uintptr_t swapArt = artA;
					artA = artB;
					artB = swapArt;
				}
				UnsafeConnectionKey key;
				key.artA = artA;
				key.artB = artB;
				key.type = static_cast<int>(connection.type);
				summary.keys.insert(key);
			}
		}
		return summary;
	}

	static bool ValidateCandidatePaths(const std::vector<DuctworkPath>& candidatePaths,
		const std::vector<DuctworkPath>& documentContextPaths,
		const UnsafeConnectionSummary& currentUnsafeConnections,
		int dirtyIndexA,
		int dirtyIndexB,
		UnsafeConnectionSummary& outCandidateUnsafeConnections)
	{
		const int dirtyIndices[2] = { dirtyIndexA, dirtyIndexB };
		for (int idx = 0; idx < 2; ++idx) {
			const int dirtyIndex = dirtyIndices[idx];
			if (dirtyIndex < 0 || dirtyIndex >= static_cast<int>(candidatePaths.size())) {
				continue;
			}
			if (PathHasNonAdjacentSelfIntersection(candidatePaths[static_cast<size_t>(dirtyIndex)])) {
				return false;
			}
		}

		outCandidateUnsafeConnections =
			SummarizeUnsafeConnectionsWithContext(candidatePaths, documentContextPaths);
		if (outCandidateUnsafeConnections.count > currentUnsafeConnections.count) {
			return false;
		}
		for (std::set<UnsafeConnectionKey>::const_iterator it = outCandidateUnsafeConnections.keys.begin();
			it != outCandidateUnsafeConnections.keys.end();
			++it) {
			if (currentUnsafeConnections.keys.find(*it) == currentUnsafeConnections.keys.end()) {
				return false;
			}
		}
		return true;
	}
}

OrthoResult DuctworkOrtho::ApplyToPaths(std::vector<DuctworkPath>& paths, double snapThresholdDegrees,
	bool hasRotationOverride, double rotationOverrideDegrees,
	const std::vector<DuctworkConnection>& preConnections,
	const std::vector<DuctworkPath>& documentContextPaths,
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
	UnsafeConnectionSummary currentUnsafeConnections =
		SummarizeUnsafeConnectionsWithContext(paths, documentContextPaths);

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

		const DuctworkPoint& aPt = paths[conn.a].points[static_cast<size_t>(conn.endpointA)];
		const DuctworkPoint& bPt = paths[conn.b].points[static_cast<size_t>(conn.endpointB)];
		DuctworkPoint merged = {};
		merged.x = (aPt.x + bPt.x) * 0.5;
		merged.y = (aPt.y + bPt.y) * 0.5;
		std::vector<DuctworkPath> candidatePaths(paths);
		candidatePaths[conn.a].points[static_cast<size_t>(conn.endpointA)] = merged;
		candidatePaths[conn.b].points[static_cast<size_t>(conn.endpointB)] = merged;
		UnsafeConnectionSummary candidateUnsafeConnections;
		if (!ValidateCandidatePaths(candidatePaths,
			documentContextPaths,
			currentUnsafeConnections,
			conn.a,
			conn.b,
			candidateUnsafeConnections)) {
			continue;
		}
		paths.swap(candidatePaths);
		currentUnsafeConnections = candidateUnsafeConnections;
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
			std::vector<DuctworkPath> candidatePaths(paths);
			candidatePaths[i].points[moveIndex].x = newX;
			candidatePaths[i].points[moveIndex].y = newY;

			UnsafeConnectionSummary candidateUnsafeConnections;
			if (!ValidateCandidatePaths(candidatePaths,
				documentContextPaths,
				currentUnsafeConnections,
				static_cast<int>(i),
				-1,
				candidateUnsafeConnections)) {
				continue;
			}

			paths.swap(candidatePaths);
			currentUnsafeConnections = candidateUnsafeConnections;
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
			std::vector<DuctworkPath> candidatePaths(paths);
			candidatePaths[conn.a].points[static_cast<size_t>(conn.endpointA)] = merged;
			candidatePaths[conn.b].points[static_cast<size_t>(conn.endpointB)] = merged;
			UnsafeConnectionSummary candidateUnsafeConnections;
			if (!ValidateCandidatePaths(candidatePaths,
				documentContextPaths,
				currentUnsafeConnections,
				conn.a,
				conn.b,
				candidateUnsafeConnections)) {
				continue;
			}
			paths.swap(candidatePaths);
			currentUnsafeConnections = candidateUnsafeConnections;
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
			std::vector<DuctworkPath> candidatePaths(paths);
			candidatePaths[endpointPath].points[static_cast<size_t>(endpointIndex)] = projected;
			UnsafeConnectionSummary candidateUnsafeConnections;
			if (!ValidateCandidatePaths(candidatePaths,
				documentContextPaths,
				currentUnsafeConnections,
				endpointPath,
				-1,
				candidateUnsafeConnections)) {
				continue;
			}
			paths.swap(candidatePaths);
			currentUnsafeConnections = candidateUnsafeConnections;
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
			AIRealPoint oldAnchor = segments[segIndex].p;
			AIReal dx = static_cast<AIReal>(pt.x) - oldAnchor.h;
			AIReal dy = static_cast<AIReal>(pt.y) - oldAnchor.v;
			segments[segIndex].p.h = static_cast<AIReal>(pt.x);
			segments[segIndex].p.v = static_cast<AIReal>(pt.y);
			if (eligible[i]) {
				segments[segIndex].in = segments[segIndex].p;
				segments[segIndex].out = segments[segIndex].p;
				segments[segIndex].corner = true;
			} else {
				segments[segIndex].in.h += dx;
				segments[segIndex].in.v += dy;
				segments[segIndex].out.h += dx;
				segments[segIndex].out.v += dy;
			}
		}
		if (!sAIPath->SetPathSegments(art, 0, count, &segments[0])) {
			++result.pathsTouched;
		}
	}

	return result;
}
