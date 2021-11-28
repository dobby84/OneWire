/*
* OneWire.cpp
*
* Created: 05.11.2021 15:12:18
* Author : Manuel
*/

//#define OVERRIDE

#ifndef OVERRIDE			/* Time definition from Maxim application node */
	#define _timeA 6		// Write: time for "1" bit
	#define _timeB 54		// Write: time keep bus high after writing "1" bit (64us - 10us = 54us)
	#define _timeC 54		// Write: time for "0" bit (60us - 6us = 54us)
	#define _timeD 10		// Write: time keep bus high after writing "0" bit
	#define _timeE 9		// Read: time until sample input state
	#define _timeF 55		// Read: time for recovery after reading
	#define _timeH 480		// Reset: time for drive bus low
	#define _timeI 70		// Reset: time until sample input state
	#define _timeG 0		// Reset: delay before reset pulse
	#define _timeJ 410		// Reset: time for become bus high after present pulse
#else
	#define	_timeH 70		// Reset: time for drive bus low
	#define	_timeI 8.5 		// Reset: time for release bus
	#define	_timeG 2.5		// Reset: delay before reset pulse
#endif


#define F_CPU 16000000UL
#include <util/delay.h>
#include "OneWire.h"

static uint8_t crcTable[] = {
	0x00, 0x5E, 0xBC, 0xE2, 0x61, 0x3F, 0xDD, 0x83, 0xC2, 0x9C, 0x7E, 0x20, 0xA3, 0xFD, 0x1F, 0x41,
	0x9D, 0xC3, 0x21, 0x7F, 0xFC, 0xA2, 0x40, 0x1E, 0x5F, 0x01, 0xE3, 0xBD, 0x3E, 0x60, 0x82, 0xDC,
	0x23, 0x7D, 0x9F, 0xC1, 0x42, 0x1C, 0xFE, 0xA0, 0xE1, 0xBF, 0x5D, 0x03, 0x80, 0xDE, 0x3C, 0x62,
	190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
	70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
	219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
	101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
	248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
	140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
	17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
	175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
	50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
	202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
	87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
	233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

/*-------------------------------------------------------------------------------------------------
* OneWire Interface
*--------------------------------------------------------------------------------------------------
*/
OwInterface::OwInterface(volatile uint8_t* ddir, volatile uint8_t* port, volatile uint8_t* pinr, const uint8_t pin){
	ddir_ = (uint8_t*)ddir;
	port_ = (uint8_t*)port;
	pinr_ = (uint8_t*)pinr;
	pin_ = (1 << pin);			// save configured pin number
	*ddir_ &= (~pin_);			// config pin as input
	*port_ &= (~pin_);			// set output register drive pin low
}

bool OwInterface::reset(){
	bool resetDone = 0;
	_delay_us(_timeG);
	*ddir_ |= pin_;
	_delay_us(_timeH);
	*ddir_ &= (~pin_);
	_delay_us(_timeI);
	resetDone = !(*pinr_ & pin_);
	_delay_us(_timeJ);
	return resetDone;
}

bool OwInterface::readBit(){
	bool value;
	*ddir_ |= pin_;
	_delay_us(_timeA);
	*ddir_ &= (~pin_);
	_delay_us(_timeE);
	value = (*pinr_ & pin_);
	_delay_us(_timeF);
	return value;
}

void OwInterface::writeBit(int value){
	*ddir_ |= pin_;
	_delay_us(_timeA);
	if (!value) _delay_us(_timeC);
	*ddir_ &= (~pin_);
	_delay_us(_timeD);
	if (value) _delay_us(_timeB);
}

/*-------------------------------------------------------------------------------------------------
* OneWire Bus
*--------------------------------------------------------------------------------------------------
*/
OwBus::OwBus(OwInterface& interface){
	interface_ = &interface;
}

bool OwBus::reset(){
	return (*interface_).reset();
}

void OwBus::setSingleDevice(bool mode){
	singleDevice_ = mode;
}

bool OwBus::getSingleDevice(){
	return singleDevice_;
}

void OwBus::sendByte(uint8_t bData){
	int i;
	for (i = 0; i < 8; i++){			// Loop to write each bit in the byte, LS-bit first
		(*interface_).writeBit(bData & 0x01);	// shift the data byte for the next bit
		bData >>= 1;
	}
}

uint8_t OwBus::receiveByte(){
	int i, result=0;
	for (i = 0; i < 8; i++){
		result >>= 1;							// shift the result to get it ready for the next bit
		if ((*interface_).readBit())			// if result is one, then set MS bit
		result |= 0x80;
	}
	return result;
}

/*
void OwBus::sendStream(uint8_t data[]){
	uint8_t i = 0;
	if (!(*interface_).reset()) return 0;
	for (i = 0; i < 5; i++){				// Loop to write each bit in the byte, LS-bit first
		sendByte(data[i]);	// shift the data byte for the next bit
	}
}
*/



/*-------------------------------------------------------------------------------------------------
* OneWire Device
*--------------------------------------------------------------------------------------------------
*/
OwDevice::OwDevice(OwBus& bus){
	bus_ = &bus;
	interface_ = (*bus_).interface_;
}

bool OwDevice::search(){
	// perform one wire reset
	if (!(*interface_).reset() || lastDeviceFlag_){
		lastDiscrepancy_ = 0;
		lastFamilyDiscrepancy_ = 0;
		lastDeviceFlag_ = false;
		return 0;
	}
	// initialize variables for search
	idBitNumber_ = 1;
	lastZero_ = 0;
	byteMask_ = 0;
	bitMask_ = 1;
	// start search with one wire command 0xF0
	(*bus_).sendByte(0xF0);
		
	do{
		idBit_ = (*interface_).readBit();
		cmpIdBit_ = (*interface_).readBit();

		if (idBit_ && cmpIdBit_){
			lastDiscrepancy_ = 0;
			lastFamilyDiscrepancy_ = 0;
			lastDeviceFlag_ = false;
			return 0;
		}
		
		if (!(idBit_ || cmpIdBit_)){
			if (idBitNumber_ == lastDiscrepancy_){
				searchDirection_ = true;
			}
			else{
				if (idBitNumber_ > lastDiscrepancy_){
					searchDirection_ = false;
				}
				else{
					if (searchDirection_ == true){romNo_[byteMask_] |= bitMask_;}
					else{romNo_[byteMask_] &= ~bitMask_;}
				}
			}
			if (!searchDirection_){
				lastZero_ = idBitNumber_;
				if (lastZero_ < 9) lastFamilyDiscrepancy_ = lastZero_;
			}
		}
		else{
			searchDirection_ = idBit_;
		}
		
		if (searchDirection_ == true){romNo_[byteMask_] |= bitMask_;}
		else{romNo_[byteMask_] &= ~bitMask_;}
			
		(*interface_).writeBit(searchDirection_);

		idBitNumber_++;
		bitMask_ <<= 1;
		
		if (bitMask_ == 0){
			byteMask_++;
			bitMask_ = 1;
		}
	}while(idBitNumber_ < 65);
	
	lastDiscrepancy_ = lastZero_;
	
	if (lastDiscrepancy_ == 0) lastDeviceFlag_ = true;
	if ((calcCRC8((uint8_t*)romNo_, 8)) == 0){  // (calcCRC8((uint8_t*)romNo_, 8))==0
		return 1;
	}
	else{
		lastDiscrepancy_ = 0;
		lastFamilyDiscrepancy_ = 0;
		lastDeviceFlag_ = false;
		return 0;
	}
}

void OwDevice::resetSearch(){  // experimental
	lastDiscrepancy_ = 0;
	lastFamilyDiscrepancy_ = 0;
	lastDeviceFlag_ = false;
}

uint8_t OwDevice::calcCRC8(const uint8_t *data, uint8_t len){
	uint8_t crc8 = 0x00;
	
	while (len--) {
		uint8_t extract = *data++;
		for (uint8_t tempI = 8; tempI; tempI--) {
			uint8_t sum = (crc8 ^ extract) & 0x01;
			crc8 >>= 1;
			if (sum) {
				crc8 ^= 0x8C; // Polynom x^8 + x^5 + x^4 + 1
			}
			extract >>= 1;
		}
	}
	return crc8;
}

uint8_t OwDevice::tableCRC8(uint8_t *p, uint8_t len)
{
	uint8_t i;
	uint8_t crc = 0x00;

	while (len--) {
		i = (crc ^ *p++) & 0xFF;
		crc = (crcTable[i] ^ (crc << 8)) & 0xFF;
	}
	return crc & 0xFF;
}

OwDS18B20::OwDS18B20(OwBus& bus) : OwDevice::OwDevice (bus){
	bus_ = &bus;
}

bool OwDS18B20::setResolution(uint8_t resolution){
	uint8_t commandSet[5] {0xCC, 0x4E, 0x0, 0x0, 0x0};
	int i = 0;

	commandSet[4] = ((resolution - 9) << 5) + 0x1F;
	if (!(*interface_).reset()) return 0;
	for (i = 0; i < 5; i++){				// Loop to write each bit in the byte, LS-bit first
		(*bus_).sendByte(commandSet[i]);	// shift the data byte for the next bit
	}
	return 1;
}

uint8_t OwDS18B20::getResolution(){
	uint8_t command[] {0xCC, 0xBE};		//* maybe single commands and roll out the loop 
	uint8_t data[9];
	int i;
	
	if(!(*interface_).reset()) return 0;
	for (i = 0; i < 2; i++){			// Loop to write each bit in the byte, LS-bit first
		(*bus_).sendByte(command[i]);	// shift the data byte for the next bit
	}
	i = 0;
	for (i = 0; i < 9; i++){
		data[i] = (*bus_).receiveByte();
	}
	if (calcCRC8(data,9)) return 0;
	return (data[4] >> 5) + 9;
}

void OwDS18B20::convertTemp(bool singleMode){
	uint8_t command[] {0xCC, 0x44};	
	int i;
	
	if (!(*interface_).reset()) return;
	if (singleMode){
		for (i = 0; i < 2; i++){			// Loop to write each bit in the byte, LS-bit first
			(*bus_).sendByte(command[i]);	// shift the data byte for the next bit
		}		
	}
}

uint8_t OwDS18B20::receiveTemp(bool singleMode){
	uint8_t command[] {0xCC, 0xBE};			//* maybe single commands and roll out the loop 
	int i;
	
	if(!(*interface_).reset()) return 0;
	if (singleMode){
		for (i = 0; i < 2; i++){			// Loop to write each bit in the byte, LS-bit first
			(*bus_).sendByte(command[i]);	// shift the data byte for the next bit
		}
	}
	i = 0;
	for (i = 0; i < 9; i++){
		data[i] = (*bus_).receiveByte();
	}
	return calcCRC8(data, 9);	
}

uint16_t OwDS18B20::getTemp(){
	return (uint16_t)((data[1] << 8) | data[0]) * 10 / 16;
}