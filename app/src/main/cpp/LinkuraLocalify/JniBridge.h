#pragma once

// JNI Bridge functions for communication between C++ and Java

// Pause camera info loop for specified milliseconds
// This function can be called from any C++ code (like HookCamera.cpp)
// to temporarily disable camera info sending to Java
// 
// Parameters:
//   delayMillis - delay in milliseconds before re-enabling the loop
// 
// Note: Multiple calls will reset the timer (debounce behavior)
//       The delay will start from the last call
void pauseCameraInfoLoopFromNative(long delayMillis);

// Pause camera info loop for default 3 seconds
// This is a convenience overload that uses the default delay
void pauseCameraInfoLoopFromNative();