#pragma once

#include "portaudiocpp/PortAudioCpp.hxx"

#include <array>
#include <json/json.h>

class CMainFrame;

namespace modplug {
namespace audioio {

struct paudio_settings {
    double sample_rate;
    portaudio::SampleDataFormat sample_format;

    PaHostApiTypeId host_api;
    PaDeviceIndex device;

    PaTime latency;
    unsigned int buffer_length;
    unsigned int channels;
};
Json::Value json_of_paudio_settings(const paudio_settings &);
paudio_settings paudio_settings_of_json(Json::Value &);




class paudio_callback {
public:
    paudio_callback(CMainFrame &main_frame, paudio_settings &settings);

    int invoke(const void *, void *, unsigned long, const PaStreamCallbackTimeInfo *, PaStreamCallbackFlags);


private:
    CMainFrame &main_frame;
    paudio_settings &settings; 
};



class paudio {
public:
    paudio(paudio_settings &settings, portaudio::System &system, CMainFrame &);
    ~paudio();

    void start();
    void stop();
    void close();

private:
    const bool interleaved;
    paudio_settings settings;
    paudio_callback callback;

    portaudio::MemFunCallbackStream<paudio_callback> stream;
};

}
}