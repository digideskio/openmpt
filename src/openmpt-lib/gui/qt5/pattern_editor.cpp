#include "stdafx.h"

#include "../../Resource.h"
#include "../../pervasives/pervasives.hpp"

#include "pattern_editor.hpp"
#include "pattern_editor_column.hpp"
#include "util.hpp"

#include <GL/gl.h>
#include <GL/glu.h>

#include "keymap.hpp"

using namespace modplug::pervasives;
using namespace modplug::tracker;

namespace modplug {
namespace gui {
namespace qt5 {

pattern_editor_draw::pattern_editor_draw(
    module_renderer &renderer,
    const colors_t &colors,
    pattern_editor &parent
) : renderer(renderer)
  , font_metrics(small_pattern_font)
  //, font_metrics(medium_pattern_font)
  , is_dragging(false)
  , first_column(0)
  , parent(parent)
{
    setFocusPolicy(Qt::ClickFocus);

    font_bitmap = QImage(":/openmpt/bitmapfonts/pattern_editor.png")
                 .convertToFormat(QImage::Format_MonoLSB);
    font_bitmap.setColor(0, qRgba(0xff, 0xff, 0xff, 0x00));
    font_bitmap.setColor(1, qRgba(0xff, 0xff, 0xff, 0xff));
}

void pattern_editor_draw::initializeGL() {
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY_EXT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    font_texture = bindTexture(font_bitmap);

    resizeGL(100, 100);

    glBindTexture(GL_TEXTURE_2D, font_texture);
}

void pattern_editor_draw::resizeGL(int width, int height) {
    DEBUG_FUNC("width: %d, height: %d", width, height);
    ghettotimer homesled(__FUNCTION__);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, width, height, 0);
    glViewport(0, 0, width, height);
    glMatrixMode(GL_MODELVIEW);

    clipping_rect = QRect(0, 0, width, height);

    this->width = width;
    this->height = height;
}

void pattern_editor_draw::paintGL() {
    ghettotimer homesled(__FUNCTION__);
    chnindex_t channel_count = renderer.GetNumChannels();

    draw_state state = {
        renderer,

        renderer.GetNumPatterns(),
        clipping_rect,

        playback_pos,
        active_pos,

        colors,
        corners,
        selection.end
    };

    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
    //         GL_ACCUM_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    int left = 0;

    for (chnindex_t idx = first_column;
         idx < channel_count && left < width;
         ++idx)
    {
        note_column notehomie(left, idx, font_metrics);
        notehomie.draw_column(state);
        left += notehomie.width();
    }
}

bool pattern_editor_draw::position_from_point(const QPoint &point,
                                              editor_position_t &pos)
{
    const chnindex_t channel_count = renderer.GetNumChannels();
    int left = 0;
    bool success = false;

    for (chnindex_t idx = first_column; idx < channel_count; ++idx) {
        note_column notehomie(left, idx, font_metrics);
        success = notehomie.position_from_point(point, pos);
        if (success) {
            break;
        }
        left += notehomie.width();
    }

    return success;
}

void pattern_editor_draw::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton) {
        is_dragging = true;
        parent.set_selection_start(mapToParent(event->pos()));
        parent.set_selection_end(mapToParent(event->pos()));
        updateGL();
    }
}

void pattern_editor_draw::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton && is_dragging) {
        parent.set_selection_end(mapToParent(event->pos()));
        updateGL();
    }
}

void pattern_editor_draw::mouseReleaseEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton) {
        is_dragging = false;
        parent.set_selection_end(mapToParent(event->pos()));
    }
}


pattern_editor_row_header::pattern_editor_row_header(pattern_editor &parent)
    : parent(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
}

QSize pattern_editor_row_header::sizeHint() const {
    QStyleOptionHeader opt;
    opt.fontMetrics = QFontMetrics(font());
    opt.text = "1023";
    opt.section = 1;

    QSize ret = style()->sizeFromContents(
        QStyle::CT_HeaderSection, &opt, QSize(), this);
    DEBUG_FUNC("ret width = %d, height = %d", ret.width(), ret.height());

    return ret;
}

void pattern_editor_row_header::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    QRect current = this->rect();
    int top = current.top();
    current.setHeight(parent.draw.font_metrics.height);

    QStyleOptionHeader opt;
    opt.section = 1;
    opt.textAlignment = Qt::AlignHCenter | Qt::AlignVCenter;
    opt.state = QStyle::State_Enabled;

    for (rowindex_t row = 0; row < parent.active_pattern()->GetNumRows(); ++row) {
        current.moveTop(top);
        opt.text = QString::number(row);
        opt.rect = current;
        style()->drawControl(QStyle::CE_HeaderSection, &opt, &painter, this);
        style()->drawControl(QStyle::CE_HeaderLabel, &opt, &painter, this);
        top += parent.draw.font_metrics.height;
    }
}

pattern_editor_column_header::pattern_editor_column_header(
    pattern_editor &parent
) : parent(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
}

QSize pattern_editor_column_header::sizeHint() const {
    QStyleOptionHeader opt;
    opt.fontMetrics = QFontMetrics(font());
    opt.text = "X";
    opt.section = 1;

    QSize ret = style()->sizeFromContents(
        QStyle::CT_HeaderSection, &opt, QSize(), this);
    DEBUG_FUNC("ret width = %d, height = %d", ret.width(), ret.height());

    return ret;
}

void pattern_editor_column_header::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    QRect current = this->rect();
    int left = current.left();
    current.setWidth(parent.draw.font_metrics.width);

    QStyleOptionHeader opt;
    opt.section = 1;
    opt.textAlignment = Qt::AlignHCenter | Qt::AlignVCenter;
    opt.state = QStyle::State_Enabled;

    for (chnindex_t column = parent.draw.first_column;
         column < parent.active_pattern()->GetNumChannels();
         ++column)
    {
        current.moveLeft(left);
        opt.rect = current;
        const char *fmt = parent.draw.renderer.m_bChannelMuteTogglePending[column]
            ? "[Channel %1]"
            : "Channel %1";
        opt.text = QString(fmt).arg(column + 1);
        style()->drawControl(QStyle::CE_Header, &opt, &painter, this);
        left += parent.draw.font_metrics.width;
    }
};

pattern_editor::pattern_editor(
    module_renderer &renderer,
    const keymap_t &keymap,
    const keymap_t &it_keymap,
    const keymap_t &xm_keymap,
    const colors_t &colors
) : keymap(keymap)
  , it_keymap(it_keymap)
  , xm_keymap(xm_keymap)
  , follow_playback(true)
  , draw(renderer, colors, *this)
  , row_header(*this)
  , column_header(*this)

  , select_column("Select Column", this)
  , select_pattern("Select Pattern", this)
  , cut("Cut", this)
  , copy("Copy", this)
  , paste("Paste", this)
  , undo("Undo", this)
  , redo("Redo", this)
  , clear_selection("Clear Selection", this)
  , interpolate_note("Interpolate Note", this)
  , interpolate_volume("Interpolate Volume", this)
  , interpolate_effect("Interpolate Effect", this)
  , transpose_semiup("Transpose +1", this)
  , transpose_semidown("Transpose -1", this)
  , transpose_octup("Transpose +12", this)
  , transpose_octdown("Transpose -12", this)
  , amplify("Amplify...", this)
  , change_instrument("Change Instrument", this)
  , grow_selection("Grow Selection", this)
  , shrink_selection("Shrink Selection", this)
  , insert_row("Insert Row", this)
  , delete_row("Delete Row", this)
{
    auto grid = new QGridLayout(this->viewport());
    grid->setMargin(0);
    grid->setSpacing(0);
    auto derf = new QPushButton;
    derf->setMinimumSize(1, 1);
    derf->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    grid->addWidget(derf, 0, 0);
    grid->addWidget(&column_header, 0, 1);
    grid->addWidget(&row_header   , 1, 0);
    grid->addWidget(&draw, 1, 1);

    verticalScrollBar()->setSingleStep(1);
    horizontalScrollBar()->setSingleStep(1);

    setFrameStyle(QFrame::Plain);

    set_base_octave(4);
    update_colors(colors);

    context_menu.addAction(&select_column);
    context_menu.addAction(&select_pattern);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(
        this,
        &pattern_editor::customContextMenuRequested,
        [&] (const QPoint &point) {
            auto global = mapToGlobal(point);
            context_menu.exec(global);
        }
    );
}

void pattern_editor::update_colors(const colors_t &newcolors) {
    draw.colors = newcolors;
    draw.updateGL();
}

void pattern_editor::update_playback_position(const player_position_t &position) {
    draw.playback_pos = position;
    if (follow_playback) {
        draw.active_pos = draw.playback_pos;
    }
    draw.updateGL();
}

void pattern_editor::set_base_octave(uint8_t octave) {
    _base_octave = octave;
}

uint8_t pattern_editor::base_octave() {
    return _base_octave;
}

void pattern_editor::move_to(const editor_position_t &target) {
    draw.selection.start = target;
    draw.selection.end   = target;
    recalc_corners();
    draw.update();
}

void pattern_editor::collapse_selection() {
    auto &newpos = draw.selection.start = draw.selection.end = pos();
    recalc_corners();
    move_to(newpos);
}

editor_position_t
pattern_editor::pos_move_by_row(const editor_position_t &in, int amount) const {
    auto newpos = in;
    bool down = amount < 0;
    uint32_t absamount = down ? -amount : amount;

    if (down) {
        newpos.row = absamount > newpos.row
            ? 0
            : newpos.row - absamount;
    } else {
        //XXXih: :[
        newpos.row += absamount;
        auto numrows = active_pattern()->GetNumRows();
        if (newpos.row >= numrows) {
            newpos.row = numrows - 1; //XXXih: :[[[
        }
    }

    return newpos;
}

editor_position_t
pattern_editor::pos_move_by_subcol(const editor_position_t &in, int amount) const {
    auto newpos = in;

    bool left = amount < 0;
    uint32_t absamount = left ? -amount : amount;

    //TODO: multiple column types
    if (left) {
        for (size_t i = 0; i < absamount; ++i) {
            if (newpos.subcolumn == ElemMin) {
                if (newpos.column > 0) {
                    newpos.subcolumn = (elem_t) (ElemMax - 1);
                    newpos.column -= 1;
                } else {
                    break;
                }
            } else {
                --newpos.subcolumn;
            }
        }
    } else {
        uint32_t bad_max = draw.renderer.GetNumChannels();
        for (size_t i = 0; i < absamount; ++i) {
            if (newpos.subcolumn == ElemMax - 1) {
                if (newpos.column < bad_max - 1) {
                    newpos.subcolumn = ElemMin;
                    newpos.column += 1;
                } else {
                    break;
                }
            } else {
                ++newpos.subcolumn;
            }
        }
    }

    return newpos;
}

bool pattern_editor::position_from_point(const QPoint &point,
                                         editor_position_t &pos)
{
    return draw.position_from_point(draw.mapFromParent(point), pos);
}

void pattern_editor::recalc_corners() {
    draw.corners = normalize_selection(draw.selection);
}

bool pattern_editor::set_pos_from_point(const QPoint &point,
                                        editor_position_t &pos)
{
    editor_position_t newpos;
    if (position_from_point(point, newpos)) {
        pos = newpos;
        return true;
    }
    return false;
}

void pattern_editor::set_selection_start(const QPoint &point) {
    if (set_pos_from_point(point, draw.selection.start)) {
        recalc_corners();
    }
}

void pattern_editor::set_selection_start(const editor_position_t &pos) {
    draw.selection.start = pos;
    recalc_corners();
}

void pattern_editor::set_selection_end(const QPoint &point) {
    if (set_pos_from_point(point, draw.selection.end)) {
        recalc_corners();
    }
}

void pattern_editor::set_selection_end(const editor_position_t &pos) {
    draw.selection.end = pos;
    recalc_corners();
}

void pattern_editor::set_active_order(modplug::tracker::orderindex_t idx) {
    draw.active_pos.pattern = draw.renderer.Order[idx];
    draw.update();
}

void pattern_editor::update_scrollbars(QSize container, QSize viewport) {
    const QSize sbsize(verticalScrollBar()->width(), horizontalScrollBar()->height());
    const auto pattern = pattern_size();
    const QSize editor(
        pattern.width() + row_header.width(),
        pattern.height() + column_header.height()
    );

    auto needs_vertical_scrollbar = editor.height() > container.height();
    auto needs_horizontal_scrollbar = editor.width() > container.width();

    if (needs_vertical_scrollbar) {
        if (editor.width() > container.width() - sbsize.width()) {
            needs_horizontal_scrollbar = true;
        }
    }

    if (needs_horizontal_scrollbar) {
        if (editor.height() > container.height() - sbsize.height()) {
            needs_vertical_scrollbar = true;
        }
    }

    auto update = [&] (bool active, QScrollBar *sb, int (QSize::*length)() const) {
        if (active) {
            auto slider_length = (pattern.*length)();
            sb->setRange(0, slider_length);
            sb->setPageStep((pattern.*length)());
        } else {
            sb->setRange(0, 0);
        }
    };

    update(needs_vertical_scrollbar,   verticalScrollBar(),   &QSize::height);
    update(needs_horizontal_scrollbar, horizontalScrollBar(), &QSize::width);
}

const editor_position_t &pattern_editor::pos() const {
    return draw.selection.end;
}

keycontext_t pattern_editor::keycontext() const {
    switch (pos().subcolumn) {
    case ElemNote:  return keycontext_t::NoteCol;
    case ElemInstr: return keycontext_t::InstrCol;
    case ElemVol:   return keycontext_t::VolCol;
    case ElemCmd:   return keycontext_t::CmdCol;
    case ElemParam: return keycontext_t::ParamCol;
    default:        return keycontext_t::Global;
    }
}

CPattern * pattern_editor::active_pattern() {
    auto patternidx = draw.active_pos.pattern;
    CPattern *ret = &draw.renderer.Patterns[patternidx];
    return ret;
}

const CPattern * pattern_editor::active_pattern() const {
    auto patternidx = draw.active_pos.pattern;
    CPattern *ret = &draw.renderer.Patterns[patternidx];
    return ret;
}

modevent_t *pattern_editor::active_event() {
    return active_pattern()->GetpModCommand(pos().row, pos().column);
}

QSize pattern_editor::pattern_size() {
    chnindex_t channel_count = draw.renderer.GetNumChannels();

    int left = 0;

    for (chnindex_t idx = 0; idx < channel_count; ++idx) {
        note_column notehomie(left, idx, draw.font_metrics);
        left += notehomie.width();
    }

    auto pattern = active_pattern();
    auto height = draw.font_metrics.height * pattern->GetNumRows();

    QSize ret(left, height);
    return ret;
}

const normalized_selection_t & pattern_editor::selection_corners() {
    return draw.corners;
}

bool pattern_editor::invoke_key(const keymap_t &km, key_t key) {
    auto action = action_of_key(km, pattern_actionmap, key);
    if (action) {
        action(*this);
        return true;
    } else {
        return false;
    }
}

void pattern_editor::ensure_selection_end_visible() {
}

void pattern_editor::keyPressEvent(QKeyEvent *event) {
    Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    auto context_key = key_t(modifiers, event->key(), keycontext());
    auto pattern_key = key_t(modifiers, event->key());

    if (invoke_key(keymap,    context_key)) return;
    if (invoke_key(it_keymap, context_key)) return;
    if (invoke_key(keymap,    pattern_key)) return;

    QAbstractScrollArea::keyPressEvent(event);
}

void pattern_editor::resizeEvent(QResizeEvent *event) {
    update_scrollbars(size(), event->size());
}

void pattern_editor::paintEvent(QPaintEvent *) {
    draw.updateGL();
}

void pattern_editor::scrollContentsBy(int, int) {
    const chnindex_t channel_count = draw.renderer.GetNumChannels();
    auto horiz = horizontalScrollBar()->value();
    int col = horiz / draw.font_metrics.width;

    draw.first_column = col;
    draw.updateGL();
    column_header.update();
}






}
}
}
