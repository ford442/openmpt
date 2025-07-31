/**
 * js_interface.cpp
 * This file uses the specific API required by the user's libopenmpt branch,
 * combining direct member access and specific public methods.
 */

#include "common/stdafx.h"
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstring> // Needed for std::memcpy

// Core libopenmpt headers
#include "soundlib/Sndfile.h"
#include "soundlib/mod_specifications.h"
#include "soundlib/ModInstrument.h"
#include "soundlib/ModSample.h"
#include "soundlib/pattern.h"
#include "soundlib/patternContainer.h"
#include "soundlib/XMTools.h"

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
        sndFile.SetTitle(j.value("songName", "AI Song"));
        sndFile.Order().SetDefaultSpeed(j.value("speed", 6));
        sndFile.Order().SetDefaultTempo(TEMPO(j.value("tempo", 125.0)));
        
        // --- Create Instruments and Samples ---
        if (j.contains("instruments")) {
            for (size_t i = 0; i < j["instruments"].size(); ++i) {
                const auto& inst_json = j["instruments"][i];
                INSTRUMENTINDEX instIndex = static_cast<INSTRUMENTINDEX>(i + 1);

                if(instIndex >= MAX_INSTRUMENTS) continue;

                ModInstrument *pInst = sndFile.AllocateInstrument(instIndex);
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
                        if(sample.AllocateSample())
                        {
                            std::memcpy(sample.samplev(), sample_data.data(), sample.nLength);
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
            sndFile.Order().clear();
            for(auto pat : order)
            {
                sndFile.Order().push_back(pat);
            }
        }

        // --- Save to Memory ---
        std::stringstream memStream;
        if(!XMTools::Save(sndFile, memStream))
        {
            return {}; // Saving failed
        }
        std::string const& s = memStream.str();
        return std::vector<char>(s.begin(), s.end());

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
```


```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AI Music - Algorithmic MOD Generator</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
        body { font-family: 'Inter', sans-serif; }
        #player-frame { border: 2px dashed #4a5568; }
    </style>
</head>
<body class="bg-gray-900 text-gray-200 flex flex-col items-center justify-center min-h-screen p-4 space-y-8">

    <!-- Control Panel -->
    <div class="w-full max-w-2xl bg-gray-800 rounded-2xl shadow-2xl p-8">
        <h1 class="text-3xl font-bold text-white mb-4 text-center">Algorithmic Pattern Generator (MOD)</h1>
        
        <div class="bg-gray-700 p-4 rounded-lg mb-4">
            <h2 class="text-xl font-semibold mb-2">Generated Pattern Data:</h2>
            <pre id="pattern-display" class="bg-gray-900 text-green-400 p-2 rounded h-32 overflow-y-auto text-xs">--- Click the button to generate a pattern ---</pre>
        </div>
        
        <div class="flex justify-center space-x-4">
            <button id="generate-and-send-button" class="w-full bg-violet-600 hover:bg-violet-700 text-white font-bold py-3 px-4 rounded-lg text-xl">
                Generate Melody & Broadcast
            </button>
            <a id="download-button" class="w-full bg-gray-600 hover:bg-gray-700 text-white font-bold py-3 px-4 rounded-lg text-xl text-center pointer-events-none opacity-50">
                Download .MOD
            </a>
        </div>
    </div>

    <script>
        'use strict';
        
        let pattern = [];
        const channel = new BroadcastChannel('ai-music-channel');
        let currentSongBufferUrl = null;

        // Amiga periods for a C-Major scale
        const cMajorScale = [214, 190, 170, 160, 143, 127, 113, 107];
        const noteNames = { 214: 'C-4', 190: 'D-4', 170: 'E-4', 160: 'F-4', 143: 'G-4', 127: 'A-4', 113: 'B-4', 107: 'C-5' };

        function updatePatternDisplay() {
            const display = document.getElementById('pattern-display');
            let text = '';
            for(let i=0; i<64; i++) {
                const note = pattern.find(n => n.row === i);
                const rowStr = `Row ${String(i).padStart(2, '0')}:`;
                if (note) {
                    if(note.period > 0) {
                        text += `${rowStr} Note ${noteNames[note.period] || note.period}, Inst=${note.instrument}\n`;
                    } else {
                         text += `${rowStr} --- Note Cut (C00) ---\n`;
                    }
                } else {
                    text += `${rowStr} ---\n`;
                }
            }
            display.textContent = text;
        }
        
        // --- Algorithmic Melody Generation ---
        function generateAlgorithmicPattern() {
            const newPattern = [];
            let currentRow = 0;
            let currentNoteIndex = Math.floor(Math.random() * cMajorScale.length); // Start on a random note in the scale

            for(let i=0; i < 8; i++) { // Generate 8 notes
                if(currentRow >= 62) break;

                // Add the note
                newPattern.push({ row: currentRow, channel: 0, period: cMajorScale[currentNoteIndex], instrument: 1 });
                // Add the note cut
                newPattern.push({ row: currentRow + 2, channel: 0, period: 0, instrument: 0 });

                // Decide where to go next (random walk up or down the scale)
                const step = Math.random() > 0.5 ? 1 : -1;
                currentNoteIndex += step;

                // Clamp the index to stay within the scale
                if (currentNoteIndex < 0) currentNoteIndex = 1;
                if (currentNoteIndex >= cMajorScale.length) currentNoteIndex = cMajorScale.length - 2;
                
                currentRow += 4; // Move to the next note position
            }
            newPattern.sort((a, b) => a.row - b.row);
            return newPattern;
        }

        document.getElementById('generate-and-send-button').addEventListener('click', () => {
            pattern = generateAlgorithmicPattern();
            updatePatternDisplay();

            const sampleData = new Int8Array(4096).map((_, i) => (i % 256) / 256 * 255 - 128);
            const modFileBuffer = createModFile(sampleData, pattern);
            
            if (modFileBuffer) {
                channel.postMessage({ type: 'playSong', buffer: modFileBuffer });

                const downloadButton = document.getElementById('download-button');
                if (currentSongBufferUrl) URL.revokeObjectURL(currentSongBufferUrl);
                const blob = new Blob([modFileBuffer], { type: 'audio/mod' });
                currentSongBufferUrl = URL.createObjectURL(blob);
                downloadButton.href = currentSongBufferUrl;
                downloadButton.download = 'ai-generated-song.mod';
                downloadButton.classList.remove('opacity-50', 'pointer-events-none');
            } else {
                console.error("Failed to generate MOD file.");
            }
        });
        
        updatePatternDisplay();

        function createModFile(sampleData, patternData) {
            const numRows = 64;
            const songLength = 8;
            const numPatterns = 1;
            const numChannels = 4;
            const headerSize = 1084;
            const patternSize = numRows * numChannels * 4 * numPatterns;
            const totalSize = headerSize + patternSize + sampleData.length;
            
            const buffer = new ArrayBuffer(totalSize);
            const view = new DataView(buffer);
            let offset = 0;

            const writeString = (str, len) => {
                for (let i = 0; i < len; i++) {
                    view.setUint8(offset + i, i < str.length ? str.charCodeAt(i) : 0);
                }
                offset += len;
            };

            writeString("AI-Generated Song", 20);

            // Sample 1 Header
            writeString("Sawtooth", 22);
            view.setUint16(offset, sampleData.length / 2, false); offset += 2;
            view.setUint8(offset, 0); offset++; // Finetune
            view.setUint8(offset, 64); offset++; // Volume
            view.setUint16(offset, 0, false); offset += 2; // Loop start
            view.setUint16(offset, sampleData.length / 2, false); offset += 2; // Loop length

            // 30 empty sample headers
            for (let i = 0; i < 30; i++) {
                offset += 30;
            }

            view.setUint8(offset++, songLength);
            view.setUint8(offset++, 127); // Compatibility byte
            
            // Pattern order table
            for (let i = 0; i < 128; i++) {
                view.setUint8(offset + i, i < songLength ? 0 : 0);
            }
            offset += 128;

            writeString("M.K.", 4);

            // Pattern Data
            for (let row = 0; row < numRows; row++) {
                const note = patternData.find(n => n.row === row);
                for (let ch = 0; ch < numChannels; ch++) {
                    const data = (ch === 0) ? note : null;
                    const period = data?.period || 0;
                    const sampleNum = data?.instrument || 0;
                    
                    let effectCmd = 0;
                    let effectParam = 0;

                    if (period === 0 && sampleNum === 0 && data) {
                        effectCmd = 0x0C;
                        effectParam = 0x00;
                    }
                    
                    const byte1 = ((sampleNum & 0xF0) >> 4) | ((period >> 8) & 0x0F);
                    const byte2 = period & 0xFF;
                    const byte3 = ((sampleNum & 0x0F) << 4) | (effectCmd & 0x0F);
                    const byte4 = effectParam;

                    view.setUint8(offset++, byte1);
                    view.setUint8(offset++, byte2);
                    view.setUint8(offset++, byte3);
                    view.setUint8(offset++, byte4);
                }
            }

            sampleData.forEach(byte => view.setInt8(offset++, byte));

            if (offset !== totalSize) {
                console.error(`MOD file size mismatch! Calculated: ${totalSize}, Written: ${offset}`);
                return null;
            }

            return buffer;
        }
    </script>
</body>
</html>
