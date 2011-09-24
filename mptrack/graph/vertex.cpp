/*
Copyright (c) 2011, Imran Hameed 
All rights reserved. 

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met: 

 * Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer. 
 * Redistributions in binary form must reproduce the above copyright 
   notice, this list of conditions and the following disclaimer in the 
   documentation and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY 
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY 
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH 
DAMAGE. 
*/

#include "stdafx.h"

#include "vertex.h"
#include "../mptrack.h"

namespace modplug {
namespace graph {


vertex::vertex(id_t id, std::string name = "anon") : id(id), name(name) {
    _input_channels  = 0;
    _output_channels = 0;
    _input_arrows.resize(MAX_CHANNEL_ENDPOINTS);
    _output_arrows.resize(MAX_CHANNEL_ENDPOINTS);
    std::fill_n(ghetto_channels, modplug::mixer::MIX_BUFFER_SIZE * 2 + 2, 0);
    modplug::pervasives::debug_log("vertex::vertex(%s)", name.c_str());
}

vertex::~vertex() {
    modplug::pervasives::debug_log("vertex::~vertex(%s)", name.c_str());
}


arrow::arrow(id_t id, vertex *head, size_t head_channel, vertex *tail, size_t tail_channel) :
    id(id), head(head), tail(tail),
    head_channel(head_channel),
    tail_channel(tail_channel) { }


}
}
