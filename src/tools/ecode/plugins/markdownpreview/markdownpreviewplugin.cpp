#include "markdownpreviewplugin.hpp"
#include "../pluginmanager.hpp"
#include <eepp/system/log.hpp>
#include <eepp/system/clock.hpp>
using namespace EE::System;

namespace ecode {

static const String::HashType DebounceId =
	String::hash( "MarkdownPreviewPlugin::update" );

Plugin* MarkdownPreviewPlugin::New( PluginManager* pluginManager ) {
	return eeNew( MarkdownPreviewPlugin, ( pluginManager, false ) );
}

Plugin* MarkdownPreviewPlugin::NewSync( PluginManager* pluginManager ) {
	return eeNew( MarkdownPreviewPlugin, ( pluginManager, true ) );
}

MarkdownPreviewPlugin::MarkdownPreviewPlugin( PluginManager* pluginManager, bool sync )
	: PluginBase( pluginManager ) {
	Clock clock;
	// Kein Config-Loading nötig — synchrone Initialisierung
	mReady = true;
	subscribeFileSystemListener();
	fireReadyCbs();
	setReady( clock.getElapsedTime() );
}

MarkdownPreviewPlugin::~MarkdownPreviewPlugin() {
	Lock l( mMutex );
	mPreviews.clear();
	// PluginBase cleanup of editors/listeners follows
}

} // namespace ecode
