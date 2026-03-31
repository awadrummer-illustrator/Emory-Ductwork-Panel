#ifndef __SELECTDUCTWORKPARTSSUITES_H__
#define __SELECTDUCTWORKPARTSSUITES_H__

#include "IllustratorSDK.h"
#include "Suites.hpp"

#include "AIArt.h"
#include "AIArtSet.h"
#include "AIMenu.h"
#include "AILayer.h"
#include "AIUser.h"
#include "AIDocument.h"

extern "C" SPBlocksSuite* sSPBlocks;
extern "C" AIMenuSuite* sAIMenu;
extern "C" AIUnicodeStringSuite* sAIUnicodeString;
extern "C" AIArtSuite* sAIArt;
extern "C" AIArtSetSuite* sAIArtSet;
extern "C" AILayerSuite* sAILayer;
extern "C" AIUserSuite* sAIUser;
extern "C" AIDocumentSuite* sAIDocument;

#endif // __SELECTDUCTWORKPARTSSUITES_H__
