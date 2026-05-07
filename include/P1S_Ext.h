#pragma once
#include <stdint.h>

void P1S_ext_Init();
bool P1S_ext_IsOnline();					 // true if data received within last 90s
bool P1S_ext_ActiveForSlot(uint8_t slot);	 // online AND assigned to given display slot
