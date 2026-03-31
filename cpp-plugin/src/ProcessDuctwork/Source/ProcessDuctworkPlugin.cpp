#include "IllustratorSDK.h"
#include "ProcessDuctworkPlugin.h"
#include "AppContext.hpp"
#include "ProcessDuctworkLog.h"
#include "ProcessDuctworkConstants.h"
#include "ProcessDuctworkConnections.h"
#include "ProcessDuctworkCompound.h"
#include "ProcessDuctworkGeometry.h"
#include "ProcessDuctworkMath.h"
#include "ProcessDuctworkMetadata.h"
#include "ProcessDuctworkLayers.h"
#include "ProcessDuctworkOrtho.h"
#include "ProcessDuctworkCarve.h"
#include "ProcessDuctworkParts.h"
#include "ProcessDuctworkSelection.h"
#include "ProcessDuctworkStyles.h"
#include "AILiveEdit.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>

namespace
{
	class LiveEditSuspendScope
	{
	public:
		LiveEditSuspendScope()
		{
			fActive = sAILiveEdit && sAILiveEdit->SuspendLiveEditing() == kNoErr;
		}
		~LiveEditSuspendScope()
		{
			if (fActive && sAILiveEdit) {
				sAILiveEdit->ResumeLiveEditing();
			}
		}
	private:
		bool fActive = false;
	};

	class CepSuspendScope
	{
	public:
		CepSuspendScope()
		{
			const char* appData = std::getenv("APPDATA");
			if (!appData || appData[0] == '\0') {
				return;
			}
			std::filesystem::path basePath(appData);
			mFlagPath = basePath / "Adobe" / "CEP" / "extensions" / "Emory-Ductwork-Panel" / "md_cep_suspend.flag";
			std::ofstream out(mFlagPath.string(), std::ios::out | std::ios::trunc);
			DuctworkLog::Write("CEP suspend flag=" + mFlagPath.string());
		}

		~CepSuspendScope()
		{
			if (mFlagPath.empty()) {
				return;
			}
			std::error_code ec;
			std::filesystem::remove(mFlagPath, ec);
		}
	private:
		std::filesystem::path mFlagPath;
	};

	class StepTimer
	{
	public:
		explicit StepTimer(const char* label)
			: mLabel(label),
			mStart(std::chrono::steady_clock::now())
		{
		}
		void LogElapsed(const char* suffix = nullptr) const
		{
			const auto end = std::chrono::steady_clock::now();
			const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - mStart).count();
			std::ostringstream out;
			out << "Timing " << mLabel;
			if (suffix && suffix[0] != '\0') {
				out << " " << suffix;
			}
			out << " ms=" << ms;
			DuctworkLog::Write(out.str());
		}
	private:
		const char* mLabel;
		std::chrono::steady_clock::time_point mStart;
	};

	class RotationOverrideScope
	{
	public:
		RotationOverrideScope(bool hasOverride, double rotationOverride)
		{
			DuctworkParts::SetGlobalRotationOverride(hasOverride, rotationOverride);
		}
		~RotationOverrideScope()
		{
			DuctworkParts::SetGlobalRotationOverride(false, 0.0);
		}
	};

	class RegisterRotationScope
	{
	public:
		explicit RegisterRotationScope(bool rotateRegisters)
		{
			DuctworkParts::SetGlobalRotateRegisters(rotateRegisters);
		}
		~RegisterRotationScope()
		{
			DuctworkParts::SetGlobalRotateRegisters(false);
		}
	};

	void CollectSelectableParts(AIArtHandle art, std::vector<AIArtHandle>& outParts)
	{
		if (!art || !sAIArt) {
			return;
		}

		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return;
		}

		if (type == kPlacedArt || type == kRasterArt) {
			outParts.push_back(art);
		}

		AIArtHandle child = nullptr;
		if (!sAIArt->GetArtFirstChild(art, &child) && child) {
			AIArtHandle current = child;
			while (current) {
				CollectSelectableParts(current, outParts);
				AIArtHandle next = nullptr;
				if (sAIArt->GetArtSibling(current, &next)) {
					break;
				}
				current = next;
			}
		}
	}

	void ClearSelection()
	{
		if (!sAIArtSet || !sAIArt) {
			return;
		}
		AIArtSet selectedSet = nullptr;
		if (sAIArtSet->NewArtSet(&selectedSet)) {
			return;
		}
		size_t selectedCount = 0;
		if (!sAIArtSet->SelectedArtSet(selectedSet)) {
			if (!sAIArtSet->CountArtSet(selectedSet, &selectedCount)) {
				for (size_t i = 0; i < selectedCount; ++i) {
					AIArtHandle art = nullptr;
					if (sAIArtSet->IndexArtSet(selectedSet, i, &art) || !art) {
						continue;
					}
					sAIArt->SetArtUserAttr(art, kArtSelected | kArtFullySelected, 0);
				}
			}
		}
		sAIArtSet->DisposeArtSet(&selectedSet);
	}

	void SelectArtList(const std::vector<AIArtHandle>& artList)
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

	void FilterValidArtList(const std::vector<AIArtHandle>& artList, std::vector<AIArtHandle>& outValid)
	{
		outValid.clear();
		if (!sAIArt) {
			return;
		}
		for (size_t i = 0; i < artList.size(); ++i) {
			if (!artList[i]) {
				continue;
			}
			short type = kUnknownArt;
			if (sAIArt->GetArtType(artList[i], &type) == kNoErr) {
				outValid.push_back(artList[i]);
			}
		}
	}

	void CollectAllPathsFromArt(AIArtHandle art, std::vector<AIArtHandle>& outPaths)
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
				CollectAllPathsFromArt(current, outPaths);
				AIArtHandle next = nullptr;
				if (sAIArt->GetArtSibling(current, &next)) {
					break;
				}
				current = next;
			}
		}
	}

	void CollectDuctworkLayerPaths(std::vector<AIArtHandle>& outPaths)
	{
		outPaths.clear();
		if (!sAILayer || !sAIArt) {
			return;
		}

		AILayerHandle layer = nullptr;
		if (sAILayer->GetFirstLayer(&layer)) {
			return;
		}

		while (layer) {
			ai::UnicodeString layerTitle;
			if (!sAILayer->GetLayerTitle(layer, layerTitle)) {
				std::string name = layerTitle.as_UTF8();
				if (DuctworkLayers::IsLineLayerName(name)) {
					AIArtHandle layerGroup = nullptr;
					if (!sAIArt->GetFirstArtOfLayer(layer, &layerGroup) && layerGroup) {
						CollectAllPathsFromArt(layerGroup, outPaths);
					}
				}
			}
			AILayerHandle next = nullptr;
			if (sAILayer->GetNextLayer(layer, &next)) {
				break;
			}
			layer = next;
		}
	}

	bool BuildProcessPathEntry(AIArtHandle art, DuctworkPath& outEntry)
	{
		std::vector<DuctworkPoint> points;
		bool closed = false;
		if (!DuctworkGeometry::GetPathPoints(art, points, closed)) {
			return false;
		}

		outEntry.art = art;
		outEntry.points = points;
		outEntry.closed = closed;
		outEntry.layerName = DuctworkGeometry::GetArtLayerName(art);
		return DuctworkGeometry::IsCenterlineCandidate(art, outEntry.points, outEntry.closed, outEntry.layerName);
	}

	void ExpandPathBounds(const DuctworkPath& path,
		double& minX,
		double& minY,
		double& maxX,
		double& maxY,
		bool& boundsInit)
	{
		for (size_t p = 0; p < path.points.size(); ++p) {
			if (!boundsInit) {
				minX = maxX = path.points[p].x;
				minY = maxY = path.points[p].y;
				boundsInit = true;
			} else {
				if (path.points[p].x < minX) minX = path.points[p].x;
				if (path.points[p].x > maxX) maxX = path.points[p].x;
				if (path.points[p].y < minY) minY = path.points[p].y;
				if (path.points[p].y > maxY) maxY = path.points[p].y;
			}
		}
	}

	bool SavePreOrthoCopy(AIDocumentHandle document)
	{
		if (!sAIDocument || !sAIFilePath || !sAIFileFormat || !document) {
			return false;
		}

		ai::FilePath fileSpec;
		if (sAIDocument->GetDocumentFileSpecificationFromHandle(document, fileSpec) != kNoErr) {
			DuctworkLog::Write("PreOrtho: no document file path");
			return false;
		}

		ai::UnicodeString fullPath;
		if (sAIFilePath->GetFullPath(fileSpec, false, fullPath) != kNoErr) {
			DuctworkLog::Write("PreOrtho: failed to read full path");
			return false;
		}

		std::filesystem::path sourcePath(fullPath.as_UTF8().c_str());
		if (sourcePath.empty()) {
			DuctworkLog::Write("PreOrtho: empty source path");
			return false;
		}

		std::filesystem::path targetPath =
			sourcePath.parent_path() /
			(sourcePath.stem().string() + "_preortho" + sourcePath.extension().string());

		ai::UnicodeString targetPathStr = ai::UnicodeString::FromUTF8(targetPath.string());
		ai::FilePath targetFile(targetPathStr);

		AIFileFormatHandle formatHandle = nullptr;
		if (sAIDocument->GetDocumentFileFormat(&formatHandle) != kNoErr || !formatHandle) {
			DuctworkLog::Write("PreOrtho: failed to get document format");
			return false;
		}

		const char* formatName = nullptr;
		if (sAIFileFormat->GetFileFormatName(formatHandle, &formatName) != kNoErr || !formatName) {
			DuctworkLog::Write("PreOrtho: failed to get format name");
			return false;
		}

		ASErr err = sAIDocument->WriteDocument(targetFile, formatName, false);
		if (err != kNoErr) {
			DuctworkLog::Write("PreOrtho: write failed");
			DuctworkLog::Error("PreOrtho WriteDocument", err);
			return false;
		}

		DuctworkLog::Write("PreOrtho: saved copy " + targetPath.string());
		return true;
	}

	bool SnapThermostatEndpoints(std::vector<DuctworkPath>& paths, double snapDistance)
	{
		if (!sAIPath || !sAIArt) {
			return false;
		}
		const double snapDist2 = snapDistance * snapDistance;
		std::vector<DuctworkPoint> blueEndpoints;
		for (size_t i = 0; i < paths.size(); ++i) {
			const DuctworkPath& path = paths[i];
			if (path.layerName != "Blue Ductwork" || path.points.size() < 2 || path.closed) {
				continue;
			}
			blueEndpoints.push_back(path.points.front());
			blueEndpoints.push_back(path.points.back());
		}
		if (blueEndpoints.empty()) {
			return false;
		}

		bool changedAny = false;
		for (size_t i = 0; i < paths.size(); ++i) {
			DuctworkPath& path = paths[i];
			if (path.layerName != "Thermostat Lines" || path.points.size() < 2 || path.closed) {
				continue;
			}

			bool changed = false;
			const size_t lastIdx = path.points.size() - 1;
			size_t endpointIndices[2] = { 0, lastIdx };
			for (int e = 0; e < 2; ++e) {
				const size_t idx = endpointIndices[e];
				const DuctworkPoint& endpoint = path.points[idx];
				double bestDist2 = snapDist2;
				DuctworkPoint bestPoint = endpoint;
				for (size_t b = 0; b < blueEndpoints.size(); ++b) {
					const double dist2 = DuctworkMath::Dist2(endpoint, blueEndpoints[b]);
					if (dist2 <= bestDist2) {
						bestDist2 = dist2;
						bestPoint = blueEndpoints[b];
					}
				}
				if (bestDist2 <= snapDist2 && (bestPoint.x != endpoint.x || bestPoint.y != endpoint.y)) {
					path.points[idx] = bestPoint;
					changed = true;
				}
			}

			if (changed) {
				changedAny = true;
				const size_t pointCount = path.points.size();
				ai::int16 count = 0;
				if (sAIPath->GetPathSegmentCount(path.art, &count) == kNoErr &&
					count == static_cast<ai::int16>(pointCount)) {
					std::vector<AIPathSegment> segments(static_cast<size_t>(count));
					if (sAIPath->GetPathSegments(path.art, 0, count, &segments[0]) == kNoErr) {
						// Only update the endpoints that were snapped, preserving handles
						for (int e = 0; e < 2; ++e) {
							const ai::int16 segIndex = (e == 0) ? 0 : static_cast<ai::int16>(count - 1);
							const DuctworkPoint& newPt = path.points[static_cast<size_t>(segIndex)];
							AIRealPoint oldAnchor = segments[segIndex].p;
							AIReal dx = static_cast<AIReal>(newPt.x) - oldAnchor.h;
							AIReal dy = static_cast<AIReal>(newPt.y) - oldAnchor.v;
							// Move anchor point
							segments[segIndex].p.h = static_cast<AIReal>(newPt.x);
							segments[segIndex].p.v = static_cast<AIReal>(newPt.y);
							// Preserve relative handle positions (keeps curves intact)
							segments[segIndex].in.h += dx;
							segments[segIndex].in.v += dy;
							segments[segIndex].out.h += dx;
							segments[segIndex].out.v += dy;
							// Do NOT change corner flag - preserve curve type
						}
						sAIPath->SetPathSegments(path.art, 0, count, &segments[0]);
					}
				}
			}
		}
		return changedAny;
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

	void AppendUnitEndpointPairs(const std::vector<DuctworkPath>& paths,
		double closeDist,
		std::vector<DuctworkConnection>& outConnections)
	{
		const double closeDist2 = closeDist * closeDist;
		for (size_t i = 0; i < paths.size(); ++i) {
			const DuctworkPath& a = paths[i];
			if (a.closed || a.points.size() < 2) {
				continue;
			}
			const size_t aEnd = a.points.size() - 1;
			const DuctworkPoint aPts[2] = { a.points[0], a.points[aEnd] };
			for (size_t j = i + 1; j < paths.size(); ++j) {
				const DuctworkPath& b = paths[j];
				if (b.closed || b.points.size() < 2) {
					continue;
				}
				if (!IsUnitPairLayerName(a.layerName, b.layerName)) {
					continue;
				}
				const size_t bEnd = b.points.size() - 1;
				const DuctworkPoint bPts[2] = { b.points[0], b.points[bEnd] };
				const int aIdx[2] = { 0, static_cast<int>(aEnd) };
				const int bIdx[2] = { 0, static_cast<int>(bEnd) };
				for (int ai = 0; ai < 2; ++ai) {
					for (int bi = 0; bi < 2; ++bi) {
						if (DuctworkMath::Dist2(aPts[ai], bPts[bi]) > closeDist2) {
							continue;
						}
						DuctworkConnection conn;
						conn.a = static_cast<int>(i);
						conn.b = static_cast<int>(j);
						conn.type = kConnectionEndpointToEndpoint;
						conn.segA = -1;
						conn.segB = -1;
						conn.endpointA = aIdx[ai];
						conn.endpointB = bIdx[bi];
						conn.point.x = (aPts[ai].x + bPts[bi].x) * 0.5;
						conn.point.y = (aPts[ai].y + bPts[bi].y) * 0.5;
						outConnections.push_back(conn);
					}
				}
			}
		}
	}

	struct EndpointFlags
	{
		bool any[2] = { false, false };
		bool endpoint[2] = { false, false };
	};

	void CollectEndpointFlagsForSelection(const std::vector<DuctworkPath>& selected,
		const std::vector<DuctworkPath>& allPaths,
		const std::vector<DuctworkConnection>& allConnections,
		std::vector<EndpointFlags>& outFlags)
	{
		outFlags.assign(selected.size(), EndpointFlags{});
		if (selected.empty() || allPaths.empty() || allConnections.empty()) {
			return;
		}

		std::unordered_map<AIArtHandle, size_t> selectedIndex;
		for (size_t i = 0; i < selected.size(); ++i) {
			if (selected[i].art) {
				selectedIndex[selected[i].art] = i;
			}
		}

		auto markEndpoint = [&](int pathIndex, int endpointIndex, DuctworkConnectionType type) {
			if (pathIndex < 0 || pathIndex >= static_cast<int>(allPaths.size())) {
				return;
			}
			if (endpointIndex < 0) {
				return;
			}
			AIArtHandle art = allPaths[static_cast<size_t>(pathIndex)].art;
			if (!art) {
				return;
			}
			std::unordered_map<AIArtHandle, size_t>::const_iterator it = selectedIndex.find(art);
			if (it == selectedIndex.end()) {
				return;
			}
			const size_t selIdx = it->second;
			const size_t endIndex = selected[selIdx].points.empty() ? 0 : (selected[selIdx].points.size() - 1);
			int endpointSlot = -1;
			if (endpointIndex == 0) {
				endpointSlot = 0;
			} else if (endpointIndex == static_cast<int>(endIndex)) {
				endpointSlot = 1;
			} else {
				return;
			}
			outFlags[selIdx].any[endpointSlot] = true;
			if (type == kConnectionEndpointToEndpoint) {
				outFlags[selIdx].endpoint[endpointSlot] = true;
			}
		};

		for (size_t i = 0; i < allConnections.size(); ++i) {
			const DuctworkConnection& conn = allConnections[i];
			if (conn.endpointA >= 0) {
				markEndpoint(conn.a, conn.endpointA, conn.type);
			}
			if (conn.endpointB >= 0) {
				markEndpoint(conn.b, conn.endpointB, conn.type);
			}
		}
	}

	void AppendExternalEndpointConnections(const std::vector<DuctworkPath>& selected,
		const std::vector<EndpointFlags>& flags,
		std::vector<DuctworkConnection>& outConnections)
	{
		for (size_t i = 0; i < selected.size(); ++i) {
			if (selected[i].points.size() < 2) {
				continue;
			}
			for (int endpointSlot = 0; endpointSlot < 2; ++endpointSlot) {
				if (!flags[i].any[endpointSlot]) {
					continue;
				}
				DuctworkConnection conn;
				conn.a = static_cast<int>(i);
				conn.b = -1;
				conn.type = flags[i].endpoint[endpointSlot] ? kConnectionEndpointToEndpoint : kConnectionEndpointToSegment;
				conn.segA = -1;
				conn.segB = -1;
				conn.endpointA = (endpointSlot == 0) ? 0 : static_cast<int>(selected[i].points.size() - 1);
				conn.endpointB = -1;
				conn.point = (endpointSlot == 0) ? selected[i].points.front() : selected[i].points.back();
				outConnections.push_back(conn);
			}
		}
	}

	void WriteDuctRoleMetadata(const std::vector<DuctworkPath>& selected,
		const std::vector<EndpointFlags>& flags,
		const std::vector<DuctworkPath>& allPaths,
		const std::vector<DuctworkConnection>& allConnections)
	{
		for (size_t i = 0; i < selected.size(); ++i) {
			AIArtHandle art = selected[i].art;
			if (!art) {
				continue;
			}
			const std::string& layerName = selected[i].layerName;
			bool isGreenLayer = (layerName == "Green Ductwork" || layerName == "Light Green Ductwork");
			if (isGreenLayer) {
				bool hasBlueEndpointJoin = false;
				const double maxDist = 2.0;
				const double maxDist2 = maxDist * maxDist;
				for (size_t p = 0; p < allPaths.size(); ++p) {
					if (allPaths[p].layerName != "Blue Ductwork") {
						continue;
					}
					if (allPaths[p].points.size() < 2) {
						continue;
					}
					const DuctworkPoint bluePts[2] = { allPaths[p].points.front(), allPaths[p].points.back() };
					const DuctworkPoint greenPts[2] = { selected[i].points.front(), selected[i].points.back() };
					for (int gi = 0; gi < 2 && !hasBlueEndpointJoin; ++gi) {
						for (int bi = 0; bi < 2; ++bi) {
							if (DuctworkMath::Dist2(greenPts[gi], bluePts[bi]) <= maxDist2) {
								hasBlueEndpointJoin = true;
								break;
							}
						}
					}
					if (hasBlueEndpointJoin) {
						break;
					}
				}

				const std::string role = hasBlueEndpointJoin ? "branch" : "trunk";
				DuctworkMetadata::SetString(art, "ductRole", role);
				DuctworkMetadata::SetString(art, "ductRoleReason", "blue-endpoint");
				DuctworkMetadata::SetDouble(art, "ductRoleVersion", 1.0);
				continue;
			}

			const bool hasEndpointConnection = flags[i].endpoint[0] || flags[i].endpoint[1];
			const std::string role = hasEndpointConnection ? "trunk" : "branch";
			DuctworkMetadata::SetString(art, "ductRole", role);
			DuctworkMetadata::SetString(art, "ductRoleReason", "endpoint-connection");
			DuctworkMetadata::SetDouble(art, "ductRoleVersion", 1.0);
		}
	}

	void FinalizeArtCaches(const std::vector<AIArtHandle>& arts)
	{
		if (!sAIArt) {
			return;
		}
		for (size_t i = 0; i < arts.size(); ++i) {
			if (!arts[i]) {
				continue;
			}
			sAIArt->SetArtBounds(arts[i]);
			sAIArt->SetArtUserAttr(arts[i], kArtStyleIsDirty, 0);
		}
	}

	std::map<std::string, std::string> ParseKeyValuePayload(const std::string& payload)
	{
		std::map<std::string, std::string> result;
		std::stringstream ss(payload);
		std::string token;
		while (std::getline(ss, token, ';')) {
			if (token.empty()) {
				continue;
			}
			const size_t sep = token.find('=');
			if (sep == std::string::npos) {
				continue;
			}
			std::string key = token.substr(0, sep);
			std::string value = token.substr(sep + 1);
			if (!key.empty()) {
				result[key] = value;
			}
		}
		return result;
	}

	double ParseDouble(const std::map<std::string, std::string>& data, const char* key, double fallback)
	{
		std::map<std::string, std::string>::const_iterator it = data.find(key);
		if (it == data.end()) {
			return fallback;
		}
		try {
			return std::stod(it->second);
		} catch (...) {
			return fallback;
		}
	}

	bool ParseBool(const std::map<std::string, std::string>& data, const char* key, bool fallback)
	{
		std::map<std::string, std::string>::const_iterator it = data.find(key);
		if (it == data.end()) {
			return fallback;
		}
		const std::string value = it->second;
		if (value == "1" || value == "true" || value == "True") {
			return true;
		}
		if (value == "0" || value == "false" || value == "False") {
			return false;
		}
		return fallback;
	}
}

Plugin* AllocatePlugin(SPPluginRef pluginRef)
{
	DuctworkLog::Write("AllocatePlugin");
	return new ProcessDuctworkPlugin(pluginRef);
}

void FixupReload(Plugin* plugin)
{
	ProcessDuctworkPlugin::FixupVTable((ProcessDuctworkPlugin*)plugin);
}

namespace {
	ProcessDuctworkPlugin* gProcessDuctworkPlugin = nullptr;
}

ProcessDuctworkPlugin::ProcessDuctworkPlugin(SPPluginRef pluginRef)
	: Plugin(pluginRef),
	fProcessMenuItem(NULL),
	fProcessNoCompoundMenuItem(NULL),
	fProcessNoStylesMenuItem(NULL),
	fProcessNoPartsMenuItem(NULL),
	fProcessNoGraphicsMenuItem(NULL),
	fProcessNoMetaMenuItem(NULL),
	fProcessDirectPlaceMenuItem(NULL),
	fProcessPlacedApiMenuItem(NULL),
	fSelectPartsMenuItem(NULL),
	fDuctworkMenuItem(NULL),
	fPanelMenuItem(NULL),
	fAboutPluginMenu(NULL),
	fLastOptions(),
	fSelectionChangedNotifier(NULL),
	fPanel(),
	fGapToggleTool(NULL),
	fGapHealTool(NULL),
	fCursorResourceManager(NULL),
	fGapPreviewAnnotator(NULL),
	fGapPreviewView(NULL),
	fGapPreviewVisible(false),
	fGapPreviewShowHeal(false),
	fGapPreviewShowGap(false),
	fGapPreviewIsRegister(false),
	fGapPreviewStart(),
	fGapPreviewEnd(),
	fGapPreviewBounds(),
	fGapPreviewHoverArt(nullptr),
	fGapPreviewLastHoverArt(nullptr)
{
	strncpy(fPluginName, kProcessDuctworkPluginName, kMaxStringLength);
	gProcessDuctworkPlugin = this;
}

ProcessDuctworkPlugin* ProcessDuctworkPlugin::GetInstance()
{
	return gProcessDuctworkPlugin;
}

ASErr ProcessDuctworkPlugin::RunProcessPlacedApiFromPanel()
{
	DuctworkLog::Write("Panel process placed api");
	ProcessDuctworkOptions defaults;
	double rotationOverride = 0.0;
	if (fPanel.GetRotationOverrideValue(rotationOverride)) {
		defaults.hasRotationOverride = true;
		defaults.rotationOverride = rotationOverride;
	}
	defaults.skipPlacedMetadata = false;
	defaults.placedApiGraphics = true;
	ai::UnicodeString outMsg;
	return ProcessDuctwork(defaults, true, &outMsg);
}

ASErr ProcessDuctworkPlugin::StartupPlugin(SPInterfaceMessage* message)
{
	DuctworkLog::Write("StartupPlugin begin");
	DuctworkLog::Write("BuildId: 2026-01-19-PanelTransform-v28");
	ASErr error = Plugin::StartupPlugin(message);
	if (!error) {
		error = AddMenus(message);
	}
	if (!error) {
		error = AddTools(message);
	}
	if (!error && sAIAnnotator) {
		if (!fGapPreviewAnnotator) {
			ASErr annotatorErr = sAIAnnotator->AddAnnotator(message->d.self, "Ductwork Gap Preview", &fGapPreviewAnnotator);
			if (annotatorErr) {
				DuctworkLog::Error("AddAnnotator GapPreview", annotatorErr);
			} else {
				sAIAnnotator->SetAnnotatorActive(fGapPreviewAnnotator, true);
				DuctworkLog::Write("AddAnnotator GapPreview ok");
			}
		}
	}
	if (!error) {
		error = fPanel.Create(message->d.self);
	}
	if (!error && sAINotifier) {
		sAINotifier->AddNotifier(fPluginRef, "ProcessDuctworkSelection", kAIArtSelectionChangedNotifier, &fSelectionChangedNotifier);
	}
	DuctworkLog::Write("StartupPlugin end");
	return error;
}

ASErr ProcessDuctworkPlugin::PostStartupPlugin()
{
	ASErr error = Plugin::PostStartupPlugin();
	if (!error && sAIUser && !fCursorResourceManager) {
		sAIUser->CreateCursorResourceMgr(fPluginRef, &fCursorResourceManager);
	}
	return error;
}

ASErr ProcessDuctworkPlugin::ShutdownPlugin(SPInterfaceMessage* message)
{
	ClearGapPreview();
	if (fGapPreviewAnnotator && sAIAnnotator) {
		sAIAnnotator->SetAnnotatorActive(fGapPreviewAnnotator, false);
	}
	if (fCursorResourceManager && sAIUser) {
		sAIUser->DisposeCursorResourceMgr(fCursorResourceManager);
		fCursorResourceManager = nullptr;
	}
	return Plugin::ShutdownPlugin(message);
}

ASErr ProcessDuctworkPlugin::Message(char* caller, char* selector, void* message)
{
	ASErr error = kNoErr;

	try {
		if (strcmp(caller, kCallerAIAnnotation) == 0) {
			AIAnnotatorMessage* msg = reinterpret_cast<AIAnnotatorMessage*>(message);
			if (msg && msg->annotator == fGapPreviewAnnotator) {
				if (strcmp(selector, kSelectorAIDrawAnnotation) == 0) {
					return DrawGapPreview(msg);
				}
				if (strcmp(selector, kSelectorAIInvalAnnotation) == 0) {
					return InvalidateGapPreview(msg);
				}
			}
		}
		if (strcmp(caller, kCallerAITool) == 0) {
			AIToolMessage* toolMsg = reinterpret_cast<AIToolMessage*>(message);
			if (toolMsg && strcmp(selector, kSelectorAIDeselectTool) == 0) {
				if (toolMsg->tool == fGapToggleTool || toolMsg->tool == fGapHealTool) {
					ClearGapPreview();
				}
			}
		}
		if (strcmp(caller, kCallerAIScriptMessage) == 0) {
			AIScriptMessage* msg = (AIScriptMessage*)message;
			if (strcmp(selector, kProcessDuctworkScriptSelector) == 0) {
				fLastOptions = ProcessDuctworkOptions();
				std::string json = msg->inParam.as_UTF8();
				if (!json.empty()) {
					fLastOptions.ParseFromJson(json);
				}
				ai::UnicodeString outMsg;
				error = ProcessDuctwork(fLastOptions, false, &outMsg);
				msg->outParam = outMsg;
				return error;
			}
			if (strcmp(selector, kEmoryDuctworkPanelScriptSelector) == 0) {
				const std::string payload = msg->inParam.as_UTF8();
				const std::map<std::string, std::string> data = ParseKeyValuePayload(payload);
				std::string action;
				std::map<std::string, std::string>::const_iterator it = data.find("action");
				if (it != data.end()) {
					action = it->second;
				}

				if (action == "transform") {
					const double targetScale = ParseDouble(data, "scale", 100.0);
					const double targetRotation = ParseDouble(data, "rotation", 0.0);
					const bool livePreview = ParseBool(data, "live", false);
					const bool scaleDirty = ParseBool(data, "scaleDirty", false);
					const bool rotateDirty = ParseBool(data, "rotateDirty", false);
					fPanel.SetTransformDirtyFlags(scaleDirty, rotateDirty);
					std::string messageText;
					const bool ok = fPanel.ApplyTransformSelection(targetScale, targetRotation, false, false, livePreview, &messageText);
					if (!livePreview) {
						fPanel.UpdateSelectionSummary();
					}
					std::ostringstream out;
					out << "{\"ok\":" << (ok ? "true" : "false")
						<< ",\"message\":\"" << messageText << "\"}";
					msg->outParam = ai::UnicodeString::FromUTF8(out.str());
					return kNoErr;
				}

				if (action == "get-angle") {
					double angle = 0.0;
					const bool ok = fPanel.TryComputeSelectionAngle(angle);
					std::ostringstream out;
					if (ok) {
						out << "{\"ok\":true,\"angle\":" << angle << ",\"message\":\"Angle: " << angle << "\"}";
					} else {
						out << "{\"ok\":false,\"message\":\"Please select a line.\"}";
					}
					msg->outParam = ai::UnicodeString::FromUTF8(out.str());
					return kNoErr;
				}

				if (action == "set-override") {
					const double overrideAngle = ParseDouble(data, "value", 0.0);
					fPanel.SetRotationOverrideValue(overrideAngle, true);
					msg->outParam = ai::UnicodeString::FromUTF8("{\"ok\":true,\"message\":\"Rotation override set.\"}");
					return kNoErr;
				}

				if (action == "clear-override") {
					fPanel.SetRotationOverrideValue(0.0, false);
					msg->outParam = ai::UnicodeString::FromUTF8("{\"ok\":true,\"message\":\"Rotation override cleared.\"}");
					return kNoErr;
				}

				if (action == "reset-strokes") {
					fPanel.ResetStrokes();
					fPanel.UpdateSelectionSummary();
					msg->outParam = ai::UnicodeString::FromUTF8("{\"ok\":true,\"message\":\"Strokes reset.\"}");
					return kNoErr;
				}

				if (action == "reset-scale") {
					fPanel.ResetScale();
					fPanel.UpdateSelectionSummary();
					msg->outParam = ai::UnicodeString::FromUTF8("{\"ok\":true,\"message\":\"Scale reset.\"}");
					return kNoErr;
				}

				if (action == "reset-rotation") {
					fPanel.ResetRotation();
					fPanel.UpdateSelectionSummary();
					msg->outParam = ai::UnicodeString::FromUTF8("{\"ok\":true,\"message\":\"Rotation reset.\"}");
					return kNoErr;
				}

				if (action == "reset-original") {
					fPanel.ResetTransformToOriginal();
					fPanel.UpdateSelectionSummary();
					msg->outParam = ai::UnicodeString::FromUTF8("{\"ok\":true,\"message\":\"Reset to original.\"}");
					return kNoErr;
				}

				if (action == "quick-rotate") {
					const double angle = ParseDouble(data, "value", 0.0);
					fPanel.ApplyQuickRotate(angle);
					fPanel.UpdateSelectionSummary();
					std::ostringstream out;
					out << "{\"ok\":true,\"message\":\"Rotated " << angle << " degrees.\"}";
					msg->outParam = ai::UnicodeString::FromUTF8(out.str());
					return kNoErr;
				}

				if (action == "process-placed-api") {
					ProcessDuctworkOptions defaults;
					double rotationOverride = 0.0;
					if (fPanel.GetRotationOverrideValue(rotationOverride)) {
						defaults.hasRotationOverride = true;
						defaults.rotationOverride = rotationOverride;
					}
					if (data.find("skipOrtho") != data.end()) {
						defaults.skipOrtho = ParseBool(data, "skipOrtho", defaults.skipOrtho);
					}
					if (data.find("skipAllBranchSegments") != data.end()) {
						defaults.skipAllBranchSegments = ParseBool(data, "skipAllBranchSegments", defaults.skipAllBranchSegments);
					}
					if (data.find("skipFinalRegisterSegment") != data.end()) {
						defaults.skipFinalRegisterSegment = ParseBool(data, "skipFinalRegisterSegment", defaults.skipFinalRegisterSegment);
					}
					if (data.find("skipRegisterRotation") != data.end()) {
						defaults.skipRegisterRotation = ParseBool(data, "skipRegisterRotation", defaults.skipRegisterRotation);
					}
					if (data.find("enableRegisterCarve") != data.end()) {
						defaults.enableRegisterCarve = ParseBool(data, "enableRegisterCarve", defaults.enableRegisterCarve);
					}
					if (data.find("enableOverlapCarve") != data.end()) {
						defaults.enableOverlapCarve = ParseBool(data, "enableOverlapCarve", defaults.enableOverlapCarve);
					}
					defaults.placedApiGraphics = true;
					ai::UnicodeString outMsg;
					const ASErr processErr = ProcessDuctwork(defaults, true, &outMsg);
					std::ostringstream out;
					if (processErr == kNoErr) {
						out << "{\"ok\":true,\"message\":\""
							<< outMsg.as_UTF8()
							<< "\"}";
					} else {
						out << "{\"ok\":false,\"message\":\"Process failed.\"}";
					}
					msg->outParam = ai::UnicodeString::FromUTF8(out.str());
					return kNoErr;
				}

				if (action == "toggle-connector-style") {
					std::string messageText;
					const bool ok = DuctworkGeometry::ToggleSelectedEmoryConnectorStyles(messageText);
					std::ostringstream out;
					out << "{\"ok\":" << (ok ? "true" : "false")
						<< ",\"message\":\"" << messageText << "\"}";
					msg->outParam = ai::UnicodeString::FromUTF8(out.str());
					return kNoErr;
				}

				if (action == "get-selected-anchors") {
					// Use C++ SDK GetPathSegmentSelected to detect which specific
					// anchor points are selected via the Direct Selection tool.
					// ExtendScript can't reliably detect this on all paths.
					std::ostringstream json;
					json << "{\"ok\":true,\"points\":[";
					int pointCount = 0;

					if (sAIPath && sAIArt && sAIArtSet) {
						std::vector<AIArtHandle> selectedPaths;
						DuctworkSelection::CollectSelectedPaths(selectedPaths);

						for (size_t artIdx = 0; artIdx < selectedPaths.size(); ++artIdx) {
							AIArtHandle art = selectedPaths[artIdx];
							if (!art) continue;

							ai::int16 segCount = 0;
							if (sAIPath->GetPathSegmentCount(art, &segCount) || segCount < 1) continue;

							// Get selection state of each segment
							std::vector<ai::int16> selStates(static_cast<size_t>(segCount));
							if (sAIPath->GetPathSegmentsSelected(art, 0, segCount, &selStates[0])) continue;

							// Get segment positions
							std::vector<AIPathSegment> segs(static_cast<size_t>(segCount));
							if (sAIPath->GetPathSegments(art, 0, segCount, &segs[0])) continue;

							// Get layer name
							std::string layerName;
							AILayerHandle layer = nullptr;
							if (sAIArt->GetLayerOfArt(art, &layer) == kNoErr && layer) {
								ai::UnicodeString title;
								if (sAILayer->GetLayerTitle(layer, title) == kNoErr) {
									layerName = title.as_UTF8();
								}
							}

							for (ai::int16 segIdx = 0; segIdx < segCount; ++segIdx) {
								// kSegmentPointSelected (1) means anchor point is selected
								if (selStates[segIdx] == kSegmentPointSelected ||
									selStates[segIdx] == kSegmentInAndOutSelected) {
									if (pointCount > 0) json << ",";
									json << "{\"x\":" << segs[segIdx].p.h
										 << ",\"y\":" << segs[segIdx].p.v
										 << ",\"layer\":\"" << layerName << "\"}";
									++pointCount;
								}
							}
						}
					}
					json << "],\"count\":" << pointCount << "}";
					msg->outParam = ai::UnicodeString::FromUTF8(json.str());
					return kNoErr;
				}

				msg->outParam = ai::UnicodeString::FromUTF8("{\"ok\":false,\"message\":\"Unknown action.\"}");
				return kNoErr;
			}
		}

		error = Plugin::Message(caller, selector, message);
	}
	catch (ai::Error& ex) {
		error = ex;
	}
	catch (...) {
		error = kCantHappenErr;
	}

	if (error) {
		if (error == kUnhandledMsgErr) {
			error = kNoErr;
		}
		else {
			Plugin::ReportError(error, caller, selector, message);
		}
	}

	return error;
}

ASErr ProcessDuctworkPlugin::AddMenus(SPInterfaceMessage* message)
{
	ASErr error = kNoErr;

	SDKAboutPluginsHelper aboutPluginsHelper;
	aboutPluginsHelper.AddAboutPluginsMenuItem(
		message,
		kSDKDefAboutSDKCompanyPluginsGroupName,
		ai::UnicodeString(kSDKDefAboutSDKCompanyPluginsGroupNameString),
		"Process Emory Ductwork...",
		&fAboutPluginMenu);

	AIMenuGroup ductworkObjectTopGroup = nullptr;
	error = sAIMenu->AddMenuGroup("DuctworkObjectTopGroup", kMenuGroupAddAboveNearGroup, kObjectUtilsMenuGroup, &ductworkObjectTopGroup);
	if (error) {
		DuctworkLog::Error("AddMenuGroup DuctworkObjectTopGroup", error);
		return error;
	}

	AIPlatformAddMenuItemDataUS menuData;
	menuData.groupName = "DuctworkObjectTopGroup";
	const bool enableLegacyProcessMenu = false;
	if (enableLegacyProcessMenu) {
		menuData.itemText = ai::UnicodeString::FromUTF8(kProcessDuctworkMenuItem);
		error = sAIMenu->AddMenuItem(message->d.self, "EmoryDuctwork", &menuData, 0, &fProcessMenuItem);
		if (!error && fProcessMenuItem) {
			sAIMenu->UpdateMenuItemAutomatically(fProcessMenuItem,
				kAutoEnableMenuItemAction,
				0, 0,
				0, 0,
				kIfOpenDocument,
				0);
		}
	}
	menuData.itemText = ai::UnicodeString::FromUTF8(kSelectDuctworkPartsMenuItem);
	error = sAIMenu->AddMenuItem(message->d.self, "SelectDuctworkParts", &menuData, 0, &fSelectPartsMenuItem);
	if (!error && fSelectPartsMenuItem) {
		sAIMenu->UpdateMenuItemAutomatically(fSelectPartsMenuItem,
			kAutoEnableMenuItemAction,
			0, 0,
			0, 0,
			kIfOpenDocument,
			0);
	}

	menuData.itemText = ai::UnicodeString::FromUTF8(kProcessDuctworkPlacedApiMenuItem);
	error = sAIMenu->AddMenuItem(message->d.self, "ProcessDuctworkPlacedApi", &menuData, 0, &fProcessPlacedApiMenuItem);
	if (!error && fProcessPlacedApiMenuItem) {
		sAIMenu->UpdateMenuItemAutomatically(fProcessPlacedApiMenuItem,
			kAutoEnableMenuItemAction,
			0, 0,
			0, 0,
			kIfOpenDocument,
			0);
	}

	if (!error) {
		error = sAIMenu->AddMenuItemZString(message->d.self, "Ductwork Panel", kOtherPalettesMenuGroup,
			ZREF("Ductwork Panel"), kMenuItemNoOptions, &fPanelMenuItem);
		if (!error && fPanelMenuItem) {
			sAIMenu->UpdateMenuItemAutomatically(fPanelMenuItem,
				kAutoEnableMenuItemAction,
				0, 0,
				0, 0,
				kIfOpenDocument,
				0);
		}
	}

	return error;
}

ASErr ProcessDuctworkPlugin::AddTools(SPInterfaceMessage* message)
{
	if (!sAITool) {
		return kNoErr;
	}

	ASErr error = kNoErr;
	ai::int32 options = kToolWantsToTrackCursorOption;

	AIAddToolData toggleData;
	toggleData.title = ai::UnicodeString::FromUTF8("Ductwork Gap Toggle");
	toggleData.tooltip = ai::UnicodeString::FromUTF8("Toggle carved gap at clicked intersection");
	toggleData.sameGroupAs = kNoTool;
	toggleData.sameToolsetAs = kNoTool;
	toggleData.normalIconResID = kDuctworkGapToggleToolIconResourceID;
	toggleData.darkIconResID = kDuctworkGapToggleToolIconResourceID;
	toggleData.iconType = ai::IconType::kSVG;

	error = sAITool->AddTool(message->d.self, kDuctworkGapToggleToolName, toggleData, options, &fGapToggleTool);
	if (error) {
		DuctworkLog::Error("AddTool GapToggle", error);
		fGapToggleTool = nullptr;
		error = kNoErr;
	}
	if (fGapToggleTool) {
		DuctworkLog::Write("AddTool GapToggle ok");
	}

	AIToolType gapToggleToolNumber = kNoTool;
	if (fGapToggleTool) {
		ASErr gapToggleLookup = sAITool->GetToolNumberFromHandle(fGapToggleTool, &gapToggleToolNumber);
		if (gapToggleLookup != kNoErr) {
			gapToggleToolNumber = kNoTool;
			DuctworkLog::Error("AddTool GapToggle lookup", gapToggleLookup);
		} else {
			DuctworkLog::Write("AddTool GapToggle number=" + std::to_string(static_cast<int>(gapToggleToolNumber)));
		}
	}

	AIAddToolData healData;
	healData.title = ai::UnicodeString::FromUTF8("Ductwork Gap Heal/Create");
	healData.tooltip = ai::UnicodeString::FromUTF8("Heal or create gap at clicked intersection");
	healData.sameGroupAs = (gapToggleToolNumber != kNoTool) ? gapToggleToolNumber : kNoTool;
	healData.sameToolsetAs = (gapToggleToolNumber != kNoTool) ? gapToggleToolNumber : kNoTool;
	healData.normalIconResID = kDuctworkGapHealToolIconResourceID;
	healData.darkIconResID = kDuctworkGapHealToolIconResourceID;
	healData.iconType = ai::IconType::kSVG;

	error = sAITool->AddTool(message->d.self, kDuctworkGapHealToolName, healData, options, &fGapHealTool);
	if (error) {
		DuctworkLog::Error("AddTool GapHeal", error);
		fGapHealTool = nullptr;
		error = kNoErr;
	} else {
		DuctworkLog::Write("AddTool GapHeal ok");
	}
	return error;
}

ASErr ProcessDuctworkPlugin::GoMenuItem(AIMenuMessage* message)
{
	if (message->menuItem == fAboutPluginMenu) {
		DuctworkLog::Write("GoMenuItem about");
		SDKAboutPluginsHelper aboutPluginsHelper;
		aboutPluginsHelper.PopAboutBox(message, "About Emory Ductwork", kSDKDefAboutSDKCompanyPluginsAlertString);
		return kNoErr;
	}

	if (message->menuItem == fPanelMenuItem) {
		AIPanelRef panel = fPanel.GetPanel();
		if (panel && sAIPanel) {
			AIBoolean shown = false;
			if (sAIPanel->IsShown(panel, shown) == kNoErr) {
				sAIPanel->Show(panel, shown ? false : true);
			} else {
				sAIPanel->Show(panel, true);
			}
		}
		return kNoErr;
	}

	if (message->menuItem == fProcessMenuItem) {
		DuctworkLog::Write("GoMenuItem process");
		ProcessDuctworkOptions defaults;
		double rotationOverride = 0.0;
		if (fPanel.GetRotationOverrideValue(rotationOverride)) {
			defaults.hasRotationOverride = true;
			defaults.rotationOverride = rotationOverride;
		}
		ai::UnicodeString outMsg;
		return ProcessDuctwork(defaults, true, &outMsg);
	}

	if (message->menuItem == fProcessNoCompoundMenuItem) {
		DuctworkLog::Write("GoMenuItem process no compound");
		ProcessDuctworkOptions defaults;
		double rotationOverride = 0.0;
		if (fPanel.GetRotationOverrideValue(rotationOverride)) {
			defaults.hasRotationOverride = true;
			defaults.rotationOverride = rotationOverride;
		}
		defaults.skipCompounding = true;
		ai::UnicodeString outMsg;
		return ProcessDuctwork(defaults, true, &outMsg);
	}

	if (message->menuItem == fProcessNoStylesMenuItem) {
		DuctworkLog::Write("GoMenuItem process no styles");
		ProcessDuctworkOptions defaults;
		double rotationOverride = 0.0;
		if (fPanel.GetRotationOverrideValue(rotationOverride)) {
			defaults.hasRotationOverride = true;
			defaults.rotationOverride = rotationOverride;
		}
		defaults.skipStyles = true;
		ai::UnicodeString outMsg;
		return ProcessDuctwork(defaults, true, &outMsg);
	}

	if (message->menuItem == fProcessNoPartsMenuItem) {
		DuctworkLog::Write("GoMenuItem process no parts");
		ProcessDuctworkOptions defaults;
		double rotationOverride = 0.0;
		if (fPanel.GetRotationOverrideValue(rotationOverride)) {
			defaults.hasRotationOverride = true;
			defaults.rotationOverride = rotationOverride;
		}
		defaults.skipParts = true;
		ai::UnicodeString outMsg;
		return ProcessDuctwork(defaults, true, &outMsg);
	}

	if (message->menuItem == fProcessNoGraphicsMenuItem) {
		DuctworkLog::Write("GoMenuItem process no graphics");
		ProcessDuctworkOptions defaults;
		double rotationOverride = 0.0;
		if (fPanel.GetRotationOverrideValue(rotationOverride)) {
			defaults.hasRotationOverride = true;
			defaults.rotationOverride = rotationOverride;
		}
		defaults.skipGraphics = true;
		ai::UnicodeString outMsg;
		return ProcessDuctwork(defaults, true, &outMsg);
	}

	if (message->menuItem == fProcessNoMetaMenuItem) {
		DuctworkLog::Write("GoMenuItem process no metadata");
		ProcessDuctworkOptions defaults;
		double rotationOverride = 0.0;
		if (fPanel.GetRotationOverrideValue(rotationOverride)) {
			defaults.hasRotationOverride = true;
			defaults.rotationOverride = rotationOverride;
		}
		defaults.skipPlacedMetadata = true;
		ai::UnicodeString outMsg;
		return ProcessDuctwork(defaults, true, &outMsg);
	}

	if (message->menuItem == fProcessDirectPlaceMenuItem) {
		DuctworkLog::Write("GoMenuItem process direct place");
		ProcessDuctworkOptions defaults;
		double rotationOverride = 0.0;
		if (fPanel.GetRotationOverrideValue(rotationOverride)) {
			defaults.hasRotationOverride = true;
			defaults.rotationOverride = rotationOverride;
		}
		defaults.skipPlacedMetadata = false;
		defaults.directPlaceGraphics = true;
		ai::UnicodeString outMsg;
		return ProcessDuctwork(defaults, true, &outMsg);
	}

	if (message->menuItem == fProcessPlacedApiMenuItem) {
		DuctworkLog::Write("GoMenuItem process placed api");
		ProcessDuctworkOptions defaults;
		double rotationOverride = 0.0;
		if (fPanel.GetRotationOverrideValue(rotationOverride)) {
			defaults.hasRotationOverride = true;
			defaults.rotationOverride = rotationOverride;
		}
		defaults.skipPlacedMetadata = false;
		defaults.placedApiGraphics = true;
		ai::UnicodeString outMsg;
		return ProcessDuctwork(defaults, true, &outMsg);
	}

	if (message->menuItem == fSelectPartsMenuItem) {
		DuctworkLog::Write("GoMenuItem select parts");
		AIDocumentHandle document = nullptr;
		if (sAIDocument && sAIDocument->GetDocument(&document) == kNoErr && document) {
			return SelectDuctworkParts(document);
		}
		return kNoErr;
	}

	return kNoErr;
}

ASErr ProcessDuctworkPlugin::Notify(AINotifierMessage* message)
{
	if (message && message->notifier == fSelectionChangedNotifier) {
		fPanel.UpdateSelectionSummary();
	}
	return kNoErr;
}

ASErr ProcessDuctworkPlugin::DrawGapPreview(AIAnnotatorMessage* message)
{
	if (!message || !message->drawer || !fGapPreviewVisible || !sAIAnnotatorDrawer) {
		return kNoErr;
	}
	DuctworkLog::Write("GapPreview draw");
	AIRGBColor color{};
	if (fGapPreviewShowHeal) {
		color.red = 0.2f;
		color.green = 0.9f;
		color.blue = 0.2f;
		sAIAnnotatorDrawer->SetLineDashed(message->drawer, false);
	} else {
		color.red = 0.95f;
		color.green = 0.2f;
		color.blue = 0.2f;
		sAIAnnotatorDrawer->SetLineDashed(message->drawer, true);
		AIFloat dashes[2] = { 8.0f, 8.0f };
		sAIAnnotatorDrawer->SetLineDashedEx(message->drawer, dashes, 2);
	}
	sAIAnnotatorDrawer->SetColor(message->drawer, color);
	sAIAnnotatorDrawer->SetLineWidth(message->drawer, 8.0f);
	sAIAnnotatorDrawer->DrawLine(message->drawer, fGapPreviewStart, fGapPreviewEnd);
	return kNoErr;
}

ASErr ProcessDuctworkPlugin::InvalidateGapPreview(AIAnnotatorMessage* message)
{
	if (!message || message->annotator != fGapPreviewAnnotator || !fGapPreviewVisible) {
		return kNoErr;
	}
	if (sAIAnnotator && message->view) {
		sAIAnnotator->InvalAnnotationRect(message->view, &fGapPreviewBounds);
	}
	return kNoErr;
}

void ProcessDuctworkPlugin::ClearGapPreview()
{
	if (!fGapPreviewVisible || !sAIAnnotator) {
		fGapPreviewVisible = false;
		fGapPreviewLastHoverArt = nullptr;
		return;
	}
	AIDocumentViewHandle view = fGapPreviewView;
	if (!view && sAIDocumentView) {
		sAIDocumentView->GetNthDocumentView(0, &view);
	}
	if (view) {
		sAIAnnotator->InvalAnnotationRect(view, &fGapPreviewBounds);
	}
	fGapPreviewVisible = false;
	fGapPreviewLastHoverArt = nullptr;
}

void ProcessDuctworkPlugin::UpdateGapPreview(const AIRealPoint& cursor, const std::string& layerHint)
{
	if (!sAIDocumentView || !sAIAnnotator) {
		return;
	}
	if (!fGapPreviewAnnotator) {
		DuctworkLog::Write("GapPreview: annotator missing");
		return;
	}
	AIDocumentViewHandle view = nullptr;
	if (sAIDocumentView->GetNthDocumentView(0, &view) || !view) {
		return;
	}
	DuctworkCarve::GapToolPreview preview;
	const DuctworkPoint rawClick{ cursor.h, cursor.v };
	DuctworkLog::Write(std::string("GapPreview rawClick=[") +
		std::to_string(rawClick.x) + "," + std::to_string(rawClick.y) + "]");
	AIRealPoint artPoint = cursor;
	AIRealPoint viewClick{};
	bool viewClickOk = false;
	if (sAIDocumentView->FixedArtworkPointToViewPoint(view, &artPoint, &viewClick) == kNoErr) {
		viewClickOk = true;
	} else {
		AIPoint viewPoint{};
		if (sAIDocumentView->ArtworkPointToViewPoint(view, &artPoint, &viewPoint) == kNoErr) {
			viewClick.h = viewPoint.h;
			viewClick.v = viewPoint.v;
			viewClickOk = true;
		}
	}
	DuctworkLog::Write(std::string("GapPreview viewClick=[") +
		std::to_string(viewClick.h) + "," + std::to_string(viewClick.v) + "] ok=" +
		(viewClickOk ? "1" : "0"));
	if (!fGapPreviewHoverArt) {
		AIArtHandle nearest = nullptr;
		DuctworkPoint nearestPoint{};
		if (DuctworkCarve::FindPreferredArtNearPoint(rawClick, layerHint, nearest, &nearestPoint) && nearest) {
			fGapPreviewHoverArt = nearest;
			if (layerHint.empty()) {
				const std::string foundLayer = DuctworkGeometry::GetArtLayerName(nearest);
				if (!foundLayer.empty()) {
					DuctworkLog::Write(std::string("GapPreview layerHint fallback=") + foundLayer);
					UpdateGapPreview(cursor, foundLayer);
					return;
				}
			}
			DuctworkLog::Write("GapPreview hoverArt fallback=1");
		} else {
			DuctworkLog::Write("GapPreview hoverArt fallback=0");
		}
	}
	bool hasPreview = DuctworkCarve::ComputeGapToolPreview(
		rawClick,
		layerHint,
		fGapPreviewHoverArt,
		preview,
		viewClickOk ? view : nullptr,
		viewClickOk ? &viewClick : nullptr);
	{
		std::ostringstream stream;
		stream << "GapPreview compute artClick=" << (hasPreview ? 1 : 0)
			<< " valid=" << (preview.valid ? 1 : 0)
			<< " hoverArtPtr=" << fGapPreviewHoverArt
			<< " layerHint=" << (layerHint.empty() ? "<none>" : layerHint);
		DuctworkLog::Write(stream.str());
	}
	if ((!hasPreview || !preview.valid) && viewClickOk) {
		hasPreview = DuctworkCarve::ComputeGapToolPreview(rawClick, layerHint, fGapPreviewHoverArt, preview, nullptr, nullptr);
		if (hasPreview && preview.valid) {
			DuctworkLog::Write("GapPreview fallback art-only");
		}
	}
	if (!hasPreview || !preview.valid) {
		if (fGapPreviewVisible) {
			ClearGapPreview();
		}
		return;
	}

	AIPoint startView{};
	AIPoint endView{};
	AIRealPoint startArt{};
	AIRealPoint endArt{};
	startArt.h = preview.start.x;
	startArt.v = preview.start.y;
	endArt.h = preview.end.x;
	endArt.v = preview.end.y;
	AIRealPoint startViewReal{};
	AIRealPoint endViewReal{};
	if (sAIDocumentView->FixedArtworkPointToViewPoint(view, &startArt, &startViewReal) ||
		sAIDocumentView->FixedArtworkPointToViewPoint(view, &endArt, &endViewReal)) {
		sAIDocumentView->ArtworkPointToViewPoint(view, &startArt, &startView);
		sAIDocumentView->ArtworkPointToViewPoint(view, &endArt, &endView);
		startViewReal.h = startView.h;
		startViewReal.v = startView.v;
		endViewReal.h = endView.h;
		endViewReal.v = endView.v;
	}

	double dx = endViewReal.h - startViewReal.h;
	double dy = endViewReal.v - startViewReal.v;
	const double len = std::sqrt(dx * dx + dy * dy);
	const double minLen = 6.0;
	if (len > 0.0 && len < minLen) {
		const double scale = minLen / len;
		const double midX = (startViewReal.h + endViewReal.h) * 0.5;
		const double midY = (startViewReal.v + endViewReal.v) * 0.5;
		dx *= scale * 0.5;
		dy *= scale * 0.5;
		startViewReal.h = midX - dx;
		startViewReal.v = midY - dy;
		endViewReal.h = midX + dx;
		endViewReal.v = midY + dy;
	}

	auto clampShort = [](double value) -> short {
		if (value > 32000.0) {
			return 32000;
		}
		if (value < -32000.0) {
			return -32000;
		}
		return static_cast<short>(std::lround(value));
	};
	startView.h = clampShort(startViewReal.h);
	startView.v = clampShort(startViewReal.v);
	endView.h = clampShort(endViewReal.h);
	endView.v = clampShort(endViewReal.v);
	DuctworkLog::Write(std::string("GapPreview viewStart=[") +
		std::to_string(startViewReal.h) + "," + std::to_string(startViewReal.v) + "]" +
		" viewEnd=[" + std::to_string(endViewReal.h) + "," + std::to_string(endViewReal.v) + "]");

	const short padding = 6;
	const short minX = (startView.h < endView.h) ? startView.h : endView.h;
	const short maxX = (startView.h > endView.h) ? startView.h : endView.h;
	const short minY = (startView.v < endView.v) ? startView.v : endView.v;
	const short maxY = (startView.v > endView.v) ? startView.v : endView.v;
	AIRect bounds{};
	bounds.left = static_cast<short>(minX - padding);
	bounds.right = static_cast<short>(maxX + padding);
	bounds.top = static_cast<short>(maxY + padding);
	bounds.bottom = static_cast<short>(minY - padding);

	const AIRect prevBounds = fGapPreviewBounds;
	const bool wasVisible = fGapPreviewVisible;
	const bool hoverChanged = (fGapPreviewHoverArt != fGapPreviewLastHoverArt);
	const bool changed = !fGapPreviewVisible || hoverChanged ||
		fGapPreviewStart.h != startView.h ||
		fGapPreviewStart.v != startView.v ||
		fGapPreviewEnd.h != endView.h ||
		fGapPreviewEnd.v != endView.v ||
		fGapPreviewShowHeal != preview.showHeal ||
		fGapPreviewShowGap != preview.showGap;

	fGapPreviewVisible = true;
	fGapPreviewShowHeal = preview.showHeal;
	fGapPreviewShowGap = preview.showGap;
	fGapPreviewIsRegister = preview.isRegister;
	fGapPreviewStart = startView;
	fGapPreviewEnd = endView;
	fGapPreviewBounds = bounds;
	fGapPreviewView = view;
	fGapPreviewLastHoverArt = fGapPreviewHoverArt;

	AIRect invalidRect = bounds;
	if (wasVisible) {
		invalidRect.left = (prevBounds.left < bounds.left) ? prevBounds.left : bounds.left;
		invalidRect.right = (prevBounds.right > bounds.right) ? prevBounds.right : bounds.right;
		invalidRect.top = (prevBounds.top > bounds.top) ? prevBounds.top : bounds.top;
		invalidRect.bottom = (prevBounds.bottom < bounds.bottom) ? prevBounds.bottom : bounds.bottom;
	}
	{
		std::ostringstream stream;
		stream << "GapPreview invalidate changed=" << (changed ? 1 : 0)
			<< " hoverChanged=" << (hoverChanged ? 1 : 0)
			<< " wasVisible=" << (wasVisible ? 1 : 0)
			<< " bounds=[" << bounds.left << "," << bounds.bottom << "," << bounds.right << "," << bounds.top << "]"
			<< " invalid=[" << invalidRect.left << "," << invalidRect.bottom << "," << invalidRect.right << "," << invalidRect.top << "]";
		DuctworkLog::Write(stream.str());
	}
	sAIAnnotator->InvalAnnotationRect(view, &invalidRect);
	if (sAIDocument) {
		sAIDocument->RedrawDocument();
		DuctworkLog::Write("GapPreview redraw");
	}
	if (changed) {
		DuctworkLog::Write(std::string("GapPreview update ") +
			(preview.showHeal ? "heal" : "gap") +
			" start=[" + std::to_string(preview.start.x) + "," + std::to_string(preview.start.y) + "]" +
			" end=[" + std::to_string(preview.end.x) + "," + std::to_string(preview.end.y) + "]");
	} else {
		DuctworkLog::Write("GapPreview update: unchanged");
	}
}

ASErr ProcessDuctworkPlugin::TrackToolCursor(AIToolMessage* message)
{
	if (!message) {
		return kNoErr;
	}
	if (message->tool == fGapToggleTool || message->tool == fGapHealTool) {
		const bool isToggle = (message->tool == fGapToggleTool);
		DuctworkLog::Write(isToggle ? "GapPreview track toggle" : "GapPreview track heal");
		std::string layerHint;
		fGapPreviewHoverArt = nullptr;
		AIRealPoint artPoint = message->cursor;
		AIRealPoint viewCursor{};
		bool viewCursorOk = false;
		if (sAIDocumentView) {
			AIDocumentViewHandle view = nullptr;
			if (!sAIDocumentView->GetNthDocumentView(0, &view) && view) {
				if (sAIDocumentView->FixedArtworkPointToViewPoint(view, &artPoint, &viewCursor) == kNoErr) {
					viewCursorOk = true;
				} else {
					AIPoint viewPoint{};
					if (sAIDocumentView->ArtworkPointToViewPoint(view, &artPoint, &viewPoint) == kNoErr) {
						viewCursor.h = viewPoint.h;
						viewCursor.v = viewPoint.v;
						viewCursorOk = true;
					}
				}
			}
		}
		if (sAIHitTest && viewCursorOk) {
			AIHitRef hitRef = nullptr;
			AIToolHitData hitData{};
			if (!sAIHitTest->HitTest(nullptr, &viewCursor, kAllHitRequest, &hitRef) && hitRef) {
				if (!sAIHitTest->GetHitData(hitRef, &hitData) && hitData.hit && hitData.object) {
					layerHint = DuctworkGeometry::GetArtLayerName(hitData.object);
					fGapPreviewHoverArt = hitData.object;
				}
				sAIHitTest->Release(hitRef);
			}
		}
		if (!fGapPreviewHoverArt) {
			AIArtHandle bestArt = nullptr;
			DuctworkPoint bestPoint{};
			double bestDist = (std::numeric_limits<double>::max)();
			std::string bestSource = "none";

			{
				AIArtHandle nearest = nullptr;
				DuctworkPoint nearestPoint{};
				const DuctworkPoint hoverClickArt{ artPoint.h, artPoint.v };
				if (DuctworkCarve::FindPreferredArtNearPoint(hoverClickArt, layerHint, nearest, &nearestPoint) && nearest) {
					const double dist = DuctworkMath::Dist(nearestPoint, hoverClickArt);
					if (dist < bestDist) {
						bestDist = dist;
						bestArt = nearest;
						bestPoint = nearestPoint;
						bestSource = "art";
					}
				}
			}

			{
				AIArtHandle nearest = nullptr;
				DuctworkPoint nearestPoint{};
				const DuctworkPoint hoverClickRaw{ message->cursor.h, message->cursor.v };
				if (DuctworkCarve::FindPreferredArtNearPoint(hoverClickRaw, layerHint, nearest, &nearestPoint) && nearest) {
					const double dist = DuctworkMath::Dist(nearestPoint, hoverClickRaw);
					if (dist < bestDist) {
						bestDist = dist;
						bestArt = nearest;
						bestPoint = nearestPoint;
						bestSource = "raw";
					}
				}
			}

			if (bestArt) {
				fGapPreviewHoverArt = bestArt;
				if (layerHint.empty()) {
					const std::string foundLayer = DuctworkGeometry::GetArtLayerName(bestArt);
					if (!foundLayer.empty()) {
						layerHint = foundLayer;
					}
				}
				std::ostringstream stream;
				stream << "GapPreview hoverArt refresh=1 source=" << bestSource
					<< " dist=" << bestDist
					<< " art=" << bestArt;
				DuctworkLog::Write(stream.str());
			} else {
				DuctworkLog::Write("GapPreview hoverArt refresh=0");
			}
		}
		{
			std::ostringstream stream;
			stream << "GapPreview hoverArt=" << (fGapPreviewHoverArt ? "1" : "0")
				<< " hoverArtPtr=" << fGapPreviewHoverArt
				<< " layerHint=" << (layerHint.empty() ? "<none>" : layerHint);
			DuctworkLog::Write(stream.str());
		}
		UpdateGapPreview(message->cursor, layerHint);
		if (sAIUser && fCursorResourceManager) {
			return sAIUser->SetSVGCursor(
				isToggle ? kDuctworkGapToggleToolIconResourceID : kDuctworkGapHealToolIconResourceID,
				fCursorResourceManager);
		}
		if (!fCursorResourceManager) {
			DuctworkLog::Write("GapPreview: cursor manager missing");
		}
		return kNoErr;
	}
	return kNoErr;
}

ASErr ProcessDuctworkPlugin::ToolMouseDown(AIToolMessage* message)
{
	if (!message || !sAIHitTest) {
		return kNoErr;
	}
	if (message->tool != fGapToggleTool && message->tool != fGapHealTool) {
		return kNoErr;
	}

	AIHitRef hitRef = nullptr;
	AIToolHitData hitData{};
	std::string layerHint;
	fGapPreviewHoverArt = nullptr;
	AIRealPoint artPoint = message->cursor;
	AIRealPoint viewCursor{};
	bool viewCursorOk = false;
	if (sAIDocumentView) {
		AIDocumentViewHandle view = nullptr;
		if (!sAIDocumentView->GetNthDocumentView(0, &view) && view) {
			if (sAIDocumentView->FixedArtworkPointToViewPoint(view, &artPoint, &viewCursor) == kNoErr) {
				viewCursorOk = true;
			} else {
				AIPoint viewPoint{};
				if (sAIDocumentView->ArtworkPointToViewPoint(view, &artPoint, &viewPoint) == kNoErr) {
					viewCursor.h = viewPoint.h;
					viewCursor.v = viewPoint.v;
					viewCursorOk = true;
				}
			}
		}
	}
	if (viewCursorOk) {
		if (!sAIHitTest->HitTest(nullptr, &viewCursor, kAllHitRequest, &hitRef) && hitRef) {
			if (!sAIHitTest->GetHitData(hitRef, &hitData) && hitData.hit && hitData.object) {
				layerHint = DuctworkGeometry::GetArtLayerName(hitData.object);
				fGapPreviewHoverArt = hitData.object;
			}
			sAIHitTest->Release(hitRef);
		}
	}

	DuctworkCarve::GapToolResult result;
	const DuctworkPoint rawClick{ message->cursor.h, message->cursor.v };
	if (!fGapPreviewHoverArt) {
		AIArtHandle nearest = nullptr;
		if (DuctworkCarve::FindPreferredArtNearPoint(rawClick, layerHint, nearest) && nearest) {
			fGapPreviewHoverArt = nearest;
		}
	}
	if (message->tool == fGapToggleTool) {
		DuctworkLog::Write("GapTool toggle click");
		DuctworkCarve::ApplyGapToggleAtPoint(rawClick, layerHint, fGapPreviewHoverArt, result);
	} else {
		DuctworkLog::Write("GapTool heal/create click");
		DuctworkCarve::ApplyGapHealCreateAtPoint(rawClick, layerHint, fGapPreviewHoverArt, result);
	}

	if (sAIDocument) {
		sAIDocument->SyncDocument();
		sAIDocument->RedrawDocument();
	}
	ClearGapPreview();
	return kNoErr;
}

ASErr ProcessDuctworkPlugin::ProcessDuctwork(const ProcessDuctworkOptions& options, ASBoolean showAlert, ai::UnicodeString* outParam)
{
	AIDocumentHandle document = NULL;
	ai::UnicodeString message;

	DuctworkLog::Write("EmoryDuctwork start");
	CepSuspendScope cepSuspend;
	RotationOverrideScope rotationScope(options.hasRotationOverride, options.rotationOverride);
	RegisterRotationScope registerRotationScope(!options.skipRegisterRotation);
	StepTimer totalTimer("Total");
	{
		std::ostringstream optStream;
		optStream << "Options skipOrtho=" << options.skipOrtho
			<< " skipAllBranchSegments=" << options.skipAllBranchSegments
			<< " skipFinalRegisterSegment=" << options.skipFinalRegisterSegment
			<< " skipRegisterRotation=" << options.skipRegisterRotation
			<< " enableRegisterCarve=" << options.enableRegisterCarve
			<< " enableOverlapCarve=" << options.enableOverlapCarve
			<< " skipCompounding=" << options.skipCompounding
			<< " skipStyles=" << options.skipStyles
			<< " skipParts=" << options.skipParts
			<< " skipGraphics=" << options.skipGraphics
			<< " skipPlacedMetadata=" << options.skipPlacedMetadata
			<< " directPlaceGraphics=" << options.directPlaceGraphics
			<< " placedApiGraphics=" << options.placedApiGraphics
			<< " hasRotationOverride=" << options.hasRotationOverride;
		DuctworkLog::Write(optStream.str());
	}

	AppContext appContext(GetPluginRef());

	ASErr error = sAIDocument->GetDocument(&document);
	if (error || !document) {
		message = ai::UnicodeString::FromUTF8("No document open.");
		if (outParam) {
			*outParam = message;
		}
		if (showAlert) {
			sAIUser->MessageAlert(message);
		}
		return kNoErr;
	}

	// Release any existing compound paths on ductwork layers before processing
	// This ensures proper endpoint detection for register placement
	{
		StepTimer releaseTimer("ReleaseCompounds");
		size_t released = DuctworkCompound::ReleaseCompoundPaths(document);
		if (released > 0) {
			std::ostringstream stream;
			stream << "Released " << released << " compound path(s) before processing";
			DuctworkLog::Write(stream.str());
		}
		releaseTimer.LogElapsed();
	}

	std::vector<AIArtHandle> selectedPaths;
	size_t selectedArtCount = DuctworkSelection::CollectSelectedPaths(selectedPaths);
	std::vector<AIArtHandle> selectionSnapshot = selectedPaths;
	if (selectedArtCount == 0) {
		message = ai::UnicodeString::FromUTF8("Select ductwork paths before running Process Emory Ductwork.");
		if (outParam) {
			*outParam = message;
		}
		if (showAlert) {
			sAIUser->MessageAlert(message);
		}
		return kNoErr;
	}

	std::vector<DuctworkPath> ductworkPaths;
	double minX = 0.0;
	double minY = 0.0;
	double maxX = 0.0;
	double maxY = 0.0;
	bool boundsInit = false;
	LiveEditSuspendScope liveEditSuspender;
	StepTimer collectTimer("CollectPaths");
	for (size_t i = 0; i < selectedPaths.size(); ++i) {
		DuctworkPath entry;
		if (!BuildProcessPathEntry(selectedPaths[i], entry)) {
			continue;
		}
		ExpandPathBounds(entry, minX, minY, maxX, maxY, boundsInit);
		ductworkPaths.push_back(entry);
	}
	collectTimer.LogElapsed();

	if (ductworkPaths.empty()) {
		message = ai::UnicodeString::FromUTF8("Select open ductwork centerlines before running Process Emory Ductwork.");
		if (outParam) {
			*outParam = message;
		}
		if (showAlert) {
			sAIUser->MessageAlert(message);
		}
		return kNoErr;
	}

	size_t deletedEmoryBodies = 0;
	{
		StepTimer emoryCleanupTimer("EmoryCleanup");
		std::vector<std::string> sourceIds;
		sourceIds.reserve(ductworkPaths.size());
		for (size_t i = 0; i < ductworkPaths.size(); ++i) {
			if (!DuctworkLayers::IsColorLayerName(ductworkPaths[i].layerName)) {
				continue;
			}
			std::string sourceId;
			if (DuctworkGeometry::EnsureEmorySourceId(ductworkPaths[i].art, sourceId) && !sourceId.empty()) {
				sourceIds.push_back(sourceId);
			}
		}
		deletedEmoryBodies = DuctworkGeometry::DeleteGeneratedEmoryBodies(sourceIds);
		DuctworkLog::Write("Emory bodies deleted=" + std::to_string(deletedEmoryBodies));
		emoryCleanupTimer.LogElapsed();
	}

	OrthoResult orthoResult = {};
	if (!options.skipOrtho && !ductworkPaths.empty()) {
		SavePreOrthoCopy(document);
		if (SnapThermostatEndpoints(ductworkPaths, 12.0)) {
			DuctworkLog::Write("Thermostat endpoints snapped");
		}

		std::vector<EndpointFlags> endpointFlags;
		if (options.skipAllBranchSegments || options.skipFinalRegisterSegment) {
			StepTimer roleTimer("RoleMetadata");
			std::vector<AIArtHandle> allPathArt;
			CollectDuctworkLayerPaths(allPathArt);
			std::vector<DuctworkPath> allPaths;
			allPaths.reserve(allPathArt.size());
			for (size_t i = 0; i < allPathArt.size(); ++i) {
				DuctworkPath entry;
				if (!BuildProcessPathEntry(allPathArt[i], entry)) {
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
			AppendUnitEndpointPairs(allPaths, 10.0, allConnections);
			CollectEndpointFlagsForSelection(ductworkPaths, allPaths, allConnections, endpointFlags);
			WriteDuctRoleMetadata(ductworkPaths, endpointFlags, allPaths, allConnections);
			roleTimer.LogElapsed();
		}

		std::vector<DuctworkConnection> preConnections;
		{
			StepTimer preConnTimer("PreOrthoConnections");
			DuctworkConnections::FindConnections(
				ductworkPaths,
				2.0,
				3.0,
				15.0,
				10.0,
				true,
				preConnections);
			preConnTimer.LogElapsed();
		}
		AppendUnitEndpointPairs(ductworkPaths, 10.0, preConnections);
		if (!endpointFlags.empty()) {
			AppendExternalEndpointConnections(ductworkPaths, endpointFlags, preConnections);
		}
		StepTimer orthoTimer("Orthogonalize");
		orthoResult = DuctworkOrtho::ApplyToPaths(ductworkPaths, 17.0,
			options.hasRotationOverride, options.rotationOverride, preConnections,
			options.skipAllBranchSegments, options.skipFinalRegisterSegment);
		orthoTimer.LogElapsed();
		DuctworkLog::Write("Orthogonalize paths touched=" + std::to_string(orthoResult.pathsTouched) +
			" segmentsSnapped=" + std::to_string(orthoResult.segmentsSnapped));
	}

	std::vector<DuctworkConnection> connections;
	StepTimer connTimer("Connections");
	DuctworkConnections::FindConnections(
		ductworkPaths,
		2.0,
		3.0,
		15.0,
		10.0,
		true,
		connections);
	connTimer.LogElapsed();

	{
		std::ostringstream stream;
		stream << "Selected art=" << selectedArtCount << " selected paths=" << selectedPaths.size()
			<< " ductworkPaths=" << ductworkPaths.size()
			<< " connections=" << connections.size();
		if (boundsInit) {
			stream << " bounds=[" << minX << "," << minY << "]-[" << maxX << "," << maxY << "]";
		}
		DuctworkLog::Write(stream.str());
	}

	std::vector<DuctworkPoint> unitAnchors;
	if (!options.skipParts) {
		StepTimer partsTimer("Parts");
		DuctworkUnitStats unitStats = DuctworkParts::CreateUnitAnchorsAndGraphics(
			ductworkPaths, 10.0, 3.0, 50.0, unitAnchors,
			options.skipGraphics, options.skipPlacedMetadata, options.directPlaceGraphics, options.placedApiGraphics);
		DuctworkLog::Write("Units: anchorsCreated=" + std::to_string(unitStats.anchorsCreated) +
			" graphicsPlaced=" + std::to_string(unitStats.graphicsPlaced));

		DuctworkPartStats partStats = DuctworkParts::CreateAnchorsAndGraphics(
			ductworkPaths, connections, unitAnchors, 3.0, 50.0,
			options.skipGraphics, options.skipPlacedMetadata, options.directPlaceGraphics, options.placedApiGraphics);
		{
			std::ostringstream stream;
			stream << "Placed anchors=" << partStats.anchorsCreated
				<< " graphics=" << partStats.graphicsPlaced
				<< " skippedExisting=" << partStats.skippedExisting
				<< " skippedMissingAsset=" << partStats.skippedMissingAsset
				<< " skippedConnected=" << partStats.skippedConnected;
			DuctworkLog::Write(stream.str());
		}
		partsTimer.LogElapsed();
	} else {
		DuctworkLog::Write("Parts skipped");
	}

	if (options.enableRegisterCarve || options.enableOverlapCarve) {
		StepTimer carveTimer("Carve");
		DuctworkCarve::CarveStats carveStats;
		std::vector<AIArtHandle> updatedSelection = selectedPaths;
		if (options.enableRegisterCarve) {
			DuctworkCarve::CarveStats regStats = DuctworkCarve::ApplyRegisterCarve(ductworkPaths, updatedSelection);
			carveStats.registerCarves += regStats.registerCarves;
			carveStats.pathsSplit += regStats.pathsSplit;
		}
		if (options.enableOverlapCarve) {
			DuctworkCarve::CarveStats overlapStats = DuctworkCarve::ApplyOverlapCarve(ductworkPaths, updatedSelection);
			carveStats.overlapCarves += overlapStats.overlapCarves;
			carveStats.pathsSplit += overlapStats.pathsSplit;
		}
		selectedPaths.swap(updatedSelection);
		ductworkPaths.clear();
		for (size_t i = 0; i < selectedPaths.size(); ++i) {
			DuctworkPath entry;
			if (!BuildProcessPathEntry(selectedPaths[i], entry)) {
				continue;
			}
			ductworkPaths.push_back(entry);
		}
		connections.clear();
		DuctworkConnections::FindConnections(
			ductworkPaths,
			2.0,
			3.0,
			15.0,
			10.0,
			true,
			connections);
		std::ostringstream carveStream;
		carveStream << "Carve register=" << carveStats.registerCarves
			<< " overlap=" << carveStats.overlapCarves
			<< " pathsSplit=" << carveStats.pathsSplit;
		DuctworkLog::Write(carveStream.str());
		carveTimer.LogElapsed();
	}

	std::vector<AIArtHandle> compoundPaths;
	const bool skipEmoryLineGraphics = true;
	if (!options.skipCompounding && !skipEmoryLineGraphics) {
		StepTimer compoundTimer("Compounds");
		DuctworkCompoundStats compoundStats = DuctworkCompound::MergeConnectedPaths(ductworkPaths, connections, compoundPaths);
		{
			std::ostringstream stream;
			stream << "Compounds components=" << compoundStats.components
				<< " created=" << compoundStats.compoundsCreated
				<< " skippedSingle=" << compoundStats.skippedSingle
				<< " skippedFailed=" << compoundStats.skippedFailed
				<< " movedToLayer=" << compoundStats.movedToLayer
				<< " metaWritten=" << compoundStats.metaWritten;
			DuctworkLog::Write(stream.str());
		}
		compoundTimer.LogElapsed();
	} else {
		DuctworkLog::Write(skipEmoryLineGraphics ? "Compounds skipped for Emory body workflow" : "Compounds skipped");
	}

	if (!options.skipStyles && !skipEmoryLineGraphics) {
		StepTimer styleTimer("Styles");
		DuctworkStyleStats styleStats = DuctworkStyles::ApplyLineStyles(document, ductworkPaths);
		{
			std::ostringstream stream;
			stream << "Styles applied=" << styleStats.applied
				<< " created=" << styleStats.created
				<< " skippedMissingStyle=" << styleStats.skippedMissingStyle
				<< " skippedNoSample=" << styleStats.skippedNoSample
				<< " skippedNonLineLayer=" << styleStats.skippedNonLineLayer;
			DuctworkLog::Write(stream.str());
		}
		styleTimer.LogElapsed();
	} else {
		DuctworkLog::Write(skipEmoryLineGraphics ? "Styles skipped for Emory body workflow" : "Styles skipped");
	}

	if (!compoundPaths.empty() && !options.skipStyles && !skipEmoryLineGraphics) {
		StepTimer compoundStyleTimer("CompoundStyles");
		std::vector<DuctworkPath> compoundDuctwork;
		compoundDuctwork.reserve(compoundPaths.size());
		for (size_t i = 0; i < compoundPaths.size(); ++i) {
			DuctworkPath entry;
			entry.art = compoundPaths[i];
			entry.closed = false;
			entry.layerName = DuctworkGeometry::GetArtLayerName(compoundPaths[i]);
			compoundDuctwork.push_back(entry);
		}
		DuctworkStyleStats compoundStyleStats = DuctworkStyles::ApplyLineStyles(document, compoundDuctwork);
		std::ostringstream stream;
		stream << "Compound styles applied=" << compoundStyleStats.applied
			<< " created=" << compoundStyleStats.created
			<< " skippedMissingStyle=" << compoundStyleStats.skippedMissingStyle
			<< " skippedNoSample=" << compoundStyleStats.skippedNoSample
			<< " skippedNonLineLayer=" << compoundStyleStats.skippedNonLineLayer;
		DuctworkLog::Write(stream.str());
		compoundStyleTimer.LogElapsed();
	} else if (!compoundPaths.empty()) {
		DuctworkLog::Write(skipEmoryLineGraphics ? "Compound styles skipped for Emory body workflow" : "Compound styles skipped");
	}

	{
		StepTimer emoryBodyTimer("EmoryBodies");
		EmoryBodyStats emoryStats = DuctworkGeometry::GenerateEmoryBodies(ductworkPaths);
		emoryStats.deleted = deletedEmoryBodies;
		std::ostringstream stream;
		stream << "Emory bodies created=" << emoryStats.created
			<< " segments=" << emoryStats.segmentsCreated
			<< " connectors=" << emoryStats.connectorsCreated
			<< " guidesStyled=" << emoryStats.guidesStyled
			<< " deleted=" << emoryStats.deleted
			<< " skipped=" << emoryStats.skipped
			<< " failed=" << emoryStats.failed;
		DuctworkLog::Write(stream.str());
		emoryBodyTimer.LogElapsed();
	}

	if (outParam) {
		*outParam = ai::UnicodeString::FromUTF8("");
	}
	if (sAIDocument) {
		StepTimer finalizeTimer("Finalize");
		std::vector<AIArtHandle> finalizePaths;
		std::vector<AIArtHandle> warmPaths;
		CollectDuctworkLayerPaths(warmPaths);
		finalizePaths = warmPaths;
		FinalizeArtCaches(finalizePaths);
		if (!finalizePaths.empty()) {
			DuctworkLog::Write("Finalize caches paths=" + std::to_string(finalizePaths.size()));
		}
		if (!warmPaths.empty()) {
			DuctworkLog::Write("Warm selection paths=" + std::to_string(warmPaths.size()));
			ClearSelection();
			SelectArtList(warmPaths);
		}
		sAIDocument->SyncDocument();
		sAIDocument->RedrawDocument();
		std::vector<AIArtHandle> selectionToRestore;
		FilterValidArtList(selectionSnapshot, selectionToRestore);
		ClearSelection();
		SelectArtList(selectionToRestore);
		finalizeTimer.LogElapsed();
	}
	totalTimer.LogElapsed();
	DuctworkLog::Write("EmoryDuctwork complete");
	return kNoErr;
}

ASErr ProcessDuctworkPlugin::SelectDuctworkParts(AIDocumentHandle document)
{
	if (!document || !sAILayer || !sAIArt) {
		return kNoErr;
	}

	ClearSelection();

	size_t selectedCount = 0;
	for (size_t i = 0; i < DuctworkConstants::kPartLayerCount; ++i) {
		ai::UnicodeString layerName = ai::UnicodeString::FromUTF8(DuctworkConstants::kPartLayers[i]);
		AILayerHandle layer = nullptr;
		if (sAILayer->GetLayerByTitle(&layer, layerName) || !layer) {
			continue;
		}

		AIArtHandle layerGroup = nullptr;
		if (sAIArt->GetFirstArtOfLayer(layer, &layerGroup) || !layerGroup) {
			continue;
		}

		std::vector<AIArtHandle> parts;
		CollectSelectableParts(layerGroup, parts);
		for (size_t p = 0; p < parts.size(); ++p) {
			if (sAIArt->SetArtUserAttr(parts[p], kArtSelected | kArtFullySelected,
				kArtSelected | kArtFullySelected) == kNoErr) {
				++selectedCount;
			}
		}
	}

	DuctworkLog::Write("Select parts selected=" + std::to_string(selectedCount));
	return kNoErr;
}

