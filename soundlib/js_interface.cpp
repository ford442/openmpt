/**
 * js_interface.cpp
 * * Fixes for Create() and IsValid() based on your branch's API.
 */

#include "common/stdafx.h"
#include <string>
#include <vector>
#include <sstream>
#include <cstring> 

// Core libopenmpt headers
#include "soundlib/Sndfile.h"
#include "soundlib/mod_specifications.h"
#include "soundlib/ModInstrument.h"
#include "soundlib/ModSample.h"
#include "soundlib/pattern.h"
#include "soundlib/patternContainer.h"

// *** FIX 1: ADD THESE INCLUDES ***
#include "mpt/io/io.hpp"
#include "mpt/base/span.hpp"

// Emscripten headers
#include <emscripten/bind.h>
#include <emscripten.h> 

// The single-header JSON library
#include "include/nlohmann/json.hpp"

// Use the Emscripten and nlohmann namespaces
using namespace emscripten;
using json = nlohmann::json;

// Bring the OpenMPT namespace into scope to resolve type errors
using namespace OpenMPT;


/**
 * @brief A class to hold and interact with a single module.
 * This is the object you will control from JavaScript.
 */
class ModulePlayer {
private:
    // The core libopenmpt object that holds the song
    CSoundFile m_sndFile;
    bool m_isLoaded = false;

public:
    ModulePlayer() {
        // Constructor
    }

    /**
     * @brief Loads a module file from a byte array (e.g., from JS File object).
     * @param fileData A std::vector<char> containing the raw bytes of an .xm, .mod, etc.
     * @return true on success, false on failure.
     */
    bool loadModule(const std::vector<char>& fileData) {
        if (fileData.empty()) {
            return false;
        }

        // *** FIX 1: Wrap the memory buffer in a FileReader ***
        // Create a "file cursor" pointing to our memory buffer
        mpt::IO::FileCursor fileCursor = mpt::IO::make_file_cursor(mpt::byte_span(fileData.data(), fileData.size()));
        // Create the FileReader object that CSoundFile::Create expects
        FileReader fileReader(fileCursor);
        
        // Now, call Create with the FileReader
        m_isLoaded = m_sndFile.Create(fileReader, loadCompleteModule);
        return m_isLoaded;
    }

    /**
     * @brief Gets the song title (as a quick test).
     * @return The song title string.
     */
    std::string getSongTitle() {
        if (!m_isLoaded) {
            return "No module loaded";
        }
        return m_sndFile.GetTitle();
    }

    /**
     * @brief Gets basic song info as a JSON string.
     * @return A JSON string with song properties.
     */
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

    /**
     * @brief Gets a single pattern's data as a JSON string.
     * This is how you'd get "note data".
     * @param patternIndex The index of the pattern (0-based).
     * @return A JSON string representing the pattern.
     */
    std::string getPatternData(PATTERNINDEX patternIndex) {
        // *** FIX 2: Use GetNumPatterns() for validation ***
        if (!m_isLoaded || patternIndex >= m_sndFile.Patterns.GetNumPatterns()) {
            return "{}";
        }

        CPattern& pattern = m_sndFile.Patterns[patternIndex];
        json j;
        j["pattern"] = patternIndex;
        j["rows"] = pattern.GetNumRows();
        
        json notes = json::array();
        for (ROWINDEX row = 0; row < pattern.GetNumRows(); ++row) {
            for (CHANNELINDEX chn = 0; chn < m_sndFile.GetNumChannels(); ++chn) {
                ModCommand& m = *pattern.GetpModCommand(row, chn);
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
// This is the "bridge" that makes your C++ code available to JavaScript.
EMSCRIPTEN_BINDINGS(my_module) {

    // First, we must "register" std::vector<char> so JS can understand it
    register_vector<char>("VectorChar");

    // Next, we bind our ModulePlayer class
    class_<ModulePlayer>("ModulePlayer")
        .constructor<>() // Exposes the C++ constructor
        
        // Exposes the loadModule function.
        .function("loadModule", &ModulePlayer::loadModule)
        
        // Exposes the getSongTitle function.
        .function("getSongTitle", &ModulePlayer::getSongTitle)

        // Exposes the getSongInfo function.
        .function("getSongInfo", &ModuleCPlayer::getSongInfo)

        // Exposes the getPatternData function.
        .function("getPatternData", &ModulePlayer::getPatternData)
        ;
}
