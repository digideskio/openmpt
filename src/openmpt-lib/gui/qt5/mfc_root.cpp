#include "stdafx.h"

#include "../../MainFrm.h"
#include "app_config.hpp"
#include "mfc_root.hpp"
#include "../../audioio/paudio.hpp"
#include "../../pervasives/pervasives.hpp"

using namespace modplug::pervasives;
using namespace modplug::audioio;

namespace modplug {
namespace gui {
namespace qt5 {

mfc_root::mfc_root(app_config &settings, CMainFrame &mfc_parent) :
    QWinWidget(&mfc_parent), settings(settings), mainwnd(mfc_parent)
{
    connect(
        &settings, &app_config::audio_settings_changed,
        this, &mfc_root::update_audio_settings
    );
}

void mfc_root::update_audio_settings() {
    DEBUG_FUNC("thread id = %x", GetCurrentThreadId());

    const auto &audio_settings = settings.audio_settings();

    mainwnd.stream->close();
    if (mainwnd.renderer) {
        mainwnd.renderer->deprecated_SetResamplingMode(mainwnd.m_nSrcMode);
        mainwnd.renderer->change_audio_settings(audio_settings);
    }

    try {
        mainwnd.stream = std::make_shared<paudio>(
            audio_settings,
            settings.audio_handle(),
            mainwnd
        );
    } catch (portaudio::PaException &exc) {
        DEBUG_FUNC(
            "!!!!! caught exception; paErrorText = \"%s\" !!!!!",
            exc.paErrorText()
        );
        throw exc;
    }
    mainwnd.stream->start();
    DEBUG_FUNC("created new stream; settings = %s",
        debug_json_dump(json_of_paudio_settings(
            mainwnd.stream->settings(),
            settings.audio_handle()
        )).c_str()
    );
}

}
}
}
