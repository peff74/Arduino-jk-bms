# Arduino script with NimBLE to monitor JK-BMS 

This project is a modified version of [syssi/esphome-jk-bms](https://github.com/syssi/esphome-jk-bms/), rewritten for Arduino using NimBLE.

## Features
*   **Real-time Monitoring**:
    
    *   Battery voltage, current, state of charge, and more.
        
    *   Cell voltages and temperatures.
        
    *   BMS settings and status flags.
        
*   **Control**:
    
    *   Turn **Charging**, **Discharging**, and **Balancing** ON/OFF for each BMS.
        
*   **System Information**:
    
    *   Uptime (in seconds and formatted as days, hours, minutes, seconds).
        
    *   Free heap memory (in bytes, KB, or MB).
        
    *   Sketch information (name, compile date, ESP core version).
        
*   **Multi-BMS Support**:
    
    *   Monitor and control multiple BMS devices simultaneously.
        
*   **Web Interface**:
    
    *   Simple and responsive web interface with a black background and green text.
        
    *   Automatic refresh of data every 5 seconds.

## Disclaimer
This is an experimental project. Use it at your own risk.

## Credits
Based on the work of [syssi](https://github.com/syssi).

[![Hits](https://hits.seeyoufarm.com/api/count/incr/badge.svg?url=https%3A%2F%2Fgithub.com%2Fpeff74%2FArduino-jk-bms&count_bg=%2379C83D&title_bg=%23555555&icon=&icon_color=%23E7E7E7&title=hits&edge_flat=false)](https://hits.seeyoufarm.com)
