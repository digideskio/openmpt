#pragma once

#include <algorithm>
#include <cstdint>

// Note definitions
#define NOTE_NONE    (::modplug::tracker::note_t(0))
#define NOTE_MIDDLEC (::modplug::tracker::note_t(5*12+1))
#define NOTE_KEYOFF  (::modplug::tracker::note_t(0xFF))
#define NOTE_NOTECUT (::modplug::tracker::note_t(0xFE))

// 253, IT's action for illegal notes
// DO NOT SAVE AS 253 as this is IT's internal representation of "no note"!
#define NOTE_FADE    (::modplug::tracker::note_t(0xFD))

// 252, Param Control 'note'. Changes param value on first tick.
#define NOTE_PC      (::modplug::tracker::note_t(0xFC))

// 251, Param Control(Smooth) 'note'. Changes param value during the whole row.
#define NOTE_PCS     (::modplug::tracker::note_t(0xFB))

#define NOTE_MIN     (::modplug::tracker::note_t(1))
#define NOTE_MAX     (::modplug::tracker::note_t(120))
#define NOTE_COUNT   120

#define NOTE_MAX_SPECIAL NOTE_KEYOFF
#define NOTE_MIN_SPECIAL NOTE_PCS

// Checks whether a number represents a valid note
// (a "normal" note or no note, but not something like note off)
#define NOTE_IS_VALID(n) \
    ((n) == NOTE_NONE || ((n) >= NOTE_MIN && (n) <= NOTE_MAX))


namespace modplug {
namespace tracker {

//#include <boost/strong_typedef.hpp>
//BOOST_STRONG_TYPEDEF(uint8_t, note_t)
typedef uint8_t note_t;
typedef uint8_t instr_t;
typedef uint8_t vol_t;
typedef uint8_t volcmd_t;
typedef uint8_t cmd_t;
typedef uint8_t param_t;

struct modevent_t {
    note_t note;
    instr_t instr;
    volcmd_t volcmd;
    cmd_t command;
    vol_t vol;
    param_t param;

    // Defines the maximum value for column data when interpreted as 2-byte
    // value (for example volcmd and vol). The valid value range is
    // [0, maxColumnValue].
    enum { MaxColumnValue = 999 };

    static modevent_t empty() {
        modevent_t ret = { note_t(0), 0, 0, 0, 0, 0 };
        return ret;
    }

    bool operator == (const modevent_t& mc) const {
        return memcmp(this, &mc, sizeof(modevent_t)) == 0;
    }

    bool operator != (const modevent_t& mc) const {
        return !(*this == mc);
    }

    void Set(note_t n, instr_t ins, uint16_t volcol, uint16_t effectcol) {
        note = n;
        instr = ins;
        SetValueVolCol(volcol);
        SetValueEffectCol(effectcol);
    }

    uint16_t GetValueVolCol() const {
        return GetValueVolCol(volcmd, vol);
    }

    static uint16_t GetValueVolCol(uint8_t volcmd, uint8_t vol) {
        return (volcmd << 8) + vol;
    }

    void SetValueVolCol(const uint16_t val) {
        volcmd = static_cast<uint8_t>(val >> 8);
        vol = static_cast<uint8_t>(val & 0xFF);
    }

    uint16_t GetValueEffectCol() const {
        return GetValueEffectCol(command, param);
    }

    static uint16_t GetValueEffectCol(uint8_t command, uint8_t param) {
        return (command << 8) + param;
    }

    void SetValueEffectCol(const uint16_t val) {
        command = static_cast<uint8_t>(val >> 8);
        param = static_cast<uint8_t>(val & 0xFF);
    }

    // Clears modcommand.
    void Clear() {
        memset(this, 0, sizeof(modevent_t));
    }

    // Returns true if modcommand is empty, false otherwise.
    // If ignoreEffectValues is true (default), effect values are ignored are ignored if there is no effect command present.
    bool IsEmpty(const bool ignoreEffectValues = true) const {
        if (ignoreEffectValues) {
            return this->note == 0
                && this->instr == 0
                && this->volcmd == 0
                && this->command == 0;
        } else {
            return *this == empty();
        }
    }

    // Returns true if instrument column represents plugin index.
    bool IsInstrPlug() const {
        return IsPcNote();
    }

    // Returns true if and only if note is NOTE_PC or NOTE_PCS.
    bool IsPcNote() const {
        return note == NOTE_PC || note == NOTE_PCS;
    }

    static bool IsPcNote(const note_t note_id) {
        return note_id == NOTE_PC || note_id == NOTE_PCS;
    }

    // Swap volume and effect column (doesn't do any conversion as it's mainly for importing formats with multiple effect columns, so beware!)
    void SwapEffects() {
        std::swap(volcmd, command);
        std::swap(vol, param);
    }
};


}
}


// Volume Column commands
#define VOLCMD_NONE            0
#define VOLCMD_VOLUME          1
#define VOLCMD_PANNING         2
#define VOLCMD_VOLSLIDEUP      3
#define VOLCMD_VOLSLIDEDOWN    4
#define VOLCMD_FINEVOLUP       5
#define VOLCMD_FINEVOLDOWN     6
#define VOLCMD_VIBRATOSPEED    7
#define VOLCMD_VIBRATODEPTH    8
#define VOLCMD_PANSLIDELEFT    9
#define VOLCMD_PANSLIDERIGHT   10
#define VOLCMD_TONEPORTAMENTO  11
#define VOLCMD_PORTAUP         12
#define VOLCMD_PORTADOWN       13
#define VOLCMD_DELAYCUT        14 //currently unused
#define VOLCMD_OFFSET          15 //rewbs.volOff
#define MAX_VOLCMDS            16


// Effect column commands
#define CMD_NONE             0
#define CMD_ARPEGGIO         1
#define CMD_PORTAMENTOUP     2
#define CMD_PORTAMENTODOWN   3
#define CMD_TONEPORTAMENTO   4
#define CMD_VIBRATO          5
#define CMD_TONEPORTAVOL     6
#define CMD_VIBRATOVOL       7
#define CMD_TREMOLO          8
#define CMD_PANNING8         9
#define CMD_OFFSET           10
#define CMD_VOLUMESLIDE      11
#define CMD_POSITIONJUMP     12
#define CMD_VOLUME           13
#define CMD_PATTERNBREAK     14
#define CMD_RETRIG           15
#define CMD_SPEED            16
#define CMD_TEMPO            17
#define CMD_TREMOR           18
#define CMD_MODCMDEX         19
#define CMD_S3MCMDEX         20
#define CMD_CHANNELVOLUME    21
#define CMD_CHANNELVOLSLIDE  22
#define CMD_GLOBALVOLUME     23
#define CMD_GLOBALVOLSLIDE   24
#define CMD_KEYOFF           25
#define CMD_FINEVIBRATO      26
#define CMD_PANBRELLO        27
#define CMD_XFINEPORTAUPDOWN 28
#define CMD_PANNINGSLIDE     29
#define CMD_SETENVPOSITION   30
#define CMD_MIDI             31
#define CMD_SMOOTHMIDI       32 //rewbs.smoothVST
#define CMD_DELAYCUT         33
#define CMD_XPARAM           34 // extended pattern effect parameter mechanism
#define CMD_NOTESLIDEUP      35 // IMF Gxy
#define CMD_NOTESLIDEDOWN    36 // IMF Hxy
#define MAX_EFFECTS          37