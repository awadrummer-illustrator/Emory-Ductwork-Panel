#include "IllustratorSDK.h"
#include "EmoryDuctworkPanel.h"
#include "ProcessDuctworkPlugin.h"
#include "ProcessDuctworkMetadata.h"
#include "ProcessDuctworkSelection.h"
#include "ProcessDuctworkStyles.h"
#include "ProcessDuctworkSuites.h"
#include "ProcessDuctworkGeometry.h"
#include "ProcessDuctworkLayers.h"
#include "ProcessDuctworkLog.h"
#include "AppContext.hpp"

#ifdef WIN_ENV
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#pragma comment(lib, "comctl32.lib")
#endif

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>

#ifdef WIN_ENV
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <commctrl.h>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")
#ifndef TBM_SETBKCOLOR
#define TBM_SETBKCOLOR (WM_USER + 33)
#endif
#endif

namespace
{
	enum ControlIds
	{
		kIdRotationOverrideEdit = 1000,
		kIdRotationGetAngle,
		kIdRotationClear,
		kIdProcessPlacedApi,
		kIdResetStrokes,
		kIdResetScale,
		kIdResetRotation,
		kIdScaleSlider,
		kIdScaleEdit,
		kIdRotateSlider,
		kIdRotateEdit,
		kIdLiveCheck,
		kIdApplyTransform,
		kIdResetOriginal,
		kIdQuickRotate45,
		kIdQuickRotate90,
		kIdQuickRotate180,
		kIdQuickRotateNeg45,
		kIdQuickRotateNeg90,
		kIdQuickRotateCustom
	};

	int CountDuctworkPartDescendants(AIArtHandle art);
	int CountDuctworkLineDescendants(AIArtHandle art);
	bool IsArtSelectedAttr(AIArtHandle art);
	bool HasSelectedDescendant(AIArtHandle art);

	bool CollectSelectedArt(std::vector<AIArtHandle>& outArt)
	{
		outArt.clear();
		if (!sAIArtSet || !sAIArt) {
			return false;
		}
		AIArtSet selectedSet = nullptr;
		if (sAIArtSet->NewArtSet(&selectedSet)) {
			return false;
		}
		size_t selectedCount = 0;
		if (!sAIArtSet->SelectedArtSet(selectedSet)) {
			sAIArtSet->CountArtSet(selectedSet, &selectedCount);
		}
		DuctworkLog::Write("Panel selection: SelectedArtSet count=" + std::to_string(static_cast<int>(selectedCount)));
		if (selectedCount == 0) {
			AIArtSpec specs[1];
			specs[0].type = kAnyArt;
			specs[0].whichAttr = kArtSelected | kArtFullySelected;
			specs[0].attr = kArtSelected | kArtFullySelected;
			if (!sAIArtSet->MatchingArtSet(specs, 1, selectedSet)) {
				sAIArtSet->CountArtSet(selectedSet, &selectedCount);
			}
			DuctworkLog::Write("Panel selection: MatchingArtSet count=" + std::to_string(static_cast<int>(selectedCount)));
		}
		if (selectedCount > 0) {
			for (size_t i = 0; i < selectedCount; ++i) {
				AIArtHandle art = nullptr;
				if (sAIArtSet->IndexArtSet(selectedSet, i, &art) || !art) {
					continue;
				}
				outArt.push_back(art);
			}
		}
		sAIArtSet->DisposeArtSet(&selectedSet);

		if (outArt.empty() && sAILayer && sAIArtSet) {
			size_t scanned = 0;
			size_t flagged = 0;
			AILayerHandle layer = nullptr;
			if (!sAILayer->GetFirstLayer(&layer) && layer) {
				while (layer) {
					AIArtSet layerSet = nullptr;
					if (!sAIArtSet->NewArtSet(&layerSet)) {
						if (!sAIArtSet->LayerArtSet(layer, layerSet)) {
							size_t layerCount = 0;
							if (!sAIArtSet->CountArtSet(layerSet, &layerCount)) {
								for (size_t i = 0; i < layerCount; ++i) {
									AIArtHandle art = nullptr;
									if (sAIArtSet->IndexArtSet(layerSet, i, &art) || !art) {
										continue;
									}
									++scanned;
									ai::int32 attr = 0;
									if (!sAIArt->GetArtUserAttr(art, kArtSelected | kArtFullySelected, &attr)) {
										if ((attr & (kArtSelected | kArtFullySelected)) != 0) {
											outArt.push_back(art);
											++flagged;
										}
									}
								}
							}
						}
						sAIArtSet->DisposeArtSet(&layerSet);
					}
					AILayerHandle next = nullptr;
					if (sAILayer->GetNextLayer(layer, &next) || !next) {
						break;
					}
					layer = next;
				}
			}
			DuctworkLog::Write("Panel selection: layer scan scanned=" + std::to_string(static_cast<int>(scanned)) +
				" selected=" + std::to_string(static_cast<int>(flagged)));
		}

		if (!outArt.empty()) {
			std::vector<AIArtHandle> filtered;
			filtered.reserve(outArt.size());
			for (size_t i = 0; i < outArt.size(); ++i) {
				AIArtHandle art = outArt[i];
				if (!art) {
					continue;
				}
				if (!HasSelectedDescendant(art)) {
					filtered.push_back(art);
				}
			}
			if (filtered.size() != outArt.size()) {
				DuctworkLog::Write("Panel selection: filtered top-level=" +
					std::to_string(static_cast<int>(filtered.size())));
				outArt.swap(filtered);
			}
		}

		return !outArt.empty();
	}

	bool CollectSelectedPaths(std::vector<AIArtHandle>& outPaths)
	{
		return DuctworkSelection::CollectSelectedPaths(outPaths) > 0;
	}

	bool IsSinglePointPath(AIArtHandle art)
	{
		if (!art || !sAIArt || !sAIPath) {
			return false;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type) || type != kPathArt) {
			return false;
		}
		ai::int16 count = 0;
		if (sAIPath->GetPathSegmentCount(art, &count)) {
			return false;
		}
		return count <= 1;
	}

	AIArtHandle ResolvePlacedAncestor(AIArtHandle art)
	{
		if (!art || !sAIArt) {
			return nullptr;
		}
		AIArtHandle current = art;
		while (current) {
			short type = kUnknownArt;
			if (!sAIArt->GetArtType(current, &type) && type == kPlacedArt) {
				return current;
			}
			AIArtHandle parent = nullptr;
			if (sAIArt->GetArtParent(current, &parent) != kNoErr) {
				break;
			}
			current = parent;
		}
		return nullptr;
	}

	bool IsDuctworkLineArt(AIArtHandle art)
	{
		const std::string layerName = DuctworkGeometry::GetArtLayerName(art);
		if (layerName.empty()) {
			return false;
		}
		return DuctworkLayers::IsLineLayerName(layerName);
	}

	bool IsDuctworkPartArt(AIArtHandle art)
	{
		const std::string layerName = DuctworkGeometry::GetArtLayerName(art);
		if (layerName.empty()) {
			return false;
		}
		return DuctworkLayers::IsPartLayerName(layerName);
	}

	bool IsRotatablePartArt(AIArtHandle art)
	{
		if (!IsDuctworkPartArt(art)) {
			return false;
		}
		const std::string layerName = DuctworkGeometry::GetArtLayerName(art);
		return layerName != "Thermostats";
	}

	void CollectChildArt(AIArtHandle parent, std::vector<AIArtHandle>& outChildren)
	{
		outChildren.clear();
		if (!parent || !sAIArt) {
			return;
		}
		AIArtHandle child = nullptr;
		if (sAIArt->GetArtFirstChild(parent, &child) || !child) {
			return;
		}
		AIArtHandle current = child;
		while (current) {
			outChildren.push_back(current);
			AIArtHandle next = nullptr;
			if (sAIArt->GetArtSibling(current, &next) || !next) {
				break;
			}
			current = next;
		}
	}

	void CollectLinePathsRecursive(AIArtHandle art, std::vector<AIArtHandle>& outPaths)
	{
		if (!art || !sAIArt) {
			return;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return;
		}
		if (type == kPathArt) {
			if (IsDuctworkLineArt(art) && !IsSinglePointPath(art)) {
				outPaths.push_back(art);
			}
			return;
		}
		if (type == kGroupArt || type == kCompoundPathArt) {
			std::vector<AIArtHandle> children;
			CollectChildArt(art, children);
			for (size_t i = 0; i < children.size(); ++i) {
				CollectLinePathsRecursive(children[i], outPaths);
			}
		}
	}

	bool IsArtSelectedAttr(AIArtHandle art)
	{
		if (!art || !sAIArt) {
			return false;
		}
		ai::int32 attr = 0;
		if (sAIArt->GetArtUserAttr(art, kArtSelected | kArtFullySelected, &attr)) {
			return false;
		}
		return (attr & (kArtSelected | kArtFullySelected)) != 0;
	}

	bool HasSelectedDescendant(AIArtHandle art)
	{
		if (!art || !sAIArt) {
			return false;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return false;
		}
		if (type != kGroupArt && type != kCompoundPathArt) {
			return false;
		}
		std::vector<AIArtHandle> children;
		CollectChildArt(art, children);
		for (size_t i = 0; i < children.size(); ++i) {
			if (IsArtSelectedAttr(children[i])) {
				return true;
			}
			if (HasSelectedDescendant(children[i])) {
				return true;
			}
		}
		return false;
	}

	void CollectLinePathsForAngle(AIArtHandle art, std::vector<AIArtHandle>& outPaths, bool requireSelected)
	{
		if (!art || !sAIArt) {
			return;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return;
		}
		if (type == kPathArt) {
			if (IsDuctworkLineArt(art) && !IsSinglePointPath(art) &&
				(!requireSelected || IsArtSelectedAttr(art))) {
				outPaths.push_back(art);
			}
			return;
		}
		if (type == kGroupArt || type == kCompoundPathArt) {
			std::vector<AIArtHandle> children;
			CollectChildArt(art, children);
			for (size_t i = 0; i < children.size(); ++i) {
				CollectLinePathsForAngle(children[i], outPaths, requireSelected);
			}
		}
	}

	void CollectAllPathsForAngle(AIArtHandle art, std::vector<AIArtHandle>& outPaths, bool requireSelected)
	{
		if (!art || !sAIArt) {
			return;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return;
		}
		if (type == kPathArt) {
			if (!IsSinglePointPath(art) && (!requireSelected || IsArtSelectedAttr(art))) {
				outPaths.push_back(art);
			}
			return;
		}
		if (type == kGroupArt || type == kCompoundPathArt) {
			std::vector<AIArtHandle> children;
			CollectChildArt(art, children);
			for (size_t i = 0; i < children.size(); ++i) {
				CollectAllPathsForAngle(children[i], outPaths, requireSelected);
			}
		}
	}

	void CollectAllPathsRecursive(AIArtHandle art, std::vector<AIArtHandle>& outPaths)
	{
		if (!art || !sAIArt) {
			return;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return;
		}
		if (type == kPathArt) {
			if (!IsSinglePointPath(art)) {
				outPaths.push_back(art);
			}
			return;
		}
		if (type == kGroupArt || type == kCompoundPathArt) {
			std::vector<AIArtHandle> children;
			CollectChildArt(art, children);
			for (size_t i = 0; i < children.size(); ++i) {
				CollectAllPathsRecursive(children[i], outPaths);
			}
		}
	}

	void CollectPartItemsRecursive(AIArtHandle art, std::vector<AIArtHandle>& outParts)
	{
		if (!art || !sAIArt) {
			return;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return;
		}
		if (type == kGroupArt || type == kCompoundPathArt) {
			std::vector<AIArtHandle> children;
			CollectChildArt(art, children);
			for (size_t i = 0; i < children.size(); ++i) {
				CollectPartItemsRecursive(children[i], outParts);
			}
			return;
		}
		if (!IsDuctworkPartArt(art)) {
			return;
		}
		if (type == kPathArt && IsSinglePointPath(art)) {
			return;
		}
		outParts.push_back(art);
	}

	void CollectRotatablePartItemsRecursive(AIArtHandle art, std::vector<AIArtHandle>& outParts)
	{
		if (!art || !sAIArt) {
			return;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return;
		}
		if (type == kGroupArt || type == kCompoundPathArt) {
			std::vector<AIArtHandle> children;
			CollectChildArt(art, children);
			for (size_t i = 0; i < children.size(); ++i) {
				CollectRotatablePartItemsRecursive(children[i], outParts);
			}
			return;
		}
		if (!IsRotatablePartArt(art)) {
			return;
		}
		if (type == kPathArt && IsSinglePointPath(art)) {
			return;
		}
		outParts.push_back(art);
	}

	void CollectPartItemsSelected(AIArtHandle art, std::vector<AIArtHandle>& outParts, bool parentSelected)
	{
		if (!art || !sAIArt) {
			return;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return;
		}
		const bool selected = parentSelected || IsArtSelectedAttr(art);
		if (type == kGroupArt || type == kCompoundPathArt) {
			const bool expandAll = selected && !HasSelectedDescendant(art);
			std::vector<AIArtHandle> children;
			CollectChildArt(art, children);
			for (size_t i = 0; i < children.size(); ++i) {
				CollectPartItemsSelected(children[i], outParts, expandAll);
			}
			return;
		}
		if (!selected) {
			return;
		}
		if (!IsDuctworkPartArt(art)) {
			return;
		}
		if (type == kPathArt && IsSinglePointPath(art)) {
			return;
		}
		outParts.push_back(art);
	}

	void CollectRotatablePartItemsSelected(AIArtHandle art, std::vector<AIArtHandle>& outParts, bool parentSelected)
	{
		if (!art || !sAIArt) {
			return;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return;
		}
		const bool selected = parentSelected || IsArtSelectedAttr(art);
		if (type == kGroupArt || type == kCompoundPathArt) {
			const bool expandAll = selected && !HasSelectedDescendant(art);
			std::vector<AIArtHandle> children;
			CollectChildArt(art, children);
			for (size_t i = 0; i < children.size(); ++i) {
				CollectRotatablePartItemsSelected(children[i], outParts, expandAll);
			}
			return;
		}
		if (!selected) {
			return;
		}
		if (!IsRotatablePartArt(art)) {
			return;
		}
		if (type == kPathArt && IsSinglePointPath(art)) {
			return;
		}
		outParts.push_back(art);
	}

	void CollectLinePathsSelected(AIArtHandle art, std::vector<AIArtHandle>& outPaths, bool parentSelected)
	{
		if (!art || !sAIArt) {
			return;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return;
		}
		const bool selected = parentSelected || IsArtSelectedAttr(art);
		if (type == kPathArt) {
			if (selected && IsDuctworkLineArt(art) && !IsSinglePointPath(art)) {
				outPaths.push_back(art);
			}
			return;
		}
		if (type == kGroupArt || type == kCompoundPathArt) {
			const bool expandAll = selected && !HasSelectedDescendant(art);
			std::vector<AIArtHandle> children;
			CollectChildArt(art, children);
			for (size_t i = 0; i < children.size(); ++i) {
				CollectLinePathsSelected(children[i], outPaths, expandAll);
			}
		}
	}

	int CountDuctworkPartDescendants(AIArtHandle art)
	{
		if (!art || !sAIArt) {
			return 0;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return 0;
		}
		if (type == kPlacedArt) {
			if (IsDuctworkPartArt(art)) {
				return 1;
			}
			return 0;
		}
		if (type == kPathArt) {
			if (IsDuctworkPartArt(art) && !IsSinglePointPath(art)) {
				return 1;
			}
			return 0;
		}
		if (type == kGroupArt || type == kCompoundPathArt) {
			int count = 0;
			std::vector<AIArtHandle> children;
			CollectChildArt(art, children);
			for (size_t i = 0; i < children.size(); ++i) {
				count += CountDuctworkPartDescendants(children[i]);
			}
			return count;
		}
		return 0;
	}

	int CountDuctworkLineDescendants(AIArtHandle art)
	{
		if (!art || !sAIArt) {
			return 0;
		}
		short type = kUnknownArt;
		if (sAIArt->GetArtType(art, &type)) {
			return 0;
		}
		if (type == kPathArt) {
			if (IsDuctworkLineArt(art) && !IsSinglePointPath(art)) {
				return 1;
			}
			return 0;
		}
		if (type == kGroupArt || type == kCompoundPathArt) {
			int count = 0;
			std::vector<AIArtHandle> children;
			CollectChildArt(art, children);
			for (size_t i = 0; i < children.size(); ++i) {
				count += CountDuctworkLineDescendants(children[i]);
			}
			return count;
		}
		return 0;
	}


	void ReselectArtList(const std::vector<AIArtHandle>& artList)
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

	bool GetStrokeWidth(AIArtHandle path, double& outWidth)
	{
		outWidth = 0.0;
		if (!path || !sAIPathStyle) {
			return false;
		}
		AIPathStyle style;
		AIBoolean fillVisible = false;
		AIBoolean strokeVisible = false;
		if (sAIPathStyle->GetPathStyleEx(path, &style, &fillVisible, &strokeVisible)) {
			return false;
		}
		if (!strokeVisible || !style.strokePaint) {
			return false;
		}
		outWidth = static_cast<double>(style.stroke.width);
		return true;
	}

	bool ScaleStrokeWidth(AIArtHandle path, double scaleFactor)
	{
		if (!path || !sAIPathStyle) {
			return false;
		}
		AIPathStyle style;
		AIBoolean fillVisible = false;
		AIBoolean strokeVisible = false;
		if (sAIPathStyle->GetPathStyleEx(path, &style, &fillVisible, &strokeVisible)) {
			return false;
		}
		if (!strokeVisible || !style.strokePaint) {
			return false;
		}
		style.stroke.width = static_cast<AIReal>(style.stroke.width * scaleFactor);
		if (style.stroke.width < 0.01f) {
			style.stroke.width = 0.01f;
		}
		return sAIPathStyle->SetPathStyleEx(path, &style, fillVisible, strokeVisible) == kNoErr;
	}

	bool ScaleLineStrokeWidths(AIArtHandle path, double scaleFactor)
	{
		if (!path) {
			return false;
		}
		if (sAIArtStyle && sAIArtStyleParser) {
			AIArtStyleHandle artStyle = nullptr;
			if (sAIArtStyle->GetArtStyle(path, &artStyle) == kNoErr && artStyle) {
				AIStyleParser parser = nullptr;
				if (sAIArtStyleParser->NewParser(&parser) == kNoErr && parser) {
					bool modified = false;
					if (sAIArtStyleParser->ParseStyle(parser, artStyle) == kNoErr) {
						const ai::int32 count = sAIArtStyleParser->CountPaintFields(parser);
						for (ai::int32 i = 0; i < count; ++i) {
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
							if (sAIArtStyleParser->GetStroke(field, &stroke, &paintData) != kNoErr) {
								continue;
							}
							stroke.width = static_cast<AIReal>(stroke.width * scaleFactor);
							if (stroke.width < 0.01f) {
								stroke.width = 0.01f;
							}
							if (sAIArtStyleParser->SetStroke(field, &stroke, &paintData) == kNoErr) {
								modified = true;
							}
						}
					}
					if (modified) {
						AIArtStyleHandle newStyle = nullptr;
						if (sAIArtStyleParser->CreateNewStyle(parser, &newStyle) == kNoErr && newStyle) {
							sAIArtStyle->SetArtStyle(path, newStyle);
							sAIArtStyleParser->DisposeParser(parser);
							return true;
						}
					}
					sAIArtStyleParser->DisposeParser(parser);
				}
			}
		}
		return ScaleStrokeWidth(path, scaleFactor);
	}

	bool GetMaxStrokeWidth(AIArtHandle path, double& outWidth)
	{
		outWidth = 0.0;
		if (!path) {
			return false;
		}
		if (sAIArtStyle && sAIArtStyleParser) {
			AIArtStyleHandle artStyle = nullptr;
			if (sAIArtStyle->GetArtStyle(path, &artStyle) == kNoErr && artStyle) {
				AIStyleParser parser = nullptr;
				if (sAIArtStyleParser->NewParser(&parser) == kNoErr && parser) {
					double maxWidth = 0.0;
					if (sAIArtStyleParser->ParseStyle(parser, artStyle) == kNoErr) {
						const ai::int32 count = sAIArtStyleParser->CountPaintFields(parser);
						for (ai::int32 i = 0; i < count; ++i) {
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
								if (stroke.width > maxWidth) {
									maxWidth = static_cast<double>(stroke.width);
								}
							}
						}
					}
					sAIArtStyleParser->DisposeParser(parser);
					if (maxWidth > 0.0) {
						outWidth = maxWidth;
						return true;
					}
				}
			}
		}
		return GetStrokeWidth(path, outWidth);
	}

	double GetBaselineMaxStrokeWidth(AIArtHandle art)
	{
		const std::string layerName = DuctworkGeometry::GetArtLayerName(art);
		if (layerName == "Thermostat Lines") {
			return 4.0;
		}
		return 8.0;
	}

	void EnsureOriginalTransform(AIArtHandle art, double currentScale, double currentRotation)
	{
		double originalScale = 0.0;
		if (!DuctworkMetadata::GetDouble(art, "MDUX_OriginalScale", originalScale)) {
			DuctworkMetadata::SetDouble(art, "MDUX_OriginalScale", currentScale);
		}
		double originalRotation = 0.0;
		if (!DuctworkMetadata::GetDouble(art, "MDUX_OriginalRotation", originalRotation)) {
			DuctworkMetadata::SetDouble(art, "MDUX_OriginalRotation", currentRotation);
		}
	}

	double ReadOriginalScale(AIArtHandle art, double fallback)
	{
		double value = 0.0;
		if (DuctworkMetadata::GetDouble(art, "MDUX_OriginalScale", value)) {
			return value;
		}
		return fallback;
	}

	double ReadOriginalRotation(AIArtHandle art, double fallback)
	{
		double value = 0.0;
		if (DuctworkMetadata::GetDouble(art, "MDUX_OriginalRotation", value)) {
			return value;
		}
		return fallback;
	}

	std::wstring FormatDecimal(double value)
	{
		std::wostringstream out;
		out << std::fixed << std::setprecision(2) << value;
		std::wstring text = out.str();
		while (text.size() > 1 && text.back() == L'0') {
			text.pop_back();
		}
		if (!text.empty() && text.back() == L'.') {
			text.pop_back();
		}
		return text;
	}

	bool GetArtCenter(AIArtHandle art, AIRealPoint& outCenter)
	{
		if (!art || !sAIArt) {
			return false;
		}
		AIRealRect bounds;
		AIErr err = sAIArt->GetArtBounds(art, &bounds);
		if (err != kNoErr) {
			DuctworkLog::Write("Panel GetArtBounds err=" + std::to_string(static_cast<int>(err)));
			err = sAIArt->GetArtTransformBounds(art, nullptr, kVisibleBounds | kExcludeGuideBounds, &bounds);
			if (err != kNoErr) {
				DuctworkLog::Write("Panel GetArtTransformBounds err=" + std::to_string(static_cast<int>(err)));
				return false;
			}
		}
		outCenter.h = (bounds.left + bounds.right) * 0.5f;
		outCenter.v = (bounds.top + bounds.bottom) * 0.5f;
		return true;
	}

	bool ApplyTransform(AIArtHandle art, double rotationDeg, double scale)
	{
		if (!art) {
			DuctworkLog::Write("Panel TransformArt skip: null art");
			return false;
		}
		if (!sAITransformArt) {
			DuctworkLog::Write("Panel TransformArt skip: suite unavailable");
			return false;
		}
		AIRealPoint center;
		if (!GetArtCenter(art, center)) {
			DuctworkLog::Write("Panel TransformArt skip: GetArtCenter failed");
			return false;
		}
		const double radians = rotationDeg * 3.14159265358979323846 / 180.0;
		const double cosA = std::cos(radians) * scale;
		const double sinA = std::sin(radians) * scale;
		const double tx = center.h - (cosA * center.h - sinA * center.v);
		const double ty = center.v - (sinA * center.h + cosA * center.v);

		AIRealMatrix matrix;
		matrix.a = static_cast<AIReal>(cosA);
		matrix.b = static_cast<AIReal>(sinA);
		matrix.c = static_cast<AIReal>(-sinA);
		matrix.d = static_cast<AIReal>(cosA);
		matrix.tx = static_cast<AIReal>(tx);
		matrix.ty = static_cast<AIReal>(ty);

		const ai::int32 flags = kTransformObjects | kTransformChildren;
		const AIErr err = sAITransformArt->TransformArt(art, &matrix, static_cast<AIReal>(1.0), flags);
		DuctworkLog::Write("Panel TransformArt result=" + std::to_string(static_cast<int>(err)));
		return err == kNoErr;
	}

	bool GetPlacedRotationDegrees(AIArtHandle art, double& outRotation)
	{
		outRotation = 0.0;
		if (!art || !sAIPlaced) {
			return false;
		}
		AIRealMatrix matrix{};
		if (sAIPlaced->GetPlacedMatrix(art, &matrix) != kNoErr) {
			return false;
		}
		outRotation = std::atan2(static_cast<double>(matrix.b), static_cast<double>(matrix.a)) * 180.0 /
			3.14159265358979323846;
		return true;
	}

	void SetControlFont(HWND control, HFONT font)
	{
		if (control && font) {
			SendMessage(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
		}
	}

	void ApplyFlatStyle(HWND control)
	{
		if (!control) {
			return;
		}
		LONG style = GetWindowLongW(control, GWL_STYLE);
		SetWindowLongW(control, GWL_STYLE, style | BS_FLAT);
		SetWindowTheme(control, L"", L"");
	}

	double Clamp(double value, double minValue, double maxValue)
	{
		return std::min(std::max(value, minValue), maxValue);
	}
}

EmoryDuctworkPanel::EmoryDuctworkPanel()
{
}

EmoryDuctworkPanel::~EmoryDuctworkPanel()
{
	Destroy();
}

ASErr EmoryDuctworkPanel::Create(SPPluginRef pluginRef)
{
	fPluginRef = pluginRef;
	if (!sAIPanel) {
		return kNoErr;
	}

	AISize panelSize = { 280, 420 };
	AIErr err = sAIPanel->Create(pluginRef,
		ai::UnicodeString("Ductwork"),
		ai::UnicodeString("Ductwork"),
		3,
		panelSize,
		true,
		nullptr,
		this,
		fPanel);
	if (err || !fPanel) {
		return err;
	}

#ifdef WIN_ENV
	if (sAIPanel->GetPlatformWindow(fPanel, fPanelWindow) == kNoErr && fPanelWindow) {
		fDefaultWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(fPanelWindow, GWLP_WNDPROC,
			reinterpret_cast<LONG_PTR>(&EmoryDuctworkPanel::PanelWndProc)));
		SetPropW(fPanelWindow, L"PDW_PANEL", this);
		CreateControls(fPanelWindow);
	}
#endif

	return kNoErr;
}

void EmoryDuctworkPanel::Destroy()
{
#ifdef WIN_ENV
	if (fPanelWindow && fDefaultWndProc) {
		SetWindowLongPtr(fPanelWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(fDefaultWndProc));
	}
	if (fFont) {
		DeleteObject(fFont);
		fFont = nullptr;
	}
	if (fPanelBrush) {
		DeleteObject(fPanelBrush);
		fPanelBrush = nullptr;
	}
	if (fEditBrush) {
		DeleteObject(fEditBrush);
		fEditBrush = nullptr;
	}
	if (fButtonBrush) {
		DeleteObject(fButtonBrush);
		fButtonBrush = nullptr;
	}
	fPanelWindow = nullptr;
#endif
	fPanel = nullptr;
}

void EmoryDuctworkPanel::UpdateSelectionSummary()
{
#ifdef WIN_ENV
	std::vector<AIArtHandle> selection;
	CollectSelectedArt(selection);
	if (!selection.empty()) {
		fCachedSelection = selection;
	} else if (!fCachedSelection.empty()) {
		selection = fCachedSelection;
	}
	if (selection.empty()) {
		SetStatusText(L"No selection.");
	} else {
		std::wostringstream status;
		status << L"Selection: " << static_cast<int>(selection.size());
		SetStatusText(status.str().c_str());
	}
	std::vector<AIArtHandle> linePaths;
	std::vector<AIArtHandle> partItems;
	std::vector<AIArtHandle> rotatableParts;
	for (size_t i = 0; i < selection.size(); ++i) {
		CollectLinePathsSelected(selection[i], linePaths, true);
		CollectPartItemsSelected(selection[i], partItems, true);
		CollectRotatablePartItemsSelected(selection[i], rotatableParts, true);
	}
	std::set<AIArtHandle> partSet;
	std::set<AIArtHandle> rotatableSet;
	std::vector<AIArtHandle> resolvedParts;
	std::vector<AIArtHandle> resolvedRotatable;
	resolvedParts.reserve(partItems.size());
	for (size_t i = 0; i < partItems.size(); ++i) {
		AIArtHandle art = partItems[i];
		AIArtHandle placed = ResolvePlacedAncestor(art);
		if (placed) {
			art = placed;
		}
		if (partSet.insert(art).second) {
			resolvedParts.push_back(art);
		}
	}
	resolvedRotatable.reserve(rotatableParts.size());
	for (size_t i = 0; i < rotatableParts.size(); ++i) {
		AIArtHandle art = rotatableParts[i];
		AIArtHandle placed = ResolvePlacedAncestor(art);
		if (placed) {
			art = placed;
		}
		if (rotatableSet.insert(art).second) {
			resolvedRotatable.push_back(art);
		}
	}
	partItems.swap(resolvedParts);
	rotatableParts.swap(resolvedRotatable);

	std::vector<AIArtHandle> scaleTargets = partItems;
	scaleTargets.insert(scaleTargets.end(), linePaths.begin(), linePaths.end());

	DuctworkMetadata::TransformSummary scaleSummary = DuctworkMetadata::SummarizeSelectionTransform(scaleTargets);
	DuctworkMetadata::TransformSummary rotationSummary = DuctworkMetadata::SummarizeSelectionTransform(rotatableParts);

	if (!scaleTargets.empty()) {
		fScaleValue = scaleSummary.scale;
		UpdateScaleUI(scaleSummary.scale, scaleSummary.mixedScale);
	} else {
		UpdateScaleUI(fScaleValue, true);
	}

	if (!rotatableParts.empty()) {
		fRotationValue = rotationSummary.rotation;
		UpdateRotationUI(rotationSummary.rotation, rotationSummary.mixedRotation);
	} else {
		UpdateRotationUI(fRotationValue, true);
	}
	fScaleUserChanged = false;
	fRotationUserChanged = false;
#endif
}

void EmoryDuctworkPanel::SetRotationOverrideValue(double value, bool hasValue)
{
	fRotationOverrideValue = value;
	fHasRotationOverride = hasValue;
	UpdateRotationOverrideUI();
}

bool EmoryDuctworkPanel::GetRotationOverrideValue(double& outValue) const
{
	if (!fHasRotationOverride) {
		return false;
	}
	outValue = fRotationOverrideValue;
	return true;
}

#ifdef WIN_ENV
LRESULT CALLBACK EmoryDuctworkPanel::PanelWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	EmoryDuctworkPanel* panel = reinterpret_cast<EmoryDuctworkPanel*>(GetPropW(hWnd, L"PDW_PANEL"));
	if (panel) {
		LRESULT result = 0;
		if (panel->HandlePanelMessage(hWnd, msg, wParam, lParam)) {
			return result;
		}
		if (panel->fDefaultWndProc) {
			return CallWindowProc(panel->fDefaultWndProc, hWnd, msg, wParam, lParam);
		}
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT EmoryDuctworkPanel::HandlePanelMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_ERASEBKGND:
	{
		if (fPanelBrush) {
			HDC dc = reinterpret_cast<HDC>(wParam);
			RECT rect = {};
			GetClientRect(hWnd, &rect);
			FillRect(dc, &rect, fPanelBrush);
			return 1;
		}
		break;
	}
	case WM_CTLCOLORSTATIC:
	{
		HDC dc = reinterpret_cast<HDC>(wParam);
		SetTextColor(dc, fColorText);
		SetBkColor(dc, fColorBg);
		return reinterpret_cast<LRESULT>(fPanelBrush ? fPanelBrush : GetStockObject(NULL_BRUSH));
	}
	case WM_CTLCOLOREDIT:
	{
		HDC dc = reinterpret_cast<HDC>(wParam);
		SetTextColor(dc, fColorText);
		SetBkColor(dc, fColorEdit);
		return reinterpret_cast<LRESULT>(fEditBrush ? fEditBrush : GetStockObject(NULL_BRUSH));
	}
	case WM_CTLCOLORBTN:
	{
		HDC dc = reinterpret_cast<HDC>(wParam);
		SetTextColor(dc, fColorText);
		SetBkColor(dc, fColorButton);
		return reinterpret_cast<LRESULT>(fButtonBrush ? fButtonBrush : GetStockObject(NULL_BRUSH));
	}
	case WM_COMMAND:
	{
		const int id = LOWORD(wParam);
		const int notify = HIWORD(wParam);
		if (id == kIdRotationGetAngle && notify == BN_CLICKED) {
			double angle = 0.0;
			if (TryComputeSelectionAngle(angle)) {
				SetRotationOverrideValue(angle, true);
				SetStatusText(L"Angle captured.");
			} else {
				SetStatusText(L"Select a ductwork line.");
			}
			return 0;
		}
		if (id == kIdRotationClear && notify == BN_CLICKED) {
			SetRotationOverrideValue(0.0, false);
			return 0;
		}
		if (id == kIdProcessPlacedApi && notify == BN_CLICKED) {
			ProcessDuctworkPlugin* plugin = ProcessDuctworkPlugin::GetInstance();
			if (plugin) {
				plugin->RunProcessPlacedApiFromPanel();
			}
			return 0;
		}
		if (id == kIdApplyTransform && notify == BN_CLICKED) {
			ApplyTransformFromUI();
			return 0;
		}
		if (id == kIdResetOriginal && notify == BN_CLICKED) {
			ResetTransformToOriginal();
			return 0;
		}
		if (id == kIdResetRotation && notify == BN_CLICKED) {
			ResetRotation();
			return 0;
		}
		if (id == kIdResetScale && notify == BN_CLICKED) {
			ResetScale();
			return 0;
		}
		if (id == kIdResetStrokes && notify == BN_CLICKED) {
			ResetStrokes();
			return 0;
		}
		if (id == kIdQuickRotate45 && notify == BN_CLICKED) {
			ApplyQuickRotate(45.0);
			return 0;
		}
		if (id == kIdQuickRotate90 && notify == BN_CLICKED) {
			ApplyQuickRotate(90.0);
			return 0;
		}
		if (id == kIdQuickRotate180 && notify == BN_CLICKED) {
			ApplyQuickRotate(180.0);
			return 0;
		}
		if (id == kIdQuickRotateNeg45 && notify == BN_CLICKED) {
			ApplyQuickRotate(-45.0);
			return 0;
		}
		if (id == kIdQuickRotateNeg90 && notify == BN_CLICKED) {
			ApplyQuickRotate(-90.0);
			return 0;
		}
		if (id == kIdQuickRotateCustom && notify == BN_CLICKED) {
			wchar_t buffer[64] = L"0";
			if (fRotationEditTransform && GetWindowTextW(fRotationEditTransform, buffer, 64) > 0) {
				double angle = _wtof(buffer);
				ApplyQuickRotate(angle);
			}
			return 0;
		}
		if (id == kIdRotationOverrideEdit && (notify == EN_KILLFOCUS || notify == EN_CHANGE)) {
			ApplyRotationOverride();
			return 0;
		}
		if (id == kIdScaleEdit && notify == EN_KILLFOCUS) {
			double value = GetScaleValue();
			SetScaleValue(value);
			fScaleUserChanged = true;
			fRotationUserChanged = false;
			if (fLiveCheck && SendMessage(fLiveCheck, BM_GETCHECK, 0, 0) == BST_CHECKED) {
				ApplyTransformFromUI();
			}
			return 0;
		}
		if (id == kIdRotateEdit && notify == EN_KILLFOCUS) {
			double value = GetRotationValue();
			SetRotationValue(value);
			fScaleUserChanged = false;
			fRotationUserChanged = true;
			if (fLiveCheck && SendMessage(fLiveCheck, BM_GETCHECK, 0, 0) == BST_CHECKED) {
				ApplyTransformFromUI();
			}
			return 0;
		}
		break;
	}
	case WM_HSCROLL:
	{
		HWND trackbar = reinterpret_cast<HWND>(lParam);
		if (trackbar == fScaleSlider) {
			HandleTrackbarScroll(trackbar, true, LOWORD(wParam));
			return 0;
		}
		if (trackbar == fRotationSlider) {
			HandleTrackbarScroll(trackbar, false, LOWORD(wParam));
			return 0;
		}
		break;
	}
	default:
		break;
	}
	return 0;
}

bool EmoryDuctworkPanel::GetSelectionForAction(std::vector<AIArtHandle>& outSelection, bool allowCache)
{
	outSelection.clear();
	if (CollectSelectedArt(outSelection) && !outSelection.empty()) {
		fCachedSelection = outSelection;
		return true;
	}
	if (allowCache && !fCachedSelection.empty()) {
		outSelection = fCachedSelection;
		DuctworkLog::Write("Panel selection: using cached selection size=" +
			std::to_string(static_cast<int>(outSelection.size())));
		return !outSelection.empty();
	}
	DuctworkLog::Write("Panel selection: no cached selection");
	return false;
}

void EmoryDuctworkPanel::CreateControls(HWND parent)
{
	INITCOMMONCONTROLSEX icc = {};
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_BAR_CLASSES;
	InitCommonControlsEx(&icc);

	fFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	fPanelBrush = CreateSolidBrush(fColorBg);
	fEditBrush = CreateSolidBrush(fColorEdit);
	fButtonBrush = CreateSolidBrush(fColorButton);
	const int margin = 8;
	int y = 8;
	const int labelWidth = 110;
	const int editWidth = 70;
	const int btnWidth = 70;
	const int rowHeight = 22;

	HWND rotLabel = CreateWindowExW(0, L"STATIC", L"Rotation override (Â°)", WS_CHILD | WS_VISIBLE,
		margin, y, 200, rowHeight, parent, nullptr, nullptr, nullptr);
	SetControlFont(rotLabel, fFont);
	y += rowHeight + 2;

	fRotationEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
		WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
		margin, y, editWidth, rowHeight, parent, reinterpret_cast<HMENU>(kIdRotationOverrideEdit),
		GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fRotationEdit, fFont);

	fRotationGetBtn = CreateWindowExW(0, L"BUTTON", L"Get Angle", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		margin + editWidth + 6, y, btnWidth, rowHeight, parent, reinterpret_cast<HMENU>(kIdRotationGetAngle),
		GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fRotationGetBtn, fFont);
	ApplyFlatStyle(fRotationGetBtn);

	fRotationClearBtn = CreateWindowExW(0, L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		margin + editWidth + btnWidth + 12, y, btnWidth, rowHeight, parent, reinterpret_cast<HMENU>(kIdRotationClear),
		GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fRotationClearBtn, fFont);
	ApplyFlatStyle(fRotationClearBtn);

	y += rowHeight + 10;

	fProcessPlacedApiBtn = CreateWindowExW(0, L"BUTTON", L"Process Ductwork (Placed)",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, margin, y, 200, rowHeight, parent,
		reinterpret_cast<HMENU>(kIdProcessPlacedApi), GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fProcessPlacedApiBtn, fFont);
	ApplyFlatStyle(fProcessPlacedApiBtn);
	y += rowHeight + 10;

	fResetStrokesBtn = CreateWindowExW(0, L"BUTTON", L"Reset Strokes", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		margin, y, 110, rowHeight, parent, reinterpret_cast<HMENU>(kIdResetStrokes),
		GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fResetStrokesBtn, fFont);
	ApplyFlatStyle(fResetStrokesBtn);

	fResetScaleBtn = CreateWindowExW(0, L"BUTTON", L"Reset Scale", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		margin + 116, y, 80, rowHeight, parent, reinterpret_cast<HMENU>(kIdResetScale),
		GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fResetScaleBtn, fFont);
	ApplyFlatStyle(fResetScaleBtn);

	fResetRotationBtn = CreateWindowExW(0, L"BUTTON", L"Reset Rotation", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		margin + 202, y, 90, rowHeight, parent, reinterpret_cast<HMENU>(kIdResetRotation),
		GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fResetRotationBtn, fFont);
	ApplyFlatStyle(fResetRotationBtn);

	y += rowHeight + 8;

	HWND scaleLabel = CreateWindowExW(0, L"STATIC", L"Scale (%)", WS_CHILD | WS_VISIBLE,
		margin, y, labelWidth, rowHeight, parent, nullptr, nullptr, nullptr);
	SetControlFont(scaleLabel, fFont);
	fScaleSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ,
		margin + labelWidth, y, 140, rowHeight, parent, reinterpret_cast<HMENU>(kIdScaleSlider),
		GetModuleHandleW(nullptr), nullptr);
	SendMessage(fScaleSlider, TBM_SETRANGE, TRUE, MAKELONG(100, 4000));
	SendMessage(fScaleSlider, TBM_SETPOS, TRUE, 1000);
	fScaleLastPos = 1000;
	SendMessage(fScaleSlider, TBM_SETBKCOLOR, 0, fColorBg);
	SetWindowTheme(fScaleSlider, L"", L"");
	fScaleEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"100",
		WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
		margin + labelWidth + 146, y, editWidth, rowHeight, parent, reinterpret_cast<HMENU>(kIdScaleEdit),
		GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fScaleSlider, fFont);
	SetControlFont(fScaleEdit, fFont);
	y += rowHeight + 6;

	HWND rotateLabel = CreateWindowExW(0, L"STATIC", L"Rotate (Â°)", WS_CHILD | WS_VISIBLE,
		margin, y, labelWidth, rowHeight, parent, nullptr, nullptr, nullptr);
	SetControlFont(rotateLabel, fFont);
	fRotationSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ,
		margin + labelWidth, y, 140, rowHeight, parent, reinterpret_cast<HMENU>(kIdRotateSlider),
		GetModuleHandleW(nullptr), nullptr);
	SendMessage(fRotationSlider, TBM_SETRANGE, TRUE, MAKELONG(-1800, 1800));
	SendMessage(fRotationSlider, TBM_SETPOS, TRUE, 0);
	fRotationLastPos = 0;
	SendMessage(fRotationSlider, TBM_SETBKCOLOR, 0, fColorBg);
	SetWindowTheme(fRotationSlider, L"", L"");
	fRotationEditTransform = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0",
		WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
		margin + labelWidth + 146, y, editWidth, rowHeight, parent, reinterpret_cast<HMENU>(kIdRotateEdit),
		GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fRotationSlider, fFont);
	SetControlFont(fRotationEditTransform, fFont);
	y += rowHeight + 6;

	fLiveCheck = CreateWindowExW(0, L"BUTTON", L"Live Preview", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		margin, y, 120, rowHeight, parent, reinterpret_cast<HMENU>(kIdLiveCheck),
		GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fLiveCheck, fFont);
	ApplyFlatStyle(fLiveCheck);
	SendMessage(fLiveCheck, BM_SETCHECK, BST_CHECKED, 0);
	y += rowHeight + 6;

	fApplyBtn = CreateWindowExW(0, L"BUTTON", L"Apply Transform", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		margin, y, 120, rowHeight, parent, reinterpret_cast<HMENU>(kIdApplyTransform),
		GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fApplyBtn, fFont);
	ApplyFlatStyle(fApplyBtn);

	fResetOriginalBtn = CreateWindowExW(0, L"BUTTON", L"Reset to Original", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		margin + 130, y, 140, rowHeight, parent, reinterpret_cast<HMENU>(kIdResetOriginal),
		GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fResetOriginalBtn, fFont);
	ApplyFlatStyle(fResetOriginalBtn);
	y += rowHeight + 12;

	fQuickRotate45 = CreateWindowExW(0, L"BUTTON", L"45Â°", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		margin, y, 60, rowHeight, parent, reinterpret_cast<HMENU>(kIdQuickRotate45),
		GetModuleHandleW(nullptr), nullptr);
	fQuickRotate90 = CreateWindowExW(0, L"BUTTON", L"90Â°", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		margin + 66, y, 60, rowHeight, parent, reinterpret_cast<HMENU>(kIdQuickRotate90),
		GetModuleHandleW(nullptr), nullptr);
	fQuickRotate180 = CreateWindowExW(0, L"BUTTON", L"180Â°", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		margin + 132, y, 60, rowHeight, parent, reinterpret_cast<HMENU>(kIdQuickRotate180),
		GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fQuickRotate45, fFont);
	SetControlFont(fQuickRotate90, fFont);
	SetControlFont(fQuickRotate180, fFont);
	ApplyFlatStyle(fQuickRotate45);
	ApplyFlatStyle(fQuickRotate90);
	ApplyFlatStyle(fQuickRotate180);
	y += rowHeight + 4;

	fQuickRotateNeg45 = CreateWindowExW(0, L"BUTTON", L"-45Â°", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		margin, y, 60, rowHeight, parent, reinterpret_cast<HMENU>(kIdQuickRotateNeg45),
		GetModuleHandleW(nullptr), nullptr);
	fQuickRotateNeg90 = CreateWindowExW(0, L"BUTTON", L"-90Â°", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		margin + 66, y, 60, rowHeight, parent, reinterpret_cast<HMENU>(kIdQuickRotateNeg90),
		GetModuleHandleW(nullptr), nullptr);
	fQuickRotateCustom = CreateWindowExW(0, L"BUTTON", L"Custom", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		margin + 132, y, 60, rowHeight, parent, reinterpret_cast<HMENU>(kIdQuickRotateCustom),
		GetModuleHandleW(nullptr), nullptr);
	SetControlFont(fQuickRotateNeg45, fFont);
	SetControlFont(fQuickRotateNeg90, fFont);
	SetControlFont(fQuickRotateCustom, fFont);
	ApplyFlatStyle(fQuickRotateNeg45);
	ApplyFlatStyle(fQuickRotateNeg90);
	ApplyFlatStyle(fQuickRotateCustom);
	y += rowHeight + 8;

	fStatusText = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
		margin, y, 240, rowHeight, parent, nullptr, nullptr, nullptr);
	SetControlFont(fStatusText, fFont);
}

void EmoryDuctworkPanel::ApplyTransformFromUI()
{
	AppContext appContext(fPluginRef);
	const double targetScale = GetScaleValue();
	const double targetRotation = GetRotationValue();

	ApplyTransformSelection(targetScale, targetRotation, false, true, false, nullptr);
}

bool EmoryDuctworkPanel::ApplyTransformSelection(double targetScale, double targetRotation, bool allowCache, bool updateUI, bool livePreview, std::string* outMessage)
{
	AppContext appContext(fPluginRef);
	std::vector<AIArtHandle> selection;
	if (!GetSelectionForAction(selection, allowCache)) {
		DuctworkLog::Write("Panel ApplyTransform: no selection");
		if (outMessage) {
			*outMessage = "No selection.";
		}
		if (updateUI) {
			SetStatusText(L"No selection.");
		}
		return false;
	}

	if (!selection.empty()) {
		short type0 = kUnknownArt;
		sAIArt->GetArtType(selection[0], &type0);
		DuctworkLog::Write("Panel ApplyTransform: selection[0]=" + DuctworkGeometry::GetArtLayerName(selection[0]) +
			" type=" + std::to_string(static_cast<int>(type0)) +
			" partCount=" + std::to_string(CountDuctworkPartDescendants(selection[0])) +
			" lineCount=" + std::to_string(CountDuctworkLineDescendants(selection[0])));
		if (selection.size() > 1) {
			short type1 = kUnknownArt;
			sAIArt->GetArtType(selection[1], &type1);
			DuctworkLog::Write("Panel ApplyTransform: selection[1]=" + DuctworkGeometry::GetArtLayerName(selection[1]) +
				" type=" + std::to_string(static_cast<int>(type1)) +
				" partCount=" + std::to_string(CountDuctworkPartDescendants(selection[1])) +
				" lineCount=" + std::to_string(CountDuctworkLineDescendants(selection[1])));
		}
	}

	DuctworkLog::Write("Panel ApplyTransform: selection=" + std::to_string(static_cast<int>(selection.size())) +
		" targetScale=" + std::to_string(targetScale) +
		" targetRotation=" + std::to_string(targetRotation));

	const std::vector<AIArtHandle> selectionSnapshot = selection;
	std::vector<AIArtHandle> linePaths;
	std::vector<AIArtHandle> partItems;
	std::vector<AIArtHandle> rotatableParts;
	for (size_t i = 0; i < selection.size(); ++i) {
		CollectLinePathsSelected(selection[i], linePaths, true);
		CollectPartItemsSelected(selection[i], partItems, true);
		CollectRotatablePartItemsSelected(selection[i], rotatableParts, true);
	}

	DuctworkLog::Write("Panel ApplyTransform: linePaths=" + std::to_string(static_cast<int>(linePaths.size())) +
		" partItems=" + std::to_string(static_cast<int>(partItems.size())) +
		" rotatableParts=" + std::to_string(static_cast<int>(rotatableParts.size())));

	bool applyScale = fScaleUserChanged;
	bool applyRotation = fRotationUserChanged;
	if (!applyScale && !applyRotation) {
		applyScale = std::fabs(targetScale - 100.0) >= 0.0001;
		applyRotation = std::fabs(targetRotation) >= 0.0001;
	}

	size_t lineScaled = 0;
	size_t partTransformed = 0;

	for (size_t i = 0; i < linePaths.size(); ++i) {
		AIArtHandle art = linePaths[i];
		double currentScale = 100.0;
		const double baselineStroke = GetBaselineMaxStrokeWidth(art);
		double currentStroke = 0.0;
		if (GetMaxStrokeWidth(art, currentStroke) && currentStroke > 0.0 && baselineStroke > 0.0) {
			currentScale = (currentStroke / baselineStroke) * 100.0;
			DuctworkMetadata::SetDouble(art, "MDUX_OriginalStrokeWidth", baselineStroke);
		}
		if (!applyScale) {
			continue;
		}
		const double scaleFactor = (currentScale == 0.0) ? 1.0 : (targetScale / currentScale);
		if (std::fabs(scaleFactor - 1.0) < 0.0001) {
			continue;
		}
		if (ScaleLineStrokeWidths(art, scaleFactor)) {
			++lineScaled;
			DuctworkMetadata::SetDouble(art, "MDUX_CurrentScale", targetScale);
		}
	}

	for (size_t i = 0; i < partItems.size(); ++i) {
		AIArtHandle art = partItems[i];
		if (!art) {
			continue;
		}
		const double currentScale = DuctworkMetadata::ReadScaleOrDefault(art, 100.0);
		const double currentRotation = DuctworkMetadata::ReadRotationOrDefault(art, 0.0);
		EnsureOriginalTransform(art, currentScale, currentRotation);
		const double scaleFactor = applyScale ? ((currentScale == 0.0) ? 1.0 : (targetScale / currentScale)) : 1.0;
		const bool canRotate = IsRotatablePartArt(art);
		const double rotationDelta = (canRotate && applyRotation) ? (targetRotation - currentRotation) : 0.0;
		if (std::fabs(scaleFactor - 1.0) < 0.0001 && std::fabs(rotationDelta) < 0.0001) {
			continue;
		}
		short artType = kUnknownArt;
		if (sAIArt) {
			sAIArt->GetArtType(art, &artType);
		}
		double beforePlacedRotation = 0.0;
		const bool trackPlacedRotation = (artType == kPlacedArt && applyScale && !applyRotation &&
			GetPlacedRotationDegrees(art, beforePlacedRotation));
		if (artType == kPlacedArt) {
			DuctworkLog::Write("Panel Part: scaleFactor=" + std::to_string(scaleFactor) +
				" rotationDelta=" + std::to_string(rotationDelta) +
				" metaScale=" + std::to_string(currentScale) +
				" metaRotation=" + std::to_string(currentRotation) +
				" placedRotationBefore=" + std::to_string(beforePlacedRotation));
		}
		const bool applied = ApplyTransform(art, rotationDelta, scaleFactor);
		if (!applied) {
			DuctworkLog::Write("Panel ApplyTransform: transform failed");
			continue;
		}
		if (trackPlacedRotation) {
			double afterPlacedRotation = 0.0;
			if (GetPlacedRotationDegrees(art, afterPlacedRotation)) {
				DuctworkLog::Write("Panel Part: placedRotationAfter=" + std::to_string(afterPlacedRotation));
				if (std::fabs(afterPlacedRotation - beforePlacedRotation) > 0.01) {
					DuctworkLog::Write("Panel Part: restoring placed rotation delta=" +
						std::to_string(beforePlacedRotation - afterPlacedRotation));
					ApplyTransform(art, beforePlacedRotation - afterPlacedRotation, 1.0);
				}
			}
		}
		if (canRotate && std::fabs(rotationDelta) >= 0.0001 && artType == kPlacedArt) {
			AIBoolean updated = false;
			ASErr linkErr = sAIArt->UpdateArtworkLink(art, true, &updated);
			if (linkErr != kNoErr) {
				DuctworkLog::Write("Panel ApplyTransform: UpdateArtworkLink failed");
				DuctworkLog::Error("Panel UpdateArtworkLink", linkErr);
			}
		}
		++partTransformed;
		if (applyScale) {
			DuctworkMetadata::SetDouble(art, "MDUX_CurrentScale", targetScale);
		}
		if (canRotate && applyRotation && std::fabs(rotationDelta) >= 0.0001) {
			DuctworkMetadata::SetDouble(art, "MDUX_CumulativeRotation", targetRotation);
			DuctworkMetadata::SetDouble(art, "MDUX_RotationOverride", targetRotation);
		}
	}

	if (updateUI) {
		if (linePaths.empty() && partItems.empty()) {
			SetStatusText(L"No ductwork parts or lines selected.");
		} else if (lineScaled == 0 && partTransformed == 0) {
			SetStatusText(L"No transform changes.");
		} else {
			SetStatusText(L"Transform applied.");
		}
		UpdateSelectionSummary();
	}

	ReselectArtList(selectionSnapshot);
	if (outMessage) {
		if (linePaths.empty() && partItems.empty()) {
			*outMessage = "No ductwork parts or lines selected.";
		} else if (lineScaled == 0 && partTransformed == 0) {
			*outMessage = "No transform changes.";
		} else {
			*outMessage = "Transformed " + std::to_string(lineScaled + partTransformed) + " item(s).";
		}
	}

	return (lineScaled > 0 || partTransformed > 0);
}

void EmoryDuctworkPanel::ApplyQuickRotate(double angle)
{
	AppContext appContext(fPluginRef);
	std::vector<AIArtHandle> selection;
	if (!GetSelectionForAction(selection, false)) {
		SetStatusText(L"No selection.");
		return;
	}
	const std::vector<AIArtHandle> selectionSnapshot = selection;
	std::vector<AIArtHandle> partItems;
	for (size_t i = 0; i < selection.size(); ++i) {
		CollectRotatablePartItemsRecursive(selection[i], partItems);
	}
	for (size_t i = 0; i < partItems.size(); ++i) {
		AIArtHandle art = partItems[i];
		if (!art) {
			continue;
		}
		const double currentRotation = DuctworkMetadata::ReadRotationOrDefault(art, 0.0);
		const double rotationDelta = angle - currentRotation;
		ApplyTransform(art, rotationDelta, 1.0);
		DuctworkMetadata::SetDouble(art, "MDUX_CumulativeRotation", angle);
		DuctworkMetadata::SetDouble(art, "MDUX_RotationOverride", angle);
	}
	SetRotationValue(angle);
	SetStatusText(L"Rotation applied.");
	UpdateSelectionSummary();
	ReselectArtList(selectionSnapshot);
}

void EmoryDuctworkPanel::ResetTransformToOriginal()
{
	AppContext appContext(fPluginRef);
	std::vector<AIArtHandle> selection;
	if (!GetSelectionForAction(selection, false)) {
		SetStatusText(L"No selection.");
		return;
	}
	std::vector<AIArtHandle> linePaths;
	std::vector<AIArtHandle> partItems;
	std::vector<AIArtHandle> rotatableParts;
	for (size_t i = 0; i < selection.size(); ++i) {
		CollectLinePathsRecursive(selection[i], linePaths);
		CollectPartItemsRecursive(selection[i], partItems);
		CollectRotatablePartItemsRecursive(selection[i], rotatableParts);
	}

	for (size_t i = 0; i < linePaths.size(); ++i) {
		AIArtHandle art = linePaths[i];
		double currentScale = DuctworkMetadata::ReadScaleOrDefault(art, 100.0);
		const double originalScale = ReadOriginalScale(art, 100.0);
		double currentStroke = 0.0;
		const double baselineStroke = GetBaselineMaxStrokeWidth(art);
		if (baselineStroke > 0.0 && GetMaxStrokeWidth(art, currentStroke) && currentStroke > 0.0) {
			currentScale = (currentStroke / baselineStroke) * 100.0;
			DuctworkMetadata::SetDouble(art, "MDUX_OriginalStrokeWidth", baselineStroke);
		}
		const double scaleFactor = (currentScale == 0.0) ? 1.0 : (originalScale / currentScale);
		DuctworkLog::Write("Panel ResetOriginal: line scale originalScale=" + std::to_string(originalScale) +
			" currentScale=" + std::to_string(currentScale) +
			" scaleFactor=" + std::to_string(scaleFactor) +
			" originalStroke=" + std::to_string(baselineStroke) +
			" currentStroke=" + std::to_string(currentStroke));
		if (std::fabs(scaleFactor - 1.0) < 0.0001) {
			continue;
		}
		if (ScaleLineStrokeWidths(art, scaleFactor)) {
			DuctworkMetadata::SetDouble(art, "MDUX_CurrentScale", originalScale);
		}
	}

	for (size_t i = 0; i < partItems.size(); ++i) {
		AIArtHandle art = partItems[i];
		if (!art) {
			continue;
		}
		const double currentScale = DuctworkMetadata::ReadScaleOrDefault(art, 100.0);
		const double currentRotation = DuctworkMetadata::ReadRotationOrDefault(art, 0.0);
		const double originalScale = ReadOriginalScale(art, 100.0);
		const double originalRotation = ReadOriginalRotation(art, 0.0);
		const double scaleFactor = (currentScale == 0.0) ? 1.0 : (originalScale / currentScale);
		const bool canRotate = IsRotatablePartArt(art);
		const double rotationDelta = canRotate ? (originalRotation - currentRotation) : 0.0;
		ApplyTransform(art, rotationDelta, scaleFactor);
		DuctworkMetadata::SetDouble(art, "MDUX_CurrentScale", originalScale);
		if (canRotate) {
			DuctworkMetadata::SetDouble(art, "MDUX_CumulativeRotation", originalRotation);
			DuctworkMetadata::SetDouble(art, "MDUX_RotationOverride", originalRotation);
		}
	}
	SetScaleValue(100.0);
	SetRotationValue(0.0);
	SetStatusText(L"Reset to original.");
	UpdateSelectionSummary();
}

void EmoryDuctworkPanel::ResetRotation()
{
	AppContext appContext(fPluginRef);
	std::vector<AIArtHandle> selection;
	if (!GetSelectionForAction(selection, false)) {
		SetStatusText(L"No selection.");
		return;
	}
	const std::vector<AIArtHandle> selectionSnapshot = selection;
	std::vector<AIArtHandle> partItems;
	for (size_t i = 0; i < selection.size(); ++i) {
		CollectRotatablePartItemsRecursive(selection[i], partItems);
	}
	for (size_t i = 0; i < partItems.size(); ++i) {
		AIArtHandle art = partItems[i];
		if (!art) {
			continue;
		}
		const double currentRotation = DuctworkMetadata::ReadRotationOrDefault(art, 0.0);
		const double originalRotation = ReadOriginalRotation(art, 0.0);
		const double rotationDelta = originalRotation - currentRotation;
		if (std::fabs(rotationDelta) < 0.0001) {
			continue;
		}
		ApplyTransform(art, rotationDelta, 1.0);
		DuctworkMetadata::SetDouble(art, "MDUX_CumulativeRotation", originalRotation);
		DuctworkMetadata::SetDouble(art, "MDUX_RotationOverride", originalRotation);
	}
	SetStatusText(L"Rotation reset.");
	UpdateSelectionSummary();
	ReselectArtList(selectionSnapshot);
}

void EmoryDuctworkPanel::ResetScale()
{
	AppContext appContext(fPluginRef);
	std::vector<AIArtHandle> selection;
	if (!GetSelectionForAction(selection, false)) {
		SetStatusText(L"No selection.");
		return;
	}
	std::vector<AIArtHandle> linePaths;
	std::vector<AIArtHandle> partItems;
	for (size_t i = 0; i < selection.size(); ++i) {
		CollectLinePathsRecursive(selection[i], linePaths);
		CollectPartItemsRecursive(selection[i], partItems);
	}

	for (size_t i = 0; i < linePaths.size(); ++i) {
		AIArtHandle art = linePaths[i];
		double currentScale = DuctworkMetadata::ReadScaleOrDefault(art, 100.0);
		const double originalScale = ReadOriginalScale(art, 100.0);
		double currentStroke = 0.0;
		const double baselineStroke = GetBaselineMaxStrokeWidth(art);
		if (baselineStroke > 0.0 && GetMaxStrokeWidth(art, currentStroke) && currentStroke > 0.0) {
			currentScale = (currentStroke / baselineStroke) * 100.0;
			DuctworkMetadata::SetDouble(art, "MDUX_OriginalStrokeWidth", baselineStroke);
		}
		const double scaleFactor = (currentScale == 0.0) ? 1.0 : (originalScale / currentScale);
		DuctworkLog::Write("Panel ResetScale: line scale originalScale=" + std::to_string(originalScale) +
			" currentScale=" + std::to_string(currentScale) +
			" scaleFactor=" + std::to_string(scaleFactor) +
			" originalStroke=" + std::to_string(baselineStroke) +
			" currentStroke=" + std::to_string(currentStroke));
		if (std::fabs(scaleFactor - 1.0) < 0.0001) {
			continue;
		}
		if (ScaleLineStrokeWidths(art, scaleFactor)) {
			DuctworkMetadata::SetDouble(art, "MDUX_CurrentScale", originalScale);
		}
	}

	for (size_t i = 0; i < partItems.size(); ++i) {
		AIArtHandle art = partItems[i];
		if (!art) {
			continue;
		}
		const double currentScale = DuctworkMetadata::ReadScaleOrDefault(art, 100.0);
		const double originalScale = ReadOriginalScale(art, 100.0);
		const double scaleFactor = (currentScale == 0.0) ? 1.0 : (originalScale / currentScale);
		ApplyTransform(art, 0.0, scaleFactor);
		DuctworkMetadata::SetDouble(art, "MDUX_CurrentScale", originalScale);
	}
	SetScaleValue(100.0);
	SetStatusText(L"Scale reset.");
	UpdateSelectionSummary();
}

void EmoryDuctworkPanel::ResetStrokes()
{
	AppContext appContext(fPluginRef);
	AIDocumentHandle document = nullptr;
	if (!sAIDocument || sAIDocument->GetDocument(&document) || !document) {
		SetStatusText(L"No document.");
		return;
	}
	std::vector<AIArtHandle> paths;
	if (!CollectSelectedPaths(paths)) {
		SetStatusText(L"No paths selected.");
		return;
	}
	std::vector<DuctworkPath> ductworkPaths;
	for (size_t i = 0; i < paths.size(); ++i) {
		std::vector<DuctworkPoint> points;
		bool closed = false;
		if (!DuctworkGeometry::GetPathPoints(paths[i], points, closed)) {
			continue;
		}
		DuctworkPath entry;
		entry.art = paths[i];
		entry.points = points;
		entry.closed = closed;
		entry.layerName = DuctworkGeometry::GetArtLayerName(paths[i]);
		ductworkPaths.push_back(entry);
	}
	DuctworkStyles::ApplyLineStyles(document, ductworkPaths);
	SetStatusText(L"Strokes reset.");
}

void EmoryDuctworkPanel::UpdateUIFromSummary()
{
	UpdateSelectionSummary();
	UpdateRotationOverrideUI();
}

void EmoryDuctworkPanel::SetTransformDirtyFlags(bool scaleDirty, bool rotateDirty)
{
	fScaleUserChanged = scaleDirty;
	fRotationUserChanged = rotateDirty;
}

void EmoryDuctworkPanel::UpdateScaleUI(double value, bool mixed)
{
	if (!fScaleEdit || !fScaleSlider) {
		return;
	}
	if (mixed) {
		SetWindowTextW(fScaleEdit, L"");
		return;
	}
	fUpdatingUI = true;
	const std::wstring text = FormatDecimal(value);
	SetWindowTextW(fScaleEdit, text.c_str());
	const int pos = static_cast<int>(std::round(value / fScaleBaseStep));
	SendMessage(fScaleSlider, TBM_SETPOS, TRUE, pos);
	fScaleLastPos = pos;
	fUpdatingUI = false;
}

void EmoryDuctworkPanel::UpdateRotationUI(double value, bool mixed)
{
	if (!fRotationEditTransform || !fRotationSlider) {
		return;
	}
	if (mixed) {
		SetWindowTextW(fRotationEditTransform, L"");
		return;
	}
	fUpdatingUI = true;
	const std::wstring text = FormatDecimal(value);
	SetWindowTextW(fRotationEditTransform, text.c_str());
	const int pos = static_cast<int>(std::round(value / fRotationBaseStep));
	SendMessage(fRotationSlider, TBM_SETPOS, TRUE, pos);
	fRotationLastPos = pos;
	fUpdatingUI = false;
}

double EmoryDuctworkPanel::GetScaleValue() const
{
	wchar_t buffer[64] = L"";
	if (!fScaleEdit || GetWindowTextW(fScaleEdit, buffer, 64) <= 0) {
		return fScaleValue;
	}
	return Clamp(_wtof(buffer), 10.0, 400.0);
}

double EmoryDuctworkPanel::GetRotationValue() const
{
	wchar_t buffer[64] = L"";
	if (!fRotationEditTransform || GetWindowTextW(fRotationEditTransform, buffer, 64) <= 0) {
		return fRotationValue;
	}
	return Clamp(_wtof(buffer), -180.0, 180.0);
}

void EmoryDuctworkPanel::SetScaleValue(double value)
{
	value = Clamp(value, 10.0, 400.0);
	fScaleValue = value;
	UpdateScaleUI(value, false);
}

void EmoryDuctworkPanel::SetRotationValue(double value)
{
	value = Clamp(value, -180.0, 180.0);
	fRotationValue = value;
	UpdateRotationUI(value, false);
}

void EmoryDuctworkPanel::SetStatusText(const wchar_t* text)
{
	if (fStatusText) {
		SetWindowTextW(fStatusText, text ? text : L"");
	}
}

void EmoryDuctworkPanel::UpdateRotationOverrideUI()
{
	if (!fRotationEdit) {
		return;
	}
	if (!fHasRotationOverride) {
		SetWindowTextW(fRotationEdit, L"");
		return;
	}
	std::wostringstream out;
	out << static_cast<int>(std::round(fRotationOverrideValue));
	SetWindowTextW(fRotationEdit, out.str().c_str());
}

void EmoryDuctworkPanel::ApplyRotationOverride()
{
	if (!fRotationEdit) {
		return;
	}
	wchar_t buffer[64] = L"";
	if (GetWindowTextW(fRotationEdit, buffer, 64) <= 0) {
		fHasRotationOverride = false;
		return;
	}
	double value = _wtof(buffer);
	if (!std::isfinite(value)) {
		fHasRotationOverride = false;
		return;
	}
	fHasRotationOverride = true;
	fRotationOverrideValue = NormalizeAngle(value);
	UpdateRotationOverrideUI();
}

void EmoryDuctworkPanel::HandleTrackbarScroll(HWND trackbar, bool isScale, int scrollCode)
{
	if (!trackbar) {
		return;
	}
	if (fUpdatingUI) {
		return;
	}
	const int pos = static_cast<int>(SendMessage(trackbar, TBM_GETPOS, 0, 0));
	const int delta = pos - (isScale ? fScaleLastPos : fRotationLastPos);
	DuctworkLog::Write(std::string("Panel Trackbar: ") + (isScale ? "scale" : "rotation") +
		" pos=" + std::to_string(pos) +
		" delta=" + std::to_string(delta) +
		" code=" + std::to_string(scrollCode));
	const bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	const double speed = shiftDown ? 1.0 : 0.25;
	if (isScale) {
		fScaleUserChanged = true;
		fRotationUserChanged = false;
		double value = fScaleValue + (static_cast<double>(delta) * fScaleBaseStep * speed);
		value = Clamp(value, 10.0, 400.0);
		fScaleValue = value;
		UpdateScaleUI(value, false);
		fScaleLastPos = static_cast<int>(std::round(value / fScaleBaseStep));
		SendMessage(trackbar, TBM_SETPOS, TRUE, fScaleLastPos);
		const bool live = fLiveCheck && SendMessage(fLiveCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
		if (live || scrollCode == TB_ENDTRACK || delta != 0) {
			ApplyTransformFromUI();
		}
	} else {
		fScaleUserChanged = false;
		fRotationUserChanged = true;
		double value = fRotationValue + (static_cast<double>(delta) * fRotationBaseStep * speed);
		value = Clamp(value, -180.0, 180.0);
		fRotationValue = value;
		UpdateRotationUI(value, false);
		fRotationLastPos = static_cast<int>(std::round(value / fRotationBaseStep));
		SendMessage(trackbar, TBM_SETPOS, TRUE, fRotationLastPos);
		const bool live = fLiveCheck && SendMessage(fLiveCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
		if (live || scrollCode == TB_ENDTRACK || delta != 0) {
			ApplyTransformFromUI();
		}
	}
}

double EmoryDuctworkPanel::NormalizeAngle(double angle) const
{
	angle = std::fmod(angle, 360.0);
	if (angle < -180.0) angle += 360.0;
	if (angle > 180.0) angle -= 360.0;
	if (angle > 90.0) angle -= 180.0;
	else if (angle < -90.0) angle += 180.0;
	return angle;
}

double EmoryDuctworkPanel::ComputeSelectionAngle() const
{
	double angle = 0.0;
	if (!TryComputeSelectionAngle(angle)) {
		return 0.0;
	}
	return angle;
}

bool EmoryDuctworkPanel::TryComputeSelectionAngle(double& outAngle) const
{
	outAngle = 0.0;
	AppContext appContext(fPluginRef);
	std::vector<AIArtHandle> selection;
	if (!CollectSelectedArt(selection) || selection.empty()) {
		if (!fCachedSelection.empty()) {
			selection = fCachedSelection;
			DuctworkLog::Write("Panel GetAngle: using cached selection size=" +
				std::to_string(static_cast<int>(selection.size())));
		} else {
			DuctworkLog::Write("Panel GetAngle: no selection");
			return false;
		}
	}

	std::vector<AIArtHandle> paths;
	for (size_t i = 0; i < selection.size(); ++i) {
		CollectLinePathsForAngle(selection[i], paths, true);
	}
	if (paths.empty()) {
		for (size_t i = 0; i < selection.size(); ++i) {
			CollectLinePathsForAngle(selection[i], paths, false);
		}
	}
	if (paths.empty()) {
		for (size_t i = 0; i < selection.size(); ++i) {
			CollectAllPathsForAngle(selection[i], paths, true);
		}
	}
	if (paths.empty()) {
		for (size_t i = 0; i < selection.size(); ++i) {
			CollectAllPathsForAngle(selection[i], paths, false);
		}
	}
	if (paths.empty()) {
		if (!CollectSelectedPaths(paths)) {
			DuctworkLog::Write("Panel GetAngle: no paths selected");
			return false;
		}
	}
	if (paths.empty()) {
		DuctworkLog::Write("Panel GetAngle: no paths resolved");
		return false;
	}

	DuctworkLog::Write("Panel GetAngle: paths=" + std::to_string(static_cast<int>(paths.size())));

	double longestLength = 0.0;
	double longestAngle = 0.0;
	for (size_t i = 0; i < paths.size(); ++i) {
		std::vector<DuctworkPoint> points;
		bool closed = false;
		if (!DuctworkGeometry::GetPathPoints(paths[i], points, closed) || points.size() < 2) {
			continue;
		}
		if (i < 3) {
			DuctworkLog::Write("Panel GetAngle: path[" + std::to_string(static_cast<int>(i)) +
				"] points=" + std::to_string(static_cast<int>(points.size())));
		}
		for (size_t j = 0; j + 1 < points.size(); ++j) {
			const DuctworkPoint& p0 = points[j];
			const DuctworkPoint& p1 = points[j + 1];
			const double dx = p1.x - p0.x;
			const double dy = p1.y - p0.y;
			const double length = std::sqrt(dx * dx + dy * dy);
			if (i < 1 && j < 3) {
				std::ostringstream seg;
				seg << "Panel GetAngle: seg[" << j << "] dx=" << dx
					<< " dy=" << dy << " len=" << length;
				DuctworkLog::Write(seg.str());
			}
			if (length > longestLength) {
				longestLength = length;
				const double angle = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;
				longestAngle = angle;
			}
		}
	}

	if (longestLength <= 0.0) {
		DuctworkLog::Write("Panel GetAngle: no valid segments");
		return false;
	}
	const double rounded = std::round(longestAngle * 10.0) / 10.0;
	const double normalized = NormalizeAngle(rounded);
	DuctworkLog::Write("Panel GetAngle: longestLen=" + std::to_string(longestLength) +
		" angle=" + std::to_string(longestAngle) +
		" rounded=" + std::to_string(rounded) +
		" normalized=" + std::to_string(normalized));
	outAngle = normalized;
	return true;
}
#endif

