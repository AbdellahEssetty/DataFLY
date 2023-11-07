| Supported Targets | ESP32 | ESP32-C3 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- |

# DataFLY: Lightweight data logger for micro-mobility vehicles

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This (unfinished) project was the topic of my end-of-study internship with the purpose of designing and creating an affordable and minimal data logger for micro-mobility vehicles. The DataFLY has static and minimal functionalities that makes it efficient with dealing with the requirements thats modern vehicles support. 

The project was unfinished due to time restraints (didn't finish the PCB, if I have the time I may revisit later).

## Hardware

The hardware is based on an ESP32 board (it can work with the ESP32-S3).
Additional hardware:
1. MCP2551 CAN Transceiver.
2. MCP2515 SPI based CAN Controller.
3. SPI SD-card reader.
4. SIM7080G module for cellular communication and GNSS.

## Software 

The software is using ESP-idf as a framework, FreeRTOS as a Real-Time-Operating-System. Alongside plenty of open-source libraries which are really appreciated (check them in components/).

Another piece of software is the DataFLY UI. A web user interface to monitor the states of multiple data loggers and the associated vehicles at once. 
