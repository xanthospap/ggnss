#include "navrnx.hpp"
#include "ggdatetime/datetime_read.hpp"
#include "nvarstr.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

using ngpt::NavDataFrame;
using ngpt::NavigationRnx;

/// No header line can have more than 80 chars. However, there are cases when
/// they  exceed this limit, just a bit ...
constexpr int MAX_HEADER_CHARS{85};

/// Max header lines.
constexpr int MAX_HEADER_LINES{1000};

/// Max record characters (for a navigation data block)
constexpr int MAX_RECORD_CHARS{128};

/// Size of 'END OF HEADER' C-string.
/// std::strlen is not 'constexr' so eoh_size can't be one either. Note however
/// that gcc has a builtin constexpr strlen function (if we want to disable this
/// we can do so with -fno-builtin).
#ifdef __clang__
const std::size_t eoh_size{std::strlen("END OF HEADER")};
#else
constexpr std::size_t eoh_size{std::strlen("END OF HEADER")};
#endif

/// @brief Navigation data block lines per satellite system
/// @param[in]  s SATELLITE_SYSTEM
/// @param[out] records_in_last_line Number of records that the last line holds.
/// @return       The number of lines in a nav. RINEX v3.x data block for this
///               satellite system. A negative return value, denotes an error.
int __lines_per_satsys_v3__(ngpt::SATELLITE_SYSTEM s,
                            int &records_in_last_line) noexcept {
  using ngpt::SATELLITE_SYSTEM;

  int lines_to_read = -10;
  switch (s) {
  case SATELLITE_SYSTEM::gps:
  case SATELLITE_SYSTEM::qzss:
  case SATELLITE_SYSTEM::beidou:
    records_in_last_line = 2;
    lines_to_read = 8;
    break;
  case SATELLITE_SYSTEM::galileo:
  case SATELLITE_SYSTEM::irnss:
    records_in_last_line = 1;
    lines_to_read = 8;
    break;
  case SATELLITE_SYSTEM::glonass:
  case SATELLITE_SYSTEM::sbas:
    records_in_last_line = 4;
    lines_to_read = 4;
    break;
  default:
    break;
  }
  return lines_to_read;
}

/// @details: Resolve a Nav. RINEX v3.x data block to a NavDataFrame. The
///           function expect that the first line to be read is:
///           "SV/ EPOCH / SV CLK". Depending on the satellite system (to be
///           resolved from the first line) it will read the respective number
///           of lines and resolve them.
/// @param[in] inp Input file (nav RINEX v3) stream, placed before (aka first
///                line to be read is:) "SV/ EPOCH / SV CLK"
/// @return    Anything other than 0 denotes an error.
/// @todo the line toc__ = ngpt::strptime_ymd_hms<ngpt::seconds>(line+3); may
/// throw! what can i do about this?
int NavDataFrame::set_from_rnx3(std::ifstream &inp) noexcept {
  char line[MAX_RECORD_CHARS];
  char *str_end;

#ifdef DEBUG
  if (!inp.good())
    return 50;
#endif

  // Read the first line.
  // ------------------------------------------------------------
  if (!inp.getline(line, MAX_RECORD_CHARS)) {
    return 1;
  }
  sys__ = ngpt::char_to_satsys(*line);
  toc__ = ngpt::strptime_ymd_hms<ngpt::seconds>(line + 3);
  prn__ = std::strtol(line + 1, &str_end, 10);
  if (!prn__ || errno == ERANGE) {
    errno = 0;
    return 2;
  }
  // replace 'D' or 'd' with 'e' in remaining floats
  __for2cpp__(line + 23);
  if (!__char2double__<3, 19>(line + 23, data__)) {
    errno = 0;
    return 3;
  }

  int last_line_recs = 0;
  int ln = 0;
  int lines_in_block = __lines_per_satsys_v3__(sys__, last_line_recs) - 1;
  if (lines_in_block < 0) {
    return 4;
  }
  // read all but the last line (** all but galileo or beidou **)
  if (sys__ != SATELLITE_SYSTEM::galileo && sys__ != SATELLITE_SYSTEM::beidou) {
    for (ln = 0; ln < lines_in_block - 1; ln++) {
      if (!inp.getline(line, MAX_RECORD_CHARS)) {
        return 5;
      }
      // replace 'D' or 'd' with 'e'
      __for2cpp__(line);
      // read 4 doubles into data__
      if (!__char2double__<4, 19>(line + 4, data__ + 3 + ln * 4)) {
        errno = 0;
        return 6;
      }
    }
  } else { // galileo and beidou have an empty record in line #5
    for (ln = 0; ln < lines_in_block - 1; ln++) {
      if (!inp.getline(line, MAX_RECORD_CHARS)) {
        return 5;
      }
      // replace 'D' or 'd' with 'e'
      __for2cpp__(line);
      // read 4 doubles into data__
      if (ln != 4) {
        if (!__char2double__<4, 19>(line + 4, data__ + 3 + ln * 4)) {
          errno = 0;
          return 6;
        }
      } else {
        if (!__char2double__<3, 19>(line + 4, data__ + 3 + ln * 4)) {
          errno = 0;
          return 6;
        }
      }
    }
  }

  // read last line
  if (!inp.getline(line, MAX_RECORD_CHARS)) {
    return 7;
  }
  // replace 'D' or 'd' with 'e'
  __for2cpp__(line);
  // read remaining last_line_recs doubles into data__
  if (!__char2double__<19>(line + 4, data__ + 3 + ln * 4, last_line_recs)) {
    errno = 0;
    return 8;
  }

  // If we read a GLONASS navigation frame, convert SV state vector to meters
  // (originaly in km)
  if (sys__ == SATELLITE_SYSTEM::glonass) {
    for (int i : {3, 4, 5, 7, 8, 9, 11, 12, 13})
      data__[i] *= 1e3;
  }

  // set ToE
#ifdef DEBUG
  // assert we don't have fractional seconds!
  if (sys__ == SATELLITE_SYSTEM::glonass)
    assert(std::abs(data__[2] - (int)data__[2]) < 1e-15);
  if (sys__ == SATELLITE_SYSTEM::gps || sys__ == SATELLITE_SYSTEM::galileo ||
      sys__ == SATELLITE_SYSTEM::beidou)
    assert(std::abs(data__[11] - (int)data__[11]) < 1e-15);
#endif
  if (sys__ == SATELLITE_SYSTEM::gps || sys__ == SATELLITE_SYSTEM::glonass ||
      sys__ == SATELLITE_SYSTEM::galileo || sys__ == SATELLITE_SYSTEM::beidou) {
    toe__ = this->toe2date<ngpt::seconds>();
  }

  return 0;
}

/// @details NavigationRnx constructor, using a filename. The constructor will
///          initialize (set) the _filename attribute and also (try to)
///          open the input stream (i.e. _istream).
///          If the file is successefuly opened, the constructor will read
///          the header and assign info.
/// @param[in] filename  The filename of the Rinex file
NavigationRnx::NavigationRnx(const char *filename)
    : __filename(filename), __istream(filename, std::ios_base::in),
      __satsys(SATELLITE_SYSTEM::mixed), __version(0e0), __end_of_head(0) {
  int j;
  if ((j = read_header())) {
    if (__istream.is_open())
      __istream.close();
    throw std::runtime_error(
        "[ERROR] Failed to read (nav) RINEX header; Error Code: " +
        std::to_string(j));
  }
}

/// Read a RINEX Navigation v3.x header and assign vital information.
/// The function will read all header lines, stoping after the line:
/// "END OF HEADER"
/// @return  Anything other than 0 denotes an error.
int NavigationRnx::read_header() noexcept {
  char line[MAX_HEADER_CHARS];
  char *str_end;

  // The stream should be open by now!
  if (!__istream.is_open())
    return 1;

  // Go to the top of the file.
  __istream.seekg(0);

  // Read the first line. Get version, data-type and sat. system.
  // ------------------------------------------------------------
  __istream.getline(line, MAX_HEADER_CHARS);
  __version = std::strtof(line, &str_end);
  if (str_end == line)
    return 10; // transformation to float has failed
  if (line[20] != 'N')
    return 11;
  try {
    __satsys = ngpt::char_to_satsys(line[40]);
  } catch (std::runtime_error &e) {
    return 12;
  }

  // Keep on readling lines until 'END OF HEADER'.
  // ----------------------------------------------------
  int dummy_it = 0;
  __istream.getline(line, MAX_HEADER_CHARS);
  while (dummy_it < MAX_HEADER_LINES &&
         std::strncmp(line + 60, "END OF HEADER", eoh_size)) {
    __istream.getline(line, MAX_HEADER_CHARS);
    dummy_it++;
  }
  if (dummy_it >= MAX_HEADER_LINES) {
    return 20;
  }

  // Mark the end of header
  __end_of_head = __istream.tellg();

  // All done !
  return 0;
}

/// @details Read the next nav data block and assign it.
/// param[in] nav A NavDataFrame where the read in RINEX block will be resolved
///               to.
/// @return   < 0 EOF encountered
///           = 0 All ok; block resolved & nav assigned
///           > 0 Error; block not resolved
int NavigationRnx::read_next_record(NavDataFrame &nav) noexcept {
#ifdef DEBUG
  if (!__istream.good())
    return 50;
#endif
  int c;
  if ((c = __istream.peek()) != EOF) {
    return nav.set_from_rnx3(__istream);
  }
  if (__istream.eof())
    return clear_stream(-1);
  return 50;
}

/// @details This function will read and ignore a satellite navigation block.
///          It will "peek" and resolve the satellite system (aka it is expected
///          that the next line to be read in the input stream is
///          "SV / EPOCH / SV CLK", and read the respective number of following
///          lines.
/// @return  An integer is returned, denoting:
///           < 0 EOF encountered; satellite system not resolved
///           = 0 All ok; satellite system resolved
///           > 0 Error; satellite system not resolved
int NavigationRnx::ignore_next_block() noexcept {
#ifdef DEBUG
  if (!__istream.good())
    return 50;
#endif
  char s = __istream.peek();
  if (s == EOF)
    return clear_stream(-1);

  ngpt::SATELLITE_SYSTEM sys;
  try {
    sys = ngpt::char_to_satsys(s); // this may throw
  } catch (std::exception &) {
    return 1;
  }

  char line[MAX_RECORD_CHARS];
  int last_line_recs = 0;
  int lines_in_block = __lines_per_satsys_v3__(sys, last_line_recs);
  // read and skip lines
  for (int ln = 0; ln < lines_in_block; ln++) {
    if (!__istream.getline(line, MAX_RECORD_CHARS)) {
      return 2;
    }
  }

  return 0;
}

/// @details Peak following line (actually the first character) and resolve
///          the satellite system of the block that follows.
/// @param[out] status The function status; this can hold:
///           < 0 EOF encountered; satellite system not resolved
///           = 0 All ok; satellite system resolved
///           > 0 Error; satellite system not resolved
ngpt::SATELLITE_SYSTEM NavigationRnx::peak_satsys(int &status) noexcept {
#ifdef DEBUG
  if (!__istream.good()) {
    status = 10;
    return SATELLITE_SYSTEM::mixed;
  }
#endif
  status = 0;
  char s = __istream.peek();
  if (s == EOF) {
    status = clear_stream(-1);
    return SATELLITE_SYSTEM::mixed;
  }

  try {
    return ngpt::char_to_satsys(s); // this may throw
  } catch (std::exception &) {
    status = 1;
    return SATELLITE_SYSTEM::mixed;
  }
}

/// @details Set the nav RINEX stream to end of header, ready to restart
///          reading nav data blocks
void NavigationRnx::rewind(pos_type pos) noexcept {
  if (pos == pos_type(-1)) {
    __istream.seekg(__end_of_head);
  } else {
    __istream.seekg(pos);
  }
  return;
}

/// @param[out] curpos  Current position (before starting the function) of the
///                     instane's stream; after the execution you can rewing
///                     back to curpos and pretend nothing changed
/// @param[out] frame   NavDataFrame read (if found) that matches the requested
///                     satellite system and prn
/// @param[in]  sys     Requested satellite system
/// @param[in]  prn     Requested prn; if set to -1, it means any satellite of
///                     the requested satellite system
/// @return An integer denoting the following:
///                     * -1 EOF encountered before matching system and sv
///                     *  0 All ok! system and sv matched and frame resolved
///                     * >0 An error occured
int NavigationRnx::find_next(pos_type &curpos, NavDataFrame &frame,
                             SATELLITE_SYSTEM sys, int prn) noexcept {
#ifdef DEBUG
  if (!__istream.good())
    return 50;
#endif
  curpos = __istream.tellg();
  int status = 0, dummy_it = 0;
  SATELLITE_SYSTEM csys;
  do {
    csys = this->peak_satsys(status);
    if (status)
      return status;
    if (csys == sys) {
      if ((status = this->read_next_record(frame)))
        return clear_stream(status);
      if (prn == -1 || (prn == frame.prn()))
        return 0;
    } else {
      if ((status = this->ignore_next_block()))
        return clear_stream(status);
    }
  } while (dummy_it < 5000);

  return 100;
}

/// Clear the instance's stream (failbit and eofbit) and return the input
/// parameter exit_status
/// @param[in] exit_status What the function returns
/// @return The function returns the exit_status parameter
int NavigationRnx::clear_stream(int exit_status) noexcept {
  __istream.clear();
  return exit_status;
}
/*
NavDataFrame&
NavDataFrame::operator=(const NavDataFrame& other) noexcept
{
  if (this!=&other) {
    sys__ = other.sys__;
    prn__ = other.prn__;
    toc__ = other.toc__;
    std::memcpy(data__, other.data__, sizeof(double)*31);
  }
  return *this;
}
*/
