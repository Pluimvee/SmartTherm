#include "SmartControl.h"
#define LOG_REMOTE
#define LOG_LEVEL 3
#include <Logging.h>
#include <Timer.h>

#define STATISTICS_BUFFER_SIZE   10          // number of values to store
#define STATISTICS_BUFFER_TIMER  (30*1000)   // minimum time between entries

////////////////////////////////////////////////////////////////////////////////////////////
// °C
////////////////////////////////////////////////////////////////////////////////////////////
Temperature::Temperature(float val, uint16_t max_age, float min, float max, float max_diff_psec, float k)
: _statistics(STATISTICS_BUFFER_SIZE), _stat_interval(STATISTICS_BUFFER_TIMER)
, _cur_val(val), _max_age(max_age)      // maximum age of a value
, _min_val(min), _max_val(max)          // min and max absolute boundaries
, _max_diff_psec(max_diff_psec), _k(k)  // max delta with previous measurements
{
  _age.set(0);  // ensure the _age has passed to indicate invalid value
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
bool Temperature::set(float value, bool validate) 
{
  bool spike = false;
  if (validate)
  {
    if (!valid())    // if last value has become invalid, then also the _statistics
      _statistics.clear();
    // do nothing if exceeding min, max boundaries
    if (_max_val !=0.0f && value > _max_val)
      return false;
    if (_min_val !=0.0f && value < _min_val)
      return false;
    
    // do nothing if exceeding threshold
    if (_max_diff_psec !=0.0f && valid() && _age.elapsed() > 0) // if previous value is valid (&& prevent div_by_zero)
    {
      float change_per_sec = std::abs(value-_cur_val) * 1000.0f / _age.elapsed();
      if (change_per_sec > _max_diff_psec) {
        INFO("Temp exceeding threshold of %.3f C/s (change is %.3f C/s)", _max_diff_psec, change_per_sec);
        return false;
      }
    }
    // spikes
    if (_k !=0.0f && _statistics.bufferIsFull()) {
      if (spike = (std::abs(value - _statistics.getFastAverage()) > (_k * _statistics.getStandardDeviation())))
        DEBUG("SPIKE detected!!! value %.2f", value);
    }
  }
  if (_stat_interval)           // Note: we do add spikes to update stddev !
    _statistics.add(value);

  if (!spike) {
    _cur_val = value;
    _age.set(_max_age * 60000); // flag as valid for max_age minutes
  }
  return true;  // value set
}

////////////////////////////////////////////////////////////////////////////////////////////
float Temperature::get() const {
  return _cur_val;    // what else can we do -> Dont place any logic as in some use cases the value will not expire!
}

////////////////////////////////////////////////////////////////////////////////////////////
bool Temperature::valid() const {
  return _max_age ==0 || !_age.passed();
}

////////////////////////////////////////////////////////////////////////////////////////////
float Temperature::average() const 
{
  if (_statistics.getCount() > 0)
    return _statistics.getFastAverage();
  return _cur_val;    // is most likely the initial/default value
}

////////////////////////////////////////////////////////////////////////////////////////////
const char *Temperature::toString(uint8_t precision, bool celcius) const
{
  if (!valid())
    return "n/a";   // or --
  snprintf(_to_string, sizeof(_to_string), "%.*f%c", precision, _cur_val, celcius?'C':'\0'); // °C

  return _to_string;
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
