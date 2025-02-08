#include "SmartControl.h"
#define LOG_REMOTE
#define LOG_LEVEL 3
#include <Logging.h>
#include <Timer.h>

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
#define STOOKLIJN_W35       0.5f        // stooklijn W35
#define STOOKLIJN_W55       1.175f      // stooklijn W55
#define STOOKLIJN_FACTOR    0.55f       // Default stooklijn factor
#define INSIDE_FACTOR       0.45f       // inside delta to affect stooklijn
#define CURVE_FACTOR        0.30f       // inside delta to affect stooklijn

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
HeatingCurve::HeatingCurve() {
  _factorA = STOOKLIJN_FACTOR;      // outside 0,7 (6) too warm; 0,65 (2.57 out) too warm; 
  _factorB = INSIDE_FACTOR;         // inside
  _factorC = CURVE_FACTOR;          // curve
  _factor  = _factorA;              // inital factor is set to baseline 
}

////////////////////////////////////////////////////////////////////////////////////////////
//returns current value, or if a newval is given it will change the value and return the previous one
float HeatingCurve::factorA(float newval) {
  float oldval = _factorA;
  if (newval >= 0.0f && newval <= 1.5f) {
    _factorA = newval;
    INFO("Changed heatingcurve factorA from %0.2f to %0.2f", oldval, newval);
  }
  return oldval;
}
float HeatingCurve::factorB(float newval) {
  float oldval = _factorB;
  if (newval >= 0.0f && newval <= 1.5f) {
    _factorB = newval;
    INFO("Changed heatingcurve factorB from %0.2f to %0.2f", oldval, newval);
  }
  return oldval;
}
float HeatingCurve::factorC(float newval) {
  float oldval = _factorC;
  if (newval >= 0.0f && newval <= 1.5f) {
    _factorC = newval;
    INFO("Changed heatingcurve factorC from %0.2f to %0.2f", oldval, newval);
  }
  return oldval;
}

////////////////////////////////////////////////////////////////////////////////////////////
// returns setpoint
float HeatingCurve::calculate(Temperature *Tcurrent, Temperature *Ttarget, Temperature *Toutside)
{
  float current = Tcurrent->average();    // Diff
  float outside = Toutside->average();    // Diff
  float target  = Ttarget->average();     // to apply a smooth change 

  double delta_outside  = target - outside;
  double delta_inside   = target - current;
  INFO("roomcur:%.2f, roomset:%.2f, outside:%.2f, deltaT-out:%.2f, deltaT-in:%.2f", current, target, outside, delta_outside, delta_inside);

  // first we calculate the factor (we use a double for precision)
  double factor =  _factorB *delta_inside;  // use factor B for the delat inside
  if (delta_outside <0)                     // do we need cooling?
    factor *=-1;                            // then reverse the outcome
  if (factor <0)                            // we only adjust to more heating/cooling needed. 
    factor = 0.0f;                          // Nature will do the rest
  factor += _factorA;                       // Add the retain temperature factor
  if (factor > STOOKLIJN_W55)               // our top limit is the 55 degtees heating curve
    factor = STOOKLIJN_W55;

  // now we calculate the heating curve
  double curve = delta_outside;
  if (_factorC > 0)
    curve = _factorC /target *std::pow(delta_outside,2) + (1-_factorC) *delta_outside;
  
  // the requested water temperature is the target roomtemp + the resulting factor * the total deltaT (target room - outside + target room - current room)
  float setpoint = target + factor * curve;
  _factor = factor;
  INFO("Set point using factor:%.5f, setpoint:%.2f", _factor, setpoint);

  return setpoint;
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
