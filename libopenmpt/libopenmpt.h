#ifndef LIBOPENMPT_H
#define LIBOPENMPT_H
#include "libopenmpt_config.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
LIBOPENMPT_API uint32_t openmpt_get_library_version(void);
LIBOPENMPT_API uint32_t openmpt_get_core_version(void);
#define OPENMPT_STRING_LIBRARY_VERSION  LIBOPENMPT_DEPRECATED_STRING( "library_version" )
#define OPENMPT_STRING_LIBRARY_FEATURES LIBOPENMPT_DEPRECATED_STRING( "library_features" )
#define OPENMPT_STRING_CORE_VERSION     LIBOPENMPT_DEPRECATED_STRING( "core_version" )
#define OPENMPT_STRING_BUILD            LIBOPENMPT_DEPRECATED_STRING( "build" )
#define OPENMPT_STRING_CREDITS          LIBOPENMPT_DEPRECATED_STRING( "credits" )
#define OPENMPT_STRING_CONTACT          LIBOPENMPT_DEPRECATED_STRING( "contact" )
#define OPENMPT_STRING_LICENSE          LIBOPENMPT_DEPRECATED_STRING( "license" )
LIBOPENMPT_API void openmpt_free_string(const char *str);
LIBOPENMPT_API const char *openmpt_get_string(const char *key);
LIBOPENMPT_API const char *openmpt_get_supported_extensions(void);
LIBOPENMPT_API int openmpt_is_extension_supported(const char *extension);
#define OPENMPT_STREAM_SEEK_SET 0
#define OPENMPT_STREAM_SEEK_CUR 1
#define OPENMPT_STREAM_SEEK_END 2
typedef size_t ( *openmpt_stream_read_func )(void *stream, void *dst, size_t bytes);
typedef int ( *openmpt_stream_seek_func )(void *stream, int64_t offset, int whence);
typedef int64_t ( *openmpt_stream_tell_func )(void *stream);
typedef struct openmpt_stream_callbacks {
openmpt_stream_read_func read;
openmpt_stream_seek_func seek;
openmpt_stream_tell_func tell;
} openmpt_stream_callbacks;
typedef void ( *openmpt_log_func )(const char *message, void *user);
LIBOPENMPT_API void openmpt_log_func_default(const char *message, void *user);
LIBOPENMPT_API void openmpt_log_func_silent(const char *message, void *user);
#define OPENMPT_ERROR_OK                     0
#define OPENMPT_ERROR_BASE                   256
#define OPENMPT_ERROR_UNKNOWN                ( OPENMPT_ERROR_BASE + 1 )
#define OPENMPT_ERROR_EXCEPTION              ( OPENMPT_ERROR_BASE + 11 )
#define OPENMPT_ERROR_OUT_OF_MEMORY          ( OPENMPT_ERROR_BASE + 21 )
#define OPENMPT_ERROR_RUNTIME                ( OPENMPT_ERROR_BASE + 30 )
#define OPENMPT_ERROR_RANGE                  ( OPENMPT_ERROR_BASE + 31 )
#define OPENMPT_ERROR_OVERFLOW               ( OPENMPT_ERROR_BASE + 32 )
#define OPENMPT_ERROR_UNDERFLOW              ( OPENMPT_ERROR_BASE + 33 )
#define OPENMPT_ERROR_LOGIC                  ( OPENMPT_ERROR_BASE + 40 )
#define OPENMPT_ERROR_DOMAIN                 ( OPENMPT_ERROR_BASE + 41 )
#define OPENMPT_ERROR_LENGTH                 ( OPENMPT_ERROR_BASE + 42 )
#define OPENMPT_ERROR_OUT_OF_RANGE           ( OPENMPT_ERROR_BASE + 43 )
#define OPENMPT_ERROR_INVALID_ARGUMENT       ( OPENMPT_ERROR_BASE + 44 )
#define OPENMPT_ERROR_GENERAL                ( OPENMPT_ERROR_BASE + 101 )
#define OPENMPT_ERROR_INVALID_MODULE_POINTER ( OPENMPT_ERROR_BASE + 102 )
#define OPENMPT_ERROR_ARGUMENT_NULL_POINTER  ( OPENMPT_ERROR_BASE + 103 )
LIBOPENMPT_API int openmpt_error_is_transient(int error);
LIBOPENMPT_API const char *openmpt_error_string(int error);
#define OPENMPT_ERROR_FUNC_RESULT_NONE    0
#define OPENMPT_ERROR_FUNC_RESULT_LOG     ( 1 << 0 )
#define OPENMPT_ERROR_FUNC_RESULT_STORE   ( 1 << 1 )
#define OPENMPT_ERROR_FUNC_RESULT_DEFAULT ( OPENMPT_ERROR_FUNC_RESULT_LOG | OPENMPT_ERROR_FUNC_RESULT_STORE )
typedef int ( *openmpt_error_func )(int error, void *user);
LIBOPENMPT_API int openmpt_error_func_default(int error, void *user);
LIBOPENMPT_API int openmpt_error_func_log(int error, void *user);
LIBOPENMPT_API int openmpt_error_func_store(int error, void *user);
LIBOPENMPT_API int openmpt_error_func_ignore(int error, void *user);
LIBOPENMPT_API int openmpt_error_func_errno(int error, void *user);
LIBOPENMPT_API void *openmpt_error_func_errno_userdata(int *error);
LIBOPENMPT_API LIBOPENMPT_DEPRECATED double
openmpt_could_open_probability(openmpt_stream_callbacks stream_callbacks, void *stream, double effort,
                               openmpt_log_func logfunc, void *user);
LIBOPENMPT_API LIBOPENMPT_DEPRECATED double
openmpt_could_open_propability(openmpt_stream_callbacks stream_callbacks, void *stream, double effort,
                               openmpt_log_func logfunc, void *user);
LIBOPENMPT_API double
openmpt_could_open_probability2(openmpt_stream_callbacks stream_callbacks, void *stream, double effort,
                                openmpt_log_func logfunc, void *loguser, openmpt_error_func errfunc, void *erruser,
                                int *error, const char **error_message);
LIBOPENMPT_API size_t openmpt_probe_file_header_get_recommended_size(void);
#define OPENMPT_PROBE_FILE_HEADER_FLAGS_MODULES       0x1ull
#define OPENMPT_PROBE_FILE_HEADER_FLAGS_CONTAINERS    0x2ull
#define OPENMPT_PROBE_FILE_HEADER_FLAGS_DEFAULT       ( OPENMPT_PROBE_FILE_HEADER_FLAGS_MODULES | OPENMPT_PROBE_FILE_HEADER_FLAGS_CONTAINERS )
#define OPENMPT_PROBE_FILE_HEADER_FLAGS_NONE          0x0ull
#define OPENMPT_PROBE_FILE_HEADER_RESULT_SUCCESS      1
#define OPENMPT_PROBE_FILE_HEADER_RESULT_FAILURE      0
#define OPENMPT_PROBE_FILE_HEADER_RESULT_WANTMOREDATA ( -1 )
#define OPENMPT_PROBE_FILE_HEADER_RESULT_ERROR        ( -255 )
LIBOPENMPT_API int
openmpt_probe_file_header(uint64_t flags, const void *data, size_t size, uint64_t filesize, openmpt_log_func logfunc,
                          void *loguser, openmpt_error_func errfunc, void *erruser, int *error,
                          const char **error_message);
LIBOPENMPT_API int
openmpt_probe_file_header_without_filesize(uint64_t flags, const void *data, size_t size, openmpt_log_func logfunc,
                                           void *loguser, openmpt_error_func errfunc, void *erruser, int *error,
                                           const char **error_message);
LIBOPENMPT_API int
openmpt_probe_file_header_from_stream(uint64_t flags, openmpt_stream_callbacks stream_callbacks, void *stream,
                                      openmpt_log_func logfunc, void *loguser, openmpt_error_func errfunc,
                                      void *erruser, int *error, const char **error_message);
typedef struct openmpt_module openmpt_module;
typedef struct openmpt_module_initial_ctl {
const char *ctl;
const char *value;
} openmpt_module_initial_ctl;
LIBOPENMPT_API LIBOPENMPT_DEPRECATED openmpt_module *
openmpt_module_create(openmpt_stream_callbacks stream_callbacks, void *stream, openmpt_log_func logfunc, void *loguser,
                      const openmpt_module_initial_ctl *ctls);
LIBOPENMPT_API openmpt_module *
openmpt_module_create2(openmpt_stream_callbacks stream_callbacks, void *stream, openmpt_log_func logfunc, void *loguser,
                       openmpt_error_func errfunc, void *erruser, int *error, const char **error_message,
                       const openmpt_module_initial_ctl *ctls);
LIBOPENMPT_API LIBOPENMPT_DEPRECATED openmpt_module *
openmpt_module_create_from_memory(const void *filedata, size_t filesize, openmpt_log_func logfunc, void *loguser,
                                  const openmpt_module_initial_ctl *ctls);
LIBOPENMPT_API openmpt_module *
openmpt_module_create_from_memory2(const void *filedata, size_t filesize, openmpt_log_func logfunc, void *loguser,
                                   openmpt_error_func errfunc, void *erruser, int *error, const char **error_message,
                                   const openmpt_module_initial_ctl *ctls);
LIBOPENMPT_API void openmpt_module_destroy(openmpt_module *mod);
LIBOPENMPT_API void openmpt_module_set_log_func(openmpt_module *mod, openmpt_log_func logfunc, void *loguser);
LIBOPENMPT_API void openmpt_module_set_error_func(openmpt_module *mod, openmpt_error_func errfunc, void *erruser);
LIBOPENMPT_API int openmpt_module_error_get_last(openmpt_module *mod);
LIBOPENMPT_API const char *openmpt_module_error_get_last_message(openmpt_module *mod);
LIBOPENMPT_API void openmpt_module_error_set_last(openmpt_module *mod, int error);
LIBOPENMPT_API void openmpt_module_error_clear(openmpt_module *mod);
#define OPENMPT_MODULE_RENDER_MASTERGAIN_MILLIBEL        1
#define OPENMPT_MODULE_RENDER_STEREOSEPARATION_PERCENT   2
#define OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH 3
#define OPENMPT_MODULE_RENDER_VOLUMERAMPING_STRENGTH     4
#define OPENMPT_MODULE_COMMAND_NOTE                      0
#define OPENMPT_MODULE_COMMAND_INSTRUMENT                1
#define OPENMPT_MODULE_COMMAND_VOLUMEEFFECT              2
#define OPENMPT_MODULE_COMMAND_EFFECT                    3
#define OPENMPT_MODULE_COMMAND_VOLUME                    4
#define OPENMPT_MODULE_COMMAND_PARAMETER                 5
LIBOPENMPT_API int openmpt_module_select_subsong(openmpt_module *mod, int32_t subsong);
LIBOPENMPT_API int32_t openmpt_module_get_selected_subsong(openmpt_module *mod);
LIBOPENMPT_API int openmpt_module_set_repeat_count(openmpt_module *mod, int32_t repeat_count);
LIBOPENMPT_API int32_t openmpt_module_get_repeat_count(openmpt_module *mod);
LIBOPENMPT_API double openmpt_module_get_duration_seconds(openmpt_module *mod);
LIBOPENMPT_API double openmpt_module_set_position_seconds(openmpt_module *mod, double seconds);
LIBOPENMPT_API double openmpt_module_get_position_seconds(openmpt_module *mod);
LIBOPENMPT_API double openmpt_module_set_position_order_row(openmpt_module *mod, int32_t order, int32_t row);
LIBOPENMPT_API int openmpt_module_get_render_param(openmpt_module *mod, int param, int32_t *value);
LIBOPENMPT_API int openmpt_module_set_render_param(openmpt_module *mod, int param, int32_t value);
LIBOPENMPT_API size_t openmpt_module_read_mono(openmpt_module *mod, int32_t samplerate, size_t count, int16_t *mono);
LIBOPENMPT_API size_t
openmpt_module_read_stereo(openmpt_module *mod, int32_t samplerate, size_t count, int16_t *left, int16_t *right);
LIBOPENMPT_API size_t
openmpt_module_read_quad(openmpt_module *mod, int32_t samplerate, size_t count, int16_t *left, int16_t *right,
                         int16_t *rear_left, int16_t *rear_right);
LIBOPENMPT_API size_t
openmpt_module_read_float_mono(openmpt_module *mod, int32_t samplerate, size_t count, float *mono);
LIBOPENMPT_API size_t
openmpt_module_read_float_stereo(openmpt_module *mod, int32_t samplerate, size_t count, float *left, float *right);
LIBOPENMPT_API size_t
openmpt_module_read_float_quad(openmpt_module *mod, int32_t samplerate, size_t count, float *left, float *right,
                               float *rear_left, float *rear_right);
LIBOPENMPT_API size_t openmpt_module_read_interleaved_stereo(openmpt_module *mod, int32_t samplerate, size_t count,
                                                             int16_t *interleaved_stereo);
LIBOPENMPT_API size_t
openmpt_module_read_interleaved_quad(openmpt_module *mod, int32_t samplerate, size_t count, int16_t *interleaved_quad);
LIBOPENMPT_API size_t
openmpt_module_read_interleaved_float_stereo(openmpt_module *mod, int32_t samplerate, size_t count,
                                             float *interleaved_stereo);
LIBOPENMPT_API size_t openmpt_module_read_interleaved_float_quad(openmpt_module *mod, int32_t samplerate, size_t count,
                                                                 float *interleaved_quad);
LIBOPENMPT_API const char *openmpt_module_get_metadata_keys(openmpt_module *mod);
LIBOPENMPT_API const char *openmpt_module_get_metadata(openmpt_module *mod, const char *key);
LIBOPENMPT_API double openmpt_module_get_current_estimated_bpm(openmpt_module *mod);
LIBOPENMPT_API int32_t openmpt_module_get_current_speed(openmpt_module *mod);
LIBOPENMPT_API int32_t openmpt_module_get_current_tempo(openmpt_module *mod);
LIBOPENMPT_API int32_t openmpt_module_get_current_order(openmpt_module *mod);
LIBOPENMPT_API int32_t openmpt_module_get_current_pattern(openmpt_module *mod);
LIBOPENMPT_API int32_t openmpt_module_get_current_row(openmpt_module *mod);
LIBOPENMPT_API int32_t openmpt_module_get_current_playing_channels(openmpt_module *mod);
LIBOPENMPT_API float openmpt_module_get_current_channel_vu_mono(openmpt_module *mod, int32_t channel);
LIBOPENMPT_API float openmpt_module_get_current_channel_vu_left(openmpt_module *mod, int32_t channel);
LIBOPENMPT_API float openmpt_module_get_current_channel_vu_right(openmpt_module *mod, int32_t channel);
LIBOPENMPT_API float openmpt_module_get_current_channel_vu_rear_left(openmpt_module *mod, int32_t channel);
LIBOPENMPT_API float openmpt_module_get_current_channel_vu_rear_right(openmpt_module *mod, int32_t channel);
LIBOPENMPT_API int32_t openmpt_module_get_num_subsongs(openmpt_module *mod);
LIBOPENMPT_API int32_t openmpt_module_get_num_channels(openmpt_module *mod);
LIBOPENMPT_API int32_t openmpt_module_get_num_orders(openmpt_module *mod);
LIBOPENMPT_API int32_t openmpt_module_get_num_patterns(openmpt_module *mod);
LIBOPENMPT_API int32_t openmpt_module_get_num_instruments(openmpt_module *mod);
LIBOPENMPT_API int32_t openmpt_module_get_num_samples(openmpt_module *mod);
LIBOPENMPT_API const char *openmpt_module_get_subsong_name(openmpt_module *mod, int32_t index);
LIBOPENMPT_API const char *openmpt_module_get_channel_name(openmpt_module *mod, int32_t index);
LIBOPENMPT_API const char *openmpt_module_get_order_name(openmpt_module *mod, int32_t index);
LIBOPENMPT_API const char *openmpt_module_get_pattern_name(openmpt_module *mod, int32_t index);
LIBOPENMPT_API const char *openmpt_module_get_instrument_name(openmpt_module *mod, int32_t index);
LIBOPENMPT_API const char *openmpt_module_get_sample_name(openmpt_module *mod, int32_t index);
LIBOPENMPT_API int32_t openmpt_module_get_order_pattern(openmpt_module *mod, int32_t order);
LIBOPENMPT_API int32_t openmpt_module_get_pattern_num_rows(openmpt_module *mod, int32_t pattern);
LIBOPENMPT_API uint8_t
openmpt_module_get_pattern_row_channel_command(openmpt_module *mod, int32_t pattern, int32_t row, int32_t channel,
                                               int command);
LIBOPENMPT_API const char *
openmpt_module_format_pattern_row_channel_command(openmpt_module *mod, int32_t pattern, int32_t row, int32_t channel,
                                                  int command);
LIBOPENMPT_API const char *
openmpt_module_highlight_pattern_row_channel_command(openmpt_module *mod, int32_t pattern, int32_t row, int32_t channel,
                                                     int command);
LIBOPENMPT_API const char *
openmpt_module_format_pattern_row_channel(openmpt_module *mod, int32_t pattern, int32_t row, int32_t channel,
                                          size_t width, int pad);
LIBOPENMPT_API const char *
openmpt_module_highlight_pattern_row_channel(openmpt_module *mod, int32_t pattern, int32_t row, int32_t channel,
                                             size_t width, int pad);
LIBOPENMPT_API const char *openmpt_module_get_ctls(openmpt_module *mod);
LIBOPENMPT_API LIBOPENMPT_DEPRECATED const char *openmpt_module_ctl_get(openmpt_module *mod, const char *ctl);
LIBOPENMPT_API int openmpt_module_ctl_get_boolean(openmpt_module *mod, const char *ctl);
LIBOPENMPT_API int64_t openmpt_module_ctl_get_integer(openmpt_module *mod, const char *ctl);
LIBOPENMPT_API double openmpt_module_ctl_get_floatingpoint(openmpt_module *mod, const char *ctl);
LIBOPENMPT_API const char *openmpt_module_ctl_get_text(openmpt_module *mod, const char *ctl);
LIBOPENMPT_API LIBOPENMPT_DEPRECATED int
openmpt_module_ctl_set(openmpt_module *mod, const char *ctl, const char *value);
LIBOPENMPT_API int openmpt_module_ctl_set_boolean(openmpt_module *mod, const char *ctl, int value);
LIBOPENMPT_API int openmpt_module_ctl_set_integer(openmpt_module *mod, const char *ctl, int64_t value);
LIBOPENMPT_API int openmpt_module_ctl_set_floatingpoint(openmpt_module *mod, const char *ctl, double value);
LIBOPENMPT_API int openmpt_module_ctl_set_text(openmpt_module *mod, const char *ctl, const char *value);
#ifdef __cplusplus
}
#endif
#endif /* LIBOPENMPT_H */
