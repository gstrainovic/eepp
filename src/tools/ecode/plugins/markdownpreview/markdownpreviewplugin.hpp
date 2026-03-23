#ifndef ECODE_MARKDOWNPREVIEWPLUGIN_HPP
#define ECODE_MARKDOWNPREVIEWPLUGIN_HPP

#include "../plugin.hpp"
#include "../pluginmanager.hpp"
#include <eepp/config.hpp>
#include <eepp/ui/uicodeeditor.hpp>
#include <eepp/ui/uimarkdownview.hpp>
#include <eepp/ui/uiscrollview.hpp>
using namespace EE;
using namespace EE::UI;

namespace ecode {

class MarkdownPreviewPlugin : public PluginBase {
  public:
	static PluginDefinition Definition() {
		return { "markdownpreview",
				 "Markdown Preview",
				 "Live Markdown preview in a split tab.",
				 MarkdownPreviewPlugin::New,
				 { 0, 1, 0 },
				 MarkdownPreviewPlugin::NewSync };
	}

	static Plugin* New( PluginManager* pluginManager );
	static Plugin* NewSync( PluginManager* pluginManager );

	virtual ~MarkdownPreviewPlugin();

	std::string getId() override { return Definition().id; }
	std::string getTitle() override { return Definition().name; }
	std::string getDescription() override { return Definition().description; }

	virtual void onRegisterListeners( UICodeEditor* editor, std::vector<Uint32>& listeners ) override;
	virtual void onUnregisterEditor( UICodeEditor* editor ) override;
	virtual void onDocumentChanged( UICodeEditor* editor, TextDocument* oldDoc ) override;
	virtual bool onCreateContextMenu( UICodeEditor* editor, UIPopUpMenu* menu,
					  const Vector2i& position, const Uint32& flags ) override;

  private:
	void updatePreview( UICodeEditor* editor );
	bool isMarkdownFile( UICodeEditor* editor );

	UnorderedMap<UICodeEditor*, UIScrollView*> mPreviews;

	MarkdownPreviewPlugin( PluginManager* pluginManager, bool sync );
};

} // namespace ecode

#endif // ECODE_MARKDOWNPREVIEWPLUGIN_HPP
