#ifndef ECODE_MARKDOWNPREVIEWPLUGIN_HPP
#define ECODE_MARKDOWNPREVIEWPLUGIN_HPP

#include "../plugin.hpp"
#include "../pluginmanager.hpp"
#include <eepp/config.hpp>
#include <eepp/ui/uicodeeditor.hpp>
#include <eepp/ui/uimarkdownview.hpp>
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

	std::string getId() { return Definition().id; }
	std::string getTitle() { return Definition().name; }
	std::string getDescription() { return Definition().description; }

	void onRegisterListeners( UICodeEditor* editor, std::vector<Uint32>& listeners );
	void onUnregisterEditor( UICodeEditor* editor );
	bool onCreateContextMenu( UICodeEditor* editor, UIPopUpMenu* menu,
							  const Vector2i& position, const Uint32& flags );

  private:
	void updatePreview( UICodeEditor* editor );
	bool isMarkdownFile( UICodeEditor* editor );

	UnorderedMap<UICodeEditor*, UIMarkdownView*> mPreviews;

	MarkdownPreviewPlugin( PluginManager* pluginManager, bool sync );
};

} // namespace ecode

#endif // ECODE_MARKDOWNPREVIEWPLUGIN_HPP
