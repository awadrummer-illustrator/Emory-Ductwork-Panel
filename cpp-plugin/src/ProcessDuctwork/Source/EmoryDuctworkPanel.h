#ifndef __EmoryDuctworkPanel_H__
#define __EmoryDuctworkPanel_H__

#include "IllustratorSDK.h"
#include "AIPanel.h"

#ifdef WIN_ENV
#include <windows.h>
#endif

#include <vector>
#include <string>

class EmoryDuctworkPanel
{
public:
	EmoryDuctworkPanel();
	~EmoryDuctworkPanel();

	ASErr Create(SPPluginRef pluginRef);
	void Destroy();

	void UpdateSelectionSummary();
	void SetRotationOverrideValue(double value, bool hasValue);
	bool GetRotationOverrideValue(double& outValue) const;
	bool ApplyTransformSelection(double targetScale, double targetRotation, bool allowCache, bool updateUI, bool livePreview, std::string* outMessage);
	void SetTransformDirtyFlags(bool scaleDirty, bool rotateDirty);
	bool TryComputeSelectionAngle(double& outAngle) const;
#ifdef WIN_ENV
	void ApplyQuickRotate(double angle);
	void ResetTransformToOriginal();
	void ResetRotation();
	void ResetScale();
	void ResetStrokes();
#endif

	AIPanelRef GetPanel() const { return fPanel; }

private:
#ifdef WIN_ENV
	static LRESULT CALLBACK PanelWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT HandlePanelMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void CreateControls(HWND parent);
	bool GetSelectionForAction(std::vector<AIArtHandle>& outSelection, bool allowCache);
	void ApplyTransformFromUI();
	void UpdateUIFromSummary();
	void UpdateScaleUI(double value, bool mixed);
	void UpdateRotationUI(double value, bool mixed);
	double GetScaleValue() const;
	double GetRotationValue() const;
	void SetScaleValue(double value);
	void SetRotationValue(double value);
	void SetStatusText(const wchar_t* text);
	void UpdateRotationOverrideUI();
	void ApplyRotationOverride();
	void HandleTrackbarScroll(HWND trackbar, bool isScale, int scrollCode);
	double NormalizeAngle(double angle) const;
	double ComputeSelectionAngle() const;

	HWND fPanelWindow = nullptr;
	WNDPROC fDefaultWndProc = nullptr;
	HWND fRotationEdit = nullptr;
	HWND fRotationGetBtn = nullptr;
	HWND fRotationClearBtn = nullptr;
	HWND fProcessPlacedApiBtn = nullptr;
	HWND fScaleSlider = nullptr;
	HWND fScaleEdit = nullptr;
	HWND fRotationSlider = nullptr;
	HWND fRotationEditTransform = nullptr;
	HWND fLiveCheck = nullptr;
	HWND fApplyBtn = nullptr;
	HWND fResetOriginalBtn = nullptr;
	HWND fResetStrokesBtn = nullptr;
	HWND fResetScaleBtn = nullptr;
	HWND fResetRotationBtn = nullptr;
	HWND fQuickRotate45 = nullptr;
	HWND fQuickRotate90 = nullptr;
	HWND fQuickRotate180 = nullptr;
	HWND fQuickRotateNeg45 = nullptr;
	HWND fQuickRotateNeg90 = nullptr;
	HWND fQuickRotateCustom = nullptr;
	HWND fStatusText = nullptr;
	HFONT fFont = nullptr;
	HBRUSH fPanelBrush = nullptr;
	HBRUSH fEditBrush = nullptr;
	HBRUSH fButtonBrush = nullptr;
	COLORREF fColorBg = RGB(5, 10, 20);
	COLORREF fColorText = RGB(224, 240, 255);
	COLORREF fColorButton = RGB(26, 58, 90);
	COLORREF fColorEdit = RGB(10, 20, 35);
	COLORREF fColorBorder = RGB(0, 170, 255);

	double fRotationOverrideValue = 0.0;
	bool fHasRotationOverride = false;
	double fScaleValue = 100.0;
	double fRotationValue = 0.0;
	bool fScaleUserChanged = false;
	bool fRotationUserChanged = false;
	double fScaleBaseStep = 0.1;
	double fRotationBaseStep = 0.1;
	int fScaleLastPos = 0;
	int fRotationLastPos = 0;
	std::vector<AIArtHandle> fCachedSelection;
	bool fUpdatingUI = false;
#endif

	AIPanelRef fPanel = nullptr;
	SPPluginRef fPluginRef = nullptr;
};

#endif // __EmoryDuctworkPanel_H__

