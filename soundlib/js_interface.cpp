/**
 * js_interface.cpp
 * (File saving is temporarily disabled in this version)
 */

#include "common/stdafx.h"
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstring> 

// Core libopenmpt headers
#include "soundlib/Sndfile.h"
#include "soundlib/mod_specifications.h"
#include "soundlib/ModInstrument.h"
#include "soundlib/ModSample.h"
#include "soundlib/pattern.h"
#include "soundlib/patternContainer.h"
#include "soundlib/XMTools.h"

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

// --- Emscripten JavaScript Download Function ---
// (We keep this here, but we won't call it)
EM_JS(void, download_file, (const char* filename, const char* mime_type, const void* buffer, size_t buffer_size), {
  var js_filename = UTF8ToString(filename);
  var js_mime_type = UTF8ToString(mime_type);
  var blob = new Blob([new Uint8Array(Module.HEAPU8.buffer, buffer, buffer_size)], { type: js_mime_type });
  var a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = js_filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
});


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
        
        // --- (All your instrument, sample, and pattern creation code goes here...) ---
        // ... (omitted for brevity, your existing code is correct)
        
        // --- Save to Memory ---
        // SAVING IS DISABLED FOR THIS BUILD
        /*
        std::stringstream memStream;
        if(!XMTools::Save(sndFile, memStream))
        {
            return {}; // Saving failed
        }
        std::string const& s = memStream.str();
        return std::vector<char>(s.begin(), s.end());
        */
       
        return {}; // Return empty vector because saving is off

    } catch (const json::exception& e) {
        return {};
    }
    
    return {}; // Return empty vector on failure
}

/**
 * @brief Wrapper function to be called from JS.
 * Generates the module and triggers a download.
 * (DOWNLOAD IS DISABLED IN THIS BUILD)
 */
static void GenerateAndDownloadModule(const std::string &json_string, const std::string &filename) {
    
    std::vector<char> module_data = CreateModuleFromJSON(json_string);
    
    /*
    // DOWNLOAD IS DISABLED
    if (!module_data.empty()) {
        // Use the EM_JS function to trigger the download
        download_file(filename.c_str(), "audio/x-mod", module_data.data(), module_data.size());
    }
    */
}

// --- Emscripten Bindings ---
EMSCRIPTEN_BINDINGS(my_module_exporter) {
    // Expose the wrapper function to JavaScript
    // It will compile and run, but will not trigger a download
    function("GenerateAndDownloadModule", &GenerateAndDownloadModule);
}
