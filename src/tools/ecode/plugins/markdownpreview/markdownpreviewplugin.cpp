#include "markdownpreviewplugin.hpp"
#include "../pluginmanager.hpp"
#include <eepp/system/log.hpp>
#include <eepp/system/clock.hpp>
#include <eepp/ui/uipopupmenu.hpp>
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
	std::string path = editor->getDocument().getFilePath();
	if ( path.empty() )
		return false;
	String::toLowerInPlace( path );
	return String::endsWith( path, ".md" ) || String::endsWith( path, ".markdown" );
}

void MarkdownPreviewPlugin::onRegisterListeners( UICodeEditor* editor,
												  std::vector<Uint32>& listeners ) {
	// Immer TextChanged handler registrieren (liefert aktuellsten Dokumentstatus).
	listeners.push_back(
		editor->on( Event::OnTextChanged, [this, editor]( const Event* ) {
			if ( isMarkdownFile( editor ) ) {
				editor->debounce( [this, editor] { updatePreview( editor ); },
					  Milliseconds( 500 ), DebounceId );
			}
		} ) );

	if ( isMarkdownFile( editor ) ) {
		editor->getDocument().setCommand(
			"open-markdown-preview",
			[this, editor]( TextDocument::Client* ) { updatePreview( editor ); } );
	}
}

void MarkdownPreviewPlugin::onUnregisterEditor( UICodeEditor* editor ) {
	editor->removeActionsByTag( DebounceId );

	// Remove custom command to avoid dangling callbacks after the plugin is disabled/reloaded
	editor->getDocument().removeCommand( "open-markdown-preview" );

	// Keep preview alive even if the editor/tab is closed, to match plugin requirement
	// that preview can exist independently from the editor tab.
}

void MarkdownPreviewPlugin::onDocumentChanged( UICodeEditor* editor, TextDocument* oldDoc ) {
	if ( oldDoc )
		oldDoc->removeCommand( "open-markdown-preview" );

	if ( isMarkdownFile( editor ) ) {
		editor->getDocument().setCommand(
			"open-markdown-preview",
			[this, editor]( TextDocument::Client* ) { updatePreview( editor ); } );
	}
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
		view->on( Event::OnClose, [this, editor, view]( const Event* ) {
			Lock l( mMutex );
		
auto it = mPreviews.find( editor );
			if ( it != mPreviews.end() && it->second == view ) {
				mPreviews.erase( it );
			}
		} );
		auto [tab, w] = splitter->createWidget(
			view, i18n( "markdown_preview", "Markdown Preview" ) );
		(void)tab; (void)w;
	}

	const auto text = editor->getDocument().getText().toUtf8();
	view->loadFromString( text );
}

bool MarkdownPreviewPlugin::onCreateContextMenu( UICodeEditor* editor,
												  UIPopUpMenu* menu,
												  const Vector2i& /*position*/,
												  const Uint32& /*flags*/ ) {
	if ( !isMarkdownFile( editor ) )
		return false;

	// In case Document changed into markdown and commands not registered,
	// ensure command exists for current doc.
	editor->getDocument().setCommand(
		"open-markdown-preview",
		[this, editor]( TextDocument::Client* ) { updatePreview( editor ); } );

	menu->addSeparator();
	menu->add( i18n( "open_markdown_preview", "Open Markdown Preview" ),
			   nullptr, "open-markdown-preview" )
		->setId( "open-markdown-preview" );
	return false; // false = menu stays open
}

} // namespace ecode

