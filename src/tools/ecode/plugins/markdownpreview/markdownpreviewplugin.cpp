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

void MarkdownPreviewPlugin::updatePreview( UICodeEditor* editor ) {
	if ( !editorExists( editor ) )
		return;

	auto* splitter = getManager()->getSplitter();
	if ( !splitter )
		return;

	Lock l( mMutex );
	auto it = mPreviews.find( editor );
	UIMarkdownView* view =
		( it != mPreviews.end() ) ? it->second : nullptr;

	// Pointer validieren: Tab könnte inzwischen geschlossen sein
	if ( view && !splitter->ownedWidgetExists( view ) ) {
		mPreviews.erase( it );
		view = nullptr;
	}

	if ( !view ) {
		view = UIMarkdownView::New();
		mPreviews[editor] = view;
		auto [tab, w] = splitter->createWidget(
			view, i18n( "markdown_preview", "Markdown Preview" ) );
		(void)tab; (void)w;
	}

	const auto text = editor->getDocument().getText().toUtf8();
	view->loadFromString( text );
}

} // namespace ecode
