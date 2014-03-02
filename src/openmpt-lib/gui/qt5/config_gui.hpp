#pragma once
#include <QtGui>
#include <memory>

#include "config_dialog.hpp"
#include "keymap.hpp"
#include "../../pervasives/pervasives.hpp"
#include "colors.hpp"

class module_renderer;

namespace modplug {
namespace gui {
namespace qt5 {

class pattern_editor;
class app_config;

class config_gui_main : public config_page
{
    Q_OBJECT
public:
    config_gui_main(app_config &);

    virtual void refresh();
    virtual void apply_changes();
    void set_colors(const colors_t &preset);

public slots:
    void preset_clicked();

private:

    app_config &context;
    colors_t colors;

    QPushButton preset_it;
    QPushButton preset_xm;
    QPushButton preset_mpt;
    QPushButton preset_buzz;

    keymap_t emptymap;

    pattern_editor *demo;
    std::unique_ptr<module_renderer> demo_dummy;
};

}
}
}
