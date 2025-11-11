/**
 * js_interface.cpp
 * * Removed "using namespace OpenMPT;" and added explicit namespaces.
 * * Fixed FileCursor construction by using FileReader alias directly.
 * * Fixed ModLoadingFlags by using the plain enum value.
 */

#include "common/stdafx.h"
#include <string>
#include <vector>
#include <sstream>
#include <cstring> 
#include <cstddef> // For std::byte

// Core libopenmpt headers
#include "soundlib/Sndfile.h"
#include "soundlib/mod_specifications.h"
#include "soundlib/ModInstrument.h"
#include "soundlib/ModSample.h"
#include "soundlib/pattern.h"
#include "soundlib/patternContainer.h"

// mpt includes
#include "mpt/IO/IO.h"
#include "mpt/base/span.h"

// Full FileReader definition
#include "common/FileReader.h"

// Emscripten headers
#include <emscripten/bind.h>
#include <emscripten.h> 

// The single-header JSON library
#include "include/nlohmann/json.hpp"

// Use the Emscripten and nlohmann namespaces
using namespace emscripten;
using json = nlohmann::json;

// NOTE: We have REMOVED "using namespace OpenMPT;" to avoid ambiguity.

/**
 * @brief A class to hold and interact with a single module.
 * This is the object you will control from JavaScript.
 */
class ModulePlayer {
private:
    // *** FIX: Added OpenMPT:: namespace prefix ***
    OpenMPT::CSoundFile m_sndFile;
    bool m_isLoaded = false;

public:
    ModulePlayer() { }

    bool loadModule(const std::vector<char>& fileData) {
        if (fileData.empty()) {
            return false;
        }

        ::mpt::const_byte_span byteSpan(
            reinterpret_cast<const std::byte*>(fileData.data()), 
            fileData.size()
        );
        
        // *** FIX 1: Construct the FileReader alias directly from the span ***
        OpenMPT::FileReader fileReader(byteSpan);
        
        // *** FIX 2: Use the plain enum value with its namespace ***
        m_isLoaded = m_sndFile.Create(fileReader, OpenMPT::loadCompleteModule);
        return m_isLoaded;
    }

    std::string getSongTitle() {
        if (!m_isLoaded) {
            return "No module loaded";
        }
        return m_sndFile.GetTitle();
    }

    std::string getSongInfo() {
        if (!m_isLoaded) {
            return "{}";
        }
        json j;
        j["title"] = m_sndFile.GetTitle();
        j["channels"] = m_sndFile.GetNumChannels();
        j["patterns"] = m_sndFile.Patterns.GetNumPatterns();
        j["instruments"] = m_sndFile.GetNumInstruments();
        j["samples"] = m_sndFile.GetNumSamples();
        j["speed"] = m_sndFile.Order().GetDefaultSpeed();
        j["tempo"] = m_sndFile.Order().GetDefaultTempo().ToDouble();
        return j.dump();
    }

    // *** FIX: Added OpenMPT:: namespace prefix ***
    std::string getPatternData(OpenMPT::PATTERNINDEX patternIndex) {
        if (!m_isLoaded || patternIndex >= m_sndFile.Patterns.GetNumPatterns()) {
            return "{}";
        }

        // *** FIX: Added OpenMPT:: namespace prefix ***
        OpenMPT::CPattern& pattern = m_sndFile.Patterns[patternIndex];
        json j;
        j["pattern"] = patternIndex;
        j["rows"] = pattern.GetNumRows();
        
        json notes = json::array();
        // *** FIX: Added OpenMPT:: namespace prefix ***
        for (OpenMPT::ROWINDEX row = 0; row < pattern.GetNumRows(); ++row) {
            // *** FIX: Added OpenMPT:: namespace prefix ***
            for (OpenMPT::CHANNELINDEX chn = 0; chn < m_sndFile.GetNumChannels(); ++chn) {
                // *** FIX: Added OpenMPT:: namespace prefix ***
                OpenMPT::ModCommand& m = *pattern.GetpModCommand(row, chn);
                if (!m.IsEmpty()) {
                    json note_event;
                    note_event["row"] = row;
                    note_event["channel"] = chn;
                    if (m.note) note_event["note"] = m.note;
                    if (m.instr) note_event["instrument"] = m.instr;
                    if (m.volcmd) note_event["volcmd"] = m.volcmd;
                    if (m.vol) note_event["volume"] = m.vol;
                    if (m.command) note_event["command"] = m.command;
                    if (m.param) note_event["param"] = m.param;
                    notes.push_back(note_event);
                }
            }
        }
        j["data"] = notes;
        return j.dump();
    }
};


// --- Emscripten Bindings ---
EMSCRIPTEN_BINDINGS(my_module) {

    register_vector<char>("VectorChar");

    class_<ModulePlayer>("ModulePlayer")
        .constructor<>()
        .function("loadModule", &ModulePlayer::loadModule)
        .function("getSongTitle", &ModulePlayer::getSongTitle)
        .function("getSongInfo", &ModulePlayer::getSongInfo)
        .function("getPatternData", &ModulePlayer::getPatternData)
        ;
}
