#pragma once

#include <inttypes.h>

namespace hiddev {

	enum class ReportType: uint8_t {
		Invalid = 0,
		Input = 1,
		Output = 2,
		Feature = 3
	};

	enum class Protocol: uint8_t {
		Boot = 0,
		Report = 1
	};

	class Device;
	class Driver;



	class Driver {
		friend Device;
	protected:
		Device &device;
		virtual bool sendInputReport(uint8_t reportNum, const uint8_t* reportBuffer, uint16_t reportSize) = 0;

	public:
		Driver(Device &device);
		~Driver();
	};



	class Device {
		friend Driver;
	protected:
		Driver* driver;
	public:
		// Returns the HID descriptor.
		// The buffers must be managed by the instance and will NOT be deallocated by the caller.
		virtual void getDescriptor(const uint8_t* &descriptorBuffer, uint16_t &descriptorSize) = 0;

		// Notifies that the device has been attached / dettached to/from the host
		virtual void start();
		virtual void stop();

		// Asynchronous IO
		bool sendInputReport(uint8_t reportNum, const uint8_t* reportBuffer, uint16_t reportSize);
		virtual bool receivedOutputReport(ReportType reportType, uint8_t reportNum, const uint8_t* reportBuffer, uint16_t reportSize);

		// Synchronous IO
		virtual bool getReport(ReportType reportType, uint8_t reportNum, uint8_t* reportBuffer, uint16_t &reportSize);
		virtual bool setReport(ReportType reportType, uint8_t reportNum, const uint8_t* reportBuffer, uint16_t reportSize);

		virtual bool getIdle(uint16_t &period_ms);
		virtual bool setIdle(uint16_t period_ms);

		virtual bool getProtocol(Protocol &protocol);
		virtual bool setProtocol(Protocol protocol);

		/**
		 * Returns whenever reports of a given ReportType are indexed by a reportNum
		 */
		virtual bool isNumberedReport(ReportType reportType) = 0;

		/**
		 * Returns the a report size for a given ReportType / ReportNum
		 * If the report is numbered and reportNum == 0, it should return the maximum report size for that reportType.
		 * If there is no such report, the result should be 0 
		 */
		virtual uint16_t getReportSize(ReportType reportType, uint8_t reportNum) = 0;
	};
}
