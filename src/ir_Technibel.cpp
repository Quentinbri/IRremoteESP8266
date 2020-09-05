// Copyright 2020 Quentin Briollant

/// @file
/// @brief Support for Technibel protocol.

#include "ir_Technibel.h"
#include "IRrecv.h"
#include "IRsend.h"
#include "IRtext.h"
#include "IRutils.h"
#include <algorithm>

using irutils::addBoolToString;
using irutils::addModeToString;
using irutils::addFanToString;
using irutils::addLabeledString;
using irutils::addTempToString;
using irutils::minsToString;
using irutils::setBit;
using irutils::setBits;

const uint16_t kTechnibelAcHdrMark = 8836;;
const uint16_t kTechnibelAcHdrSpace = 4380;;
const uint16_t kTechnibelAcBitMark = 523;;
const uint16_t kTechnibelAcOneSpace = 1696;;
const uint16_t kTechnibelAcZeroSpace = 564;;
const uint32_t kTechnibelAcGap = kDefaultMessageGap;
const uint16_t kTechnibelAcFreq = 38000;
const uint16_t kTechnibelAcOverhead = 3;


#if SEND_TECHNIBEL_AC
/// Send an Technibel AC formatted message.
/// Status: STABLE / Reported as working on a real device.
/// @param[in] data containing the IR command.
/// @param[in] nbits Nr. of bits to send. usually kTechnibelAcBits
/// @param[in] repeat Nr. of times the message is to be repeated.
void IRsend::sendTechnibelAc(const uint64_t data, const uint16_t nbits,
                             const uint16_t repeat) {
  sendGeneric(kTechnibelAcHdrMark, kTechnibelAcHdrSpace,
              kTechnibelAcBitMark, kTechnibelAcOneSpace,
              kTechnibelAcBitMark, kTechnibelAcZeroSpace,
              kTechnibelAcBitMark, kTechnibelAcGap,
              data, nbits, kTechnibelAcFreq, true,  // LSB First.
              repeat, kDutyDefault);
}
#endif  // SEND_TECHNIBEL_AC

#if DECODE_TECHNIBEL_AC
/// Status: STABLE / Reported as working on a real device
/// @param[in,out] results Ptr to data to decode & where to store the decode
/// @param[in] offset The starting index to use when attempting to decode the
///   raw data. Typically/Defaults to kStartOffset.
/// @param[in] nbits The number of data bits to expect (kTechnibelAcBits).
/// @param[in] strict Flag indicating if we should perform strict matching.
/// @return A boolean. True if it can decode it, false if it can't.
bool IRrecv::decodeTechnibelAc(decode_results *results, uint16_t offset,
                               const uint16_t nbits, const bool strict) {
  if (results->rawlen < 2 * nbits + kTechnibelAcOverhead - offset) {
    return false;  // Too short a message to match.
  }
  if (strict && nbits != kTechnibelAcBits) {
    return false;
  }

  uint64_t data = 0;

  // Header + Data + Footer
  if (!matchGeneric(results->rawbuf + offset, &data,
                    results->rawlen - offset, nbits,
                    kTechnibelAcHdrMark, kTechnibelAcHdrSpace,
                    kTechnibelAcBitMark, kTechnibelAcOneSpace,
                    kTechnibelAcBitMark, kTechnibelAcZeroSpace,
                    kTechnibelAcBitMark, kTechnibelAcGap, true,
                    _tolerance, kMarkExcess, true)) return false;

  // Success
  results->decode_type = decode_type_t::TECHNIBEL_AC;
  results->bits = nbits;
  results->value = data;
  results->command = 0;
  results->address = 0;
  return true;
}
#endif  // DECODE_TECHNIBEL_AC

/// Class constructor
/// @param[in] pin GPIO to be used when sending.
/// @param[in] inverted Is the output signal to be inverted?
/// @param[in] use_modulation Is frequency modulation to be used?
IRTechnibelAc::IRTechnibelAc(const uint16_t pin, const bool inverted,
                             const bool use_modulation)
      : _irsend(pin, inverted, use_modulation) { stateReset(); }

/// Set up hardware to be able to send a message.
void IRTechnibelAc::begin(void) { _irsend.begin(); }

#if SEND_TECHNIBEL_AC
/// Send the current internal state as an IR message.
/// @param[in] repeat Nr. of times the message will be repeated.
void IRTechnibelAc::send(const uint16_t repeat) {
  _irsend.sendTechnibelAc(getRaw(), kTechnibelAcBits, repeat);
}
#endif  // SEND_TECHNIBEL_AC

/// Compute the checksum of the internal state.
/// @param[in] state A valid code for this protocol.
/// @return The checksum byte of the internal state.
uint8_t IRTechnibelAc::calcChecksum(const uint64_t state) {
  uint8_t sum = 0;
  // Add up all the 8 bit data chunks.
  for (uint8_t offset = kTechnibelAcTimerHoursOffset;
        offset < kTechnibelAcHeaderOffset; offset += 8) {
    sum += GETBITS64(state, offset, 8);
  }
  sum = invertBits(sum, 8);
  sum += 1;
  return sum;
}

/// Set the checksum of the internal state.
void IRTechnibelAc::checksum(void) {
  setBits(&remote_state, kTechnibelAcChecksumOffset, kTechnibelAcChecksumSize,
          calcChecksum(remote_state));
}

/// Reset the internal state of the emulation.
/// @note Mode:Cool, Power:Off, fan:Low, temp:20, swing:Off, sleep:Off
void IRTechnibelAc::stateReset(void) {
  _saved_temp = 20;  // DegC  (Random reasonable default value)
  _saved_temp_units = 0;  // Celsius

  off();
  setTemp(_saved_temp);
  setTempUnit(_saved_temp_units);
  setMode(kTechnibelAcCool);
  setFan(kTechnibelAcFanLow);
  setSwing(false);
  setSleep(false);
}

/// Get a copy of the internal state/code for this protocol.
/// @return A code for this protocol based on the current internal state.
uint64_t IRTechnibelAc::getRaw(void) {
  setBits(&remote_state, kTechnibelAcHeaderOffset, kTechnibelAcHeaderSize,
          kTechnibelAcHeader);
  checksum();
  return remote_state;
}

/// Set the internal state from a valid code for this protocol.
/// @param[in] state A valid code for this protocol.
void IRTechnibelAc::setRaw(const uint64_t state) {
  remote_state = state;
}

/// Set the requested power state of the A/C to on.
void IRTechnibelAc::on(void) { setPower(true); }

/// Set the requested power state of the A/C to off.
void IRTechnibelAc::off(void) { setPower(false); }

/// Change the power setting.
/// @param[in] on true, the setting is on. false, the setting is off.
void IRTechnibelAc::setPower(const bool on) {
  setBit(&remote_state, kTechnibelAcPowerBit, on);
}

/// Get the value of the current power setting.
/// @return true, the setting is on. false, the setting is off.
bool IRTechnibelAc::getPower(void) {
  return GETBIT64(remote_state, kTechnibelAcPowerBit);
}

/// Set the temperature unit setting.
/// @param[in] fahrenheit true, the unit is °F. false, the unit is °C.
void IRTechnibelAc::setTempUnit(const bool fahrenheit) {
  setBit(&remote_state, kTechnibelAcTempUnitBit, fahrenheit);
}

/// Get the temperature unit setting.
/// @return true, the unit is °F. false, the unit is °C.
bool IRTechnibelAc::getTempUnit(void) {
  return GETBIT64(remote_state, kTechnibelAcTempUnitBit);
}

/// Set the temperature.
/// @param[in] degrees The temperature in degrees.
/// @param[in] fahrenheit The temperature unit: true=°F, false(default)=°C.
void IRTechnibelAc::setTemp(const uint8_t degrees, const bool fahrenheit) {
  uint8_t temp;
  uint8_t temp_min = kTechnibelAcTempMinC;
  uint8_t temp_max = kTechnibelAcTempMaxC;
  setTempUnit(fahrenheit);
  if (fahrenheit) {
    temp_min = kTechnibelAcTempMinF;
    temp_max = kTechnibelAcTempMaxF;
  }
  temp = std::max(temp_min, degrees);
  temp = std::min(temp_max, temp);
  _saved_temp = temp;
  _saved_temp_units = fahrenheit;

  setBits(&remote_state, kTechnibelAcTempOffset, kTechnibelAcTempSize, temp);
}

/// Get the current temperature setting.
/// @return The current setting for temp. in degrees.
uint8_t IRTechnibelAc::getTemp(void) {
  return GETBITS64(remote_state, kTechnibelAcTempOffset, kTechnibelAcTempSize);
}

/// Set the speed of the fan.
/// @param[in] speed The desired setting.
void IRTechnibelAc::setFan(const uint8_t speed) {
  // Mode fan speed rules.
  if (getMode() == kTechnibelAcDry && speed != kTechnibelAcFanLow) {
    setFan(kTechnibelAcFanLow);
    return;
  }
  // Bounds check enforcement
  if (speed > kTechnibelAcFanHigh) {
    setFan(kTechnibelAcFanHigh);
  } else if (speed < kTechnibelAcFanLow) {
    setFan(kTechnibelAcFanLow);
  } else {
    setBits(&remote_state, kTechnibelAcFanOffset, kTechnibelAcFanSize, speed);
  }
}

/// Get the current fan speed setting.
/// @return The current fan speed/mode.
uint8_t IRTechnibelAc::getFan(void) {
  return GETBITS64(remote_state, kTechnibelAcFanOffset, kTechnibelAcFanSize);
}

/// Convert a stdAc::fanspeed_t enum into it's native speed.
/// @param[in] speed The enum to be converted.
/// @return The native equivilant of the enum.
uint8_t IRTechnibelAc::convertFan(const stdAc::fanspeed_t speed) {
  switch (speed) {
    case stdAc::fanspeed_t::kMin:
    case stdAc::fanspeed_t::kLow:
      return kTechnibelAcFanLow;
    case stdAc::fanspeed_t::kMedium:
      return kTechnibelAcFanMedium;
    case stdAc::fanspeed_t::kHigh:
    case stdAc::fanspeed_t::kMax:
      return kTechnibelAcFanHigh;
    default:
      return kTechnibelAcFanLow;
  }
}

/// Convert a native fan speed into its stdAc equivilant.
/// @param[in] speed The native setting to be converted.
/// @return The stdAc equivilant of the native setting.
stdAc::fanspeed_t IRTechnibelAc::toCommonFanSpeed(const uint8_t speed) {
  switch (speed) {
    case kTechnibelAcFanHigh:   return stdAc::fanspeed_t::kHigh;
    case kTechnibelAcFanMedium: return stdAc::fanspeed_t::kMedium;
    case kTechnibelAcFanLow:    return stdAc::fanspeed_t::kLow;
    default:                    return stdAc::fanspeed_t::kLow;
  }
}

/// Get the operating mode setting of the A/C.
/// @return The current operating mode setting.
uint8_t IRTechnibelAc::getMode(void) {
  return GETBITS64(remote_state, kTechnibelAcModeOffset,
                            kTechnibelAcModeSize);
}

/// Set the operating mode of the A/C.
/// @param[in] mode The desired operating mode.
void IRTechnibelAc::setMode(const uint8_t mode) {
  switch (mode) {
    case kTechnibelAcHeat:
    case kTechnibelAcFan:
    case kTechnibelAcDry:
    case kTechnibelAcCool:
      break;
    default:
      setMode(kTechnibelAcCool);
      return;
  }
  setBits(&remote_state, kTechnibelAcModeOffset, kTechnibelAcModeSize, mode);
  setFan(getFan());  // Re-force any fan speed constraints.
  // Restore previous temp settings for cool mode.
  setTemp(_saved_temp, _saved_temp_units);
}

/// Convert a stdAc::opmode_t enum into its native mode.
/// @param[in] mode The enum to be converted.
/// @return The native equivilant of the enum.
uint8_t IRTechnibelAc::convertMode(const stdAc::opmode_t mode) {
  switch (mode) {
    case stdAc::opmode_t::kCool:
      return kTechnibelAcCool;
    case stdAc::opmode_t::kHeat:
      return kTechnibelAcHeat;
    case stdAc::opmode_t::kDry:
      return kTechnibelAcDry;
    case stdAc::opmode_t::kFan:
      return kTechnibelAcFan;
    default:
      return kTechnibelAcCool;
  }
}

/// Convert a native mode into its stdAc equivilant.
/// @param[in] mode The native setting to be converted.
/// @return The stdAc equivilant of the native setting.
stdAc::opmode_t IRTechnibelAc::toCommonMode(const uint8_t mode) {
  switch (mode) {
    case kTechnibelAcCool:  return stdAc::opmode_t::kCool;
    case kTechnibelAcHeat:  return stdAc::opmode_t::kHeat;
    case kTechnibelAcDry:   return stdAc::opmode_t::kDry;
    case kTechnibelAcFan:   return stdAc::opmode_t::kFan;
    default:                return stdAc::opmode_t::kAuto;
  }
}

/// Set the (vertical) swing setting of the A/C.
/// @param[in] on true, the setting is on. false, the setting is off.
void IRTechnibelAc::setSwing(const bool on) {
  setBit(&remote_state, kTechnibelAcSwingBit, on);
}

/// Get the (vertical) swing setting of the A/C.
/// @return true, the setting is on. false, the setting is off.
bool IRTechnibelAc::getSwing(void) {
  return GETBIT64(remote_state, kTechnibelAcSwingBit);
}

/// Convert a stdAc::swingv_t enum into it's native swing.
/// @param[in] swing The enum to be converted.
/// @return true, the swing is on. false, the swing is off.
bool IRTechnibelAc::convertSwing(const stdAc::swingv_t swing) {
  switch (swing) {
    case stdAc::swingv_t::kOff:
      return false;
    default:
      return true;
  }
}

/// Convert a native swing into its stdAc equivilant.
/// @param[in] swing true, the swing is on. false, the swing is off.
/// @return The stdAc equivilant of the native setting.
stdAc::swingv_t IRTechnibelAc::toCommonSwing(const bool swing) {
  if (swing)
    return stdAc::swingv_t::kAuto;
  else
    return stdAc::swingv_t::kOff;
}

/// Set the Sleep setting of the A/C.
/// @param[in] on true, the setting is on. false, the setting is off.
void IRTechnibelAc::setSleep(const bool on) {
  setBit(&remote_state, kTechnibelAcSleepBit, on);
}

/// Get the Sleep setting of the A/C.
/// @return true, the setting is on. false, the setting is off.
bool IRTechnibelAc::getSleep(void) {
  return GETBIT64(remote_state, kTechnibelAcSleepBit);
}

/// Is the timer function enabled?
/// @return true, the setting is on. false, the setting is off.
void IRTechnibelAc::setTimerEnabled(const bool on) {
  setBit(&remote_state, kTechnibelAcTimerEnableBit, on);
}

/// Is the timer function enabled?
/// @return true, the setting is on. false, the setting is off.
bool IRTechnibelAc::getTimerEnabled(void) {
  return GETBIT64(remote_state, kTechnibelAcTimerEnableBit);
}

/// Set the timer for when the A/C unit will switch off.
/// @param[in] nr_of_mins Number of minutes before power off.
///   `0` will clear the timer. Max is 24 hrs (1440 mins).
/// @note Time is stored internaly in hours.
void IRTechnibelAc::setTimer(const uint16_t nr_of_mins) {
  uint8_t hours = nr_of_mins / 60;
  uint8_t value = std::min(kTechnibelAcTimerMax, hours);
  setBits(&remote_state, kTechnibelAcTimerHoursOffset, kTechnibelAcHoursSize,
          value);
  // Enable or not?
  setTimerEnabled(value > 0);
}

/// Get the timer time for when the A/C unit will switch power state.
/// @return The number of minutes left on the timer. `0` means off.
uint16_t IRTechnibelAc::getTimer(void) {
  uint16_t mins = 0;
  if (getTimerEnabled()) {
    mins = GETBITS64(remote_state, kTechnibelAcTimerHoursOffset,
                     kTechnibelAcHoursSize) * 60;
  }
  return mins;
}

/// Convert the current internal state into its stdAc::state_t equivilant.
/// @return The stdAc equivilant of the native settings.
stdAc::state_t IRTechnibelAc::toCommon(void) {
  stdAc::state_t result;
  result.protocol = decode_type_t::TECHNIBEL_AC;
  result.power = getPower();
  result.mode = toCommonMode(getMode());
  result.celsius = !getTempUnit();
  result.degrees = getTemp();
  result.fanspeed = toCommonFanSpeed(getFan());
  result.sleep = getSleep() ? 0 : -1;
  result.swingv = toCommonSwing(getSwing());
  // Not supported.
  result.model = -1;
  result.turbo = false;
  result.swingh = stdAc::swingh_t::kOff;
  result.light = false;
  result.filter = false;
  result.econo = false;
  result.quiet = false;
  result.clean = false;
  result.beep = false;
  result.clock = -1;
  return result;
}

/// Convert the current internal state into a human readable string.
/// @return A human readable string.
String IRTechnibelAc::toString(void) {
  String result = "";
  result.reserve(100);  // Reserve some heap for the string to reduce fragging.
  result += addBoolToString(getPower(), kPowerStr, false);
  result += addModeToString(getMode(), kTechnibelAcCool, kTechnibelAcCool,
                            kTechnibelAcHeat, kTechnibelAcDry,
                            kTechnibelAcFan);
  result += addFanToString(getFan(), kTechnibelAcFanHigh, kTechnibelAcFanLow,
                           kTechnibelAcFanLow, kTechnibelAcFanLow,
                           kTechnibelAcFanMedium);
  result += addTempToString(getTemp(), !getTempUnit());
  result += addBoolToString(getSleep(), kSleepStr);
  result += addBoolToString(getSwing(), kSwingVStr);
  if (getTimerEnabled())
    result += addLabeledString(irutils::minsToString(getTimer()),
                               kTimerStr);
  else
    result += addBoolToString(false, kTimerStr);
  return result;
}
