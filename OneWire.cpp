/*
* OneWire.cpp
*
* Created: 05.11.2021 15:12:18
* Author : Manuel
*/

//#define OVERDRIVE

#ifndef OVERDRIVE	    // Time definition from Maxim application node
    #define _timeA 6    // Write: time for "1" bit
    #define _timeB 54   // Write: time keep bus high after writing "1" bit (64us - 10us = 54us)
    #define _timeC 54   // Write: time for "0" bit (60us - 6us = 54us)
    #define _timeD 10   // Write: time keep bus high after writing "0" bit
    #define _timeE 9    // Read: time until sample input state
    #define _timeF 55   // Read: time for recovery after reading
    #define _timeH 480  // Reset: time for drive bus low
    #define _timeI 70   // Reset: time until sample input state
    #define _timeG 0    // Reset: delay before reset pulse
    #define _timeJ 410  // Reset: time for become bus high after present pulse
#else
    #define	_timeH 70   // Reset: time for drive bus low
    #define	_timeI 8.5  // Reset: time for release bus
    #define	_timeG 2.5  // Reset: delay before reset pulse
#endif

#define MATCH_ROM       0x55
#define SKIP_ROM        0xCC
#define READ_SCRATCH    0xBE
#define CONVERT_T       0x44
#define CONVERT_V       0xB4
#define WRITE_SCRATCH   0x4E
#define RECALL_MEMORY   0xB8
#define COPY_SCRATCH    0x48

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
    116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53
};

//---------------------------------------------------------------------------------
// Class OneWire Interface
//---------------------------------------------------------------------------------
//
OwInterface::OwInterface(volatile uint8_t* ddir, volatile uint8_t* port, volatile uint8_t* pinr, const uint8_t pin){
    ddir_ = (uint8_t*)ddir;
    port_ = (uint8_t*)port;
    pinr_ = (uint8_t*)pinr;
    pin_ = (1 << pin);      // save configured pin number
    *ddir_ &= (~pin_);      // config pin as input
    *port_ &= (~pin_);      // set output register drive pin low
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

//---------------------------------------------------------------------------------
// Class OneWire Bus
//---------------------------------------------------------------------------------
//
OwBus::OwBus(OwInterface& interface){
    interface_ = &interface;
}

bool OwBus::reset(){
    return (*interface_).reset();
}

void OwBus::sendByte(uint8_t bData){
    int i;
    for (i = 0; i < 8; i++){                    // <-Loop to write every bit into
        (*interface_).writeBit(bData & 0x01);   //   the byte, LSB first
        bData >>= 1;                            // <-shifts the data bits for the
    }                                           //   next bit
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

//---------------------------------------------------------------------------------
// Class OneWire Device
//---------------------------------------------------------------------------------
//
OwDevice::OwDevice(OwBus& bus){
    bus_ = &bus;
    interface_ = (*bus_).interface_;
}

//---------------------------------------------------------------------------------
// Algorithm to searching present devices on the bus. 
// 
// return 1 - search successful and device found
// return 0 - no or no more device found
//
// ToDo:
//
bool OwDevice::search(){
	if (!(*interface_).reset() || lastDeviceFlag_){		// perform one wire reset
		lastDiscrepancy_ = 0;
		lastFamilyDiscrepancy_ = 0;
		lastDeviceFlag_ = false;
		return 0;
	}
	idBitNumber_ = 1;									// initialize variables for search
	lastZero_ = 0;
	byteMask_ = 0;
	bitMask_ = 1;
	(*bus_).sendByte(0xF0);								// start search with one wire command 0xF0
		
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
	if ((calcCRC8((uint8_t*)romNo_, 8)) == 0){			// (calcCRC8((uint8_t*)romNo_, 8))==0
		return 1;
	}
	else{
		lastDiscrepancy_ = 0;
		lastFamilyDiscrepancy_ = 0;
		lastDeviceFlag_ = false;
		return 0;
	}
}

void OwDevice::matchRom(uint8_t* address){
    uint8_t i = 0;
    
    (*bus_).sendByte(MATCH_ROM);
    for (i = 0; i < 8; i++){					// Loop to write ROM address to bus
    	(*bus_).sendByte(*(address + i));
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
//---------------------------------------------------------------------------------
// Writes data to the scratch pad and validates of correct transmission. Validation
// for correct transmission is optional and disable by default.
//
// parameter 1  - ROM address of OneWire device
// parameter 2  - byte to write in scratch pad
// parameter 3  - page of scratch pad which is to be written
// parameter 4  - enable/disable of validation for transmission (1 = enable)
//
// return 1 - transmission successful
// return 2 - reset fault
// return 3 - CRC fault
// return 4 - data transmission fault
//
uint8_t OwDevice::writeScratch(uint8_t* address, uint8_t data, uint8_t page, uint8_t valid){
	uint8_t i = 0;
	
	if (!(*interface_).reset()) return 2;			// write data to device
	if (!address){
		(*bus_).sendByte(SKIP_ROM);
	}
	else{
		matchRom(address);
	}
	(*bus_).sendByte(WRITE_SCRATCH);
	if (page != 255) (*bus_).sendByte(page);
	(*bus_).sendByte(data);
	
	if (valid){
		if (!(*interface_).reset()) return 2;		// Read and validate data from device
		if (!address){
			(*bus_).sendByte(SKIP_ROM);
		}
		else{
			matchRom(address);
		}
		(*bus_).sendByte(READ_SCRATCH);
		if (page != 255) (*bus_).sendByte(page);
		
		for (i = 0; i < 9; i++){
			data_[i] = (*bus_).receiveByte();
		}
		if (calcCRC8(data_, 9)) return 3;
		if (data_[0] != data) return 4;
	}
	return 1;
}
//---------------------------------------------------------------------------------
// Copies the scratch pad to target page in SRAM/EEPROM.
//
// parameter 1  - ROM address of OneWire device
// parameter 2  - destination page of VRAM/EEPROM register which is to be written
//
// return 1 - configuration successful
// return 2 - reset fault
//
uint8_t OwDevice::copyScratch(uint8_t* address, uint8_t page){
	if (!(*interface_).reset()) return 2;		// Copy scratch pad to SRAM/EEPROM
	if (!address){
		(*bus_).sendByte(SKIP_ROM);
	}
	else{
		matchRom(address);
	}
	(*bus_).sendByte(COPY_SCRATCH);
	(*bus_).sendByte(page);
	return 1;
}

//---------------------------------------------------------------------------------
// Class OneWire DS18B20
//---------------------------------------------------------------------------------
//
OwDS18B20::OwDS18B20(OwBus& bus) : OwDevice::OwDevice (bus){
	bus_ = &bus;
}

bool OwDS18B20::setResolution(uint8_t* address, uint8_t resolution){
	uint8_t commandSet[3] {0x0, 0x0, 0x0};
	int i = 0;

	if(!(*interface_).reset()) return 0;			// if reset fault return with 0
	if (!address){									// ROM address not set only select device with 0xCC (SkipROM)
		(*bus_).sendByte(SKIP_ROM);
	}
	else{
		matchRom(address);
	}
	(*bus_).sendByte(WRITE_SCRATCH);
	commandSet[2] = ((resolution - 9) << 5) + 0x1F;
	for (i = 0; i < 3; i++){						// Loop to write each bit in the byte, LS-bit first
		(*bus_).sendByte(commandSet[i]);			// shift the data byte for the next bit
	}
	if (getResolution(address) == resolution) return 1;
	return 0;
}

uint8_t OwDS18B20::getResolution(uint8_t* address){
	uint8_t data[9];
	int i;
	
	if(!(*interface_).reset()) return 0;			// if reset fault return with 0
	if (!address){									// ROM address not set only select device with 0xCC (SkipROM)
		(*bus_).sendByte(SKIP_ROM);
	}
	else{
		matchRom(address);
	}
	(*bus_).sendByte(READ_SCRATCH);

	
	for (i = 0; i < 9; i++){
		data[i] = (*bus_).receiveByte();
	}
	if (calcCRC8(data,9)) return 0;					// calculate CRC8 from data if unsuccessful return 0
	return (data[4] >> 5) + 9;						// if data correct return set resolution
}

bool OwDS18B20::convertT(uint8_t* address){
	if (!(*interface_).reset()) return false;
	if (!address){
		(*bus_).sendByte(SKIP_ROM);
	}
	else{
		matchRom(address);
	}
	(*bus_).sendByte(CONVERT_T);
	return true;
}

bool OwDS18B20::receiveTemp(uint8_t* address){
	int i = 0;
	
	if(!(*interface_).reset()) return 0;			// if reset fault return with 0
	if (!address){									// ROM address not set only select device with 0xCC (SkipROM)
		(*bus_).sendByte(SKIP_ROM);
	}
	else{
		matchRom(address);
	}
	(*bus_).sendByte(READ_SCRATCH);					// command to read scratch pad from device
	for (i = 0; i < 9; i++){						// receive data from sensor
		data_[i] = (*bus_).receiveByte();
	}
	return !calcCRC8(data_, 9);						// calculate CRC8 from data if successful return 1
}

uint16_t OwDS18B20::getTempRaw(){					// normalize data to range of 0 to 2880 corresponds -55 to +125°C 
	return (uint16_t)((data_[1] << 8) | data_[0]) + 880;
}

float OwDS18B20::getTempFloat(){					// calculate temperature in degrees Celsius from RAW value 
	return (float)getTempRaw() / 16.0 - 55.0;
}


//---------------------------------------------------------------------------------
// Class OneWire DS2438
//---------------------------------------------------------------------------------
//
OwDS2438::OwDS2438(OwBus& bus) : OwDevice::OwDevice (bus){ // Constructor
	bus_ = &bus;
}

//---------------------------------------------------------------------------------
// Writes the configuration to the scratch pad, validates of correct transmission
// and copies the scratch pad to target page in SRAM/EEPROM. Validation for correct
// transmission is optional and disable by default.
//
// parameter 1  - ROM address of one wire device
// parameter 2  - byte to write in register
// parameter 3  - page of VRAM/EEPROM register which is to be written
// parameter 4  - enable/disable of validation for transmission (1 = enable)
//
// return 1 - configuration successful
// return 2 - reset fault
// return 3 - CRC fault
// return 4 - data transmission fault
//
uint8_t OwDS2438::setConfiguration(uint8_t* address, uint8_t config, uint8_t page, uint8_t valid){
	uint8_t status;
	status = writeScratch(address, config, page, valid);
	if (status != 1) return status;
	return 1;
	/*
	status = copyScratch(address, page);
	return status;
	*/
}

//---------------------------------------------------------------------------------
// Sends temperature conversation command to bus.
//
// return true  - successful
// return false - reset fault
//
bool OwDS2438::convertV(uint8_t* address){
	if (!(*interface_).reset()) return false;
	if (!address){
		(*bus_).sendByte(SKIP_ROM);
	}
	else{
		matchRom(address);
	}
	(*bus_).sendByte(CONVERT_V);
	return true;
}

//---------------------------------------------------------------------------------
// Copies the source page from the SRAM/EEPROM to the target page in the
// scratchpad.
//
// return true  - successful
// return false - reset fault
//
bool OwDS2438::recallMemory(uint8_t* address, uint8_t page){
	if (!(*interface_).reset()) return false;
	if (!address){
		(*bus_).sendByte(SKIP_ROM);
	}
	else{
		matchRom(address);
	}
	(*bus_).sendByte(RECALL_MEMORY);
	(*bus_).sendByte(page);
	return true;
}

bool OwDS2438::receiveVolt(uint8_t* address, uint8_t page){
	int i = 0;
	
	if(!(*interface_).reset()) return 0;			// if reset fault return with 0
	if (!address){									// ROM address not set only select device with 0xCC (SkipROM)
		(*bus_).sendByte(SKIP_ROM);
	}
	else{
		matchRom(address);
	}
	(*bus_).sendByte(READ_SCRATCH);					// command to read scratch pad from device
	(*bus_).sendByte(page);
	for (i = 0; i < 9; i++){						// receive data from sensor
		data_[i] = (*bus_).receiveByte();
	}
	return !calcCRC8(data_, 9);						// calculate CRC8 from data if successful return 1
}

uint16_t OwDS2438::getVoltRaw(){					// normalize data to range of 0 to 2880 corresponds -55 to +125°C
	return (uint16_t)((data_[4] << 8) | data_[3]);
}

float OwDS2438::getVoltFloat(){					// calculate temperature in degrees Celsius from RAW value
	return (float)getVoltRaw() / 100.0;
}