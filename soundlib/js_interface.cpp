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
std::vector<char> CreateModuleFromJSON(const std::string &json_string) {
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
                        
                        // Map all notes to this one sample for this instrument
                        for(size_t note = 0; note < NOTE_MAX; ++note) {
                            pInst->Keyboard[note] = sampleIndex;
                        }

                        ModSample &sample = sndFile.GetSample(sampleIndex);
                        sample.filename = sample_json.value("name", "Sample");
                        
                        std::vector<int8_t> sample_data = sample_json["data"].get<std::vector<int8_t>>();
                        
                        if (!sample_data.empty()) {
                            sndFile.ReadSampleFromMemory(sampleIndex, reinterpret_cast<const char*>(sample_data.data()), sample_data.size());
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
                        m->note = note_json.value("note", (uint8)0);
                        m->instr = note_json.value("instrument", (uint8)0);
                        m->volcmd = VOLCMD_VOLUME;
                        m->vol = note_json.value("volume", (uint8)0);
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
        // MPT_LOG_GLOBAL(LogWarning, "JSON", "JSON parsing error: " + std::string(e.what()));
        return {};
    }
}

// --- Emscripten Bindings ---
EMSCRIPTEN_BINDINGS(my_module_exporter) {
    function("CreateModuleFromJSON", &CreateModuleFromJSON);
    register_vector<char>("VectorChar");
}
```

---

### 2. The Updated JavaScript Generator Page

This page now creates a JSON object suitable for the `.xm` format, using standard note numbers.


```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AI Music - Pattern Editor (XM Format)</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
        body { font-family: 'Inter', sans-serif; }
    </style>
</head>
<body class="bg-gray-900 text-gray-200 flex flex-col items-center justify-center min-h-screen p-4 space-y-8">

    <!-- Control Panel -->
    <div class="w-full max-w-2xl bg-gray-800 rounded-2xl shadow-2xl p-8">
        <h1 class="text-3xl font-bold text-white mb-4 text-center">Pattern Control Panel (XM)</h1>
        
        <div class="bg-gray-700 p-4 rounded-lg mb-4">
            <h2 class="text-xl font-semibold mb-2">Live Pattern Data (with Key Off):</h2>
            <pre id="pattern-display" class="bg-gray-900 text-green-400 p-2 rounded h-32 overflow-y-auto text-xs"></pre>
        </div>

        <div class="grid grid-cols-3 gap-4 mb-4">
            <div>
                <label for="note-select" class="block text-sm font-medium text-gray-300">Note</label>
                <select id="note-select" class="mt-1 block w-full bg-gray-700 border-gray-600 rounded-md shadow-sm py-2 px-3 focus:outline-none focus:ring-violet-500 focus:border-violet-500">
                    <option value="49">C-4</option>
                    <option value="51">D-4</option>
                    <option value="53">E-4</option>
                    <option value="54">F-4</option>
                    <option value="56">G-4</option>
                    <option value="58">A-4</option>
                    <option value="60">B-4</option>
                    <option value="61">C-5</option>
                </select>
            </div>
            <div>
                <label for="instrument-input" class="block text-sm font-medium text-gray-300">Instrument</label>
                <input type="number" id="instrument-input" value="1" min="1" max="128" class="mt-1 block w-full bg-gray-700 border-gray-600 rounded-md shadow-sm py-2 px-3 focus:outline-none focus:ring-violet-500 focus:border-violet-500">
            </div>
            <div>
                <label for="row-input" class="block text-sm font-medium text-gray-300">Row (0-63)</label>
                <input type="number" id="row-input" value="0" min="0" max="63" class="mt-1 block w-full bg-gray-700 border-gray-600 rounded-md shadow-sm py-2 px-3 focus:outline-none focus:ring-violet-500 focus:border-violet-500">
            </div>
        </div>
        <button id="add-note-button" class="w-full bg-blue-600 hover:bg-blue-700 text-white font-bold py-2 px-4 rounded-lg mb-4">Add/Replace Note</button>
        
        <div class="flex justify-center space-x-4">
            <button id="generate-and-send-button" class="w-full bg-violet-600 hover:bg-violet-700 text-white font-bold py-3 px-4 rounded-lg text-xl">
                Generate & Broadcast to Player
            </button>
            <a id="download-button" class="w-full bg-gray-600 hover:bg-gray-700 text-white font-bold py-3 px-4 rounded-lg text-xl text-center pointer-events-none opacity-50">
                Download .XM
            </a>
        </div>
    </div>

    <script>
        'use strict';
        
        let pattern = []; // Use a simple array of note objects
        const channel = new BroadcastChannel('ai-music-channel');
        let currentSongBufferUrl = null;

        const noteNames = { 49: 'C-4', 51: 'D-4', 53: 'E-4', 54: 'F-4', 56: 'G-4', 58: 'A-4', 60: 'B-4', 61: 'C-5', 97: 'Key Off' };

        function updatePatternDisplay() {
            const display = document.getElementById('pattern-display');
            let text = '';
            for(let i=0; i<64; i++) {
                const note = pattern.find(n => n.row === i);
                const rowStr = `Row ${String(i).padStart(2, '0')}:`;
                if (note) {
                    text += `${rowStr} Note ${noteNames[note.note] || note.note}, Inst=${note.instrument || ''}\n`;
                } else {
                    text += `${rowStr} ---\n`;
                }
            }
            display.textContent = text;
        }
        
        function prefillPattern() {
            pattern = [
                { row: 0, channel: 0, note: 49, instrument: 1, volume: 64 },
                { row: 2, channel: 0, note: 97 }, // Key Off
                { row: 4, channel: 0, note: 53, instrument: 1, volume: 64 },
                { row: 6, channel: 0, note: 97 }, // Key Off
                { row: 8, channel: 0, note: 56, instrument: 1, volume: 64 },
                { row: 10, channel: 0, note: 97 }, // Key Off
                { row: 12, channel: 0, note: 61, instrument: 1, volume: 64 },
                { row: 14, channel: 0, note: 97 }, // Key Off
            ];
        }

        document.getElementById('add-note-button').addEventListener('click', () => {
            const row = parseInt(document.getElementById('row-input').value, 10);
            const note = parseInt(document.getElementById('note-select').value, 10);
            const instrument = parseInt(document.getElementById('instrument-input').value, 10);
            const keyOffRow = row + 2;

            if (row >= 0 && row < 64) {
                // Remove any existing note at this row
                pattern = pattern.filter(n => n.row !== row && n.row !== keyOffRow);
                
                pattern.push({ row, channel: 0, note, instrument, volume: 64 });
                if (keyOffRow < 64) {
                    pattern.push({ row: keyOffRow, channel: 0, note: 97 }); // Key Off note
                }
                pattern.sort((a, b) => a.row - b.row); // Keep it sorted
                updatePatternDisplay();
            }
        });
        
        document.getElementById('generate-and-send-button').addEventListener('click', () => {
            // This is a placeholder for your compiled module.
            // When your compile is ready, you'll replace this with the real call.
            alert("This button will work once your custom libopenmpt is compiled and loaded on this page.");
            
            // --- This is the code you will use with your compiled module ---
            /*
            const sampleDataArray = Array.from(new Int8Array(4096).map((_, i) => (i % 256) / 256 * 255 - 128));

            const songDescription = {
                speed: 6,
                tempo: 125,
                patternOrder: [0, 0, 0, 0, 0, 0, 0, 0],
                instruments: [
                    {
                        name: "Sawtooth",
                        sample: {
                            name: "Saw Sample",
                            data: sampleDataArray,
                            loop: true,
                            loopStart: 0,
                            loopEnd: 4096,
                            volume: 256,
                            pan: 128
                        }
                    }
                ],
                patterns: [ { rows: 64, data: pattern } ]
            };

            const songJSON = JSON.stringify(songDescription);
            
            // Assumes your compiled module is available as 'Module'
            const xmFileBuffer = Module.CreateModuleFromJSON(songJSON);
            
            if (xmFileBuffer && xmFileBuffer.length > 0) {
                channel.postMessage({ type: 'playSong', buffer: xmFileBuffer.buffer });

                const downloadButton = document.getElementById('download-button');
                if (currentSongBufferUrl) URL.revokeObjectURL(currentSongBufferUrl);
                const blob = new Blob([xmFileBuffer], { type: 'audio/xm' });
                currentSongBufferUrl = URL.createObjectURL(blob);
                downloadButton.href = currentSongBufferUrl;
                downloadButton.download = 'ai-generated-song.xm';
                downloadButton.classList.remove('opacity-50', 'pointer-events-none');
            } else {
                console.error("Failed to generate song from C++ module.");
            }
            */
        });
        
        prefillPattern();
        updatePatternDisplay();
    </script>
</body>
</html>
