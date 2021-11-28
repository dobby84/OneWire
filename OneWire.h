/*
* OneWire.h
*
* Created: 05.11.2021 16:54:35
*  Author: Manuel
*/
#ifndef ONEWIRE_H_
#define ONEWIRE_H_

#define F_CPU 16000000UL
#include <util/delay.h>
#include <util/delay_basic.h>

class OwDevice;

/*-------------------------------------------------------------------------------------------------
* OneWire Interface
*
* Class provide low level communication on a physical interface. The class handles interface
* configuration, reset routine and bit bang communication.
---------------------------------------------------------------------------------------------------
*/
class OwInterface{
	private:
		uint8_t* ddir_;		// Data direction register
		uint8_t* port_;		// Port out register
		uint8_t* pinr_;		// Port in register
		uint8_t  pin_;		// Pin of IO register
	public:
		bool reset();
		void writeBit(int);
		bool readBit();
		
		OwInterface(volatile uint8_t*, volatile uint8_t*,
		            volatile uint8_t*, const uint8_t);		// Constructor
		~OwInterface();										// Destructor
};

/*-------------------------------------------------------------------------------------------------
* OneWire Bus
*
* The class handles the byte communication on a specific interface. It provide byte send/receive
* function on one wire bus.
---------------------------------------------------------------------------------------------------
*/
class OwBus{
	private:
		OwInterface* interface_;	// pointer to binded interface
		bool singleDevice_;			// set bit means only one device is on bus
		bool isBussy_;
	public:
		bool reset();
		void setSingleDevice(bool);
		bool getSingleDevice();
		void sendByte(uint8_t);
		uint8_t receiveByte();
		// void sendStream(uint8_t);
		OwBus(OwInterface&);
		
		friend class OwDevice;

};

/*-------------------------------------------------------------------------------------------------
* OneWire Device
*
* The Class handles the communication to a specific one wire device. The parent class provide
* standard functions usable for any device on bus. The child classes provide special
* functions for a specific device like DS18B20.
---------------------------------------------------------------------------------------------------
*/
class OwDevice{
	protected:
		OwInterface* interface_;
		OwBus* bus_;				// pointer to binded bus
	private:
		uint8_t _instances = 0;
		//* variables for OneWire Search
		uint8_t lastDiscrepancy_ = 0;
		uint8_t lastFamilyDiscrepancy_ = 0;
		bool lastDeviceFlag_ = false;
		uint8_t idBitNumber_ = 0;
		uint8_t lastZero_ = 0;
		bool idBit_ = false;
		bool cmpIdBit_ = false;
		bool searchDirection_ = false;
		uint8_t bitMask_ = 1;
		uint8_t byteMask_ = 0;
	public:
		uint8_t romNo_[8];
	public:
		OwDevice(OwBus&);
		bool search();
		void resetSearch();
	protected:		
		uint8_t calcCRC8(const uint8_t*, uint8_t);
		uint8_t tableCRC8(uint8_t*, uint8_t);
};

class OwDS18B20 : private OwDevice{
	private:
		int temp;
		uint8_t data[9];
	public:
		bool setResolution(uint8_t);
		uint8_t getResolution();
		void convertTemp(bool);
		uint8_t receiveTemp(bool);
		uint16_t getTemp();
		OwDS18B20(OwBus&);
};

#endif /* ONEWIRE_H_ */