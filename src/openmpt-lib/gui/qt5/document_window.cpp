#include "stdafx.h"

#include "app_config.hpp"
#include "document_window.hpp"
#include "comment_editor.hpp"
#include "pattern_editor.hpp"
#include "sample_editor.hpp"
#include "graph_editor.hpp"
#include "pattern_editor_aux.hpp"
#include "song_overview.hpp"
#include "../MainFrm.h"

namespace modplug {
namespace gui {
namespace qt5 {

document_window::document_window(
    module_renderer *renderer,
    app_config &config,
    QWidget *parent
) : QWidget(parent)
  , global_config(config)
{
    QIcon window_icon(":/openmpt/icons/nobu-icons/mpt_document.svg");
    setWindowIcon(window_icon);
    setWindowTitle("docuhoot");
    auto layout = new QHBoxLayout;
    setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setMargin(0);

    layout->addWidget(&tab_bar);

    auto commentsplitter = new QSplitter;
    commentsplitter->setOrientation(Qt::Vertical);

    comments = new comment_editor(*renderer);
    comments->legacy_set_comments_from_module(true);

    overview = new song_overview(*renderer);

    commentsplitter->addWidget(comments);
    commentsplitter->addWidget(overview);

    editor = new pattern_editor_tab(*renderer, config);

    graph = new graph_editor(&renderer->mixgraph);

    smpedit = new sample_editor();

    tab_bar.addTab(editor, "Patterns");
    tab_bar.addTab(smpedit, "Samples");
    tab_bar.addTab(new QWidget(), "Instruments");
    tab_bar.addTab(graph, "Graph");
    tab_bar.addTab(commentsplitter, "Comments");

    connect(
        &global_config, &app_config::colors_changed,
        this, &document_window::config_colors_changed
    );
}

void document_window::test_notification(MPTNOTIFICATION * pnotify) {
    /*
    DEBUG_FUNC(
        "latency = %d; pos = %d; order = %d, row = %d",
        pnotify->dwLatency,
        pnotify->dwPos,
        pnotify->nOrder,
        pnotify->nRow
    );
    */
    const player_position_t position(
        pnotify->nOrder,
        pnotify->nPattern,
        pnotify->nRow
    );
    editor->editor.update_playback_position(position);
}

void document_window::config_colors_changed() {
    editor->editor.update_colors(global_config.colors());
}


}
}
}
