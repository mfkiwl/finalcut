/***********************************************************************
* fkeyboard.cpp - Read keyboard events                                 *
*                                                                      *
* This file is part of the FINAL CUT widget toolkit                    *
*                                                                      *
* Copyright 2018-2022 Markus Gans                                      *
*                                                                      *
* FINAL CUT is free software; you can redistribute it and/or modify    *
* it under the terms of the GNU Lesser General Public License as       *
* published by the Free Software Foundation; either version 3 of       *
* the License, or (at your option) any later version.                  *
*                                                                      *
* FINAL CUT is distributed in the hope that it will be useful, but     *
* WITHOUT ANY WARRANTY; without even the implied warranty of           *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
* GNU Lesser General Public License for more details.                  *
*                                                                      *
* You should have received a copy of the GNU Lesser General Public     *
* License along with this program.  If not, see                        *
* <http://www.gnu.org/licenses/>.                                      *
***********************************************************************/

#include <fcntl.h>
#include <sys/ioctl.h>

#if defined(__CYGWIN__)
  #include <sys/select.h>  // need for FD_ZERO, FD_SET, FD_CLR, ...
#endif

#include <algorithm>
#include <array>
#include <string>

#include "final/fapplication.h"
#include "final/fobject.h"
#include "final/input/fkeyboard.h"
#include "final/input/fkey_map.h"
#include "final/output/tty/fterm.h"
#include "final/output/tty/ftermios.h"

#if defined(__linux__)
  #include "final/output/tty/ftermlinux.h"
#endif

namespace finalcut
{

// static class attributes
uInt64 FKeyboard::key_timeout{100'000};             // 100 ms  (10 Hz)
uInt64 FKeyboard::read_blocking_time{100'000};      // 100 ms  (10 Hz)
uInt64 FKeyboard::read_blocking_time_short{5'000};  //   5 ms (200 Hz)
bool   FKeyboard::non_blocking_input_support{true};
TimeValue FKeyboard::time_keypressed{};


//----------------------------------------------------------------------
// class FKeyboard
//----------------------------------------------------------------------

// constructors and destructor
//----------------------------------------------------------------------
FKeyboard::FKeyboard()
{
  // Initialize keyboard values
  time_keypressed = TimeValue{};  // Set to epoch time

  // Get the stdin file status flags
  stdin_status_flags = fcntl(FTermios::getStdIn(), F_GETFL);

  if ( stdin_status_flags == -1 )
    std::abort();

  // Sort the known key map by string length
  auto& key_map = FKeyMap::getKeyMap();
  std::sort ( key_map.begin(), key_map.end()
            , [] (const auto& lhs, const auto& rhs)
              {
                return lhs.length < rhs.length;
              }
            );
}


// public methods of FKeyboard
//----------------------------------------------------------------------
auto FKeyboard::getInstance() -> FKeyboard&
{
  static const auto& keyboard = std::make_unique<FKeyboard>();
  return *keyboard;
}

//----------------------------------------------------------------------
void FKeyboard::fetchKeyCode()
{
  if ( fkey_queue.size() < MAX_QUEUE_SIZE )
    parseKeyBuffer();
}

//----------------------------------------------------------------------
FString FKeyboard::getKeyName (const FKey keynum) const
{
  const auto& fkeyname = FKeyMap::getKeyName();
  const auto& found_key = std::find_if
  (
    fkeyname.cbegin(),
    fkeyname.cend(),
    [&keynum] (const auto& kn)
    {
      return ( kn.num != FKey::None && kn.num == keynum );
    }
  );

  if ( found_key != fkeyname.end() )
    return {found_key->string};

  if ( keynum > 32 && keynum < 127 )
    return {char(keynum)};

  return {""};
}

//----------------------------------------------------------------------
bool FKeyboard::setNonBlockingInput (bool enable)
{
  if ( enable == non_blocking_stdin )
    return non_blocking_stdin;

  if ( enable )  // make stdin non-blocking
  {
    stdin_status_flags |= O_NONBLOCK;

    if ( fcntl (FTermios::getStdIn(), F_SETFL, stdin_status_flags) != -1 )
      non_blocking_stdin = true;
  }
  else
  {
    stdin_status_flags &= ~O_NONBLOCK;

    if ( fcntl (FTermios::getStdIn(), F_SETFL, stdin_status_flags) != -1 )
      non_blocking_stdin = false;
  }

  return non_blocking_stdin;
}

//----------------------------------------------------------------------
bool FKeyboard::hasUnprocessedInput() const noexcept
{
  return fifo_buf.hasData();
}

//----------------------------------------------------------------------
bool FKeyboard::isKeyPressed (uInt64 blocking_time)
{
  if ( has_pending_input )
    return false;

  fd_set ifds{};
  struct timeval tv{};
  const int stdin_no = FTermios::getStdIn();

  FD_ZERO(&ifds);
  FD_SET(stdin_no, &ifds);
  tv.tv_sec = tv.tv_usec = 0;  // Non-blocking input

  if ( blocking_time > 0
    && non_blocking_input_support
    && select(stdin_no + 1, &ifds, nullptr, nullptr, &tv) > 0
    && FD_ISSET(stdin_no, &ifds) )
  {
    return (has_pending_input = true);
  }

  if ( isKeypressTimeout() || ! non_blocking_input_support )
    tv.tv_usec = suseconds_t(blocking_time);
  else
    tv.tv_usec = suseconds_t(read_blocking_time_short);

  if ( ! has_pending_input
    && select(stdin_no + 1, &ifds, nullptr, nullptr, &tv) > 0
    && FD_ISSET(stdin_no, &ifds) )
  {
    has_pending_input = true;
  }

  return has_pending_input;
}

//----------------------------------------------------------------------
void FKeyboard::clearKeyBuffer() noexcept
{
  // Empty the buffer

  fkey = FKey::None;
  key = FKey::None;
  fifo_buf.clear();
}

//----------------------------------------------------------------------
void FKeyboard::clearKeyBufferOnTimeout()
{
  // Empty the buffer on timeout

  if ( fifo_buf.hasData() && isKeypressTimeout() )
    clearKeyBuffer();
}

//----------------------------------------------------------------------
void FKeyboard::escapeKeyHandling()
{
  // Send an escape key press event if there is only one 0x1b
  // in the buffer and the timeout is reached

  if ( fifo_buf.getSize() == 1
    && fifo_buf[0] == 0x1b
    && isKeypressTimeout() )
  {
    fifo_buf.clear();
    escapeKeyPressed();
  }

  // Handling of keys that are substrings of other keys
  substringKeyHandling();
}

//----------------------------------------------------------------------
void FKeyboard::processQueuedInput()
{
  while ( ! fkey_queue.empty() )
  {
    key = fkey_queue.front();
    fkey_queue.pop();

    if ( key > FKey::None )
    {
      keyPressed();

      if ( FApplication::isQuit() )
        return;

      keyReleased();

      if ( FApplication::isQuit() )
        return;

      key = FKey::None;
    }
  }
}

// private methods of FKeyboard
//----------------------------------------------------------------------
inline FKey FKeyboard::getMouseProtocolKey() const
{
  // Looking for mouse string in the key buffer

  if ( ! mouse_support )
    return NOT_SET;

  const auto buf_len = fifo_buf.getSize();

  // x11 mouse tracking
  if ( buf_len >= 6 && fifo_buf[1] == '[' && fifo_buf[2] == 'M' )
    return FKey::X11mouse;

  // SGR mouse tracking
  if ( fifo_buf[1] == '[' && fifo_buf[2] == '<' && buf_len >= 9
    && (fifo_buf[buf_len - 1] == 'M' || fifo_buf[buf_len - 1] == 'm') )
    return FKey::Extended_mouse;

  // urxvt mouse tracking
  if ( fifo_buf[1] == '[' && fifo_buf[2] >= '1' && fifo_buf[2] <= '9'
    && fifo_buf[3] >= '0' && fifo_buf[3] <= '9' && buf_len >= 9
    && fifo_buf[buf_len - 1] == 'M' )
    return FKey::Urxvt_mouse;

  return NOT_SET;
}

//----------------------------------------------------------------------
inline FKey FKeyboard::getTermcapKey()
{
  // Looking for termcap key strings in the buffer

  static_assert ( FIFO_BUF_SIZE > 0 , "FIFO buffer too small" );

  if ( key_cap_ptr.use_count() == 0 )
    return NOT_SET;

  const auto buf_len = fifo_buf.getSize();
  const auto& found_key = std::find_if
  (
    key_cap_ptr->cbegin(),
    key_cap_end,
    [this, &buf_len] (const auto& cap_key)
    {
      const auto& kstr = cap_key.string;
      const auto& klen = cap_key.length;

      if ( klen == 0 || klen != buf_len )
        return false;

      return fifo_buf.strncmp_front (kstr, klen);
    }
  );

  if ( found_key != key_cap_end )  // found
  {
    const std::size_t len = found_key->length;
    fifo_buf.pop(len);  // Remove founded entry
    return found_key->num;
  }

  return NOT_SET;
}

//----------------------------------------------------------------------
inline FKey FKeyboard::getKnownKey()
{
  // Looking for a known key strings in the buffer

  static_assert ( FIFO_BUF_SIZE > 0 , "FIFO buffer too small" );

  const auto buf_len = fifo_buf.getSize();
  const auto& key_map = FKeyMap::getKeyMap();
  const auto& found_key = std::find_if
  (
    key_map.cbegin(),
    key_map.cend(),
    [this, &buf_len] (const auto& known_key)
    {
      const auto& kstr = known_key.string;  // This string is never null
      const auto& klen = known_key.length;

      if ( klen != buf_len )
        return false;

      return fifo_buf.strncmp_front (kstr, klen);
    }
  );

  if ( found_key != key_map.cend() )  // found
  {
    const std::size_t len = found_key->length;

    if ( len == 2
      && ( fifo_buf[1] == 'O'
        || fifo_buf[1] == '['
        || fifo_buf[1] == ']' )
      && ! isKeypressTimeout() )
    {
      return FKey::Incomplete;
    }

    fifo_buf.pop(len);  // Remove founded entry
    return found_key->num;
  }

  return NOT_SET;
}

//----------------------------------------------------------------------
inline FKey FKeyboard::getSingleKey()
{
  // Looking for single key code in the buffer

  std::size_t len{1U};
  const auto& firstchar = fifo_buf.front();
  FKey keycode{};

  // Look for a utf-8 character
  if ( utf8_input && (firstchar & uChar(0xc0)) == 0xc0 )
  {
    const auto buf_len = fifo_buf.getSize();

    if ( (firstchar & uChar(0xe0)) == 0xc0 )
      len = 2U;
    else if ( (firstchar & uChar(0xf0)) == 0xe0 )
      len = 3U;
    else if ( (firstchar & uChar(0xf8)) == 0xf0 )
      len = 4U;

    if ( buf_len < len && ! isKeypressTimeout() )
      return FKey::Incomplete;

    keycode = UTF8decode(len);
  }
  else
    keycode = FKey(uChar(firstchar));

  fifo_buf.pop(len);  // Remove founded entry

  if ( keycode == FKey(0) )  // Ctrl+Space or Ctrl+@
    keycode = FKey::Ctrl_space;

  return FKey(keycode == FKey(127) ? FKey::Backspace : keycode);
}

//----------------------------------------------------------------------
inline bool FKeyboard::isKeypressTimeout()
{
  return FObject::isTimeout (time_keypressed, key_timeout);
}

//----------------------------------------------------------------------
FKey FKeyboard::UTF8decode (const std::size_t len) const noexcept
{
  using distance_type = CharRingBuffer<FIFO_BUF_SIZE>::difference_type;
  constexpr std::size_t max = 4U;
  FKey ucs{FKey::None};  // Universal coded character
  const auto begin = std::begin(fifo_buf);
  const auto end = begin + static_cast<distance_type>(std::min(len, max));

  for (auto iter{begin}; iter != end; ++iter)
  {
    const auto ch = uChar(*iter);

    if ( (ch & uChar(0xc0)) == 0x80 )
    {
      // byte 2..4 = 10xxxxxx
      ucs = (ucs << 6) | FKey(ch & uChar(0x3f));
    }
    else if ( ch < 128 )
    {
      // byte 1 = 0xxxxxxx (1 byte mapping)
      ucs = FKey(uChar(ch));
    }
    else if ( len == 2U )
    {
      // byte 1 = 110xxxxx (2 byte mapping)
      ucs = FKey(ch & uChar(0x1f));
    }
    else if ( len == 3U )
    {
      // byte 1 = 1110xxxx (3 byte mapping)
      ucs = FKey(ch & uChar(0x0f));
    }
    else if ( len == 4U )
    {
      // byte 1 = 11110xxx (4 byte mapping)
      ucs = FKey(ch & uChar(0x07));
    }
    else
    {
      // error
      ucs = NOT_SET;
    }
  }

  return ucs;
}

//----------------------------------------------------------------------
inline ssize_t FKeyboard::readKey()
{
  setNonBlockingInput();
  const ssize_t bytes = read(FTermios::getStdIn(), &read_character, 1);
  unsetNonBlockingInput();
  return bytes;
}

//----------------------------------------------------------------------
void FKeyboard::parseKeyBuffer()
{
  time_keypressed = FObject::getCurrentTime();

  while ( readKey() > 0 )
  {
    has_pending_input = false;

    if ( ! fifo_buf.isFull() )
      fifo_buf.push(read_character);

    // Read the rest from the fifo buffer
    while ( fifo_buf.hasData() && fkey != FKey::Incomplete )
    {
      fkey = parseKeyString();
      fkey = keyCorrection(fkey);

      if ( fkey == FKey::X11mouse
        || fkey == FKey::Extended_mouse
        || fkey == FKey::Urxvt_mouse )
      {
        key = fkey;
        mouseTracking();
        break;
      }

      if ( fkey != FKey::Incomplete )
        fkey_queue.push(fkey);
    }

    fkey = FKey::None;

    if ( fkey_queue.size() >= MAX_QUEUE_SIZE )
      break;
  }
}

//----------------------------------------------------------------------
FKey FKeyboard::parseKeyString()
{
  const auto& firstchar = fifo_buf.front();

  if ( firstchar == ESC[0] )
  {
    FKey keycode = getMouseProtocolKey();

    if ( keycode != NOT_SET )
      return keycode;

    keycode = getTermcapKey();

    if ( keycode != NOT_SET )
      return keycode;

    keycode = getKnownKey();

    if ( keycode != NOT_SET )
      return keycode;

    if ( ! isKeypressTimeout() )
      return FKey::Incomplete;
  }

  return getSingleKey();
}

//----------------------------------------------------------------------
FKey FKeyboard::keyCorrection (const FKey& keycode) const
{
  FKey key_correction;

#if defined(__linux__)
  static const auto& fterm_data = FTermData::getInstance();

  if ( fterm_data.isTermType(FTermType::linux_con) )
  {
    static auto& linux_console = FTermLinux::getInstance();
    key_correction = linux_console.modifierKeyCorrection(keycode);
  }
  else
    key_correction = keycode;
#else
  key_correction = keycode;
#endif

  return key_correction;
}

//----------------------------------------------------------------------
void FKeyboard::substringKeyHandling()
{
  // Some keys (Meta-O, Meta-[, Meta-]) used substrings
  // of other keys and are only processed after a timeout

  if ( fifo_buf.getSize() == 2
    && fifo_buf[0] == 0x1b
    && (fifo_buf[1] == 'O' || fifo_buf[1] == '[' || fifo_buf[1] == ']')
    && isKeypressTimeout() )
  {
    if ( fifo_buf[1] == 'O' )
      fkey = FKey::Meta_O;
    else if ( fifo_buf[1] == '[' )
      fkey = FKey::Meta_left_square_bracket;
    else
      fkey = FKey::Meta_right_square_bracket;

    fkey_queue.push(fkey);
    fifo_buf.clear();
  }
}

//----------------------------------------------------------------------
void FKeyboard::keyPressed() const
{
  keypressed_cmd.execute();
}

//----------------------------------------------------------------------
void FKeyboard::keyReleased() const
{
  keyreleased_cmd.execute();
}

//----------------------------------------------------------------------
void FKeyboard::escapeKeyPressed() const
{
  escape_key_cmd.execute();
}

//----------------------------------------------------------------------
void FKeyboard::mouseTracking() const
{
  mouse_tracking_cmd.execute();
}

}  // namespace finalcut
