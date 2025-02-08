/*
Display
- change cache to on large char map with offsets for caching and make it a member which is cleaned at construction

Temperature
- What use does the validation according to stddev bring, we need to store the spikes anyway

SmartControl
- Use factor C for cooling, or disable cooling when Outside < target. Ie dont let the WP stop when gashaard on
  We could say (FactorB *inside_delta < 0, then 0 
- find a bit of flash memmory to store settings (Factor A,B,C and target)
- Add modes: each could have its own factor (0,7 | 0,6 | 0,5)
      Store – Storing thermal energy efficiently.
      Retain – Holding thermal energy for future use.
      Release – Utilizing stored energy to reduce electricity consumption.

HeatCurve
- Ttarget is offset +delta with Toutside. As such changing Ttarget effects Tset with (1+factor)*Ttarget
maybe its better to use Tcurrent for stooklijn instead of Ttarget
and then use Ttarget - Tcurrent to change the factor
- dont let setpoint get to far away above inlet -> watch out what happens after/during defrosting!

Analysis
Date     Time    Tout    Tr      Tset    Troom   Factor  Finside   Result
16-1-23  17:30   3,22    29,51   31,72   20,88   0,65    0.0       decreasing 0,18 in 37 minutes
16-1-23  22:45   2,6                             0,6     0.0       changed factor
17-1-23  13:45   1,55    29,38   31,84   20,51   0,6     0.0       steady
17-1-23  14:00   1,55    29,4    32,75   20,51   0,6     0.0       changed Ttarget from 20,5 to 21
17-1-23  17:00   1,49    30,4    32,75   20,57   0,6     0.1       No result, change inside to 0.1

*/