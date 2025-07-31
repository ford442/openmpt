/**
 * js_interface.cpp
 * * This file contains a high-level helper function designed to be called from JavaScript.
 * It uses the nlohmann/json library to parse a JSON object describing a song,
 * then uses libopenmpt's internal classes to construct a valid module file in memory.
 * Finally, it uses Emscripten's embind to expose this function to the JavaScript world.
 */

#include <string>
#include <vector>
#include <sstream>

// Core libopenmpt headers
#include "soundlib/Sndfile.h"
// Corrected include paths based on your repository structure
#include "soundlib/ModInstrument.h"
#include "soundlib/ModSample.h"
#include "soundlib/pattern.h"
#include "soundlib/patternContainer.h"
#include "soundlib/mod_specifications.h" // Includes constants like MAX_INSTRUMENTNAME

// Emscripten binding header
#include <emscripten/bind.h>

// The single-header JSON library
// Make sure this path is correct for your project structure.
#include "../../include/nlohmann/json.hpp"

// Use the Emscripten and nlohmann namespaces
using namespace emscripten;
using json = nlohmann::json;

/**
 * @brief Creates a complete module file in memory from a JSON description.
 * * @param json_string A string containing the JSON object describing the song.
 * @return A std::vector<char> containing the bytes of the generated module file.
 * Emscripten will automatically convert this to a JavaScript Uint8Array.
 */
std::vector<char> CreateModuleFromJSON(const std::string &json_string) {
    CSoundFile sndFile;
    
    try {
        auto j = json::parse(json_string);

        // --- Basic Song Setup ---
        // Default to 4 channels, 1 pattern, 1 instrument if not specified
        sndFile.Create(j.value("instruments", 1), j.value("channels", 4), j.value("patterns", 1));
        sndFile.m_nType = MOD_TYPE_XM; // We are creating an XM file
        
        // --- Set Song Properties ---
        // Use .value() to provide default values safely
        sndFile.m_nDefaultSpeed = j.value("speed", 6);
        sndFile.m_nDefaultTempo = j.value("tempo", 125);
        
        // --- Create Instruments and Samples ---
        if (j.contains("instruments")) {
            for (size_t i = 0; i < j["instruments"].size(); ++i) {
                const auto& inst_json = j["instruments"][i];
                ModInstrument *pInst = sndFile.Instruments[i + 1];
                if (!pInst) continue;

                // Set instrument name
                strncpy(pInst->name, inst_json.value("name", "Instrument").c_str(), MAX_INSTRUMENTNAME);

                // For simplicity, we'll map one sample to each instrument
                if (inst_json.contains("sample")) {
                    const auto& sample_json = inst_json["sample"];
                    // We need to get the sample index for this instrument
                    SAMPLEINDEX sampleIndex = sndFile.GetNextFreeSample();
                    if(sampleIndex > 0 && sampleIndex <= sndFile.GetNumSamples()) {
                        pInst->AssignSample(sampleIndex);
                        ModSample &sample = sndFile.GetSample(sampleIndex);
                        
                        strncpy(sample.name, sample_json.value("name", "Sample").c_str(), MAX_SAMPLENAME);
                        
                        // Get sample data from JSON (assuming it's an array of numbers)
                        std::vector<int8_t> sample_data = sample_json["data"].get<std::vector<int8_t>>();
                        
                        if (!sample_data.empty()) {
                            sndFile.ReadSample(sampleIndex, reinterpret_cast<const char*>(sample_data.data()), sample_data.size(), 0);
                            sample.nLength = sample_data.size();
                            sample.nLoopStart = sample_json.value("loopStart", 0);
                            sample.nLoopEnd = sample_json.value("loopEnd", sample.nLength);
                            sample.uFlags.set(CHN_LOOP, sample_json.value("loop", true));
                            sample.nVolume = sample_json.value("volume", 256); // 256 is max
                            sample.nPan = sample_json.value("pan", 128);       // 128 is center
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
                        int row = note_json["row"];
                        int channel = note_json["channel"];
                        
                        ModCommand *m = pattern.GetpModCommand(row, channel);
                        m->note = note_json.value("note", 0);
                        m->instr = note_json.value("instrument", 0);
                        m->vol = note_json.value("volume", 0);
                        // You could add effects here too
                    }
                }
            }
        }

        // --- Set Pattern Order ---
        if (j.contains("patternOrder")) {
            std::vector<PATTERNINDEX> order = j["patternOrder"].get<std::vector<PATTERNINDEX>>();
            sndFile.Order.assign(order);
        }

        // --- Save to Memory ---
        std::stringstream memStream;
        sndFile.SaveXM(memStream, false);
        
        std::string const& s = memStream.str();
        return std::vector<char>(s.begin(), s.end());

    } catch (const json::exception& e) {
        // If JSON parsing fails, return an empty vector
        // You could also print the error message
        // MPT_LOG_GLOBAL(LogWarning, "JSON", "JSON parsing error: " + std::string(e.what()));
        return {};
    }
}

// --- Emscripten Bindings ---
// This is the magic that exposes your C++ function to JavaScript.
EMSCRIPTEN_BINDINGS(my_module_exporter) {
    function("CreateModuleFromJSON", &CreateModuleFromJSON);
    
    // This tells Emscripten how to handle the std::vector<char> return type.
    // It will be converted to a Uint8Array on the JavaScript side.
    register_vector<char>("VectorChar");
}
