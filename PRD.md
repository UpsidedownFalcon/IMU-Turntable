Product Requirements Document 
=============================

Contents
=========

1. [Rationale](#1-rationale)
2. [Design Brief](#2-design-brief)
3. [Existing Product Analysis](#3-existing-product-analysis) 
4. [Design Specification ](#4-initial-design-specification)
5. [Initial Design Ideas](#5-initial-design-ideas)
6. [Final Design](#6-final-design)
7. [Manufacturing](#7-manufacturing) 

## 1. Rationale 
Inertial Measurement Units (IMU sensors) widely used in wearable technologies. [Hippos Exoskeleton Incorporated](https://www.hippos.life/) are using IMU sensors in their world's first Smart Adaptive Knee Brace to prevent serious knee injuries such as ACL tears. 

In such applications, there is a demand for high frequency IMU measurements. It is also imperative to validate these high frequency IMU measurements to determine the accuracy of the IMU sensor. There is no consistent and economical method between different researchers to do so, hence why this project exists to fill that gap. 

Inertial Measurement Units (IMUs) are critical to wearable exoskeletons but lack a standardised, low-cost method to validate high-frequency data (500–1 000 Hz+) against a ground truth.

- **Problem statement:** Existing IMU validations rely on optical systems (≤240 Hz, ≈42% joint-angle error) or expensive commercial turntables, making cross-study comparison difficult.

- **Opportunity:** An open-source, 3-axis mechanical jig with rotary-encoder ground truth can deliver <9% error validation at high sampling rate

Please read *Research Proposal.pdf* for a detailed overview. 

## 2. Design Brief 
This project must produce an open-source and economical method to characterise and validate IMU sensor data against a ground truth, which can be applied across different researchers to consistently compare the accuracy of different IMU sensors at different sampling rates. 

The open-source hardware and software platform must:
1. Spin IMUs on a motorized 3-DOF jig at controlled trajectories (up to 1 000 Hz).
2. Log synchronous IMU and rotary-encoder data.
3. Compute and report accuracy metrics (RMSE, MAE, drift rates, Allan variance).
4. Be fully documented (CAD, firmware, analysis scripts) so that other labs can reproduce and compare results.

## 3. Existing Product Analysis
| Turntable                                                       | Rotation DoF | Rotation Range / °        | Rotation Resolution / °                         | Linear DoF | Linear range | Linear Resolution | Cost / £           | Open-Source | Source Link                                                                                                                                                    |
|-----------------------------------------------------------------|-------------:|---------------------------|-------------------------------------------------|-----------:|--------------|-------------------|--------------------|-------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **iMAR iTES-PDT07**                                             | 1            | Continuous (∞)            | 0.00153 ° (5.5 arcsec)                           | 0          | N/A          | N/A               | On request         | No          | https://imar-navigation.de/en/product/ites-pdt07                                                                                                               |
| **iMAR iTURN-2D1-HIL**                                          | 2            | Continuous (∞)            | 0.00000556 ° (0.02 arcsec)                       | 0          | N/A          | N/A               | On request         | No          | https://imar-navigation.de/en/product/iturn-2                                                                                                                  |
| **iMAR iTURN-3S1**                                              | 3            | Continuous (∞)            | < 0.0002 °                                      | 0          | N/A          | N/A               | On request         | No          | https://imar-navigation.de/en/product/iturn-3s1                                                                                                                |
| **LIOCREBIF Single-Axis Rate & Position Turntable**             | 1            | Continuous (∞)            | 0.0001 °                                        | 0          | N/A          | N/A               | On request         | No          | https://www.liocrebif.com/single-axis-rate-and-position-turntable                                                                                              |
| **360° Electric Rotating Photography Turntable (B0756FVW78)**   | 1            | Continuous (∞)            | ≈ 1 ° (no encoder)                                 | 0          | N/A          | N/A               | £21                | No          | https://www.amazon.co.uk/dp/B0756FVW78                                                                                                                          |
| **JAYEGT Motorized Rotating Display Stand, 7.87 in**            | 1            | Continuous (∞)            | ≈ 0.18 ° (stepper microsteps)                     | 0          | N/A          | N/A               | £22                | No          | https://www.amazon.co.uk/dp/B07XYZ1234 (approx.)                                                                                                               |
| **OpenBuilds Compact Rotary Axis (DIY)**                        | 1            | Continuous (∞)            | 0.05625 ° (1.8°/step with 1/32 microstep)         | 0          | N/A          | N/A               | £160 (~\$200*)     | Yes         | https://openbuilds.com/builds/compact-rotary-axis-laser-engraver.10204/                                                                                         |


## 4. Design Specification  
The open-source IMU turntable must: 
1. 3 degrees of rotational freedom 
2. 360° of rotation about all three axes 
3. Rotational resolution of at least 0.05625°
4. **As cheap as possible** 
5. Variable trajectory control of the turntable 
6. IMU sensor datalogging at variable frequencies 
7. Temporal synchronisation of the IMU data and turntable motion 

## 5. Initial Design Ideas 
From *Research Proposal.pdf*, the following selections have been made: 

### IMU Sensors: 
The following IMU sensors were selected due to their variety in sampling frequency specified on their datasheet and due to the support of SPI. 

**SPI will be used** to transfer IMU data to the datalogger as it is the fastest digital communication protocol between I2C and UART. 

| IMU Sensor | Communication Protocol | Sampling Frequency / Hz | Angular Resolution / ° s^{-1} | Bit Depth |
|------------|------------------------|-------------------------|-----------------------|-----------|
| Analog Devices ADIS16470 | SPI | 5000 | ±2000 | 16 - 32 bit | 
| InvenSense ICM-20602 | SPI and I2C | 1 - 32000 | ±2000 | 16 bit | 
| Bosch BMI088 | SPI and I2C | 2000 | ±2000 | 16 bit |
| Ceva BNO085 | SPI, I2C and UART | 1000 | ±2000 | 16 bit |

![Initial Sketch 1](Images_MD/Sketches1.jpg) 
![Initial Sketch 2](Images_MD/Sketches2.jpg)

From the initial design ideas, a direct drive system is the most feasible because *accuracy and precision* are a priority, so **zero backlash** is a necessity. With such a system, the maximum attainable resolution must be further investigated by researching existing rotational actuators e.g stepper motors: **EXISTING PRODUCT ANALYSIS ITERATIOIN II**. 

## 6. Final Design 

## 7. Manufacturing 
