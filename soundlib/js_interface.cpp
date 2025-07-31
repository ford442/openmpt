/**
 * js_interface.cpp
 * * This file contains a high-level helper function designed to be called from JavaScript.
 * It uses the nlohmann/json library to parse a JSON object describing a song,
 * then uses libopenmpt's internal classes to construct a valid module file in memory.
 * Finally, it uses Emscripten's embind to expose this function to the JavaScript world.
 */

// This is the most important include. It sets up all the basic types
// and configurations used throughout the libopenmpt project.
#include "common/stdafx.h"


#include <string>
#include <vector>
#include <sstream>

// Core libopenmpt headers
#include "soundlib/Sndfile.h"
#include "soundlib/ModInstrument.h"
#include "soundlib/ModSample.h"
#include "soundlib/pattern.h"
#include "soundlib/patternContainer.h"
#include "soundlib/mod_specifications.h"

// Emscripten binding header
#include <emscripten/bind.h>

// The single-header JSON library
#include "include/nlohmann/json.hpp"

// Use the Emscripten and nlohmann namespaces
using namespace emscripten;
using json = nlohmann::json;

// Bring the OpenMPT namespace into scope to resolve type errors
using namespace OpenMPT;

/**
 * @brief Creates a complete module file in memory from a JSON description.
 * * @param json_string A string containing the JSON object describing the song.
 * @return A std::vector<char> containing the bytes of the generated module file.
 * Emscripten will automatically convert this to a JavaScript Uint8Array.
 */
std.vector<char> CreateModuleFromJSON(const std::string &json_string) {
    CSoundFile sndFile;
    
    try {
        auto j = json::parse(json_string);

        // --- Basic Song Setup ---
        sndFile.Create(MOD_TYPE_XM, j.value("channels", 4));
        
        // --- Set Song Properties ---
        sndFile.SetDefaultSpeed(j.value("speed", 6));
        sndFile.SetDefaultTempo(TEMPO(j.value("tempo", 125.0)));
        
        // --- Create Instruments and Samples ---
        if (j.contains("instruments")) {
            for (size_t i = 0; i < j["instruments"].size(); ++i) {
                const auto& inst_json = j["instruments"][i];
                INSTRUMENTINDEX instIndex = static_cast<INSTRUMENTINDEX>(i + 1);
                
                if(instIndex > sndFile.GetNumInstruments()) {
                    sndFile.AllocateInstrument(instIndex);
                }

                ModInstrument *pInst = sndFile.Instruments[instIndex];
                if (!pInst) continue;
                
                pInst->name = inst_json.value("name", "Instrument");

                if (inst_json.contains("sample")) {
                    const auto& sample_json = inst_json["sample"];
                    SAMPLEINDEX sampleIndex = sndFile.GetNextFreeSample();
                    if(sampleIndex > 0 && sampleIndex <= sndFile.GetNumSamples()) {
                        
                        for(size_t note = 0; note < NOTE_MAX; ++note) {
                            pInst->Keyboard[note] = sampleIndex;
                        }

                        ModSample &sample = sndFile.GetSample(sampleIndex);
                        sample.filename = sample_json.value("name", "Sample");
                        
                        std::vector<int8_t> sample_data = sample_json["data"].get<std::vector<int8_t>>();
                        
                        if (!sample_data.empty()) {
                            // Use the correct function to read sample data from a memory pointer
                            sndFile.ReadSample(sampleIndex, reinterpret_cast<const char*>(sample_data.data()), sample_data.size());
                            sample.nLength = sample_data.size();
                            sample.nLoopStart = sample_json.value("loopStart", 0);
                            sample.nLoopEnd = sample_json.value("loopEnd", sample.nLength);
                            sample.uFlags.set(CHN_LOOP, sample_json.value("loop", true));
                            sample.nVolume = sample_json.value("volume", 256);
                            sample.nPan = sample_json.value("pan", 128);
                        }
                    }
                }
            }
        }

        // --- Create Patterns ---
        if (j.contains("patterns")) {
            for (size_t i = 0; i < j["patterns"].size(); ++i) {
                const auto& pattern_json = j["patterns"][i];
                sndFile.Patterns.Insert(i, pattern_json.value("rows", 64));
                CPattern &pattern = sndFile.Patterns[i];

                if (pattern_json.contains("data")) {
                    for (const auto& note_json : pattern_json["data"]) {
                        ROWINDEX row = note_json["row"];
                        CHANNELINDEX channel = note_json["channel"];
                        
                        ModCommand *m = pattern.GetpModCommand(row, channel);
                        m->note = note_json.value("note", (uint8_t)0);
                        m->instr = note_json.value("instrument", (uint8_t)0);
                        m->volcmd = VOLCMD_VOLUME;
                        m->vol = note_json.value("volume", (uint8_t)0);
                    }
                }
            }
        }

        // --- Set Pattern Order ---
        if (j.contains("patternOrder")) {
            std::vector<PATTERNINDEX> order = j["patternOrder"].get<std::vector<PATTERNINDEX>>();
            sndFile.Order.assign(order.begin(), order.end());
        }

        // --- Save to Memory ---
        std::stringstream memStream;
        // Use the correct save function from the CSoundFile class
        sndFile.SaveXM(memStream, false);
        
        std::string const& s = memStream.str();
        return std::vector<char>(s.begin(), s.end());

    } catch (const json::exception& e) {
        // MPT_LOG_GLOBAL(LogWarning, "JSON", "JSON parsing error: " + std::string(e.what()));
        return {};
    }
}

// --- Emscripten Bindings ---
EMSCRIPTEN_BINDINGS(my_module_exporter) {
    function("CreateModuleFromJSON", &CreateModuleFromJSON);
    register_vector<char>("VectorChar");
}
