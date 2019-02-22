#include <string>
#include <cstring>
#include <algorithm>
#include "antenna.hpp"

using ngpt::ReceiverAntenna;
using ngpt::SatelliteAntenna;
using ngpt::antenna_details::antenna_model_max_chars;
using ngpt::antenna_details::antenna_radome_max_chars;
using ngpt::antenna_details::antenna_serial_max_chars;
using ngpt::antenna_details::antenna_full_max_chars;
using ngpt::antenna_details::satellite_antenna_max_chars;

/// The 'NONE' radome as a c-string.
constexpr char none_radome[] = "NONE";

/// Set model+radome to " " (aka all whitespace characters). Serial is set to
/// nothing, i.e. the first of twenty chars is set to '\0'
void
ReceiverAntenna::initialize()
noexcept
{ 
  constexpr std::size_t mpr_sz = antenna_model_max_chars
                                 +1
                                 +antenna_radome_max_chars;
  std::memset(__name, ' ', mpr_sz*sizeof(char));
  __name[mpr_sz] = '\0';
}

/// Set radome type to none, aka 'NONE'.
void
ReceiverAntenna::set_none_radome()
noexcept
{
  constexpr std::size_t start_idx = antenna_model_max_chars+1;
	std::memcpy(__name+start_idx, none_radome,
              antenna_radome_max_chars*sizeof(char));
}

/// Check if the instance's radome type is empty, aka is "	  "
/// @return true if radome is empty; false otherwise
bool
ReceiverAntenna::radome_is_empty()
const noexcept
{
  constexpr std::size_t from = antenna_model_max_chars;
  constexpr std::size_t to   = antenna_model_max_chars+antenna_radome_max_chars;
	for (std::size_t i = from; i < to; i++) {
		if ( __name[i] != ' ') return false;
	}
	return true;
}

/// @details	Set an antenna/radome pair from a c-string. The input 
///				    antenna/radome pair, should follow the conventions in 
///				    rcvr_ant. If the input string has no radome, then it will be set
///           to 'NONE'. Note that the serial number will be left as is.
/// 
/// @param[in] c A c-string representing a valid antenna model name.
/// 
/// @note		  At most 20 characters will be copied; any remaining characters 
///           will be ignored.
void
ReceiverAntenna::copy_from_cstr(const char* c)
noexcept
{
	// get the size of the input string.
	std::size_t ant_size = std::strlen(c);

  // max chars to copy would be:
  constexpr std::size_t max_sx = antenna_model_max_chars
                                +antenna_radome_max_chars
                                +1;

	std::memcpy(__name, c, sizeof(char)*std::min(ant_size, max_sz) );
	if (radome_is_empty()) set_none_radome();
}

/// Constructor from antenna type. At maximum 20 chars are copied from the 
/// input string. If radome is empty (i.e. '    ') it will be set to 'NONE'.
/// Serial will be left uninitialized.
///
/// @param[in] c A c-string representing an antenna as described in rcvr_ant,
///              i.e. a string containing antenna type and radome.
ReceiverAntenna::ReceiverAntenna(const char* c)
noexcept
{this->copy_from_cstr(c);}

/// Constructor from antenna type. At maximum 20 chars are copied from the 
/// input string. If radome is empty (i.e. '    ') it will be set to 'NONE'.
/// Serial will be left uninitialized.
///
/// @param[in] c A c-string representing an antenna as described in rcvr_ant,
///              i.e. a string containing antenna type and radome.
ReceiverAntenna::ReceiverAntenna(const std::string& s)
noexcept
{this->copy_from_str(s.c_str());}

/// This function compares two Antenna instances and return true if and only
/// if the Antenna is the same, i.e. they share the same model, radome and
/// serial number.
///
/// @param[in] rhs	Antenna instance to check against
/// @return			    Returns true if the calling instance and the passed in
///					        instance are the same antenna, i.e. they share the same 
///					        model, radome and serial number.
bool
ReceiverAntenna::is_same(const ReceiverAntenna& rhs)
const noexcept
{ return !std::strncmp(__name, rhs.__name, antenna_full_max_chars); }

/// This function compares two Antenna instances and return true if and only
/// if the Antenna is of the same model+radome, i.e. they share the same model
/// and radome, disregarding any serial numbers.
///
/// @param[in] rhs	Antenna instance to check against
/// @return			    Returns true if the calling instance and the passed in
///					        instance are identical antennas, i.e. they share the same 
///					         model and radome.
bool
ReceiverAntenna::compare_model(const ReceiverAntenna& rhs)
const noexcept
{
	constexpr auto sz = antenna_model_max_chars + 1 + antenna_radome_max_chars;
	return !std::strncmp(__name, rhs.__name, sz);
}

/// Compare the antenna's serial number to the given c-string
/// @param[in] c The serial to check against
/// @return		   True if the serials match exactly; false otherwise
bool
ReceiverAntenna::compare_serial(const char* c)
const noexcept
{
  constexpr auto idx = antenna_model_max_chars +1 +antenna_radome_max_chars;
	return !std::strncmp(__name+idx, c, antenna_serial_max_chars);
}

/// Check if the Antenna instance has a serial number; that is, any of the
/// characters at the indexes of the serial (typically between 20 and 40) is
/// not a whistespace, or if the first serial char is not the null-terminating
/// char.
/// @return true if the antenna has a serial number (can be just one character)
///			         or false otherwise.
bool
ReceiverAntenna::has_serial()
const noexcept
{
	constexpr auto idx = antenna_model_max_chars+1+antenna_radome_max_chars;
  if (__name[idx]) {
    while (__name[idx]) {
      if (__name[idx] != ' ') {
        return true;
      }
      ++i;
    }
  }
	return false;
}

/// Set the antenna's serial number from a c-string; serial numbers can
/// include any character, but must be at maximum 20 chars long.
/// @param[in] c A c-string containing a serial number; note that the serial
///				       of the calling instance will be set to exactly c; i.e. any
///				       leading or trailing whitespace characters will be copied.
void
ReceiverAntenna::set_serial_nr(const char* c)
noexcept
{
	constexpr std::size_t start_idx = antenna_model_max_chars
                                   +antenna_radome_max_chars
                                   +1;
	std::memset(__name+start_idx, '\0', antenna_serial_max_chars);
	std::memcpy(__name+start_idx, c, sizeof(char) * 
						std::min(std::strlen(c), antenna_serial_max_chars));

	return;
}

/// Copy the satellite antenna given in the input c-string. Note that
/// at maximum 20 characters will be copied.
/// @param[in] c A c-string describing a satellite antenna <= 20 chars long
void
SatelliteAntenna::copy_from_cstr(const char* c)
noexcept
{
  constexpr std::size_t ant_size (std::min(std::strlen(c),
                                  satellite_antenna_max_chars));
  std::memcpy(__name, c, sizeof(char)*ant_size);
  __name[ant_size] = '\0';
}

SatelliteAntenna::SatelliteAntenna(const char* c) noexcept
{ copy_from_cstr(c); }

int
SatelliteAntenna::compare(const char* c) const noexcept
{ return std::strncmp(__name, c, satellite_antenna_max_chars); }

/// Set the model name to nothing, that is to 20 null terminating chars
void
SatelliteAntenna::initialize() noexcept
{
  constexpr std::size_t sz = satellite_antenna_max_chars+1;
  std::memset(__name, '\0', sz);
}
