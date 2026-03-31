#include "IllustratorSDK.h"
#include "ProcessDuctworkConnections.h"
#include "ProcessDuctworkLayers.h"
#include "ProcessDuctworkMath.h"

namespace
{
	bool HasVertexNear(const DuctworkPath& path, const DuctworkPoint& pt, double tolerance)
	{
		const double tol2 = tolerance * tolerance;
		for (size_t i = 0; i < path.points.size(); ++i) {
			if (DuctworkMath::Dist2(path.points[i], pt) <= tol2) {
				return true;
			}
		}
		return false;
	}

	double DistancePointToLine(const DuctworkPoint& p, const DuctworkPoint& a, const DuctworkPoint& b)
	{
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
	}

	bool SegmentsOverlapCollinear(const DuctworkPoint& a1, const DuctworkPoint& a2,
		const DuctworkPoint& b1, const DuctworkPoint& b2,
		double distTolerance, double angleToleranceDeg, DuctworkPoint& outMidpoint)
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
		const double cosTol = std::cos(angleToleranceDeg * (3.141592653589793 / 180.0));
		if (std::fabs(dot) < cosTol) {
			return false;
		}
		if (DistancePointToLine(b1, a1, a2) > distTolerance &&
			DistancePointToLine(b2, a1, a2) > distTolerance) {
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
}

void DuctworkConnections::FindConnections(const std::vector<DuctworkPath>& paths,
	double maxDist,
	double tJunctionDist,
	double endpointTolerance,
	double vertexTolerance,
	bool allowSegmentIntersectionConnect,
	std::vector<DuctworkConnection>& outConnections)
{
	outConnections.clear();
	if (paths.size() < 2) {
		return;
	}

	const double maxDist2 = maxDist * maxDist;
	const double tJunc2 = tJunctionDist * tJunctionDist;
	const double endTol2 = endpointTolerance * endpointTolerance;
	const double overlapDistTolerance = 3.0;
	const double overlapAngleTolerance = 5.0;

	for (size_t i = 0; i < paths.size(); ++i) {
		const DuctworkPath& a = paths[i];
		if (a.points.size() < 2) {
			continue;
		}
		const DuctworkPoint aStart = a.points.front();
		const DuctworkPoint aEnd = a.points.back();

		for (size_t j = i + 1; j < paths.size(); ++j) {
			const DuctworkPath& b = paths[j];
			if (b.points.size() < 2) {
				continue;
			}
			if (a.layerName != b.layerName) {
				continue;
			}
			const bool allowIntersection = allowSegmentIntersectionConnect &&
				DuctworkLayers::IsColorLayerName(a.layerName);
			const DuctworkPoint bStart = b.points.front();
			const DuctworkPoint bEnd = b.points.back();

			if (DuctworkMath::Dist2(aStart, bStart) <= maxDist2 ||
				DuctworkMath::Dist2(aStart, bEnd) <= maxDist2 ||
				DuctworkMath::Dist2(aEnd, bStart) <= maxDist2 ||
				DuctworkMath::Dist2(aEnd, bEnd) <= maxDist2) {
				DuctworkConnection conn;
				conn.a = static_cast<int>(i);
				conn.b = static_cast<int>(j);
				conn.type = kConnectionEndpointToEndpoint;
				conn.segA = -1;
				conn.segB = -1;
				conn.endpointA = -1;
				conn.endpointB = -1;
				if (DuctworkMath::Dist2(aStart, bStart) <= maxDist2) {
					conn.point = aStart;
					conn.endpointA = 0;
					conn.endpointB = 0;
				} else if (DuctworkMath::Dist2(aStart, bEnd) <= maxDist2) {
					conn.point = aStart;
					conn.endpointA = 0;
					conn.endpointB = static_cast<int>(b.points.size() - 1);
				} else if (DuctworkMath::Dist2(aEnd, bStart) <= maxDist2) {
					conn.point = aEnd;
					conn.endpointA = static_cast<int>(a.points.size() - 1);
					conn.endpointB = 0;
				} else if (DuctworkMath::Dist2(aEnd, bEnd) <= maxDist2) {
					conn.point = aEnd;
					conn.endpointA = static_cast<int>(a.points.size() - 1);
					conn.endpointB = static_cast<int>(b.points.size() - 1);
				}
				outConnections.push_back(conn);
				continue;
			}

			for (size_t aSeg = 0; aSeg + 1 < a.points.size(); ++aSeg) {
				DuctworkPoint a1 = a.points[aSeg];
				DuctworkPoint a2 = a.points[aSeg + 1];

				double t = 0.0;
				DuctworkPoint nearAStart = DuctworkMath::ClosestPointOnSegment(a1, a2, bStart, t);
				if (t > 0.0 && t < 1.0 && DuctworkMath::Dist2(nearAStart, bStart) <= tJunc2) {
					DuctworkConnection conn;
					conn.a = static_cast<int>(i);
					conn.b = static_cast<int>(j);
					conn.type = kConnectionEndpointToSegment;
					conn.point = nearAStart;
					conn.segA = static_cast<int>(aSeg);
					conn.segB = -1;
					conn.endpointA = -1;
					conn.endpointB = 0;
					outConnections.push_back(conn);
					break;
				}

				DuctworkPoint nearAEnd = DuctworkMath::ClosestPointOnSegment(a1, a2, bEnd, t);
				if (t > 0.0 && t < 1.0 && DuctworkMath::Dist2(nearAEnd, bEnd) <= tJunc2) {
					DuctworkConnection conn;
					conn.a = static_cast<int>(i);
					conn.b = static_cast<int>(j);
					conn.type = kConnectionEndpointToSegment;
					conn.point = nearAEnd;
					conn.segA = static_cast<int>(aSeg);
					conn.segB = -1;
					conn.endpointA = -1;
					conn.endpointB = static_cast<int>(b.points.size() - 1);
					outConnections.push_back(conn);
					break;
				}
			}

			for (size_t bSeg = 0; bSeg + 1 < b.points.size(); ++bSeg) {
				DuctworkPoint b1 = b.points[bSeg];
				DuctworkPoint b2 = b.points[bSeg + 1];

				double t = 0.0;
				DuctworkPoint nearBStart = DuctworkMath::ClosestPointOnSegment(b1, b2, aStart, t);
				if (t > 0.0 && t < 1.0 && DuctworkMath::Dist2(nearBStart, aStart) <= tJunc2) {
					DuctworkConnection conn;
					conn.a = static_cast<int>(i);
					conn.b = static_cast<int>(j);
					conn.type = kConnectionEndpointToSegment;
					conn.point = nearBStart;
					conn.segA = -1;
					conn.segB = static_cast<int>(bSeg);
					conn.endpointA = 0;
					conn.endpointB = -1;
					outConnections.push_back(conn);
					break;
				}

				DuctworkPoint nearBEnd = DuctworkMath::ClosestPointOnSegment(b1, b2, aEnd, t);
				if (t > 0.0 && t < 1.0 && DuctworkMath::Dist2(nearBEnd, aEnd) <= tJunc2) {
					DuctworkConnection conn;
					conn.a = static_cast<int>(i);
					conn.b = static_cast<int>(j);
					conn.type = kConnectionEndpointToSegment;
					conn.point = nearBEnd;
					conn.segA = -1;
					conn.segB = static_cast<int>(bSeg);
					conn.endpointA = static_cast<int>(a.points.size() - 1);
					conn.endpointB = -1;
					outConnections.push_back(conn);
					break;
				}
			}

			for (size_t aSeg = 0; aSeg + 1 < a.points.size(); ++aSeg) {
				DuctworkPoint a1 = a.points[aSeg];
				DuctworkPoint a2 = a.points[aSeg + 1];
				for (size_t bSeg = 0; bSeg + 1 < b.points.size(); ++bSeg) {
					DuctworkPoint b1 = b.points[bSeg];
					DuctworkPoint b2 = b.points[bSeg + 1];
					DuctworkPoint overlapMid;
					if (SegmentsOverlapCollinear(a1, a2, b1, b2, overlapDistTolerance, overlapAngleTolerance, overlapMid)) {
						DuctworkConnection conn;
						conn.a = static_cast<int>(i);
						conn.b = static_cast<int>(j);
						conn.type = kConnectionSegmentIntersection;
						conn.point = overlapMid;
						conn.segA = static_cast<int>(aSeg);
						conn.segB = static_cast<int>(bSeg);
						conn.endpointA = -1;
						conn.endpointB = -1;
						outConnections.push_back(conn);
						break;
					}
					DuctworkPoint intersection;
					if (!DuctworkMath::SegmentIntersection(a1, a2, b1, b2, intersection)) {
						continue;
					}

					if (!HasVertexNear(a, intersection, vertexTolerance) &&
						!HasVertexNear(b, intersection, vertexTolerance)) {
						if (!allowIntersection) {
							continue;
						}
					}

					const bool nearEndpoint =
						DuctworkMath::Dist2(a1, intersection) <= endTol2 ||
						DuctworkMath::Dist2(a2, intersection) <= endTol2 ||
						DuctworkMath::Dist2(b1, intersection) <= endTol2 ||
						DuctworkMath::Dist2(b2, intersection) <= endTol2;
					if (nearEndpoint) {
						continue;
					}
					DuctworkConnection conn;
					conn.a = static_cast<int>(i);
					conn.b = static_cast<int>(j);
					conn.type = kConnectionSegmentIntersection;
					conn.point = intersection;
					conn.segA = static_cast<int>(aSeg);
					conn.segB = static_cast<int>(bSeg);
					conn.endpointA = -1;
					conn.endpointB = -1;
					outConnections.push_back(conn);
				}
			}
		}
	}
}
