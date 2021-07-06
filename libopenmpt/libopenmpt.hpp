#ifndef LIBOPENMPT_HPP
#define LIBOPENMPT_HPP
#include "libopenmpt_config.h"
#include <exception>
#include <iosfwd>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <cstddef>
#include <cstdint>
namespace openmpt {
#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4275 )
#endif
class LIBOPENMPT_CXX_API exception
: public std::exception {
private:
char *text;
public:
exception(const std::string &text)
noexcept;
exception(const exception &other)
noexcept;
exception( exception
&& other )
noexcept;
exception &operator=(const exception &other)
noexcept;
exception &operator=(exception && other)
noexcept;
virtual ~
exception()
noexcept;
const char *what() const
noexcept override;
};  // class exception
#if defined( _MSC_VER )
#pragma warning( pop )
#endif
LIBOPENMPT_CXX_API std::uint32_t
get_library_version();
LIBOPENMPT_CXX_API std::uint32_t
get_core_version();
namespace string {
static const char library_version
LIBOPENMPT_ATTR_DEPRECATED[] = "library_version";
static const char library_features
LIBOPENMPT_ATTR_DEPRECATED[] = "library_features";
static const char core_version
LIBOPENMPT_ATTR_DEPRECATED[] = "core_version";
static const char build
LIBOPENMPT_ATTR_DEPRECATED[] = "build";
static const char credits
LIBOPENMPT_ATTR_DEPRECATED[] = "credits";
static const char contact
LIBOPENMPT_ATTR_DEPRECATED[] = "contact";
static const char license
LIBOPENMPT_ATTR_DEPRECATED[] = "license";
LIBOPENMPT_CXX_API std::string
get(const std::string &key);
}  // namespace string
LIBOPENMPT_CXX_API std::vector<std::string>
get_supported_extensions();
LIBOPENMPT_ATTR_DEPRECATED LIBOPENMPT_CXX_API
bool is_extension_supported(const std::string &extension);
LIBOPENMPT_CXX_API bool is_extension_supported2(std::string_view extension);
LIBOPENMPT_CXX_API double
could_open_probability(std::istream &stream, double effort = 1.0, std::ostream &log = std::clog);
LIBOPENMPT_ATTR_DEPRECATED LIBOPENMPT_CXX_API
double could_open_propability(std::istream &stream, double effort = 1.0, std::ostream &log = std::clog);
LIBOPENMPT_CXX_API std::size_t
probe_file_header_get_recommended_size();
static const std::uint64_t probe_file_header_flags_modules
LIBOPENMPT_ATTR_DEPRECATED = 0x1ull;
static const std::uint64_t probe_file_header_flags_containers
LIBOPENMPT_ATTR_DEPRECATED = 0x2ull;
static const std::uint64_t probe_file_header_flags_default
LIBOPENMPT_ATTR_DEPRECATED = 0x1ull | 0x2ull;
static const std::uint64_t probe_file_header_flags_none
LIBOPENMPT_ATTR_DEPRECATED = 0x0ull;
enum probe_file_header_flags : std::uint64_t {
probe_file_header_flags_modules2 = 0x1ull,
probe_file_header_flags_containers2 = 0x2ull,
probe_file_header_flags_default2 = probe_file_header_flags_modules2 | probe_file_header_flags_containers2,
probe_file_header_flags_none2 = 0x0ull
};
enum probe_file_header_result {
probe_file_header_result_success = 1,
probe_file_header_result_failure = 0,
probe_file_header_result_wantmoredata = -1
};
LIBOPENMPT_CXX_API int
probe_file_header(std::uint64_t flags, const std::byte *data, std::size_t size, std::uint64_t filesize);
LIBOPENMPT_CXX_API int
probe_file_header(std::uint64_t flags, const std::uint8_t *data, std::size_t size, std::uint64_t filesize);
LIBOPENMPT_CXX_API int probe_file_header(std::uint64_t flags, const std::byte *data, std::size_t size);
LIBOPENMPT_CXX_API int probe_file_header(std::uint64_t flags, const std::uint8_t *data, std::size_t size);
LIBOPENMPT_CXX_API int probe_file_header(std::uint64_t flags, std::istream &stream);
class module_impl;
class module_ext;
namespace detail {
typedef std::map <std::string, std::string> initial_ctls_map;
}  // namespace detail
class LIBOPENMPT_CXX_API module{
        friend class module_ext;
        public:
        enum render_param
        {
        RENDER_MASTERGAIN_MILLIBEL = 1,
        RENDER_STEREOSEPARATION_PERCENT = 2,
        RENDER_INTERPOLATIONFILTER_LENGTH = 3,
        RENDER_VOLUMERAMPING_STRENGTH = 4
        };
        enum command_index
        {
        command_note = 0,
        command_instrument = 1,
        command_volumeffect = 2,
        command_effect = 3,
        command_volume = 4,
        command_parameter = 5
        };
        private:
        module_impl * impl;
        private:
        module( const module & );
        void operator=( const module & );
        private:
        module();
        void set_impl( module_impl * i );
        public:
        module( std::istream & stream, std::ostream & log = std::clog, const std::map<std::string, std::string> & ctls = detail::initial_ctls_map());
        module( const std::vector<std::byte> & data, std::ostream & log = std::clog, const std::map<std::string, std::string> & ctls = detail::initial_ctls_map());
        module( const std::byte * beg, const std::byte * end, std::ostream & log = std::clog, const std::map<std::string, std::string> & ctls = detail::initial_ctls_map());
        module( const std::byte * data, std::size_t size, std::ostream & log = std::clog, const std::map<std::string, std::string> & ctls = detail::initial_ctls_map());
        module( const std::vector<std::uint8_t> & data, std::ostream & log = std::clog, const std::map<std::string, std::string> & ctls = detail::initial_ctls_map());
        module( const std::uint8_t * beg, const std::uint8_t * end, std::ostream & log = std::clog, const std::map<std::string, std::string> & ctls = detail::initial_ctls_map());
        module( const std::uint8_t * data, std::size_t size, std::ostream & log = std::clog, const std::map<std::string, std::string> & ctls = detail::initial_ctls_map());
        module( const std::vector<char> & data, std::ostream & log = std::clog, const std::map<std::string, std::string> & ctls = detail::initial_ctls_map());
        module( const char * beg, const char * end, std::ostream & log = std::clog, const std::map<std::string, std::string> & ctls = detail::initial_ctls_map());
        module( const char * data, std::size_t size, std::ostream & log = std::clog, const std::map<std::string, std::string> & ctls = detail::initial_ctls_map());
        module( const void * data, std::size_t size, std::ostream & log = std::clog, const std::map<std::string, std::string> & ctls = detail::initial_ctls_map());
        virtual ~module();
        public:
        void select_subsong( std::int32_t subsong );
        std::int32_t get_selected_subsong() const;
        void set_repeat_count( std::int32_t repeat_count );
        std::int32_t get_repeat_count() const;
        double get_duration_seconds() const;
        double set_position_seconds( double seconds );
        double get_position_seconds() const;
        double set_position_order_row( std::int32_t order, std::int32_t row );
        std::int32_t get_render_param( int param ) const;
        void set_render_param( int param, std::int32_t value );
        std::size_t read( std::int32_t samplerate, std::size_t count, std::int16_t * mono );
        std::size_t read( std::int32_t samplerate, std::size_t count, std::int16_t * left, std::int16_t * right );
        std::size_t read( std::int32_t samplerate, std::size_t count, std::int16_t * left, std::int16_t * right, std::int16_t * rear_left, std::int16_t * rear_right );
        std::size_t read( std::int32_t samplerate, std::size_t count, float * mono );
        std::size_t read( std::int32_t samplerate, std::size_t count, float * left, float * right );
        std::size_t read( std::int32_t samplerate, std::size_t count, float * left, float * right, float * rear_left, float * rear_right );
        std::size_t read_interleaved_stereo( std::int32_t samplerate, std::size_t count, std::int16_t * interleaved_stereo );
        std::size_t read_interleaved_quad( std::int32_t samplerate, std::size_t count, std::int16_t * interleaved_quad );
        std::size_t read_interleaved_stereo( std::int32_t samplerate, std::size_t count, float * interleaved_stereo );
        std::size_t read_interleaved_quad( std::int32_t samplerate, std::size_t count, float * interleaved_quad );
        std::vector<std::string> get_metadata_keys() const;
        std::string get_metadata( const std::string & key ) const;
        double get_current_estimated_bpm() const;
        std::int32_t get_current_speed() const;
        std::int32_t get_current_tempo() const;
        std::int32_t get_current_order() const;
        std::int32_t get_current_pattern() const;
        std::int32_t get_current_row() const;
        std::int32_t get_current_playing_channels() const;
        float get_current_channel_vu_mono( std::int32_t channel ) const;
        float get_current_channel_vu_left( std::int32_t channel ) const;
        float get_current_channel_vu_right( std::int32_t channel ) const;
        float get_current_channel_vu_rear_left( std::int32_t channel ) const;
        float get_current_channel_vu_rear_right( std::int32_t channel ) const;
        std::int32_t get_num_subsongs() const;
        std::int32_t get_num_channels() const;
        std::int32_t get_num_orders() const;
        std::int32_t get_num_patterns() const;
        std::int32_t get_num_instruments() const;
        std::int32_t get_num_samples() const;
        std::vector<std::string> get_subsong_names() const;
        std::vector<std::string> get_channel_names() const;
        std::vector<std::string> get_order_names() const;
        std::vector<std::string> get_pattern_names() const;
        std::vector<std::string> get_instrument_names() const;
        std::vector<std::string> get_sample_names() const;
        std::int32_t get_order_pattern( std::int32_t order ) const;
        std::int32_t get_pattern_num_rows( std::int32_t pattern ) const;
        std::uint8_t get_pattern_row_channel_command( std::int32_t pattern, std::int32_t row, std::int32_t channel, int command ) const;
        std::string format_pattern_row_channel_command( std::int32_t pattern, std::int32_t row, std::int32_t channel, int command ) const;
        std::string highlight_pattern_row_channel_command( std::int32_t pattern, std::int32_t row, std::int32_t channel, int command ) const;
        std::string format_pattern_row_channel( std::int32_t pattern, std::int32_t row, std::int32_t channel, std::size_t width = 0, bool pad = true ) const;
        std::string highlight_pattern_row_channel( std::int32_t pattern, std::int32_t row, std::int32_t channel, std::size_t width = 0, bool pad = true ) const;
        std::vector<std::string> get_ctls() const;
        LIBOPENMPT_ATTR_DEPRECATED std::string ctl_get( const std::string & ctl ) const;
        bool ctl_get_boolean( std::string_view ctl ) const;
        std::int64_t ctl_get_integer( std::string_view ctl ) const;
        double ctl_get_floatingpoint( std::string_view ctl ) const;
        std::string ctl_get_text( std::string_view ctl ) const;
        LIBOPENMPT_ATTR_DEPRECATED void ctl_set( const std::string & ctl, const std::string & value );
        void ctl_set_boolean( std::string_view ctl, bool value );
        void ctl_set_integer( std::string_view ctl, std::int64_t value );
        void ctl_set_floatingpoint( std::string_view ctl, double value );
        void ctl_set_text( std::string_view ctl, std::string_view value );
};  // class module
}  // namespace openmpt
#endif  // LIBOPENMPT_HPP