/*
 * ModConvert.cpp
 * --------------
 * Purpose: Code for converting between various module formats.
 * Notes  : Incomplete list of MPTm-only features and extensions in the old formats:
 *          Features only available for MPTm:
 *           - User definable tunings.
 *           - Extended pattern range
 *           - Extended sequence
 *           - Multiple sequences ("songs")
 *           - Pattern-specific time signatures
 *           - Pattern effects :xy, S7D, S7E
 *           - Long instrument envelopes
 *           - Envelope release node (this was previously also usable in the IT format, but is now deprecated in that format)
 *
 *          Extended features in IT/XM/S3M/MOD (not all listed below are available in all of those formats):
 *           - Plugins
 *           - Extended ranges for
 *              - Sample count
 *              - Instrument count
 *              - Pattern count
 *              - Sequence size
 *              - Row count
 *              - Channel count
 *              - Tempo limits
 *           - Extended sample/instrument properties.
 *           - MIDI mapping directives
 *           - Version info
 *           - Channel names
 *           - Pattern names
 *           - Alternative tempomodes
 *           - For more info, see e.g. SaveExtendedSongProperties(), SaveExtendedInstrumentProperties()
 *
 * Authors: OpenMPT Devs
 *
 */

#include "Stdafx.h"
#include "Moddoc.h"
#include "Mainfrm.h"
#include "ModConvert.h"
#include "tracker/sample.hpp"

#include "legacy_soundlib/misc.h"

using namespace modplug::tracker;
using namespace modplug::pervasives;

namespace legacy_soundlib = modplug::legacy_soundlib;


#define CHANGEMODTYPE_WARNING(x)    warnings.set(x);
#define CHANGEMODTYPE_CHECK(x, s)    if(warnings[x]) AddToLog(_T(s));


// Trim envelopes and remove release nodes.
void UpdateEnvelopes(modplug::tracker::modenvelope_t *mptEnv, module_renderer *pSndFile, std::bitset<wNumWarnings> &warnings)
//---------------------------------------------------------------------------------------------------------
{
    // shorten instrument envelope if necessary (for mod conversion)
    const UINT iEnvMax = pSndFile->GetModSpecifications().envelopePointsMax;

    #define TRIMENV(iEnvLen) if(iEnvLen > iEnvMax) { iEnvLen = iEnvMax; CHANGEMODTYPE_WARNING(wTrimmedEnvelopes); }

    TRIMENV(mptEnv->num_nodes);
    TRIMENV(mptEnv->loop_start);
    TRIMENV(mptEnv->loop_end);
    TRIMENV(mptEnv->sustain_start);
    TRIMENV(mptEnv->sustain_end);
    if(mptEnv->release_node != ENV_RELEASE_NODE_UNSET)
    {
        if(pSndFile->GetModSpecifications().hasReleaseNode)
        {
            TRIMENV(mptEnv->release_node);
        } else
        {
            mptEnv->release_node = ENV_RELEASE_NODE_UNSET;
            CHANGEMODTYPE_WARNING(wReleaseNode);
        }
    }

    #undef TRIMENV
}


bool CModDoc::ChangeModType(MODTYPE nNewType)
//-------------------------------------------
{
    std::bitset<wNumWarnings> warnings;
    warnings.reset();
    modplug::tracker::patternindex_t nResizedPatterns = 0;

    const MODTYPE nOldType = m_SndFile.GetType();

    if (nNewType == nOldType && nNewType == MOD_TYPE_IT)
    {
        // Even if m_nType doesn't change, we might need to change extension in itp<->it case.
        // This is because ITP is a HACK and doesn't genuinely change m_nType,
        // but uses flags instead.
        ChangeFileExtension(nNewType);
        return true;
    }

    if(nNewType == nOldType)
        return true;

    const bool oldTypeIsMOD = (nOldType == MOD_TYPE_MOD), oldTypeIsXM = (nOldType == MOD_TYPE_XM),
                oldTypeIsS3M = (nOldType == MOD_TYPE_S3M), oldTypeIsIT = (nOldType == MOD_TYPE_IT),
                oldTypeIsMPT = (nOldType == MOD_TYPE_MPT), oldTypeIsMOD_XM = (oldTypeIsMOD || oldTypeIsXM),
                oldTypeIsS3M_IT_MPT = (oldTypeIsS3M || oldTypeIsIT || oldTypeIsMPT),
                oldTypeIsIT_MPT = (oldTypeIsIT || oldTypeIsMPT);

    const bool newTypeIsMOD = (nNewType == MOD_TYPE_MOD), newTypeIsXM =  (nNewType == MOD_TYPE_XM),
                newTypeIsS3M = (nNewType == MOD_TYPE_S3M), newTypeIsIT = (nNewType == MOD_TYPE_IT),
                newTypeIsMPT = (nNewType == MOD_TYPE_MPT), newTypeIsMOD_XM = (newTypeIsMOD || newTypeIsXM),
                newTypeIsS3M_IT_MPT = (newTypeIsS3M || newTypeIsIT || newTypeIsMPT),
                newTypeIsXM_IT_MPT = (newTypeIsXM || newTypeIsIT || newTypeIsMPT),
                newTypeIsIT_MPT = (newTypeIsIT || newTypeIsMPT);

    const CModSpecifications& specs = m_SndFile.GetModSpecifications(nNewType);

    // Check if conversion to 64 rows is necessary
    for(modplug::tracker::patternindex_t nPat = 0; nPat < m_SndFile.Patterns.Size(); nPat++)
    {
        if ((m_SndFile.Patterns[nPat]) && (m_SndFile.Patterns[nPat].GetNumRows() != 64))
            nResizedPatterns++;
    }

    if((m_SndFile.GetNumInstruments() || nResizedPatterns) && (nNewType & (MOD_TYPE_MOD|MOD_TYPE_S3M)))
    {
        if(::MessageBox(NULL,
                "This operation will convert all instruments to samples,\n"
                "and resize all patterns to 64 rows.\n"
                "Do you want to continue?", "Warning", MB_YESNO | MB_ICONQUESTION) != IDYES) return false;
        BeginWaitCursor();
        BEGIN_CRITICAL();

        // Converting instruments to samples
        if(m_SndFile.m_nInstruments)
        {
            ConvertInstrumentsToSamples();
            CHANGEMODTYPE_WARNING(wInstrumentsToSamples);
        }

        // Resizing all patterns to 64 rows
        for(modplug::tracker::patternindex_t nPat = 0; nPat < m_SndFile.Patterns.Size(); nPat++) if ((m_SndFile.Patterns[nPat]) && (m_SndFile.Patterns[nPat].GetNumRows() != 64))
        {
            // try to save short patterns by inserting a pattern break.
            if(m_SndFile.Patterns[nPat].GetNumRows() < 64)
            {
                m_SndFile.TryWriteEffect(nPat, m_SndFile.Patterns[nPat].GetNumRows() - 1, CmdPatternBreak, 0, false, ChannelIndexInvalid, false, weTryNextRow);
            }
            m_SndFile.Patterns[nPat].Resize(64, false);
            CHANGEMODTYPE_WARNING(wResizedPatterns);
        }

        // Removing all instrument headers from channels
        for(modplug::tracker::chnindex_t nChn = 0; nChn < MAX_VIRTUAL_CHANNELS; nChn++)
        {
            m_SndFile.Chn[nChn].instrument = nullptr;
        }

        for(modplug::tracker::instrumentindex_t nIns = 0; nIns < m_SndFile.m_nInstruments; nIns++) if (m_SndFile.Instruments[nIns])
        {
            delete m_SndFile.Instruments[nIns];
            m_SndFile.Instruments[nIns] = nullptr;
        }
        m_SndFile.m_nInstruments = 0;
        END_CRITICAL();
        EndWaitCursor();
    } //End if (((m_SndFile.m_nInstruments) || (b64)) && (nNewType & (MOD_TYPE_MOD|MOD_TYPE_S3M)))
    BeginWaitCursor();


    /////////////////////////////
    // Converting pattern data

    for(modplug::tracker::patternindex_t nPat = 0; nPat < m_SndFile.Patterns.Size(); nPat++) if (m_SndFile.Patterns[nPat])
    {
        modplug::tracker::modevent_t *m = m_SndFile.Patterns[nPat];

        // This is used for -> MOD/XM conversion
        vector<vector<modplug::tracker::param_t> > cEffectMemory;
        cEffectMemory.resize(m_SndFile.GetNumChannels());
        for(size_t i = 0; i < m_SndFile.GetNumChannels(); i++)
        {
            cEffectMemory[i].resize(CmdMax, 0);
        }

        bool addBreak = false;    // When converting to XM, avoid the E60 bug.
        modplug::tracker::chnindex_t nChannel = m_SndFile.m_nChannels - 1;

        for (UINT len = m_SndFile.Patterns[nPat].GetNumRows() * m_SndFile.m_nChannels; len; m++, len--)
        {
            nChannel = (nChannel + 1) % m_SndFile.m_nChannels; // 0...Channels - 1

            m_SndFile.ConvertCommand(m, nOldType, nNewType);

            // Deal with effect memory for MOD/XM arpeggio
            if (oldTypeIsS3M_IT_MPT && newTypeIsMOD_XM)
            {
                switch(m->command)
                {
                case CmdArpeggio:
                case CmdS3mCmdEx:
                case CmdModCmdEx:
                    // No effect memory in XM / MOD
                    if(m->param == 0)
                        m->param = cEffectMemory[nChannel][m->command];
                    else
                        cEffectMemory[nChannel][m->command] = m->param;
                    break;
                }
            }

            // Adjust effect memory for MOD files
            if(newTypeIsMOD)
            {
                switch(m->command)
                {
                case CmdPortaUp:
                case CmdPortaDown:
                case CmdPortaVolSlide:
                case CmdVibratoVolSlide:
                case CmdVolSlide:
                    // ProTracker doesn't have effect memory for these commands, so let's try to fix them
                    if(m->param == 0)
                        m->param = cEffectMemory[nChannel][m->command];
                    else
                        cEffectMemory[nChannel][m->command] = m->param;
                    break;

                }
            }

            // When converting to XM, avoid the E60 bug.
            if(newTypeIsXM)
            {
                switch(m->command)
                {
                case CmdModCmdEx:
                    if((m->param & 0xF0) == 0x60)
                    {
                        addBreak = true;
                    }
                    break;
                case CmdPositionJump:
                case CmdPatternBreak:
                    addBreak = false;
                    break;
                }
            }
        }
        if(addBreak)
        {
            m_SndFile.TryWriteEffect(nPat, m_SndFile.Patterns[nPat].GetNumRows() - 1, CmdPatternBreak, 0, false, ChannelIndexInvalid, false, weIgnore);
        }
    }

    ////////////////////////////////////////////////
    // Converting instrument / sample / etc. data


    // Do some sample conversion
    for(modplug::tracker::sampleindex_t nSmp = 1; nSmp <= m_SndFile.GetNumSamples(); nSmp++)
    {
        // Too many samples? Only 31 samples allowed in MOD format...
        if(newTypeIsMOD && nSmp > 31 && m_SndFile.Samples[nSmp].length > 0)
        {
            CHANGEMODTYPE_WARNING(wMOD31Samples);
        }

        // No Bidi / Sustain loops / Autovibrato for MOD/S3M
        if(newTypeIsMOD || newTypeIsS3M)
        {
            // Bidi loops
            if (bitset_is_set(m_SndFile.Samples[nSmp].flags, sflag_ty::BidiLoop)) {
                bitset_remove(m_SndFile.Samples[nSmp].flags, sflag_ty::BidiLoop);
                CHANGEMODTYPE_WARNING(wSampleBidiLoops);
            }

            // Sustain loops - convert to normal loops
            if(bitset_is_set(m_SndFile.Samples[nSmp].flags, sflag_ty::SustainLoop)) {
                // We probably overwrite a normal loop here, but since sustain loops are evaluated before normal loops, this is just correct.
                m_SndFile.Samples[nSmp].loop_start = m_SndFile.Samples[nSmp].sustain_start;
                m_SndFile.Samples[nSmp].loop_end = m_SndFile.Samples[nSmp].sustain_end;
                bitset_add(m_SndFile.Samples[nSmp].flags, sflag_ty::Loop);
                CHANGEMODTYPE_WARNING(wSampleSustainLoops);
            }
            m_SndFile.Samples[nSmp].sustain_start = m_SndFile.Samples[nSmp].sustain_end = 0;
            bitset_remove(m_SndFile.Samples[nSmp].flags, sflag_ty::SustainLoop);
            bitset_remove(m_SndFile.Samples[nSmp].flags, sflag_ty::BidiSustainLoop);

            // Autovibrato
            if(m_SndFile.Samples[nSmp].vibrato_depth || m_SndFile.Samples[nSmp].vibrato_rate || m_SndFile.Samples[nSmp].vibrato_sweep)
            {
                m_SndFile.Samples[nSmp].vibrato_depth = m_SndFile.Samples[nSmp].vibrato_rate = m_SndFile.Samples[nSmp].vibrato_sweep = m_SndFile.Samples[nSmp].vibrato_type = 0;
                CHANGEMODTYPE_WARNING(wSampleAutoVibrato);
            }
        }

        // Transpose to Frequency (MOD/XM to S3M/IT/MPT)
        if(oldTypeIsMOD_XM && newTypeIsS3M_IT_MPT)
        {
            m_SndFile.Samples[nSmp].c5_samplerate = frequency_of_transpose(
                m_SndFile.Samples[nSmp].RelativeTone,
                m_SndFile.Samples[nSmp].nFineTune
            );
            m_SndFile.Samples[nSmp].RelativeTone = 0;
            m_SndFile.Samples[nSmp].nFineTune = 0;
        }

        // Frequency to Transpose, panning (S3M/IT/MPT to MOD/XM)
        if(oldTypeIsS3M_IT_MPT && newTypeIsMOD_XM)
        {
            auto &smp = m_SndFile.Samples[nSmp];
            std::tie(smp.RelativeTone, smp.nFineTune) =
                transpose_of_frequency(smp.c5_samplerate);
            if (!bitset_is_set(m_SndFile.Samples[nSmp].flags, sflag_ty::ForcedPanning)) {
                m_SndFile.Samples[nSmp].default_pan = 128;
            }
            // No relative note for MOD files
            // TODO: Pattern notes could be transposed based on the previous relative tone?
            if(newTypeIsMOD && m_SndFile.Samples[nSmp].RelativeTone != 0)
            {
                m_SndFile.Samples[nSmp].RelativeTone = 0;
                CHANGEMODTYPE_WARNING(wMODSampleFrequency);
            }
        }

        if(oldTypeIsXM && newTypeIsIT_MPT)
        {
            // Autovibrato settings (XM to IT, where sweep 0 means "no vibrato")
            if(m_SndFile.Samples[nSmp].vibrato_sweep == 0 && m_SndFile.Samples[nSmp].vibrato_rate != 0 && m_SndFile.Samples[nSmp].vibrato_depth != 0)
                m_SndFile.Samples[nSmp].vibrato_sweep = 255;
        } else if(oldTypeIsIT_MPT && newTypeIsXM)
        {
            // Autovibrato settings (IT to XM, where sweep 0 means "no sweep")
            if(m_SndFile.Samples[nSmp].vibrato_sweep == 0)
                m_SndFile.Samples[nSmp].vibrato_rate = m_SndFile.Samples[nSmp].vibrato_depth = 0;
        }
    }

    for(modplug::tracker::instrumentindex_t nIns = 1; nIns <= m_SndFile.GetNumInstruments(); nIns++)
    {
        // Convert IT/MPT to XM (fix instruments)
        if(oldTypeIsIT_MPT && newTypeIsXM)
        {
            modplug::tracker::modinstrument_t *pIns = m_SndFile.Instruments[nIns];
            if (pIns)
            {
                for (UINT k = 0; k < NoteMax; k++)
                {
                    if ((pIns->NoteMap[k]) && (pIns->NoteMap[k] != (uint8_t)(k+1)))
                    {
                        CHANGEMODTYPE_WARNING(wBrokenNoteMap);
                        break;
                    }
                }
                // Convert sustain loops to sustain "points"
                if(pIns->volume_envelope.sustain_start != pIns->volume_envelope.sustain_end)
                {
                    CHANGEMODTYPE_WARNING(wInstrumentSustainLoops);
                    pIns->volume_envelope.sustain_end = pIns->volume_envelope.sustain_start;
                }
                if(pIns->panning_envelope.sustain_start != pIns->panning_envelope.sustain_end)
                {
                    CHANGEMODTYPE_WARNING(wInstrumentSustainLoops);
                    pIns->panning_envelope.sustain_end = pIns->panning_envelope.sustain_start;
                }
                pIns->volume_envelope.flags &= ~ENV_CARRY;
                pIns->panning_envelope.flags &= ~ENV_CARRY;
                pIns->pitch_envelope.flags &= ~(ENV_CARRY|ENV_ENABLED|ENV_FILTER);
                pIns->flags &= ~INS_SETPANNING;
                pIns->default_filter_cutoff &= 0x7F;
                pIns->default_filter_resonance &= 0x7F;
            }
        }
        // Convert MPT to anything - remove instrument tunings
        if(oldTypeIsMPT)
        {
            if(m_SndFile.Instruments[nIns] != nullptr && m_SndFile.Instruments[nIns]->pTuning != nullptr)
            {
                m_SndFile.Instruments[nIns]->SetTuning(nullptr);
                CHANGEMODTYPE_WARNING(wInstrumentTuning);
            }
        }
    }

    if(newTypeIsMOD)
    {
        // Not supported in MOD format
        m_SndFile.m_nDefaultSpeed = 6;
        m_SndFile.m_nDefaultTempo = 125;
        m_SndFile.m_nDefaultGlobalVolume = MAX_GLOBAL_VOLUME;
        m_SndFile.m_nSamplePreAmp = 48;
        m_SndFile.m_nVSTiVolume = 48;
        CHANGEMODTYPE_WARNING(wMODGlobalVars);
    }

    // Is the "restart position" value allowed in this format?
    if(m_SndFile.m_nRestartPos > 0 && !module_renderer::GetModSpecifications(nNewType).hasRestartPos)
    {
        // Try to fix it
        RestartPosToPattern();
        CHANGEMODTYPE_WARNING(wRestartPos);
    }

    // Fix channel settings (pan/vol)
    for(modplug::tracker::chnindex_t nChn = 0; nChn < m_SndFile.m_nChannels; nChn++)
    {
        if(newTypeIsMOD_XM || newTypeIsS3M)
        {
            if(m_SndFile.ChnSettings[nChn].nVolume != 64 || (m_SndFile.ChnSettings[nChn].dwFlags & CHN_SURROUND))
            {
                m_SndFile.ChnSettings[nChn].nVolume = 64;
                m_SndFile.ChnSettings[nChn].dwFlags &= ~CHN_SURROUND;
                CHANGEMODTYPE_WARNING(wChannelVolSurround);
            }
        }
        if(newTypeIsXM && !oldTypeIsMOD_XM)
        {
            if(m_SndFile.ChnSettings[nChn].nPan != 128)
            {
                m_SndFile.ChnSettings[nChn].nPan = 128;
                CHANGEMODTYPE_WARNING(wChannelPanning);
            }
        }
    }

    // Check for patterns with custom time signatures (fixing will be applied in the pattern container)
    if(!module_renderer::GetModSpecifications(nNewType).hasPatternSignatures)
    {
        for(modplug::tracker::patternindex_t nPat = 0; nPat < m_SndFile.GetNumPatterns(); nPat++)
        {
            if(m_SndFile.Patterns[nPat].GetOverrideSignature())
            {
                CHANGEMODTYPE_WARNING(wPatternSignatures);
                break;
            }
        }
    }

    // Check whether the new format supports embedding the edit history in the file.
    if(oldTypeIsIT_MPT && !newTypeIsIT_MPT && GetFileHistory()->size() > 0)
    {
        CHANGEMODTYPE_WARNING(wEditHistory);
    }

    BEGIN_CRITICAL();
    m_SndFile.ChangeModTypeTo(nNewType);
    if(!newTypeIsXM_IT_MPT && (m_SndFile.m_dwSongFlags & SONG_LINEARSLIDES))
    {
        CHANGEMODTYPE_WARNING(wLinearSlides);
        m_SndFile.m_dwSongFlags &= ~SONG_LINEARSLIDES;
    }
    if(!newTypeIsIT_MPT) m_SndFile.m_dwSongFlags &= ~(SONG_ITOLDEFFECTS|SONG_ITCOMPATGXX);
    if(!newTypeIsS3M) m_SndFile.m_dwSongFlags &= ~SONG_FASTVOLSLIDES;
    if(!newTypeIsMOD) m_SndFile.m_dwSongFlags &= ~SONG_PT1XMODE;
    if(newTypeIsS3M || newTypeIsMOD) m_SndFile.m_dwSongFlags &= ~SONG_EXFILTERRANGE;
    if(oldTypeIsXM && newTypeIsIT_MPT) m_SndFile.m_dwSongFlags |= SONG_ITCOMPATGXX;

    // Adjust mix levels
    if(newTypeIsMOD || newTypeIsS3M)
    {
        m_SndFile.m_nMixLevels = mixLevels_compatible;
        m_SndFile.m_pConfig->SetMixLevels(mixLevels_compatible);
    }
    if(oldTypeIsMPT && m_SndFile.m_nMixLevels != mixLevels_compatible)
    {
        CHANGEMODTYPE_WARNING(wMixmode);
    }

    // Automatically enable compatible mode when converting from MOD and S3M, since it's automatically enabled in those formats.
    if((nOldType & (MOD_TYPE_MOD|MOD_TYPE_S3M)) && (nNewType & (MOD_TYPE_XM|MOD_TYPE_IT)))
    {
        m_SndFile.SetModFlag(MSF_COMPATIBLE_PLAY, true);
    }
    if((nNewType & (MOD_TYPE_XM|MOD_TYPE_IT)) && !m_SndFile.GetModFlag(MSF_COMPATIBLE_PLAY))
    {
        CHANGEMODTYPE_WARNING(wCompatibilityMode);
    }

    END_CRITICAL();
    ChangeFileExtension(nNewType);

    // Check mod specifications
    m_SndFile.m_nDefaultTempo = CLAMP(m_SndFile.m_nDefaultTempo, specs.tempoMin, specs.tempoMax);
    m_SndFile.m_nDefaultSpeed = CLAMP(m_SndFile.m_nDefaultSpeed, specs.speedMin, specs.speedMax);

    for(modplug::tracker::instrumentindex_t i = 1; i <= m_SndFile.m_nInstruments; i++) if(m_SndFile.Instruments[i] != nullptr)
    {
        UpdateEnvelopes(&(m_SndFile.Instruments[i]->volume_envelope), &m_SndFile, warnings);
        UpdateEnvelopes(&(m_SndFile.Instruments[i]->panning_envelope), &m_SndFile, warnings);
        UpdateEnvelopes(&(m_SndFile.Instruments[i]->pitch_envelope), &m_SndFile, warnings);
    }

    CHAR s[64];
    CHANGEMODTYPE_CHECK(wInstrumentsToSamples, "All instruments have been converted to samples.\n");
    wsprintf(s, "%d patterns have been resized to 64 rows\n", nResizedPatterns);
    CHANGEMODTYPE_CHECK(wResizedPatterns, s);
    CHANGEMODTYPE_CHECK(wSampleBidiLoops, "Sample bidi loops are not supported by the new format.\n");
    CHANGEMODTYPE_CHECK(wSampleSustainLoops, "New format doesn't support sample sustain loops.\n");
    CHANGEMODTYPE_CHECK(wSampleAutoVibrato, "New format doesn't support sample autovibrato.\n");
    CHANGEMODTYPE_CHECK(wMODSampleFrequency, "Sample C-5 frequencies will be lost.\n");
    CHANGEMODTYPE_CHECK(wBrokenNoteMap, "Note Mapping will be lost when saving as XM.\n");
    CHANGEMODTYPE_CHECK(wInstrumentSustainLoops, "Sustain loops were converted to sustain points.\n");
    CHANGEMODTYPE_CHECK(wInstrumentTuning, "Instrument tunings will be lost.\n");
    CHANGEMODTYPE_CHECK(wMODGlobalVars, "Default speed, tempo and global volume will be lost.\n");
    CHANGEMODTYPE_CHECK(wMOD31Samples, "Samples above 31 will be lost when saving as MOD. Consider rearranging samples if there are unused slots available.\n");
    CHANGEMODTYPE_CHECK(wRestartPos, "Restart position is not supported by the new format.\n");
    CHANGEMODTYPE_CHECK(wChannelVolSurround, "Channel volume and surround are not supported by the new format.\n");
    CHANGEMODTYPE_CHECK(wChannelPanning, "Channel panning is not supported by the new format.\n");
    CHANGEMODTYPE_CHECK(wPatternSignatures, "Pattern-specific time signatures are not supported by the new format.\n");
    CHANGEMODTYPE_CHECK(wLinearSlides, "Linear Frequency Slides not supported by the new format.\n");
    CHANGEMODTYPE_CHECK(wTrimmedEnvelopes, "Instrument envelopes have been shortened.\n");
    CHANGEMODTYPE_CHECK(wReleaseNode, "Instrument envelope release nodes are not supported by the new format.\n");
    CHANGEMODTYPE_CHECK(wEditHistory, "Edit history will not be saved in the new format.\n");
    CHANGEMODTYPE_CHECK(wMixmode, "Consider setting the mix levels to \"Compatible\" in the song properties when working with legacy formats.\n");
    CHANGEMODTYPE_CHECK(wCompatibilityMode, "Consider enabling the \"compatible playback\" option in the song properties to increase compatiblity with other players.\n");

    SetModified();
    UpdateAllViews(NULL, HINT_MODTYPE | HINT_MODGENERAL);
    EndWaitCursor();

    return true;
}


#undef CHANGEMODTYPE_WARNING
#undef CHANGEMODTYPE_CHECK
