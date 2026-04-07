#ifndef __ProcessDuctworkPLUGIN_H__
#define __ProcessDuctworkPLUGIN_H__

#include "ProcessDuctworkSuites.h"
#include "ProcessDuctworkID.h"
#include "Plugin.hpp"
#include "AIMenuGroups.h"
#include "SDKAboutPluginsHelper.h"
#include "SDKDef.h"
#include "AIScriptMessage.h"
#include "ProcessDuctworkOptions.h"
#include "EmoryDuctworkPanel.h"

Plugin* AllocatePlugin(SPPluginRef pluginRef);
void FixupReload(Plugin* plugin);

class ProcessDuctworkPlugin : public Plugin
{
public:
	ProcessDuctworkPlugin(SPPluginRef pluginRef);
	virtual ~ProcessDuctworkPlugin() {}
	static ProcessDuctworkPlugin* GetInstance();
	ASErr RunProcessPlacedApiFromPanel();

	FIXUP_VTABLE_EX(ProcessDuctworkPlugin, Plugin);

protected:
	virtual ASErr Message(char* caller, char* selector, void* message);
	virtual ASErr StartupPlugin(SPInterfaceMessage* message);
	virtual ASErr PostStartupPlugin();
	virtual ASErr ShutdownPlugin(SPInterfaceMessage* message);
	virtual ASErr GoMenuItem(AIMenuMessage* message);
	virtual ASErr Notify(AINotifierMessage* message);
	virtual ASErr TrackToolCursor(AIToolMessage* message);
	virtual ASErr ToolMouseDown(AIToolMessage* message);
	ASErr DrawGapPreview(AIAnnotatorMessage* message);
	ASErr InvalidateGapPreview(AIAnnotatorMessage* message);
	void UpdateGapPreview(const AIRealPoint& cursor, const std::string& layerHint);
	void ClearGapPreview();

private:
	AIMenuItemHandle fProcessMenuItem;
	AIMenuItemHandle fProcessNoCompoundMenuItem;
	AIMenuItemHandle fProcessNoStylesMenuItem;
	AIMenuItemHandle fProcessNoPartsMenuItem;
	AIMenuItemHandle fProcessNoGraphicsMenuItem;
	AIMenuItemHandle fProcessNoMetaMenuItem;
	AIMenuItemHandle fProcessDirectPlaceMenuItem;
	AIMenuItemHandle fProcessPlacedApiMenuItem;
	AIMenuItemHandle fSelectPartsMenuItem;
	AIMenuItemHandle fDuctworkMenuItem;
	AIMenuItemHandle fPanelMenuItem;
	AIMenuItemHandle fAboutPluginMenu;
	ProcessDuctworkOptions fLastOptions;
	AINotifierHandle fSelectionChangedNotifier;
	EmoryDuctworkPanel fPanel;
	AIToolHandle fGapToggleTool;
	AIToolHandle fGapHealTool;
	AIResourceManagerHandle fCursorResourceManager;
	AIAnnotatorHandle fGapPreviewAnnotator;
	AIDocumentViewHandle fGapPreviewView;
	bool fGapPreviewVisible;
	bool fGapPreviewShowHeal;
	bool fGapPreviewShowGap;
	bool fGapPreviewIsRegister;
	AIPoint fGapPreviewStart;
	AIPoint fGapPreviewEnd;
	AIRect fGapPreviewBounds;
	AIArtHandle fGapPreviewHoverArt;
	AIArtHandle fGapPreviewLastHoverArt;

	ASErr AddMenus(SPInterfaceMessage* message);
	ASErr AddTools(SPInterfaceMessage* message);
	ASErr ProcessDuctwork(const ProcessDuctworkOptions& options, ASBoolean showAlert, ai::UnicodeString* outParam);
	ASErr SelectDuctworkParts(AIDocumentHandle document);

	// Direct C++ → CEP panel communication (bypasses ExtendScript)
	void SendEventToPanel(const char* eventId, const char* jsonData);
};

#endif // __ProcessDuctworkPLUGIN_H__


