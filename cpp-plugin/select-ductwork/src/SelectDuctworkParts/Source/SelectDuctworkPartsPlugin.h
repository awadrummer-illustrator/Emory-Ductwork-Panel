#ifndef __SELECTDUCTWORKPARTSPLUGIN_H__
#define __SELECTDUCTWORKPARTSPLUGIN_H__

#include "SelectDuctworkPartsSuites.h"
#include "SelectDuctworkPartsID.h"
#include "Plugin.hpp"
#include "AIMenuGroups.h"
#include "SDKAboutPluginsHelper.h"
#include "SDKDef.h"
#include "AIScriptMessage.h"

Plugin* AllocatePlugin(SPPluginRef pluginRef);
void FixupReload(Plugin* plugin);

class SelectDuctworkPartsPlugin : public Plugin
{
public:
	SelectDuctworkPartsPlugin(SPPluginRef pluginRef);
	virtual ~SelectDuctworkPartsPlugin() {}

	FIXUP_VTABLE_EX(SelectDuctworkPartsPlugin, Plugin);

protected:
	virtual ASErr Message(char* caller, char* selector, void* message);
	virtual ASErr StartupPlugin(SPInterfaceMessage* message);
	virtual ASErr GoMenuItem(AIMenuMessage* message);

private:
	AIMenuItemHandle fSelectMenuItem;
	AIMenuItemHandle fAboutPluginMenu;

	ASErr AddMenus(SPInterfaceMessage* message);
	ASErr SelectDuctworkParts(ASBoolean showAlert, ai::UnicodeString* outParam);
	bool IsDuctworkLayerOrParent(AILayerHandle layer) const;
	bool CanSelectArt(AIArtHandle art) const;
};

#endif // __SELECTDUCTWORKPARTSPLUGIN_H__
