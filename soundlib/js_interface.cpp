/**
 * js_interface.cpp
 * This file uses the specific API required by the user's libopenmpt branch,
 * combining direct member access and specific public methods. It uses the
 * Emscripten virtual filesystem to handle file saving.
 */

#include "common/stdafx.h"
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstring> // Needed for std::memcpy

// Core libopenmpt headers
#include "soundlib/Sndfile.h"
#include "soundlib/mod_specifications.hh"
#include "soundlib/ModInstrument.h"
#include "soundlib/ModSample.h"
#include "soundlib/pattern.h"
#include "soundlib/patternContainer.h"

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
 * @param json_string A string containing the JSON object describing the song.
 * @return A std::vector<char> containing the bytes of the generated module file.
 */
static std::vector<char> CreateModuleFromJSON(const std::string &json_string) {
    CSoundFile sndFile;
    
    try {
        auto j = json::parse(json_string);

        // --- Basic Song Setup ---
        sndFile.Create(MOD_TYPE_XM, j.value("channels", 4));
        
        // --- Set Song Properties ---
        sndFile.m_songName = j.value("songName", "AI Song");
        sndFile.m_nDefaultSpeed = j.value("speed", 6);
        sndFile.m_nDefaultTempo.Set(j.value("tempo", 125.0));
        
        // --- Create Instruments and Samples ---
        if (j.contains("instruments")) {
            for (size_t i = 0; i < j["instruments"].size(); ++i) {
                const auto& inst_json = j["instruments"][i];
                INSTRUMENTINDEX instIndex = static_cast<INSTRUMENTINDEX>(i + 1);

                if(instIndex >= MAX_INSTRUMENTS) continue;

                sndFile.Instruments[instIndex] = new (std::nothrow) ModInstrument();
                ModInstrument *pInst = sndFile.Instruments[instIndex];
                if (!pInst) continue;
                
                pInst->name = inst_json.value("name", "Instrument");

                if (inst_json.contains("sample")) {
                    const auto& sample_json = inst_json["sample"];
                    SAMPLEINDEX sampleIndex = static_cast<SAMPLEINDEX>(i + 1);
                    
                    if(sampleIndex >= MAX_SAMPLES) continue;

                    for(size_t note = 0; note < NOTE_MAX; ++note) {
                        pInst->Keyboard[note] = sampleIndex;
                    }

                    ModSample &sample = sndFile.GetSample(sampleIndex);
                    sample.Initialize(MOD_TYPE_XM);
                    
                    std::vector<int8_t> sample_data = sample_json["data"].get<std::vector<int8_t>>();
                    
                    if (!sample_data.empty()) {
                        sample.nLength = sample_data.size();
                        sample.pSample = sndFile.AllocateSample(sample.nLength);
                        if(sample.pSample) {
                            std::memcpy(sample.pSample, sample_data.data(), sample.nLength);
                        }
                        
                        sample.nLoopStart = sample_json.value("loopStart", 0);
                        sample.nLoopEnd = sample_json.value("loopEnd", sample.nLength);
                        sample.uFlags.set(CHN_LOOP, sample_json.value("loop", true));
                        sample.nVolume = sample_json.value("volume", 256);
                        sample.nPan = sample_json.value("pan", 128);
                    }
                }
            }
        }

        // --- Create Patterns ---
        if (j.contains("patterns")) {
            for (size_t i = 0; i < j["patterns"].size(); ++i) {
                const auto& pattern_json = j["patterns"][i];
                if(!sndFile.Patterns.Insert(i, pattern_json.value("rows", 64))) continue;
                CPattern &pattern = sndFile.Patterns[i];

                if (pattern_json.contains("data")) {
                    for (const auto& note_json : pattern_json["data"]) {
                        ROWINDEX row = note_json["row"];
                        CHANNELINDEX channel = note_json["channel"];
                        
                        ModCommand &m = *pattern.GetpModCommand(row, channel);
                        m.note = note_json.value("note", (uint8_t)0);
                        m.instr = note_json.value("instrument", (uint8_t)0);
                        m.volcmd = VOLCMD_VOLUME;
                        m.vol = note_json.value("volume", (uint8_t)0);
                    }
                }
            }
        }

        // --- Set Pattern Order ---
        if (j.contains("patternOrder")) {
            std::vector<PATTERNINDEX> order = j["patternOrder"].get<std::vector<PATTERNINDEX>>();
            sndFile.Order.assign(order);
        }

        // --- Save to Memory via Virtual Filesystem ---
        const std::string tempFilename = "/working/temp_module.xm";
        if(!sndFile.SaveXM(mpt::PathString::FromUTF8(tempFilename), false)) {
            // Saving failed
            return {};
        }

        // Read the file back from the virtual filesystem into a buffer
        std::ifstream inFile(tempFilename, std::ios::binary | std::ios::ate);
        std::streamsize size = inFile.tellg();
        inFile.seekg(0, std::ios::beg);

        std::vector<char> buffer(size);
        if (inFile.read(buffer.data(), size)) {
            return buffer;
        }

    } catch (const json::exception& e) {
        // MPT_LOG_GLOBAL(LogWarning, "JSON", "JSON parsing error: " + std::string(e.what()));
        return {};
    }
    
    return {}; // Return empty vector on failure
}

// --- Emscripten Bindings ---
EMSCRIPTEN_BINDINGS(my_module_exporter) {
    function("CreateModuleFromJSON", &CreateModuleFromJSON);
    register_vector<char>("VectorChar");
}
