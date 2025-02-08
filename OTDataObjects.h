const char *dataId2Str(int id) {
  switch (id) {
    case 0: return "Status Master and Slave Status flags flag 8";
    case 1: return "TSet Control setpoint f8.8 ie CH water temperature setpoint (°C)";
    case 2: return "M-Config / M-MemberIDcode flag8 / u8 Master Configuration Flags / Master MemberID Code";
    case 3: return "S-Config / S-MemberIDcode flag8 / u8 Slave Configuration Flags / Slave MemberID Code";
    case 4: return "Command u8 / u8 Remote Command";
    case 5: return "ASF-flags / OEM-fault-code flag8 / u8 Application-specific fault flags and OEM fault code";
    case 6: return "RBP-flags flag8 / flag8 Remote boiler parameter transfer-enable & read/write flags";
    case 7: return "Cooling-control f8.8 Cooling control signal (%)";
    case 8: return "TsetCH2 f8.8 Control setpoint for 2e CH circuit (°C)";
    case 9: return "TrOverride f8.8 Remote override room setpoint";
    case 10: return "TSP u8 / u8 Number of Transparent-Slave-Parameters supported by slave";
    case 11: return "TSP-index / TSP-value u8 / u8 Index number / Value of referred-to transparent slave parameter.";
    case 12: return "FHB-size u8 / u8 Size of Fault-History-Buffer supported by slave";
    case 13: return "FHB-index / FHB-value u8 / u8 Index number / Value of referred-to fault-history buffer entry.";
    case 14: return "Max-rel-mod-level-setting f8.8 Maximum relative modulation level setting (%)";
    case 15: return "Max-Capacity / Min-Mod-Level u8 / u8 Maximum boiler capacity (kW) / Minimum boiler modulation level(%)";
    case 16: return "TrSet f8.8 Room Setpoint (°C)";
    case 17: return "Rel.-mod-level f8.8 Relative Modulation Level (%)";
    case 18: return "CH-pressure f8.8 Water pressure in CH circuit (bar)";
    case 19: return "DHW-flow-rate f8.8 Water flow rate in DHW circuit. (litres/minute)";
    case 20: return "Day-Time special / u8 Day of Week and Time of Day";
    case 21: return "Date u8 / u8 Calendar date";
    case 22: return "Year u16 Calendar year";
    case 23: return "TrSetCH2 f8.8 Room Setpoint for 2nd CH circuit (°C)";
    case 24: return "Tr f8.8 Room temperature (°C)";
    case 25: return "Tboiler f8.8 Boiler flow water temperature (°C)";
    case 26: return "Tdhw f8.8 DHW temperature (°C)";
    case 27: return "Toutside f8.8 Outside temperature (°C)";
    case 28: return "Tret f8.8 Return water temperature (°C)";
    case 29: return "Tstorage f8.8 Solar storage temperature (°C)";
    case 30: return "Tcollector f8.8 Solar collector temperature (°C)";
    case 31: return "TflowCH2 f8.8 Flow water temperature CH2 circuit (°C)";
    case 32: return "Tdhw2 f8.8 Domestic hot water temperature 2 (°C)";
    case 33: return "Texhaust s16 Boiler exhaust temperature (°C)";
    case 48: return "TdhwSet-UB / TdhwSet-LB s8 / s8 DHW setpoint upper & lower bounds for adjustment (°C)";
    case 49: return "MaxTSet-UB / MaxTSet-LB s8 / s8 Max CH water setpoint upper & lower bounds for adjustment (°C)";
    case 50: return "Hcratio-UB / Hcratio-LB s8 / s8 OTC heat curve ratio upper & lower bounds for adjustment";
    case 56: return "TdhwSet f8.8 DHW setpoint (°C) (Remote parameter 1)";
    case 57: return "MaxTSet f8.8 Max CH water setpoint (°C) (Remote parameters 2)";
    case 58: return "Hcratio f8.8 OTC heat curve ratio (°C) (Remote parameter 3";
    case 100: return "Remote override function flag8 / - Function of manual and program changes in master and remote room setpoint.";
    case 115: return "OEM diagnostic code u16 OEM-specific diagnostic/service code";
    case 116: return "Burner starts u16 Number of starts burner";
    case 117: return "CH pump starts u16 Number of starts CH pump";
    case 118: return "DHW pump/valve starts u16 Number of starts DHW pump/valve";
    case 119: return "DHW burner starts u16 Number of starts burner during DHW mode";
    case 120: return "Burner operation hours u16 Number of hours that burner is in operation (i.e. flame on)";
    case 121: return "CH pump operation hours u16 Number of hours that CH pump has been running";
    case 122: return "DHW pump/valve operation hours u16 Number of hours that DHW pump has been running or DHW valve has been opened";
    case 123: return "DHW burner operation hours u16 Number of hours that burner is in operation during DHW mode";
    case 124: return "OpenTherm version Master f8.8 The implemented version of the OpenTherm Protocol Specification in the master.";
    case 125: return "OpenTherm version Slave f8.8 The implemented version of the OpenTherm Protocol Specification in the slave.";
    case 126: return "Master-version u8 / u8 Master product version number and type";
    case 127: return "Slave-version u8 / u8 Slave product version number and type";
    default: return "Unknown";
  }
}


