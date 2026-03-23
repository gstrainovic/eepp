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

bool MarkdownPreviewPlugin::isMarkdownFile( UICodeEditor* editor ) {
	const auto& path = editor->getDocument().getFilePath();
	return String::endsWith( path, ".md" ) ||
		   String::endsWith( path, ".markdown" );
}

void MarkdownPreviewPlugin::onRegisterListeners( UICodeEditor* editor,
												  std::vector<Uint32>& listeners ) {
	if ( !isMarkdownFile( editor ) )
		return;

	listeners.push_back(
		editor->on( Event::OnTextChanged, [this, editor]( const Event* ) {
			editor->debounce( [this, editor] { updatePreview( editor ); },
							  Milliseconds( 500 ), DebounceId );
		} ) );

	// Befehl registrieren
	editor->getDocument().setCommand(
		"open-markdown-preview",
		[this, editor]( TextDocument::Client* ) { updatePreview( editor ); } );
}

void MarkdownPreviewPlugin::onUnregisterEditor( UICodeEditor* editor ) {
	editor->removeActionsByTag( DebounceId );
	Lock l( mMutex );
	mPreviews.erase( editor );
}

} // namespace ecode
