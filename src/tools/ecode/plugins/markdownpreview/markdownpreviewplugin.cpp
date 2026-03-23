#include "markdownpreviewplugin.hpp"
#include "../pluginmanager.hpp"
#include <eepp/system/log.hpp>
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
	// Kein async-Load nötig: kein Config-File, keine externe Ressource
	mReady = true;
	subscribeFileSystemListener();
	fireReadyCbs();
	setReady();
}

MarkdownPreviewPlugin::~MarkdownPreviewPlugin() {
	// PluginBase::~PluginBase räumt mEditors/Listener auf
}

} // namespace ecode
