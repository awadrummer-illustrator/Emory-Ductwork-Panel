#include "IllustratorSDK.h"
#include "ProcessDuctworkCarve.h"
#include "ProcessDuctworkArt.h"
#include "ProcessDuctworkConnections.h"
#include "ProcessDuctworkConstants.h"
#include "ProcessDuctworkGeometry.h"
#include "ProcessDuctworkLayers.h"
#include "ProcessDuctworkLog.h"
#include "ProcessDuctworkMath.h"
#include "ProcessDuctworkSuites.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace
{
	constexpr double kRegisterCarveHalfWidth = 13.5;
	constexpr double kRegisterDetectionThreshold = 10.0;
	constexpr double kRegisterEndpointTol = 5.0;
	constexpr double kOverlapPointTol = 1.0;
	constexpr double kOverlapEndpointTol = 6.0;
	constexpr double kOverlapSegmentTMin = 0.1;
	constexpr double kGapToolHitTol = 8.0;
	constexpr double kGapToolPreviewTol = 120.0;
	constexpr double kGapToolPreviewTolView = 60.0;
	constexpr double kGapToolEndpointTol = 10.0;
	constexpr double kGapToolCollinearTolDeg = 7.0;
	constexpr double kGapToolMaxGap = 40.0;

	struct GapEndpoint
	{
		AIArtHandle art = nullptr;
		size_t index = 0;
		DuctworkPoint point{};
		DuctworkPoint neighbor{};
		double dist = 0.0;
	};

	struct GapIntersection
	{
		bool valid = false;
		bool isRegister = false;
		DuctworkPoint point{};
		AIArtHandle artA = nullptr;
		AIArtHandle artB = nullptr;
		int segA = -1;
		int segB = -1;
		double dist = 0.0;
		std::string layerName;
	};

	const char* GetRegisterLayerForDuctwork(const std::string& ductLayer)
	{
		if (ductLayer == "Blue Ductwork") {
			return "Square Registers";
		}
		if (ductLayer == "Green Ductwork" ||
			ductLayer == "Light Green Ductwork" ||
			ductLayer == "Orange Ductwork" ||
			ductLayer == "Light Orange Ductwork") {
			return "Exhaust Registers";
		}
		return "Exhaust Registers";
	}

	bool GetSegmentViewDistance(AIDocumentViewHandle view,
		const DuctworkPoint& a,
		const DuctworkPoint& b,
		const AIRealPoint& viewClick,
		double& outDist)
	{
		if (!view || !sAIDocumentView) {
			return false;
		}
		AIRealPoint artA{};
		AIRealPoint artB{};
		artA.h = static_cast<AIReal>(a.x);
		artA.v = static_cast<AIReal>(a.y);
		artB.h = static_cast<AIReal>(b.x);
		artB.v = static_cast<AIReal>(b.y);
		AIPoint viewA{};
		AIPoint viewB{};
		if (sAIDocumentView->ArtworkPointToViewPoint(view, &artA, &viewA) ||
			sAIDocumentView->ArtworkPointToViewPoint(view, &artB, &viewB)) {
			return false;
		}
		const DuctworkPoint vA{ static_cast<double>(viewA.h), static_cast<double>(viewA.v) };
		const DuctworkPoint vB{ static_cast<double>(viewB.h), static_cast<double>(viewB.v) };
		const DuctworkPoint vClick{ static_cast<double>(viewClick.h), static_cast<double>(viewClick.v) };
		double t = 0.0;
		const DuctworkPoint closest = DuctworkMath::ClosestPointOnSegment(vA, vB, vClick, t);
		outDist = DuctworkMath::Dist(closest, vClick);
		return true;
	}

	bool GetPathPoints(AIArtHandle art, std::vector<DuctworkPoint>& outPoints, bool& outClosed)
	{
		return DuctworkGeometry::GetPathPoints(art, outPoints, outClosed);
	}

	bool SetPathPoints(AIArtHandle art, const std::vector<DuctworkPoint>& points, bool closed)
	{
		if (!art || !sAIPath) {
			return false;
		}
		const ai::int16 count = static_cast<ai::int16>(points.size());
		if (count <= 1) {
			return false;
		}
		if (sAIPath->SetPathSegmentCount(art, count)) {
			return false;
		}
		std::vector<AIPathSegment> segments(static_cast<size_t>(count));
		for (ai::int16 i = 0; i < count; ++i) {
			segments[i].p.h = static_cast<AIReal>(points[static_cast<size_t>(i)].x);
			segments[i].p.v = static_cast<AIReal>(points[static_cast<size_t>(i)].y);
			segments[i].in = segments[i].p;
			segments[i].out = segments[i].p;
			segments[i].corner = true;
		}
		if (sAIPath->SetPathSegments(art, 0, count, &segments[0])) {
			return false;
		}
		sAIPath->SetPathClosed(art, closed ? 1 : 0);
		return true;
	}

	bool SplitPathAtGap(AIArtHandle path, size_t segIndex,
		const DuctworkPoint& cutBefore, const DuctworkPoint& cutAfter,
		AIArtHandle& outFirst, AIArtHandle& outSecond)
	{
		outFirst = nullptr;
		outSecond = nullptr;
		if (!path || !sAIArt) {
			return false;
		}
		std::vector<DuctworkPoint> points;
		bool closed = false;
		if (!GetPathPoints(path, points, closed) || closed) {
			return false;
		}
		if (segIndex + 1 >= points.size()) {
			return false;
		}

		std::vector<DuctworkPoint> first;
		first.reserve(segIndex + 2);
		for (size_t i = 0; i <= segIndex; ++i) {
			first.push_back(points[i]);
		}
		first.push_back(cutBefore);

		std::vector<DuctworkPoint> second;
		second.reserve(points.size() - segIndex);
		second.push_back(cutAfter);
		for (size_t i = segIndex + 1; i < points.size(); ++i) {
			second.push_back(points[i]);
		}

		AIArtHandle firstArt = nullptr;
		AIArtHandle secondArt = nullptr;
		if (sAIArt->DuplicateArt(path, kPlaceAbove, path, &firstArt) || !firstArt) {
			return false;
		}
		if (sAIArt->DuplicateArt(path, kPlaceAbove, path, &secondArt) || !secondArt) {
			if (firstArt) {
				sAIArt->DisposeArt(firstArt);
			}
			return false;
		}
		if (!SetPathPoints(firstArt, first, false) || !SetPathPoints(secondArt, second, false)) {
			sAIArt->DisposeArt(firstArt);
			sAIArt->DisposeArt(secondArt);
			return false;
		}
		outFirst = firstArt;
		outSecond = secondArt;
		return true;
	}

	void CollectRegisterAnchors(const std::string& layerName, std::vector<DuctworkPoint>& outPoints)
	{
		outPoints.clear();
		AILayerHandle layer = DuctworkArt::FindLayerByTitle(layerName.c_str());
		if (!layer) {
			return;
		}
		std::vector<AIArtHandle> art;
		DuctworkArt::CollectLayerArt(layer, art);
		for (size_t i = 0; i < art.size(); ++i) {
			short type = 0;
			if (sAIArt->GetArtType(art[i], &type) || type != kPathArt) {
				continue;
			}
			std::vector<DuctworkPoint> points;
			bool closed = false;
			if (!DuctworkGeometry::GetPathPoints(art[i], points, closed)) {
				continue;
			}
			if (points.size() == 1) {
				outPoints.push_back(points[0]);
			}
		}
	}

	bool PointNear(const DuctworkPoint& a, const DuctworkPoint& b, double tol)
	{
		return DuctworkMath::Dist2(a, b) <= (tol * tol);
	}

	bool FindSegmentHit(const std::vector<DuctworkPoint>& points,
		const DuctworkPoint& target,
		double tol,
		size_t& outSegIndex,
		DuctworkPoint& outClosest,
		double& outT)
	{
		for (size_t i = 0; i + 1 < points.size(); ++i) {
			double t = 0.0;
			DuctworkPoint closest = DuctworkMath::ClosestPointOnSegment(points[i], points[i + 1], target, t);
			const double dist = DuctworkMath::Dist(closest, target);
			if (t > kOverlapSegmentTMin && t < (1.0 - kOverlapSegmentTMin) && dist <= tol) {
				outSegIndex = i;
				outClosest = closest;
				outT = t;
				return true;
			}
		}
		return false;
	}

	bool IsNearSegmentEndpoint(const std::vector<DuctworkPoint>& points,
		size_t segIndex,
		const DuctworkPoint& point,
		double tol)
	{
		if (segIndex + 1 >= points.size()) {
			return true;
		}
		return PointNear(points[segIndex], point, tol) ||
			PointNear(points[segIndex + 1], point, tol);
	}

	bool IsNearPathEndpoint(const std::vector<DuctworkPoint>& points,
		const DuctworkPoint& point,
		double tol)
	{
		if (points.size() < 2) {
			return true;
		}
		return PointNear(points.front(), point, tol) ||
			PointNear(points.back(), point, tol);
	}

	bool SegmentsOverlapCollinear(const DuctworkPoint& a1, const DuctworkPoint& a2,
		const DuctworkPoint& b1, const DuctworkPoint& b2,
		double distTolerance, double angleToleranceDeg, DuctworkPoint& outMidpoint)
	{
		auto distancePointToLine = [](const DuctworkPoint& p, const DuctworkPoint& a, const DuctworkPoint& b) -> double {
			const double dx = b.x - a.x;
			const double dy = b.y - a.y;
			const double len2 = dx * dx + dy * dy;
			if (len2 < 1e-6) {
				return std::sqrt(DuctworkMath::Dist2(p, a));
			}
			const double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2;
			const double projX = a.x + t * dx;
			const double projY = a.y + t * dy;
			const double ddx = p.x - projX;
			const double ddy = p.y - projY;
			return std::sqrt(ddx * ddx + ddy * ddy);
		};

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
		const double cosTol = std::cos(angleToleranceDeg * (3.141592653589793 / 180.0));
		if (std::fabs(dot) < cosTol) {
			return false;
		}
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
		if (overlapEnd < overlapStart) {
			return false;
		}
		const double mid = (overlapStart + overlapEnd) * 0.5;
		outMidpoint.x = a1.x + dirX * mid;
		outMidpoint.y = a1.y + dirY * mid;
		return true;
	}

	double ComputeAutoGapHalfWidth(const std::vector<AIArtHandle>& selection)
	{
		if (!sAIPathStyle || !sAIArt) {
			return 4.25;
		}
		double maxWidth = 0.0;
		for (size_t i = 0; i < selection.size(); ++i) {
			short type = kUnknownArt;
			if (sAIArt->GetArtType(selection[i], &type) || type != kPathArt) {
				continue;
			}
			AIPathStyle style{};
			AIBoolean fillVisible = false;
			AIBoolean strokeVisible = false;
			if (sAIPathStyle->GetPathStyleEx(selection[i], &style, &fillVisible, &strokeVisible)) {
				continue;
			}
			if (strokeVisible && style.stroke.width > maxWidth) {
				maxWidth = style.stroke.width;
			}
		}
		if (maxWidth <= 0.0) {
			return 4.25;
		}
		return (std::max)((maxWidth + 6.0) / 2.0, 4.0);
	}

	void ReplaceSelectionArt(std::vector<AIArtHandle>& selection,
		AIArtHandle original,
		AIArtHandle first,
		AIArtHandle second)
	{
		selection.erase(std::remove(selection.begin(), selection.end(), original), selection.end());
		if (first) {
			selection.push_back(first);
		}
		if (second) {
			selection.push_back(second);
		}
	}

	std::vector<AIArtHandle> FilterSelectionByLayer(const std::vector<DuctworkPath>& paths, const std::string& layerName)
	{
		std::vector<AIArtHandle> result;
		for (size_t i = 0; i < paths.size(); ++i) {
			if (paths[i].layerName == layerName) {
				result.push_back(paths[i].art);
			}
		}
		return result;
	}

	void CollectPathArtRecursive(AIArtHandle art, std::vector<AIArtHandle>& outPaths)
	{
		if (!art || !sAIArt) {
			return;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return;
		}
		if (type == kPathArt) {
			outPaths.push_back(art);
		}
		AIArtHandle child = nullptr;
		if (!sAIArt->GetArtFirstChild(art, &child) && child) {
			AIArtHandle current = child;
			while (current) {
				CollectPathArtRecursive(current, outPaths);
				AIArtHandle next = nullptr;
				if (sAIArt->GetArtSibling(current, &next)) {
					break;
				}
				current = next;
			}
		}
	}

	bool IsRegisterLayerName(const std::string& name)
	{
		for (size_t i = 0; i < DuctworkConstants::kRegisterLayerCount; ++i) {
			if (name == DuctworkConstants::kRegisterLayers[i]) {
				return true;
			}
		}
		return false;
	}

	std::vector<std::string> BuildLineLayerList(const std::string& layerHint)
	{
		std::vector<std::string> result;
		if (DuctworkLayers::IsLineLayerName(layerHint)) {
			result.push_back(layerHint);
			return result;
		}
		for (size_t i = 0; i < DuctworkConstants::kDuctworkColorLayerCount; ++i) {
			result.push_back(DuctworkConstants::kDuctworkColorLayers[i]);
		}
		return result;
	}

	void CollectLinePathsForLayer(const std::string& layerName, std::vector<DuctworkPath>& outPaths)
	{
		AILayerHandle layer = DuctworkArt::FindLayerByTitle(layerName.c_str());
		if (!layer) {
			return;
		}
		std::vector<AIArtHandle> layerArt;
		DuctworkArt::CollectLayerArt(layer, layerArt);
		for (size_t i = 0; i < layerArt.size(); ++i) {
			std::vector<AIArtHandle> paths;
			CollectPathArtRecursive(layerArt[i], paths);
			for (size_t p = 0; p < paths.size(); ++p) {
				std::vector<DuctworkPoint> points;
				bool closed = false;
				if (!DuctworkGeometry::GetPathPoints(paths[p], points, closed)) {
					continue;
				}
				DuctworkPath entry;
				entry.art = paths[p];
				entry.points = points;
				entry.closed = closed;
				entry.layerName = layerName;
				outPaths.push_back(entry);
			}
		}
	}

	void CollectLinePathsForLayers(const std::vector<std::string>& layers, std::vector<DuctworkPath>& outPaths)
	{
		outPaths.clear();
		for (size_t i = 0; i < layers.size(); ++i) {
			CollectLinePathsForLayer(layers[i], outPaths);
		}
	}

	AIArtHandle NormalizePreferredArt(AIArtHandle preferredArt,
		const DuctworkPoint& click,
		const std::vector<DuctworkPath>& paths)
	{
		if (!preferredArt || !sAIArt) {
			return preferredArt;
		}
		short type = kUnknownArt;
		if (!sAIArt->GetArtType(preferredArt, &type) && type == kPathArt) {
			return preferredArt;
		}

		std::vector<AIArtHandle> childPaths;
		CollectPathArtRecursive(preferredArt, childPaths);
		if (childPaths.empty()) {
			return preferredArt;
		}

		std::unordered_set<AIArtHandle> childSet(childPaths.begin(), childPaths.end());
		double bestDist2 = (std::numeric_limits<double>::max)();
		AIArtHandle best = nullptr;
		for (size_t i = 0; i < paths.size(); ++i) {
			if (!paths[i].art || childSet.find(paths[i].art) == childSet.end()) {
				continue;
			}
			const std::vector<DuctworkPoint>& points = paths[i].points;
			for (size_t s = 0; s + 1 < points.size(); ++s) {
				double t = 0.0;
				DuctworkPoint closest = DuctworkMath::ClosestPointOnSegment(points[s], points[s + 1], click, t);
				const double dist2 = DuctworkMath::Dist2(closest, click);
				if (dist2 < bestDist2) {
					bestDist2 = dist2;
					best = paths[i].art;
				}
			}
		}
		return best ? best : preferredArt;
	}

	bool GetEndpointCandidate(const DuctworkPath& path, size_t index, const DuctworkPoint& intersection, GapEndpoint& out)
	{
		if (path.points.size() < 2) {
			return false;
		}
		const DuctworkPoint& pt = path.points[index];
		const double dist = DuctworkMath::Dist(pt, intersection);
		if (dist > kGapToolMaxGap) {
			return false;
		}
		const size_t neighborIndex = (index == 0) ? 1 : (index - 1);
		out.art = path.art;
		out.index = index;
		out.point = pt;
		out.neighbor = path.points[neighborIndex];
		out.dist = dist;
		return true;
	}

	bool AreCollinear(const GapEndpoint& a, const GapEndpoint& b)
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
		const double cosTol = std::cos(kGapToolCollinearTolDeg * (3.141592653589793 / 180.0));
		return dot >= cosTol;
	}

	bool FindGapEndpointPairNearClick(const std::vector<DuctworkPath>& paths,
		const DuctworkPoint& click,
		AIArtHandle preferredArt,
		GapEndpoint& outA,
		GapEndpoint& outB)
	{
		std::vector<GapEndpoint> endpoints;
		for (size_t i = 0; i < paths.size(); ++i) {
			if (paths[i].closed || paths[i].points.size() < 2) {
				continue;
			}
			GapEndpoint candidate{};
			if (GetEndpointCandidate(paths[i], 0, click, candidate)) {
				endpoints.push_back(candidate);
			}
			if (GetEndpointCandidate(paths[i], paths[i].points.size() - 1, click, candidate)) {
				endpoints.push_back(candidate);
			}
		}
		if (endpoints.size() < 2) {
			return false;
		}
		double bestScore = 0.0;
		double bestFallbackScore = 0.0;
		bool found = false;
		bool foundFallback = false;
		for (size_t i = 0; i < endpoints.size(); ++i) {
			for (size_t j = i + 1; j < endpoints.size(); ++j) {
				if (endpoints[i].art == endpoints[j].art) {
					continue;
				}
				const bool involvesPreferred = !preferredArt ||
					endpoints[i].art == preferredArt ||
					endpoints[j].art == preferredArt;
				if (!AreCollinear(endpoints[i], endpoints[j])) {
					continue;
				}
				const double gapLen = DuctworkMath::Dist(endpoints[i].point, endpoints[j].point);
				if (gapLen > kGapToolMaxGap) {
					continue;
				}
				DuctworkPoint mid{};
				mid.x = (endpoints[i].point.x + endpoints[j].point.x) * 0.5;
				mid.y = (endpoints[i].point.y + endpoints[j].point.y) * 0.5;
				const double midDist = DuctworkMath::Dist(mid, click);
				if (midDist > kGapToolHitTol) {
					continue;
				}
				const double score = midDist + endpoints[i].dist + endpoints[j].dist;
				if (involvesPreferred) {
					if (!found || score < bestScore) {
						bestScore = score;
						outA = endpoints[i];
						outB = endpoints[j];
						found = true;
					}
				} else if (!preferredArt) {
					if (!found || score < bestScore) {
						bestScore = score;
						outA = endpoints[i];
						outB = endpoints[j];
						found = true;
					}
				} else {
					if (!foundFallback || score < bestFallbackScore) {
						bestFallbackScore = score;
						outA = endpoints[i];
						outB = endpoints[j];
						foundFallback = true;
					}
				}
			}
		}
		if (found) {
			return true;
		}
		return foundFallback;
	}

	bool MergeGapEndpoints(const GapEndpoint& a, const GapEndpoint& b)
	{
		if (!a.art || !b.art || a.art == b.art) {
			return false;
		}
		std::vector<DuctworkPoint> pointsA;
		std::vector<DuctworkPoint> pointsB;
		bool closedA = false;
		bool closedB = false;
		if (!GetPathPoints(a.art, pointsA, closedA) || !GetPathPoints(b.art, pointsB, closedB)) {
			return false;
		}
		if (closedA || closedB || pointsA.size() < 2 || pointsB.size() < 2) {
			return false;
		}

		if (a.index == 0) {
			std::reverse(pointsA.begin(), pointsA.end());
		}
		if (b.index != 0) {
			std::reverse(pointsB.begin(), pointsB.end());
		}

		std::vector<DuctworkPoint> merged = pointsA;
		if (!merged.empty() && !pointsB.empty()) {
			if (DuctworkMath::Dist2(merged.back(), pointsB.front()) > 1e-6) {
				merged.push_back(pointsB.front());
			}
		}
		for (size_t i = 1; i < pointsB.size(); ++i) {
			merged.push_back(pointsB[i]);
		}
		if (!SetPathPoints(a.art, merged, false)) {
			return false;
		}
		sAIArt->DisposeArt(b.art);
		return true;
	}

	bool FindNearestSegmentHit(const std::vector<DuctworkPath>& paths,
		const DuctworkPoint& target,
		AIArtHandle& outArt,
		int& outSeg,
		DuctworkPoint& outClosest)
	{
		outArt = nullptr;
		outSeg = -1;
		outClosest = {};
		double bestDist = 0.0;
		bool found = false;
		for (size_t i = 0; i < paths.size(); ++i) {
			if (paths[i].points.size() < 2 || paths[i].closed) {
				continue;
			}
			size_t segIndex = 0;
			DuctworkPoint closest{};
			double t = 0.0;
			if (!FindSegmentHit(paths[i].points, target, kRegisterDetectionThreshold, segIndex, closest, t)) {
				continue;
			}
			const double dist = DuctworkMath::Dist(closest, target);
			if (!found || dist < bestDist) {
				bestDist = dist;
				outArt = paths[i].art;
				outSeg = static_cast<int>(segIndex);
				outClosest = closest;
				found = true;
			}
		}
		return found;
	}

bool FindNearestSegmentIntersection(const std::vector<DuctworkPath>& layerPaths,
	const DuctworkPoint& click,
	AIArtHandle preferredArt,
	GapIntersection& out,
	double hitTol)
	{
		out = {};
		if (layerPaths.empty()) {
			return false;
		}

		std::vector<DuctworkConnection> intersections;
		DuctworkConnections::FindConnections(
			layerPaths,
			2.0,
			3.0,
			15.0,
			10.0,
			true,
			intersections);
		GapIntersection fallback{};
		for (size_t i = 0; i < intersections.size(); ++i) {
			if (intersections[i].type != kConnectionSegmentIntersection) {
				continue;
			}
			const DuctworkPoint& pt = intersections[i].point;
			const double dist = DuctworkMath::Dist(pt, click);
			if (hitTol > 0.0 && dist > hitTol) {
				continue;
			}
			const AIArtHandle artA = (intersections[i].a >= 0 && intersections[i].a < static_cast<int>(layerPaths.size()))
				? layerPaths[static_cast<size_t>(intersections[i].a)].art
				: nullptr;
			const AIArtHandle artB = (intersections[i].b >= 0 && intersections[i].b < static_cast<int>(layerPaths.size()))
				? layerPaths[static_cast<size_t>(intersections[i].b)].art
				: nullptr;
			const bool involvesPreferred = !preferredArt || artA == preferredArt || artB == preferredArt;
			GapIntersection* target = involvesPreferred ? &out : (preferredArt ? &fallback : &out);
			if (!target->valid || dist < target->dist) {
				target->valid = true;
				target->isRegister = false;
				target->point = pt;
				target->segA = intersections[i].segA;
				target->segB = intersections[i].segB;
				target->dist = dist;
				target->artA = artA;
				target->artB = artB;
				target->layerName = layerPaths[static_cast<size_t>(
					(intersections[i].a >= 0) ? intersections[i].a : 0)].layerName;
			}
		}

		for (size_t i = 0; i < layerPaths.size(); ++i) {
			const DuctworkPath& path = layerPaths[i];
			if (path.closed || path.points.size() < 4) {
				continue;
			}
			const size_t segCount = path.points.size() - 1;
			for (size_t aSeg = 0; aSeg + 1 < segCount; ++aSeg) {
				const DuctworkPoint a1 = path.points[aSeg];
				const DuctworkPoint a2 = path.points[aSeg + 1];
				for (size_t bSeg = aSeg + 2; bSeg + 1 < path.points.size(); ++bSeg) {
					if (bSeg == aSeg + 1) {
						continue;
					}
					DuctworkPoint b1 = path.points[bSeg];
					DuctworkPoint b2 = path.points[bSeg + 1];
					DuctworkPoint intersection{};
					bool hasIntersection = DuctworkMath::SegmentIntersection(a1, a2, b1, b2, intersection);
					if (!hasIntersection) {
						if (!SegmentsOverlapCollinear(a1, a2, b1, b2, 3.0, 5.0, intersection)) {
							continue;
						}
					}
					if (IsNearPathEndpoint(path.points, intersection, kOverlapEndpointTol)) {
						continue;
					}
					const double dist = DuctworkMath::Dist(intersection, click);
					if (hitTol > 0.0 && dist > hitTol) {
						continue;
					}
					if (!out.valid || dist < out.dist) {
						const bool involvesPreferred = !preferredArt || path.art == preferredArt;
						GapIntersection* target = involvesPreferred ? &out : (preferredArt ? &fallback : &out);
						if (!target->valid || dist < target->dist) {
							target->valid = true;
							target->isRegister = false;
							target->point = intersection;
							target->segA = static_cast<int>(aSeg);
							target->segB = static_cast<int>(bSeg);
							target->dist = dist;
							target->artA = path.art;
							target->artB = path.art;
							target->layerName = path.layerName;
						}
					}
				}
			}
		}
		if (out.valid) {
			return true;
		}
		if (preferredArt && fallback.valid) {
			out = fallback;
			return true;
		}
		return false;
	}

bool FindNearestRegisterIntersection(const std::vector<DuctworkPath>& layerPaths,
	const DuctworkPoint& click,
	AIArtHandle preferredArt,
	GapIntersection& out,
	double hitTol)
	{
		out = {};
		if (layerPaths.empty()) {
			return false;
		}
	const double regHitTol = (std::max)(kRegisterDetectionThreshold, hitTol);
		GapIntersection fallback{};
		for (size_t i = 0; i < DuctworkConstants::kRegisterLayerCount; ++i) {
			std::vector<DuctworkPoint> anchors;
			CollectRegisterAnchors(DuctworkConstants::kRegisterLayers[i], anchors);
			for (size_t a = 0; a < anchors.size(); ++a) {
				const double dist = DuctworkMath::Dist(anchors[a], click);
				if (hitTol > 0.0 && dist > regHitTol) {
					continue;
				}
				AIArtHandle hitArt = nullptr;
				int segIndex = -1;
				DuctworkPoint closest{};
				if (!FindNearestSegmentHit(layerPaths, anchors[a], hitArt, segIndex, closest)) {
					continue;
				}
				const bool involvesPreferred = !preferredArt || hitArt == preferredArt;
				GapIntersection* target = involvesPreferred ? &out : (preferredArt ? &fallback : &out);
				if (!target->valid || dist < target->dist) {
					target->valid = true;
					target->isRegister = true;
					target->point = closest;
					target->artA = hitArt;
					target->artB = nullptr;
					target->segA = segIndex;
					target->segB = -1;
					target->dist = dist;
				}
			}
		}
		if (out.valid) {
			return true;
		}
		if (preferredArt && fallback.valid) {
			out = fallback;
			return true;
		}
		return false;
	}

	bool GetSegmentDirection(const DuctworkPath& path, size_t segIndex, DuctworkPoint& outDir)
	{
		if (segIndex + 1 >= path.points.size()) {
			return false;
		}
		const DuctworkPoint& a = path.points[segIndex];
		const DuctworkPoint& b = path.points[segIndex + 1];
		const double dx = b.x - a.x;
		const double dy = b.y - a.y;
		const double len = std::sqrt(dx * dx + dy * dy);
		if (len < 1e-3) {
			return false;
		}
		outDir.x = dx / len;
		outDir.y = dy / len;
		return true;
	}

	bool SplitSegmentAtPoint(AIArtHandle art, size_t segIndex, const DuctworkPoint& target, double halfWidth)
	{
		std::vector<DuctworkPoint> points;
		bool closed = false;
		if (!GetPathPoints(art, points, closed) || closed || segIndex + 1 >= points.size()) {
			return false;
		}
		double t = 0.0;
		DuctworkPoint closest = DuctworkMath::ClosestPointOnSegment(points[segIndex], points[segIndex + 1], target, t);
		const DuctworkPoint& a = points[segIndex];
		const DuctworkPoint& b = points[segIndex + 1];
		const double dx = b.x - a.x;
		const double dy = b.y - a.y;
		const double len = std::sqrt(dx * dx + dy * dy);
		if (len < 1e-3) {
			return false;
		}
		DuctworkPoint dir{ dx / len, dy / len };
		DuctworkPoint cutBefore{ closest.x - dir.x * halfWidth, closest.y - dir.y * halfWidth };
		DuctworkPoint cutAfter{ closest.x + dir.x * halfWidth, closest.y + dir.y * halfWidth };
		AIArtHandle first = nullptr;
		AIArtHandle second = nullptr;
		if (!SplitPathAtGap(art, segIndex, cutBefore, cutAfter, first, second)) {
			return false;
		}
		sAIArt->DisposeArt(art);
		return true;
	}
}

namespace DuctworkCarve
{
	CarveStats ApplyRegisterCarve(const std::vector<DuctworkPath>& paths,
		std::vector<AIArtHandle>& selection)
	{
		CarveStats stats;
		if (!sAIArt) {
			return stats;
		}

		for (size_t layerIdx = 0; layerIdx < DuctworkConstants::kDuctworkColorLayerCount; ++layerIdx) {
			const std::string ductLayer = DuctworkConstants::kDuctworkColorLayers[layerIdx];
			const char* regLayerName = GetRegisterLayerForDuctwork(ductLayer);
			std::vector<DuctworkPoint> registerCenters;
			CollectRegisterAnchors(regLayerName, registerCenters);
			if (registerCenters.empty()) {
				continue;
			}

			std::vector<AIArtHandle> workQueue = FilterSelectionByLayer(paths, ductLayer);
			while (!workQueue.empty()) {
				AIArtHandle path = workQueue.back();
				workQueue.pop_back();
				std::vector<DuctworkPoint> points;
				bool closed = false;
				if (!GetPathPoints(path, points, closed) || closed || points.size() < 2) {
					continue;
				}

				bool carved = false;
				for (size_t r = 0; r < registerCenters.size() && !carved; ++r) {
					const DuctworkPoint& reg = registerCenters[r];
					if (PointNear(points.front(), reg, kRegisterEndpointTol) ||
						PointNear(points.back(), reg, kRegisterEndpointTol)) {
						continue;
					}
					for (size_t seg = 0; seg + 1 < points.size(); ++seg) {
						const DuctworkPoint& a = points[seg];
						const DuctworkPoint& b = points[seg + 1];
						const double dx = b.x - a.x;
						const double dy = b.y - a.y;
						const double len = std::sqrt(dx * dx + dy * dy);
						if (len < 1e-3) {
							continue;
						}
						const double t = ((reg.x - a.x) * dx + (reg.y - a.y) * dy) / (len * len);
						if (t < 0.02 || t > 0.98) {
							continue;
						}
						DuctworkPoint closest;
						closest.x = a.x + t * dx;
						closest.y = a.y + t * dy;
						if (DuctworkMath::Dist(closest, reg) > kRegisterDetectionThreshold) {
							continue;
						}
						DuctworkPoint dir;
						dir.x = dx / len;
						dir.y = dy / len;
						DuctworkPoint cutBefore{ closest.x - dir.x * kRegisterCarveHalfWidth,
							closest.y - dir.y * kRegisterCarveHalfWidth };
						DuctworkPoint cutAfter{ closest.x + dir.x * kRegisterCarveHalfWidth,
							closest.y + dir.y * kRegisterCarveHalfWidth };

						AIArtHandle first = nullptr;
						AIArtHandle second = nullptr;
						if (SplitPathAtGap(path, seg, cutBefore, cutAfter, first, second)) {
							ReplaceSelectionArt(selection, path, first, second);
							sAIArt->DisposeArt(path);
							workQueue.push_back(first);
							workQueue.push_back(second);
							++stats.registerCarves;
							stats.pathsSplit += 2;
						}
						carved = true;
						break;
					}
				}
			}
		}
		return stats;
	}

	CarveStats ApplyOverlapCarve(const std::vector<DuctworkPath>& paths,
		std::vector<AIArtHandle>& selection)
	{
		CarveStats stats;
		if (!sAIArt) {
			return stats;
		}

		const double halfWidth = ComputeAutoGapHalfWidth(selection);
		int debugLogged = 0;

		for (size_t layerIdx = 0; layerIdx < DuctworkConstants::kDuctworkColorLayerCount; ++layerIdx) {
			const std::string layerName = DuctworkConstants::kDuctworkColorLayers[layerIdx];
			std::vector<DuctworkPath> layerPaths;
			for (size_t i = 0; i < paths.size(); ++i) {
				if (paths[i].layerName == layerName) {
					layerPaths.push_back(paths[i]);
				}
			}
			if (layerPaths.empty()) {
				continue;
			}

			std::vector<DuctworkConnection> intersections;
			DuctworkConnections::FindConnections(
				layerPaths,
				2.0,
				3.0,
				15.0,
				10.0,
				true,
				intersections);
			size_t countEndpointEndpoint = 0;
			size_t countEndpointSegment = 0;
			size_t countSegmentIntersection = 0;
			for (size_t i = 0; i < intersections.size(); ++i) {
				switch (intersections[i].type) {
				case kConnectionEndpointToEndpoint:
					++countEndpointEndpoint;
					break;
				case kConnectionEndpointToSegment:
					++countEndpointSegment;
					break;
				case kConnectionSegmentIntersection:
					++countSegmentIntersection;
					break;
				default:
					break;
				}
			}
			DuctworkLog::Write("OverlapCarve connections layer=" + layerName +
				" total=" + std::to_string(intersections.size()) +
				" seg=" + std::to_string(countSegmentIntersection) +
				" endSeg=" + std::to_string(countEndpointSegment) +
				" endEnd=" + std::to_string(countEndpointEndpoint));
			if (countSegmentIntersection == 0) {
				DuctworkLog::Write("OverlapCarve no segment intersections layer=" + layerName);
			}

			struct CarvePoint
			{
				DuctworkPoint point;
				int segIndex;
			};
			std::unordered_map<AIArtHandle, std::vector<CarvePoint>> carvePoints;
			size_t selfIntersections = 0;
			for (size_t i = 0; i < intersections.size(); ++i) {
				if (intersections[i].type != kConnectionSegmentIntersection) {
					continue;
				}
				const int segA = intersections[i].segA;
				const int segB = intersections[i].segB;
				if (segA < 0 || segB < 0) {
					continue;
				}
				const DuctworkPoint& pt = intersections[i].point;
				bool hasA = intersections[i].a >= 0 && intersections[i].a < static_cast<int>(layerPaths.size());
				bool hasB = intersections[i].b >= 0 && intersections[i].b < static_cast<int>(layerPaths.size());
				const DuctworkPath* pathA = hasA ? &layerPaths[intersections[i].a] : nullptr;
				const DuctworkPath* pathB = hasB ? &layerPaths[intersections[i].b] : nullptr;
				const bool nearASeg = pathA ? IsNearSegmentEndpoint(pathA->points, static_cast<size_t>(segA), pt, kOverlapEndpointTol) : true;
				const bool nearAPath = pathA ? IsNearPathEndpoint(pathA->points, pt, kOverlapEndpointTol) : true;
				const bool nearA = nearASeg || nearAPath;
				const bool nearBSeg = pathB ? IsNearSegmentEndpoint(pathB->points, static_cast<size_t>(segB), pt, kOverlapEndpointTol) : true;
				const bool nearBPath = pathB ? IsNearPathEndpoint(pathB->points, pt, kOverlapEndpointTol) : true;
				const bool nearB = nearBSeg || nearBPath;

				if (!nearA && !nearB) {
					const double lenA = (pathA && segA + 1 < static_cast<int>(pathA->points.size()))
						? DuctworkMath::Dist(pathA->points[static_cast<size_t>(segA)],
							pathA->points[static_cast<size_t>(segA + 1)])
						: 0.0;
					const double lenB = (pathB && segB + 1 < static_cast<int>(pathB->points.size()))
						? DuctworkMath::Dist(pathB->points[static_cast<size_t>(segB)],
							pathB->points[static_cast<size_t>(segB + 1)])
						: 0.0;
					const bool carveA = lenA >= lenB;
					if (carveA && pathA) {
						carvePoints[pathA->art].push_back({ pt, -1 });
					} else if (!carveA && pathB) {
						carvePoints[pathB->art].push_back({ pt, -1 });
					}
					if (debugLogged < 40) {
						DuctworkLog::Write("OverlapCarve accept AB layer=" + layerName +
							" segA=" + std::to_string(segA) +
							" segB=" + std::to_string(segB) +
							" lenA=" + std::to_string(lenA) +
							" lenB=" + std::to_string(lenB) +
							" carve=" + std::string(carveA ? "A" : "B") +
							" nearASeg=" + std::to_string(nearASeg) +
							" nearAPath=" + std::to_string(nearAPath) +
							" nearBSeg=" + std::to_string(nearBSeg) +
							" nearBPath=" + std::to_string(nearBPath) +
							" pt=[" + std::to_string(pt.x) + "," + std::to_string(pt.y) + "]");
						++debugLogged;
					}
				} else if (debugLogged < 40) {
					std::string layers;
					if (pathA) {
						layers += pathA->layerName;
					}
					if (pathB) {
						if (!layers.empty()) {
							layers += ",";
						}
						layers += pathB->layerName;
					}
					DuctworkLog::Write("OverlapCarve skip AB layers=" + layers +
						" segA=" + std::to_string(segA) +
						" segB=" + std::to_string(segB) +
						" nearASeg=" + std::to_string(nearASeg) +
						" nearAPath=" + std::to_string(nearAPath) +
						" nearBSeg=" + std::to_string(nearBSeg) +
						" nearBPath=" + std::to_string(nearBPath) +
						" pt=[" + std::to_string(pt.x) + "," + std::to_string(pt.y) + "]");
					++debugLogged;
				}
			}

			const double overlapDistTolerance = 3.0;
			const double overlapAngleTolerance = 5.0;
			size_t selfCandidates = 0;
			for (size_t i = 0; i < layerPaths.size(); ++i) {
				const DuctworkPath& path = layerPaths[i];
				if (debugLogged < 40) {
					DuctworkLog::Write("OverlapCarve self check layer=" + layerName +
						" pathIndex=" + std::to_string(i) +
						" points=" + std::to_string(path.points.size()) +
						" closed=" + std::to_string(path.closed ? 1 : 0));
					++debugLogged;
				}
				if (path.closed || path.points.size() < 4) {
					continue;
				}
				const size_t segCount = path.points.size() - 1;
				++selfCandidates;
				for (size_t aSeg = 0; aSeg + 1 < segCount; ++aSeg) {
					const DuctworkPoint a1 = path.points[aSeg];
					const DuctworkPoint a2 = path.points[aSeg + 1];
					for (size_t bSeg = aSeg + 2; bSeg + 1 < path.points.size(); ++bSeg) {
						if (bSeg == aSeg + 1) {
							continue;
						}
						if (path.closed && aSeg == 0 && bSeg == segCount - 1) {
							continue;
						}
						DuctworkPoint b1 = path.points[bSeg];
						DuctworkPoint b2 = path.points[bSeg + 1];
						DuctworkPoint intersection{};
						std::string kind = "cross";
						bool hasIntersection = DuctworkMath::SegmentIntersection(a1, a2, b1, b2, intersection);
						if (!hasIntersection) {
							if (!SegmentsOverlapCollinear(a1, a2, b1, b2, overlapDistTolerance, overlapAngleTolerance, intersection)) {
								double t = 0.0;
								DuctworkPoint closest{};
								if (DuctworkMath::Dist(DuctworkMath::ClosestPointOnSegment(b1, b2, a1, t), a1) <= kOverlapEndpointTol) {
									intersection = a1;
									kind = "endpoint";
								} else if (DuctworkMath::Dist(DuctworkMath::ClosestPointOnSegment(b1, b2, a2, t), a2) <= kOverlapEndpointTol) {
									intersection = a2;
									kind = "endpoint";
								} else if (DuctworkMath::Dist(DuctworkMath::ClosestPointOnSegment(a1, a2, b1, t), b1) <= kOverlapEndpointTol) {
									intersection = b1;
									kind = "endpoint";
								} else if (DuctworkMath::Dist(DuctworkMath::ClosestPointOnSegment(a1, a2, b2, t), b2) <= kOverlapEndpointTol) {
									intersection = b2;
									kind = "endpoint";
								} else {
									continue;
								}
							}
							if (kind.empty()) {
								kind = "overlap";
							} else if (kind == "endpoint") {
								hasIntersection = true;
							}
							if (!hasIntersection) {
								kind = "overlap";
							}
						} else {
							kind = "cross";
						}
						const bool nearPath = IsNearPathEndpoint(path.points, intersection, kOverlapEndpointTol);
						if (nearPath) {
							continue;
						}
						const double lenA = DuctworkMath::Dist(a1, a2);
						const double lenB = DuctworkMath::Dist(b1, b2);
						const bool carveA = lenA >= lenB;
						const int chosenSeg = carveA ? static_cast<int>(aSeg) : static_cast<int>(bSeg);
						carvePoints[path.art].push_back({ intersection, chosenSeg });
						++selfIntersections;
						if (debugLogged < 40) {
							DuctworkLog::Write("OverlapCarve self layer=" + layerName +
								" segA=" + std::to_string(aSeg) +
								" segB=" + std::to_string(bSeg) +
								" kind=" + kind +
								" lenA=" + std::to_string(lenA) +
								" lenB=" + std::to_string(lenB) +
								" carve=" + std::string(carveA ? "A" : "B") +
								" nearPath=" + std::to_string(nearPath) +
								" pt=[" + std::to_string(intersection.x) + "," + std::to_string(intersection.y) + "]");
							++debugLogged;
						}
					}
				}
			}
			if (selfIntersections > 0) {
				DuctworkLog::Write("OverlapCarve self intersections layer=" + layerName +
					" count=" + std::to_string(selfIntersections));
			} else if (selfCandidates > 0) {
				DuctworkLog::Write("OverlapCarve self intersections layer=" + layerName +
					" count=0 candidates=" + std::to_string(selfCandidates));
			}

			for (auto& entry : carvePoints) {
				AIArtHandle path = entry.first;
				std::vector<CarvePoint> points = entry.second;
				std::vector<AIArtHandle> workQueue;
				workQueue.push_back(path);
				while (!workQueue.empty()) {
					AIArtHandle current = workQueue.back();
					workQueue.pop_back();
					std::vector<DuctworkPoint> currentPoints;
					bool closed = false;
					if (!GetPathPoints(current, currentPoints, closed) || closed || currentPoints.size() < 2) {
						continue;
					}

					bool carved = false;
					for (size_t p = 0; p < points.size(); ++p) {
						size_t segIndex = 0;
						DuctworkPoint closest{};
						double t = 0.0;
						if (points[p].segIndex >= 0) {
							const size_t targetSeg = static_cast<size_t>(points[p].segIndex);
							if (targetSeg + 1 >= currentPoints.size()) {
								continue;
							}
							closest = DuctworkMath::ClosestPointOnSegment(
								currentPoints[targetSeg],
								currentPoints[targetSeg + 1],
								points[p].point,
								t);
							const double dist = DuctworkMath::Dist(closest, points[p].point);
							if (!(t >= 0.0 && t <= 1.0 && dist <= kOverlapPointTol)) {
								continue;
							}
							segIndex = targetSeg;
						} else {
							if (!FindSegmentHit(currentPoints, points[p].point, kOverlapPointTol, segIndex, closest, t)) {
								continue;
							}
						}
						const DuctworkPoint& a = currentPoints[segIndex];
						const DuctworkPoint& b = currentPoints[segIndex + 1];
						const double dx = b.x - a.x;
						const double dy = b.y - a.y;
						const double len = std::sqrt(dx * dx + dy * dy);
						if (len < 1e-3) {
							continue;
						}
						DuctworkPoint dir{ dx / len, dy / len };
						DuctworkPoint cutBefore{ closest.x - dir.x * halfWidth, closest.y - dir.y * halfWidth };
						DuctworkPoint cutAfter{ closest.x + dir.x * halfWidth, closest.y + dir.y * halfWidth };

						AIArtHandle first = nullptr;
						AIArtHandle second = nullptr;
						if (SplitPathAtGap(current, segIndex, cutBefore, cutAfter, first, second)) {
							if (debugLogged < 40) {
								DuctworkLog::Write("OverlapCarve split seg=" + std::to_string(segIndex) +
									" t=" + std::to_string(t) +
									" pt=[" + std::to_string(closest.x) + "," + std::to_string(closest.y) + "]");
								++debugLogged;
							}
							ReplaceSelectionArt(selection, current, first, second);
							sAIArt->DisposeArt(current);
							workQueue.push_back(first);
							workQueue.push_back(second);
							++stats.overlapCarves;
							stats.pathsSplit += 2;
						}
						carved = true;
						points.erase(points.begin() + static_cast<long long>(p));
						break;
					}
					if (!carved) {
						continue;
					}
				}
			}
		}
		return stats;
	}

	bool ApplyGapToolInternal(const DuctworkPoint& click,
		const std::string& layerHint,
		AIArtHandle preferredArt,
		bool toggleMode,
		GapToolResult& outResult)
	{
		outResult = {};
		std::vector<std::string> layers = BuildLineLayerList(layerHint);
		std::vector<DuctworkPath> layerPaths;
		CollectLinePathsForLayers(layers, layerPaths);
		if (layerPaths.empty()) {
			DuctworkLog::Write("GapTool: no line paths found");
			return false;
		}
		const AIArtHandle normalizedPreferred = NormalizePreferredArt(preferredArt, click, layerPaths);
		if (preferredArt && preferredArt != normalizedPreferred) {
			std::ostringstream stream;
			stream << "GapTool: preferred normalized " << preferredArt << " -> " << normalizedPreferred;
			DuctworkLog::Write(stream.str());
		}
		preferredArt = normalizedPreferred;

		std::unordered_map<AIArtHandle, size_t> pathIndex;
		for (size_t i = 0; i < layerPaths.size(); ++i) {
			if (layerPaths[i].art) {
				pathIndex[layerPaths[i].art] = i;
			}
		}

		GapEndpoint gapA{};
		GapEndpoint gapB{};
		const bool hasGap = FindGapEndpointPairNearClick(layerPaths, click, preferredArt, gapA, gapB);

		GapIntersection segHit{};
		GapIntersection regHit{};
		const double previewTol = preferredArt ? -1.0 : kGapToolPreviewTol;
		{
			std::ostringstream stream;
			stream << "GapPreview: previewTol=" << previewTol
				<< " preferred=" << preferredArt;
			DuctworkLog::Write(stream.str());
		}
		const bool hasSeg = FindNearestSegmentIntersection(layerPaths, click, preferredArt, segHit, previewTol);
		const bool hasReg = FindNearestRegisterIntersection(layerPaths, click, preferredArt, regHit, previewTol);

		GapIntersection bestHit = {};
		if (hasSeg && hasReg) {
			bestHit = (segHit.dist <= regHit.dist) ? segHit : regHit;
		} else if (hasSeg) {
			bestHit = segHit;
		} else if (hasReg) {
			bestHit = regHit;
		}
		{
			std::ostringstream stream;
			stream << "GapTool: click=[" << click.x << "," << click.y << "]"
				<< " hasGap=" << (hasGap ? 1 : 0)
				<< " hasSeg=" << (hasSeg ? 1 : 0)
				<< " hasReg=" << (hasReg ? 1 : 0)
				<< " bestValid=" << (bestHit.valid ? 1 : 0);
			if (bestHit.valid) {
				stream << " bestArtA=" << bestHit.artA
					<< " bestArtB=" << bestHit.artB
					<< " segA=" << bestHit.segA
					<< " segB=" << bestHit.segB
					<< " dist=" << bestHit.dist;
			}
			DuctworkLog::Write(stream.str());
		}

		bool healed = false;
		bool carved = false;

		if (hasGap) {
			if (MergeGapEndpoints(gapA, gapB)) {
				healed = true;
				DuctworkLog::Write("GapTool: healed gap");
			} else {
				DuctworkLog::Write("GapTool: failed to heal gap");
			}
			outResult.applied = healed;
			outResult.healed = healed;
			outResult.carved = false;
			outResult.toggled = toggleMode && healed;
			return outResult.applied;
		}

		if (!bestHit.valid) {
			outResult.applied = false;
			outResult.healed = false;
			outResult.carved = false;
			outResult.toggled = false;
			return false;
		}

		auto getPathForArt = [&](AIArtHandle art) -> const DuctworkPath* {
			std::unordered_map<AIArtHandle, size_t>::const_iterator it = pathIndex.find(art);
			if (it == pathIndex.end()) {
				return nullptr;
			}
			return &layerPaths[it->second];
		};

		std::vector<AIArtHandle> selection;
		selection.reserve(layerPaths.size());
		for (size_t i = 0; i < layerPaths.size(); ++i) {
			if (layerPaths[i].art) {
				selection.push_back(layerPaths[i].art);
			}
		}
		const double halfWidth = ComputeAutoGapHalfWidth(selection);

		if (bestHit.valid) {
			if (bestHit.isRegister) {
				if (bestHit.artA && bestHit.segA >= 0) {
					carved = SplitSegmentAtPoint(bestHit.artA,
						static_cast<size_t>(bestHit.segA),
						bestHit.point,
						kRegisterCarveHalfWidth);
				}
			} else if (bestHit.artA && bestHit.artB && bestHit.segA >= 0 && bestHit.segB >= 0) {
				const DuctworkPath* pathA = getPathForArt(bestHit.artA);
				const DuctworkPath* pathB = getPathForArt(bestHit.artB);
				if (pathA && pathB) {
					const size_t segA = static_cast<size_t>(bestHit.segA);
					const size_t segB = static_cast<size_t>(bestHit.segB);
					bool carveA = true;
					if (preferredArt == bestHit.artB) {
						carveA = false;
					} else if (preferredArt != bestHit.artA) {
						const double lenA = (segA + 1 < pathA->points.size())
							? DuctworkMath::Dist(pathA->points[segA], pathA->points[segA + 1])
							: 0.0;
						const double lenB = (segB + 1 < pathB->points.size())
							? DuctworkMath::Dist(pathB->points[segB], pathB->points[segB + 1])
							: 0.0;
						carveA = lenA >= lenB;
					}
					const AIArtHandle targetArt = carveA ? bestHit.artA : bestHit.artB;
					const size_t targetSeg = carveA ? segA : segB;
					carved = SplitSegmentAtPoint(targetArt, targetSeg, bestHit.point, halfWidth);
				}
			}
		}

		if (carved) {
			DuctworkLog::Write("GapTool: carved gap");
		} else if (!healed) {
			DuctworkLog::Write("GapTool: no carve applied");
		}

		outResult.applied = healed || carved;
		outResult.healed = healed;
		outResult.carved = carved;
		outResult.toggled = toggleMode && (healed || carved);
		return outResult.applied;
	}

	bool ApplyGapToggleAtPoint(const DuctworkPoint& click,
		const std::string& layerHint,
		AIArtHandle preferredArt,
		GapToolResult& outResult)
	{
		return ApplyGapToolInternal(click, layerHint, preferredArt, true, outResult);
	}

	bool ApplyGapHealCreateAtPoint(const DuctworkPoint& click,
		const std::string& layerHint,
		AIArtHandle preferredArt,
		GapToolResult& outResult)
	{
		return ApplyGapToolInternal(click, layerHint, preferredArt, false, outResult);
	}

	bool FindPreferredArtNearPoint(const DuctworkPoint& click,
		const std::string& layerHint,
		AIArtHandle& outArt,
		DuctworkPoint* outClosest)
	{
		outArt = nullptr;
		std::vector<std::string> layers = BuildLineLayerList(layerHint);
		std::vector<DuctworkPath> layerPaths;
		CollectLinePathsForLayers(layers, layerPaths);
		if (layerPaths.empty()) {
			DuctworkLog::Write("GapPreview prefer: no layer paths");
			return false;
		}

		double bestDist2 = (std::numeric_limits<double>::max)();
		DuctworkPoint bestPoint{};
		AIArtHandle bestArt = nullptr;
		for (size_t i = 0; i < layerPaths.size(); ++i) {
			const DuctworkPath& path = layerPaths[i];
			if (path.closed || path.points.size() < 2) {
				continue;
			}
			for (size_t s = 0; s + 1 < path.points.size(); ++s) {
				double t = 0.0;
				DuctworkPoint closest = DuctworkMath::ClosestPointOnSegment(path.points[s], path.points[s + 1], click, t);
				const double dist2 = DuctworkMath::Dist2(closest, click);
				if (dist2 < bestDist2) {
					bestDist2 = dist2;
					bestPoint = closest;
					bestArt = path.art;
				}
			}
		}

		if (!bestArt) {
			DuctworkLog::Write("GapPreview prefer: no best art");
			return false;
		}
		const double bestDist = std::sqrt(bestDist2);
		{
			std::ostringstream stream;
			stream << "GapPreview prefer: bestDist=" << bestDist
				<< " bestArt=" << bestArt
				<< " layerHint=" << (layerHint.empty() ? "<none>" : layerHint);
			DuctworkLog::Write(stream.str());
		}
		if (bestDist > kGapToolPreviewTol) {
			DuctworkLog::Write("GapPreview prefer: bestDist above hit tol");
			return false;
		}
		outArt = bestArt;
		if (outClosest) {
			*outClosest = bestPoint;
		}
		return true;
	}

	bool ComputeGapToolPreview(const DuctworkPoint& click,
		const std::string& layerHint,
		AIArtHandle preferredArt,
		GapToolPreview& outPreview,
		AIDocumentViewHandle view,
		const AIRealPoint* viewClick)
	{
		outPreview = {};
		std::vector<std::string> layers = BuildLineLayerList(layerHint);
		std::vector<DuctworkPath> layerPaths;
		CollectLinePathsForLayers(layers, layerPaths);
		if (layerPaths.empty()) {
			return false;
		}
		const AIArtHandle normalizedPreferred = NormalizePreferredArt(preferredArt, click, layerPaths);
		if (preferredArt && preferredArt != normalizedPreferred) {
			std::ostringstream stream;
			stream << "GapPreview: preferred normalized " << preferredArt << " -> " << normalizedPreferred;
			DuctworkLog::Write(stream.str());
		}
		preferredArt = normalizedPreferred;

		std::unordered_map<AIArtHandle, size_t> pathIndex;
		for (size_t i = 0; i < layerPaths.size(); ++i) {
			if (layerPaths[i].art) {
				pathIndex[layerPaths[i].art] = i;
			}
		}

		GapEndpoint gapA{};
		GapEndpoint gapB{};
		const bool hasGap = FindGapEndpointPairNearClick(layerPaths, click, preferredArt, gapA, gapB);
		if (hasGap) {
			outPreview.valid = true;
			outPreview.showHeal = true;
			outPreview.showGap = false;
			outPreview.isRegister = false;
			outPreview.start = gapA.point;
			outPreview.end = gapB.point;
			return true;
		}

		const double previewTol = preferredArt ? -1.0 : kGapToolPreviewTol;
		GapIntersection segHit{};
		GapIntersection regHit{};
		const bool hasSeg = FindNearestSegmentIntersection(layerPaths, click, preferredArt, segHit, previewTol);
		const bool hasReg = FindNearestRegisterIntersection(layerPaths, click, preferredArt, regHit, previewTol);
		GapIntersection bestHit = {};
		if (hasSeg && hasReg) {
			bestHit = (segHit.dist <= regHit.dist) ? segHit : regHit;
		} else if (hasSeg) {
			bestHit = segHit;
		} else if (hasReg) {
			bestHit = regHit;
		}
		if (!bestHit.valid) {
			std::ostringstream stream;
			stream << "GapPreview: no hit click=[" << click.x << "," << click.y << "]"
				<< " hasSeg=" << (hasSeg ? 1 : 0)
				<< " hasReg=" << (hasReg ? 1 : 0)
				<< " tol=" << previewTol
				<< " preferred=" << preferredArt;
			DuctworkLog::Write(stream.str());
			return false;
		}
		{
			std::ostringstream stream;
			stream << "GapPreview: hit click=[" << click.x << "," << click.y << "]"
				<< " artA=" << bestHit.artA
				<< " artB=" << bestHit.artB
				<< " segA=" << bestHit.segA
				<< " segB=" << bestHit.segB
				<< " reg=" << (bestHit.isRegister ? 1 : 0)
				<< " dist=" << bestHit.dist;
			DuctworkLog::Write(stream.str());
		}
		if (view && viewClick) {
			AIRealPoint hitArt{};
			hitArt.h = static_cast<AIReal>(bestHit.point.x);
			hitArt.v = static_cast<AIReal>(bestHit.point.y);
			AIPoint hitView{};
			if (!sAIDocumentView->ArtworkPointToViewPoint(view, &hitArt, &hitView)) {
				const double dx = static_cast<double>(hitView.h) - viewClick->h;
				const double dy = static_cast<double>(hitView.v) - viewClick->v;
				const double viewDist = std::sqrt(dx * dx + dy * dy);
				if (viewDist > kGapToolPreviewTolView) {
					std::ostringstream stream;
					stream << "GapPreview: view dist too far viewDist=" << viewDist
						<< " tol=" << kGapToolPreviewTolView;
					DuctworkLog::Write(stream.str());
					return false;
				}
			}
		}

		auto getPathForArt = [&](AIArtHandle art) -> const DuctworkPath* {
			std::unordered_map<AIArtHandle, size_t>::const_iterator it = pathIndex.find(art);
			if (it == pathIndex.end()) {
				return nullptr;
			}
			return &layerPaths[it->second];
		};

		std::vector<AIArtHandle> selection;
		selection.reserve(layerPaths.size());
		for (size_t i = 0; i < layerPaths.size(); ++i) {
			if (layerPaths[i].art) {
				selection.push_back(layerPaths[i].art);
			}
		}
		const double halfWidth = bestHit.isRegister ? kRegisterCarveHalfWidth : ComputeAutoGapHalfWidth(selection);

		const DuctworkPath* pathA = getPathForArt(bestHit.artA);
		const DuctworkPath* pathB = getPathForArt(bestHit.artB);
		if (!pathA) {
			return false;
		}

		size_t segA = (bestHit.segA >= 0) ? static_cast<size_t>(bestHit.segA) : 0;
		size_t segB = (bestHit.segB >= 0) ? static_cast<size_t>(bestHit.segB) : segA;

		if (!bestHit.isRegister && pathB && bestHit.segA >= 0 && bestHit.segB >= 0) {
			double distA = (std::numeric_limits<double>::max)();
			double distB = (std::numeric_limits<double>::max)();
			if (segA + 1 < pathA->points.size()) {
				double t = 0.0;
				DuctworkPoint closest = DuctworkMath::ClosestPointOnSegment(pathA->points[segA], pathA->points[segA + 1], click, t);
				distA = DuctworkMath::Dist(closest, click);
			}
			if (segB + 1 < pathB->points.size()) {
				double t = 0.0;
				DuctworkPoint closest = DuctworkMath::ClosestPointOnSegment(pathB->points[segB], pathB->points[segB + 1], click, t);
				distB = DuctworkMath::Dist(closest, click);
			}
			double distAView = (std::numeric_limits<double>::max)();
			double distBView = (std::numeric_limits<double>::max)();
			bool useView = false;
			if (view && viewClick && segA + 1 < pathA->points.size() && segB + 1 < pathB->points.size()) {
				const bool gotA = GetSegmentViewDistance(view, pathA->points[segA], pathA->points[segA + 1], *viewClick, distAView);
				const bool gotB = GetSegmentViewDistance(view, pathB->points[segB], pathB->points[segB + 1], *viewClick, distBView);
				useView = gotA && gotB;
			}
			const double useDistA = useView ? distAView : distA;
			const double useDistB = useView ? distBView : distB;
			const AIArtHandle proximityPreferred = (useDistA <= useDistB) ? bestHit.artA : bestHit.artB;
			if (!preferredArt || preferredArt != proximityPreferred) {
				preferredArt = proximityPreferred;
			}
			{
				std::ostringstream stream;
				stream << "GapPreview: proximity distA=" << distA
					<< " distB=" << distB;
				if (useView) {
					stream << " viewA=" << distAView
						<< " viewB=" << distBView
						<< " use=view";
				} else {
					stream << " use=art";
				}
				stream
					<< " prefer=" << preferredArt;
				DuctworkLog::Write(stream.str());
			}
		}

		const DuctworkPath* chosenPath = pathA;
		size_t chosenSeg = segA;
		if (!bestHit.isRegister && pathB && bestHit.segA >= 0 && bestHit.segB >= 0) {
			if (preferredArt == bestHit.artB) {
				chosenPath = pathB;
				chosenSeg = segB;
			} else if (preferredArt == bestHit.artA) {
				chosenPath = pathA;
				chosenSeg = segA;
			} else {
				const double lenA = (segA + 1 < pathA->points.size())
					? DuctworkMath::Dist(pathA->points[segA], pathA->points[segA + 1])
					: 0.0;
				const double lenB = (segB + 1 < pathB->points.size())
					? DuctworkMath::Dist(pathB->points[segB], pathB->points[segB + 1])
					: 0.0;
				if (lenB > lenA) {
					chosenPath = pathB;
					chosenSeg = segB;
				}
			}
		}
		{
			std::ostringstream stream;
			stream << "GapPreview: choose preferred=" << preferredArt
				<< " chosenArt=" << (chosenPath ? chosenPath->art : nullptr)
				<< " chosenSeg=" << chosenSeg
				<< " segA=" << segA
				<< " segB=" << segB
				<< " isReg=" << (bestHit.isRegister ? 1 : 0);
			DuctworkLog::Write(stream.str());
		}

		DuctworkPoint dir{};
		if (!GetSegmentDirection(*chosenPath, chosenSeg, dir)) {
			return false;
		}
		outPreview.valid = true;
		outPreview.showHeal = false;
		outPreview.showGap = true;
		outPreview.isRegister = bestHit.isRegister;
		outPreview.halfWidth = halfWidth;
		outPreview.start = { bestHit.point.x - dir.x * halfWidth, bestHit.point.y - dir.y * halfWidth };
		outPreview.end = { bestHit.point.x + dir.x * halfWidth, bestHit.point.y + dir.y * halfWidth };
		return true;
	}
}
