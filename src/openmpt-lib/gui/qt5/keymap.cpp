#include "stdafx.h"

#include <QtGui>

#include "keymap.hpp"
#include "../pervasives/pervasives.hpp"

#include "pattern_editor_actions.hpp"
#include "../../tracker/modevent.hpp"

using namespace modplug::pervasives;
using namespace modplug::tracker;

namespace modplug {
namespace gui {
namespace qt5 {

global_actionmap_t global_actionmap;
pattern_actionmap_t pattern_actionmap;

keymap_t pattern_it_fxmap;
keymap_t pattern_xm_fxmap;

template <typename f_t>
struct actionmap_assoc {
    const char *name;
    f_t action;
};

template <typename amap_t, typename f_t, size_t size>
void init_actions(amap_t &actionmap, const actionmap_assoc<f_t> (&assocs)[size]) {
    for (size_t idx = 0; idx < size; ++idx) {
        actionmap[assocs[idx].name] = assocs[idx].action;
    }
}

void maptest() {
    DEBUG_FUNC("waoaoao!");
}

const static actionmap_assoc<global_action_t> global_actions[] = {
    { "maptest", &maptest },
    { "new", &maptest },
    { "open", &maptest },
    { "save", &maptest },
    { "save_as", &maptest },
    { "export_as_wave", &maptest },
    { "export_as_midi", &maptest },
    { "export_compatible", &maptest },

    { "play_pause", &maptest },
    { "stop", &maptest },
    { "play_song_from_start", &maptest },
    { "play_song_from_cursor", &maptest },
    { "play_pattern_from_start", &maptest },
    { "play_pattern_from_cursor", &maptest },

    { "undo", &maptest },
    { "cut", &maptest },
    { "copy", &maptest },
    { "paste", &maptest },
    { "mix_paste", &maptest },
    { "mix_paste_it", &maptest },

    { "view_general", &maptest },
    { "view_pattern", &maptest },
    { "view_samples", &maptest },
    { "view_instruments", &maptest },
    { "view_graph", &maptest },
    { "view_comments", &maptest },
};

const static actionmap_assoc<pattern_action_t> pattern_actions[] = {
    { "move_up",    &pattern_editor_actions::move_up },
    { "move_down",  &pattern_editor_actions::move_down },
    { "move_left",  &pattern_editor_actions::move_left },
    { "move_right", &pattern_editor_actions::move_right },

    { "move_first_row", &pattern_editor_actions::move_first_row },
    { "move_last_row",  &pattern_editor_actions::move_last_row },
    { "move_first_col", &pattern_editor_actions::move_first_col },
    { "move_last_col",  &pattern_editor_actions::move_last_col },

    { "select_up",    &pattern_editor_actions::select_up },
    { "select_down",  &pattern_editor_actions::select_down },
    { "select_left",  &pattern_editor_actions::select_left },
    { "select_right", &pattern_editor_actions::select_right },

    { "select_first_row", &pattern_editor_actions::select_first_row },
    { "select_last_row",  &pattern_editor_actions::select_last_row },
    { "select_first_col", &pattern_editor_actions::select_first_col },
    { "select_last_col",  &pattern_editor_actions::select_last_col },

    { "clear_selected_cells", &pattern_editor_actions::clear_selected_cells },
    { "delete_row",           &pattern_editor_actions::delete_row },
    { "insert_row",           &pattern_editor_actions::insert_row },
};

template <typename ty>
struct cmd_assoc {
    const char *key;
    ty val;
};

const static cmd_assoc<volcmd_t> volcmds[] = {
    { "vol_vol",             VolCmdVol },
    { "vol_pan",             VolCmdPan },
    { "vol_slide_up",        VolCmdSlideUp },
    { "vol_slide_down",      VolCmdSlideDown },
    { "vol_fine_slide_up",   VolCmdFineUp },
    { "vol_fine_slide_down", VolCmdFineDown },
    { "vol_vibrato_speed",   VolCmdVibratoSpeed },
    { "vol_vibrato_depth",   VolCmdVibratoDepth },
    { "vol_xm_pan_left",     VolCmdPanSlideLeft },
    { "vol_xm_pan_right",    VolCmdPanSlideRight },
    { "vol_portamento",      VolCmdPortamento },
    { "vol_portamento_up",   VolCmdPortamentoUp },
    { "vol_portamento_down", VolCmdPortamentoDown },
    { "vol_offset",          VolCmdOffset },
};

const static cmd_assoc<cmd_t> cmds[] = {
    { "cmd_arpeggio",                CmdArpeggio },
    { "cmd_porta_up",                CmdPortaUp },
    { "cmd_porta_down",              CmdPortaDown },
    { "cmd_porta",                   CmdPorta },
    { "cmd_vibrato",                 CmdVibrato },
    { "cmd_porta_vol_slide",         CmdPortaVolSlide },
    { "cmd_vibrato_vol_slide",       CmdVibratoVolSlide },
    { "cmd_tremolo",                 CmdTremolo },
    { "cmd_panning",                 CmdPanning8 },
    { "cmd_offset",                  CmdOffset },
    { "cmd_vol_slide",               CmdVolSlide },
    { "cmd_position_jump",           CmdPositionJump },
    { "cmd_vol",                     CmdVol },
    { "cmd_pattern_break",           CmdPatternBreak },
    { "cmd_retrig",                  CmdRetrig },
    { "cmd_speed",                   CmdSpeed },
    { "cmd_tempo",                   CmdTempo },
    { "cmd_tremor",                  CmdTremor },
    { "cmd_mod_cmd_ex",              CmdModCmdEx },
    { "cmd_s3m_cmd_ex",              CmdS3mCmdEx },
    { "cmd_channel_vol",             CmdChannelVol },
    { "cmd_channel_vol_slide",       CmdChannelVolSlide },
    { "cmd_global_vol",              CmdGlobalVol },
    { "cmd_global_vol_slide",        CmdGlobalVolSlide },
    { "cmd_keyoff",                  CmdKeyOff },
    { "cmd_fine_vibrato",            CmdFineVibrato },
    { "cmd_panbrello",               CmdPanbrello },
    { "cmd_extra_fine_porta_updown", CmdExtraFinePortaUpDown },
    { "cmd_panning_slide",           CmdPanningSlide },
    { "cmd_set_envelope_position",   CmdSetEnvelopePosition },
    { "cmd_midi_macro",              CmdMidi },
    { "cmd_smooth_ midi_macro",      CmdSmoothMidi },
    { "cmd_delay_cut",               CmdDelayCut },
    { "cmd_extended_parameter",      CmdExtendedParameter },
    { "cmd_note_slide_up",           CmdNoteSlideUp },
    { "cmd_note_slide_down",         CmdNoteSlideDown },
};

struct itxm_default_assoc {
    int itkey;
    int xmkey;
    const char *val;
};

const static itxm_default_assoc defaultcmds[] = {
    { Qt::Key_J,       Qt::Key_0,       "cmd_arpeggio" },
    { Qt::Key_F,       Qt::Key_1,       "cmd_porta_up" },
    { Qt::Key_E,       Qt::Key_2,       "cmd_porta_down" },
    { Qt::Key_G,       Qt::Key_3,       "cmd_porta" },
    { Qt::Key_H,       Qt::Key_4,       "cmd_vibrato" },
    { Qt::Key_L,       Qt::Key_5,       "cmd_porta_vol_slide" },
    { Qt::Key_K,       Qt::Key_6,       "cmd_vibrato_vol_slide" },
    { Qt::Key_R,       Qt::Key_7,       "cmd_tremolo" },
    { Qt::Key_X,       Qt::Key_8,       "cmd_panning" },
    { Qt::Key_O,       Qt::Key_9,       "cmd_offset" },
    { Qt::Key_D,       Qt::Key_A,       "cmd_vol_slide" },
    { Qt::Key_B,       Qt::Key_B,       "cmd_position_jump" },
    { Qt::Key_unknown, Qt::Key_C,       "cmd_vol" },
    { Qt::Key_C,       Qt::Key_D,       "cmd_pattern_break" },
    { Qt::Key_Q,       Qt::Key_E,       "cmd_retrig" },
    { Qt::Key_A,       Qt::Key_unknown, "cmd_speed" },
    { Qt::Key_T,       Qt::Key_F,       "cmd_tempo" },
    { Qt::Key_I,       Qt::Key_T,       "cmd_tremor" },
    { Qt::Key_unknown, Qt::Key_E,       "cmd_mod_cmd_ex" },
    { Qt::Key_S,       Qt::Key_unknown, "cmd_s3m_cmd_ex" },
    { Qt::Key_M,       Qt::Key_unknown, "cmd_channel_vol" },
    { Qt::Key_N,       Qt::Key_unknown, "cmd_channel_vol_slide" },
    { Qt::Key_V,       Qt::Key_G,       "cmd_global_vol" },
    { Qt::Key_W,       Qt::Key_H,       "cmd_global_vol_slide" },
    { Qt::Key_unknown, Qt::Key_K,       "cmd_keyoff" },
    { Qt::Key_U,       Qt::Key_unknown, "cmd_fine_vibrato" },
    { Qt::Key_Y,       Qt::Key_Y,       "cmd_panbrello" },
    { Qt::Key_unknown, Qt::Key_X,       "cmd_extra_fine_porta_updown" },
    { Qt::Key_P,       Qt::Key_P,       "cmd_panning_slide" },
    { Qt::Key_unknown, Qt::Key_L,       "cmd_set_envelope_position" },
    { Qt::Key_Z,       Qt::Key_Z,       "cmd_midi_macro" },
};

const size_t MaxTones = 12;

const char *note_names[MaxTones] = {
    "c", "cs", "d", "ds", "e", "f", "fs", "g", "gs", "a", "as", "b"
};

template <typename ty, size_t size, typename fun_ty>
void init_cmd_actions(const cmd_assoc<ty> (&assocs)[size], fun_ty func) {
    auto &m = pattern_actionmap;

    for_each(assocs, [&m, &func] (const cmd_assoc<ty> &assoc) {
        fun_ty derf = func;
        ty val = assoc.val;
        m[assoc.key] = [val, derf] (pattern_editor &editor) {
            derf(editor, val);
        };
    });
}

template <typename fun_ty>
void init_base10_actions(const std::string &prefix, fun_ty func) {
    auto &m = pattern_actionmap;

    for (char i = '0'; i <= '9'; ++i) {
        auto fname = prefix + i;
        uint8_t digit = i - '0';
        m[fname] = [digit, func] (pattern_editor &editor) {
            func(editor, digit);
        };
    }
}

void init_default_cmd_maps() {
    auto &it = pattern_it_fxmap;
    auto &xm = pattern_xm_fxmap;

    for_each(defaultcmds, [&it, &xm] (const itxm_default_assoc &assoc) {
        it.insert(
            key_t(Qt::NoModifier, assoc.itkey, keycontext_t::CmdCol),
            assoc.val
        );
        xm.insert(
            key_t(Qt::NoModifier, assoc.xmkey, keycontext_t::CmdCol),
            assoc.val
        );
    });
}

void init_param_actions() {
    auto &m = pattern_actionmap;

    std::string param_prefix("param_");
    for (int i = 0; i <= 0xf; ++i) {
        char suffix = i >= 10 ? i - 10 + 'a' : i + '0';
        auto fname = param_prefix + suffix;
        uint8_t digit = i;
        m[fname] = [digit] (pattern_editor &editor) {
            pattern_editor_actions::insert_cmdparam(editor, digit);
        };
    }
}

void init_note_actions() {
    auto &m = pattern_actionmap;

    for (char i = '0'; i < '3'; ++i) {
        int octave = i - '0';

        std::string prefix("note_oct");
        prefix += i;
        prefix += "_";

        for (size_t tone = 0; tone < MaxTones; ++tone) {
            auto fname = prefix + note_names[tone];
            m[fname] = [octave, tone] (pattern_editor &editor) {
                pattern_editor_actions::insert_note(editor, octave, tone);
            };
        }
    }
}

void init_action_maps() {
    init_actions(global_actionmap,  global_actions);
    init_actions(pattern_actionmap, pattern_actions);

    init_cmd_actions(volcmds, &pattern_editor_actions::insert_volcmd);
    init_cmd_actions(cmds,    &pattern_editor_actions::insert_cmd);

    init_base10_actions("instr_", &pattern_editor_actions::insert_instr);
    init_base10_actions("vol_",   &pattern_editor_actions::insert_volparam);

    init_param_actions();
    init_note_actions();

    init_default_cmd_maps();
}

keymap_t default_global_keymap() {
    keymap_t km;
    auto key = [&km] (const key_t &key, const char *act) {
        km.insert(key, act);
    };
    key(key_t(Qt::NoModifier, Qt::Key_A), "maptest");

    key(key_t(Qt::ControlModifier, Qt::Key_N), "new");
    key(key_t(Qt::ControlModifier, Qt::Key_S), "save");

    key(key_t(Qt::NoModifier, Qt::Key_F5), "play_pause");
    key(key_t(Qt::NoModifier, Qt::Key_F6), "play_song_from_start");
    key(key_t(Qt::NoModifier, Qt::Key_F7), "play_pattern_from_start");
    key(key_t(Qt::NoModifier, Qt::Key_F8), "stop");
    key(key_t(Qt::NoModifier, Qt::Key_F9), "play_song_from_cursor");

    return km;
}

keymap_t default_pattern_keymap() {
    keymap_t m;
    auto key = [&m] (const key_t &key, const char *act) {
        m.insert(key, act);
    };

    key(key_t(Qt::NoModifier, Qt::Key_Up), "move_up");
    key(key_t(Qt::NoModifier, Qt::Key_Down), "move_down");
    key(key_t(Qt::NoModifier, Qt::Key_Left), "move_left");
    key(key_t(Qt::NoModifier, Qt::Key_Right), "move_right");

    key(key_t(Qt::ControlModifier, Qt::Key_Home), "move_first_row");
    key(key_t(Qt::ControlModifier, Qt::Key_End), "move_last_row");
    key(key_t(Qt::NoModifier, Qt::Key_Home), "move_first_col");
    key(key_t(Qt::NoModifier, Qt::Key_End), "move_last_col");

    key(key_t(Qt::ShiftModifier, Qt::Key_Up), "select_up");
    key(key_t(Qt::ShiftModifier, Qt::Key_Down), "select_down");
    key(key_t(Qt::ShiftModifier, Qt::Key_Left), "select_left");
    key(key_t(Qt::ShiftModifier, Qt::Key_Right), "select_right");

    key(key_t(Qt::ShiftModifier | Qt::ControlModifier, Qt::Key_Home), "select_first_row");
    key(key_t(Qt::ShiftModifier | Qt::ControlModifier, Qt::Key_End), "select_last_row");
    key(key_t(Qt::ShiftModifier, Qt::Key_Home), "select_first_col");
    key(key_t(Qt::ShiftModifier, Qt::Key_End), "select_last_col");

    key(key_t(Qt::NoModifier, Qt::Key_Delete), "clear_selected_cells");
    key(key_t(Qt::NoModifier, Qt::Key_Backspace), "delete_row");
    key(key_t(Qt::NoModifier, Qt::Key_Insert), "insert_row");

    auto notekey = [&m] (int key, const char *act) {
        return m.insert(
            key_t(Qt::NoModifier, key, keycontext_t::NoteCol),
            act
        );
    };

    notekey(Qt::Key_Q,            "note_oct0_c");
    notekey(Qt::Key_W,            "note_oct0_cs");
    notekey(Qt::Key_E,            "note_oct0_d");
    notekey(Qt::Key_R,            "note_oct0_ds");
    notekey(Qt::Key_T,            "note_oct0_e");
    notekey(Qt::Key_Y,            "note_oct0_f");
    notekey(Qt::Key_U,            "note_oct0_fs");
    notekey(Qt::Key_I,            "note_oct0_g");
    notekey(Qt::Key_O,            "note_oct0_gs");
    notekey(Qt::Key_P,            "note_oct0_a");
    notekey(Qt::Key_BracketLeft,  "note_oct0_as");
    notekey(Qt::Key_BracketRight, "note_oct0_b");

    notekey(Qt::Key_A,          "note_oct1_c");
    notekey(Qt::Key_S,          "note_oct1_cs");
    notekey(Qt::Key_D,          "note_oct1_d");
    notekey(Qt::Key_F,          "note_oct1_ds");
    notekey(Qt::Key_G,          "note_oct1_e");
    notekey(Qt::Key_H,          "note_oct1_f");
    notekey(Qt::Key_J,          "note_oct1_fs");
    notekey(Qt::Key_K,          "note_oct1_g");
    notekey(Qt::Key_L,          "note_oct1_gs");
    notekey(Qt::Key_Semicolon,  "note_oct1_a");
    notekey(Qt::Key_Apostrophe, "note_oct1_as");
    notekey(Qt::Key_Backslash,  "note_oct1_b");

    notekey(Qt::Key_Z,      "note_oct2_c");
    notekey(Qt::Key_X,      "note_oct2_cs");
    notekey(Qt::Key_C,      "note_oct2_d");
    notekey(Qt::Key_V,      "note_oct2_ds");
    notekey(Qt::Key_B,      "note_oct2_e");
    notekey(Qt::Key_N,      "note_oct2_f");
    notekey(Qt::Key_M,      "note_oct2_fs");
    notekey(Qt::Key_Comma,  "note_oct2_g");
    notekey(Qt::Key_Period, "note_oct2_gs");
    notekey(Qt::Key_Slash,  "note_oct2_a");

    auto instrkey = [&m] (int key, const char *act) {
        return m.insert(
            key_t(Qt::NoModifier, key, keycontext_t::InstrCol),
            act
        );
    };
    instrkey(Qt::Key_0, "instr_0");
    instrkey(Qt::Key_1, "instr_1");
    instrkey(Qt::Key_2, "instr_2");
    instrkey(Qt::Key_3, "instr_3");
    instrkey(Qt::Key_4, "instr_4");
    instrkey(Qt::Key_5, "instr_5");
    instrkey(Qt::Key_6, "instr_6");
    instrkey(Qt::Key_7, "instr_7");
    instrkey(Qt::Key_8, "instr_8");
    instrkey(Qt::Key_9, "instr_9");

    auto volkey = [&m] (int key, const char *act) {
        return m.insert(
            key_t(Qt::NoModifier, key, keycontext_t::VolCol),
            act
        );
    };

    volkey(Qt::Key_0, "vol_0");
    volkey(Qt::Key_1, "vol_1");
    volkey(Qt::Key_2, "vol_2");
    volkey(Qt::Key_3, "vol_3");
    volkey(Qt::Key_4, "vol_4");
    volkey(Qt::Key_5, "vol_5");
    volkey(Qt::Key_6, "vol_6");
    volkey(Qt::Key_7, "vol_7");
    volkey(Qt::Key_8, "vol_8");
    volkey(Qt::Key_9, "vol_9");

    volkey(Qt::Key_V, "vol_vol");
    volkey(Qt::Key_P, "vol_pan");
    volkey(Qt::Key_C, "vol_slide_up");
    volkey(Qt::Key_D, "vol_slide_down");
    volkey(Qt::Key_A, "vol_fine_slide_up");
    volkey(Qt::Key_B, "vol_fine_slide_down");
    volkey(Qt::Key_U, "vol_vibrato_speed");
    volkey(Qt::Key_H, "vol_vibrato");
    volkey(Qt::Key_U, "vol_vibrato_speed");
    volkey(Qt::Key_L, "vol_xm_pan_left");
    volkey(Qt::Key_R, "vol_xm_pan_right");
    volkey(Qt::Key_G, "vol_portamento");
    volkey(Qt::Key_F, "vol_portamento_up");
    volkey(Qt::Key_E, "vol_portamento_down");
    volkey(Qt::Key_O, "vol_offset");

    auto cmdkey = [&m] (int key, const char *act) {
        return m.insert(
            key_t(Qt::NoModifier, key, keycontext_t::CmdCol),
            act
        );
    };
    auto paramkey = [&m] (int key, const char *act) {
        return m.insert(
            key_t(Qt::NoModifier, key, keycontext_t::ParamCol),
            act
        );
    };

    cmdkey(Qt::Key_Backslash, "cmd_smooth_midi_macro");

    paramkey(Qt::Key_0, "param_0");
    paramkey(Qt::Key_1, "param_1");
    paramkey(Qt::Key_2, "param_2");
    paramkey(Qt::Key_3, "param_3");
    paramkey(Qt::Key_4, "param_4");
    paramkey(Qt::Key_5, "param_5");
    paramkey(Qt::Key_6, "param_6");
    paramkey(Qt::Key_7, "param_7");
    paramkey(Qt::Key_8, "param_8");
    paramkey(Qt::Key_9, "param_9");
    paramkey(Qt::Key_A, "param_a");
    paramkey(Qt::Key_B, "param_b");
    paramkey(Qt::Key_C, "param_c");
    paramkey(Qt::Key_D, "param_d");
    paramkey(Qt::Key_E, "param_e");
    paramkey(Qt::Key_F, "param_f");

    return m;
}

}
}
}
