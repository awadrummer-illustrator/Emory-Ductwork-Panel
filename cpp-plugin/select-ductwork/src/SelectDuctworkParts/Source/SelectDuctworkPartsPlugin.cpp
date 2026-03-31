#include "IllustratorSDK.h"
#include "SelectDuctworkPartsPlugin.h"
#include "AppContext.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <string>
#include <vector>

static const char* kDuctworkLayerNames[] = {
	"Thermostats",
	"Units",
	"Secondary Exhaust Registers",
	"Exhaust Registers",
	"Orange Register",
	"Rectangular Registers",
	"Square Registers",
	"Circular Registers"
};

static const size_t kDuctworkLayerCount = sizeof(kDuctworkLayerNames) / sizeof(kDuctworkLayerNames[0]);

static void LogMessage(const std::string& message)
{
	const char* temp = std::getenv("TEMP");
	if (!temp || temp[0] == '\0') {
		temp = std::getenv("TMP");
	}
	if (!temp || temp[0] == '\0') {
		return;
	}

	std::string path(temp);
	if (!path.empty()) {
		const char last = path[path.size() - 1];
		if (last != '\\' && last != '/') {
			path += "\\";
		}
	}
	path += "SelectDuctworkParts.log";

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "a") != 0 || !file) {
		return;
	}

	std::time_t now = std::time(nullptr);
	std::tm localTime{};
	localtime_s(&localTime, &now);
	char stamp[32] = { 0 };
	std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &localTime);
	std::fprintf(file, "[%s] %s\n", stamp, message.c_str());
	std::fclose(file);
}

static void LogError(const char* label, ASErr error)
{
	std::ostringstream stream;
	stream << label << " error=" << error;
	LogMessage(stream.str());
}

static bool IsDuctworkLayerName(const ai::UnicodeString& title)
{
	for (size_t i = 0; i < kDuctworkLayerCount; ++i) {
		if (title == ai::UnicodeString::FromUTF8(kDuctworkLayerNames[i])) {
			return true;
		}
	}
	return false;
}

static bool IsLayerChainEditableVisible(AILayerHandle layer)
{
	AILayerHandle current = layer;
	int guard = 0;
	while (current && guard++ < 256) {
		AIBoolean editable = false;
		AIBoolean visible = false;
		if (sAILayer->GetLayerEditable(current, &editable) || sAILayer->GetLayerVisible(current, &visible)) {
			return false;
		}
		if (!editable || !visible) {
			return false;
		}

		AILayerHandle parent = nullptr;
		if (sAILayer->GetLayerParent(current, &parent) || parent == nullptr || parent == current) {
			break;
		}
		current = parent;
	}
	return true;
}

static ASErr CollectDuctworkLayers(std::vector<AILayerHandle>& outLayers)
{
	if (!sAILayer) {
		LogMessage("CollectDuctworkLayers: sAILayer null");
		return kBadParameterErr;
	}

	for (size_t i = 0; i < kDuctworkLayerCount; ++i) {
		const char* name = kDuctworkLayerNames[i];
		ai::UnicodeString title = ai::UnicodeString::FromUTF8(name);
		AILayerHandle layer = nullptr;
		LogMessage(std::string("GetLayerByTitle: ") + name);
		ASErr error = sAILayer->GetLayerByTitle(&layer, title);
		if (error) {
			LogError("GetLayerByTitle failed", error);
			continue;
		}
		if (layer) {
			outLayers.push_back(layer);
			LogMessage(std::string("Found layer: ") + name);
		}
	}

	return kNoErr;
}

static ASErr DeselectAllArt()
{
	if (!sAIArtSet || !sAIArt) {
		return kNoErr;
	}

	AIArtSet selectedSet = nullptr;
	ASErr error = sAIArtSet->NewArtSet(&selectedSet);
	if (error) {
		return error;
	}

	error = sAIArtSet->SelectedArtSet(selectedSet);
	if (!error) {
		size_t count = 0;
		if (!sAIArtSet->CountArtSet(selectedSet, &count)) {
			for (size_t i = 0; i < count; ++i) {
				AIArtHandle art = nullptr;
				if (sAIArtSet->IndexArtSet(selectedSet, i, &art) || !art) {
					continue;
				}
				sAIArt->SetArtUserAttr(art, kArtSelected, 0);
			}
		}
	}

	sAIArtSet->DisposeArtSet(&selectedSet);
	return error;
}

Plugin* AllocatePlugin(SPPluginRef pluginRef)
{
	LogMessage("AllocatePlugin");
	return new SelectDuctworkPartsPlugin(pluginRef);
}

void FixupReload(Plugin* plugin)
{
	SelectDuctworkPartsPlugin::FixupVTable((SelectDuctworkPartsPlugin*)plugin);
}

SelectDuctworkPartsPlugin::SelectDuctworkPartsPlugin(SPPluginRef pluginRef)
	: Plugin(pluginRef),
	fSelectMenuItem(NULL),
	fAboutPluginMenu(NULL)
{
	strncpy(fPluginName, kSelectDuctworkPartsPluginName, kMaxStringLength);
}

ASErr SelectDuctworkPartsPlugin::StartupPlugin(SPInterfaceMessage* message)
{
	LogMessage("StartupPlugin begin");
	ASErr error = Plugin::StartupPlugin(message);
	if (!error) {
		error = AddMenus(message);
	}
	LogMessage("StartupPlugin end");
	return error;
}

ASErr SelectDuctworkPartsPlugin::Message(char* caller, char* selector, void* message)
{
	ASErr error = kNoErr;

	try {
		if (strcmp(caller, kCallerAIScriptMessage) == 0) {
			AIScriptMessage* msg = (AIScriptMessage*)message;
			if (strcmp(selector, kSelectDuctworkPartsScriptSelector) == 0) {
				ai::UnicodeString outMsg;
				error = SelectDuctworkParts(false, &outMsg);
				msg->outParam = outMsg;
				return error;
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

ASErr SelectDuctworkPartsPlugin::AddMenus(SPInterfaceMessage* message)
{
	ASErr error = kNoErr;

	SDKAboutPluginsHelper aboutPluginsHelper;
	aboutPluginsHelper.AddAboutPluginsMenuItem(
		message,
		kSDKDefAboutSDKCompanyPluginsGroupName,
		ai::UnicodeString(kSDKDefAboutSDKCompanyPluginsGroupNameString),
		"Select Ductwork Parts...",
		&fAboutPluginMenu);

	AIPlatformAddMenuItemDataUS menuData;
	menuData.groupName = kSelectMenuGroup;
	menuData.itemText = ai::UnicodeString::FromUTF8(kSelectDuctworkPartsMenuItem);
	error = sAIMenu->AddMenuItem(message->d.self, kSelectDuctworkPartsPluginName, &menuData, 0, &fSelectMenuItem);
	if (!error && fSelectMenuItem) {
		sAIMenu->UpdateMenuItemAutomatically(fSelectMenuItem,
			kAutoEnableMenuItemAction,
			0, 0,
			0, 0,
			kIfOpenDocument,
			0);
	}

	return error;
}

ASErr SelectDuctworkPartsPlugin::GoMenuItem(AIMenuMessage* message)
{
	if (message->menuItem == fAboutPluginMenu) {
		LogMessage("GoMenuItem about");
		SDKAboutPluginsHelper aboutPluginsHelper;
		aboutPluginsHelper.PopAboutBox(message, "About Select Ductwork Parts", kSDKDefAboutSDKCompanyPluginsAlertString);
		return kNoErr;
	}

	if (message->menuItem == fSelectMenuItem) {
		LogMessage("GoMenuItem select");
		ai::UnicodeString outMsg;
		return SelectDuctworkParts(true, &outMsg);
	}

	return kNoErr;
}

bool SelectDuctworkPartsPlugin::CanSelectArt(AIArtHandle art) const
{
	if (!art) {
		return false;
	}

	ai::int32 attr = 0;
	ASErr error = sAIArt->GetArtUserAttr(art, kArtLocked | kArtHidden, &attr);
	if (error) {
		return false;
	}

	return (attr & (kArtLocked | kArtHidden)) == 0;
}

bool SelectDuctworkPartsPlugin::IsDuctworkLayerOrParent(AILayerHandle layer) const
{
	AILayerHandle current = layer;
	int guard = 0;
	while (current && guard++ < 256) {
		ai::UnicodeString title;
		if (!sAILayer->GetLayerTitle(current, title)) {
			for (size_t i = 0; i < kDuctworkLayerCount; ++i) {
				if (title == ai::UnicodeString::FromUTF8(kDuctworkLayerNames[i])) {
					return true;
				}
			}
		}

		AILayerHandle parent = NULL;
		if (sAILayer->GetLayerParent(current, &parent) || parent == NULL || parent == current) {
			break;
		}
		current = parent;
	}

	return false;
}

ASErr SelectDuctworkPartsPlugin::SelectDuctworkParts(ASBoolean showAlert, ai::UnicodeString* outParam)
{
	ASErr error = kNoErr;
	AIDocumentHandle document = NULL;
	ai::UnicodeString message;

	LogMessage("SelectDuctworkParts start");

	AppContext appContext(GetPluginRef());
	LogMessage("AppContext set");

	LogMessage("GetDocument");
	error = sAIDocument->GetDocument(&document);
	if (error || !document) {
		message = ai::UnicodeString::FromUTF8("No document open.");
		LogMessage("No document open");
		if (outParam) {
			*outParam = message;
		}
		if (showAlert) {
			sAIUser->MessageAlert(message);
		}
		return kNoErr;
	}
	LogMessage("GetDocument ok");

	if (!sAIArtSet) {
		message = ai::UnicodeString::FromUTF8("Art set suite not available.");
		LogMessage("Art set suite not available");
		if (outParam) {
			*outParam = message;
		}
		if (showAlert) {
			sAIUser->MessageAlert(message);
		}
		return kNoErr;
	}
	LogMessage("Art set suite ok");
	if (!sAILayer) {
		message = ai::UnicodeString::FromUTF8("Layer suite not available.");
		LogMessage("Layer suite not available");
		if (outParam) {
			*outParam = message;
		}
		if (showAlert) {
			sAIUser->MessageAlert(message);
		}
		return kNoErr;
	}
	LogMessage("Layer suite ok");

	AIBoolean textFocus = false;
	LogMessage("HasTextFocus");
	if (!sAIDocument->HasTextFocus(&textFocus) && textFocus) {
		sAIDocument->LoseTextFocus();
	}
	LogMessage("HasTextFocus ok");

	LogMessage("DeselectAllArt");
	(void)DeselectAllArt();
	LogMessage("DeselectAllArt ok");

	std::vector<AILayerHandle> ductworkLayers;
	LogMessage("CollectDuctworkLayers");
	error = CollectDuctworkLayers(ductworkLayers);
	if (error) {
		message = ai::UnicodeString::FromUTF8("Failed to collect layers.");
		LogError("CollectDuctworkLayers failed", error);
		if (outParam) {
			*outParam = message;
		}
		if (showAlert) {
			sAIUser->MessageAlert(message);
		}
		return error;
	}
	LogMessage("CollectDuctworkLayers ok");

	if (ductworkLayers.empty()) {
		message = ai::UnicodeString::FromUTF8("No ductwork layers found.");
		LogMessage("No ductwork layers found");
		if (outParam) {
			*outParam = message;
		}
		if (showAlert) {
			sAIUser->MessageAlert(message);
		}
		return kNoErr;
	}

	AIArtSet layerSet = nullptr;
	ai::int32 selected = 0;

	{
		std::ostringstream stream;
		stream << "Collected ductwork layers: " << ductworkLayers.size();
		LogMessage(stream.str());
	}

	for (size_t layerIndex = 0; layerIndex < ductworkLayers.size(); ++layerIndex) {
		AILayerHandle layer = ductworkLayers[layerIndex];
		if (!layer || !IsLayerChainEditableVisible(layer)) {
			continue;
		}

		error = sAIArtSet->NewArtSet(&layerSet);
		if (error) {
			LogError("NewArtSet failed", error);
			continue;
		}

		error = sAIArtSet->LayerArtSet(layer, layerSet);
		if (!error) {
			size_t count = 0;
			if (!sAIArtSet->CountArtSet(layerSet, &count)) {
				std::ostringstream stream;
				stream << "Layer " << layerIndex << " art count " << count;
				LogMessage(stream.str());
				for (size_t i = 0; i < count; ++i) {
					AIArtHandle art = nullptr;
					if (sAIArtSet->IndexArtSet(layerSet, i, &art) || !art) {
						continue;
					}

					short type = kUnknownArt;
					if (sAIArt->GetArtType(art, &type) || type != kPlacedArt) {
						continue;
					}

					if (!CanSelectArt(art)) {
						continue;
					}

					if (!sAIArt->SetArtUserAttr(art, kArtSelected, kArtSelected)) {
						selected++;
					}
				}
			} else {
				LogMessage("CountArtSet failed");
			}
		} else {
			LogError("LayerArtSet failed", error);
		}

		sAIArtSet->DisposeArtSet(&layerSet);
		layerSet = nullptr;
	}

	std::ostringstream stream;
	stream << "Selected " << selected << " placed item(s).";
	message = ai::UnicodeString::FromUTF8(stream.str());
	LogMessage(stream.str());
	if (outParam) {
		*outParam = message;
	}

	return kNoErr;
}
