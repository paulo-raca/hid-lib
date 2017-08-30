#ifdef __linux__

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include "hiddev/uhid.h"

#define LOG(fmt, ...) fprintf(stderr, "uhid: " fmt "\n", ##__VA_ARGS__)

static hiddev::ReportType mapReportType(uint8_t reportType) {
	if (reportType == UHID_INPUT_REPORT)
		return hiddev::ReportType::Input;
	if (reportType == UHID_OUTPUT_REPORT)
		return hiddev::ReportType::Output;
	if (reportType == UHID_FEATURE_REPORT)
		return hiddev::ReportType::Feature;
	return hiddev::ReportType::Invalid;
}





hiddev::UhidDriver::UhidDriver(hiddev::HidDevice &device, bool open)
: HidDriver(device)
{
	fd = -1;
	if (open) {
		this->open();
	}
}

hiddev::UhidDriver::~UhidDriver() {
	close();
}

bool hiddev::UhidDriver::open() {
	const uint8_t* hidDescriptor = nullptr;
	uint16_t hidDescriptorLength = 0;
	device.getDescriptor(hidDescriptor, hidDescriptorLength);

	fd = ::open("/dev/uhid", O_RDWR);
	if (fd < 0) {
		LOG("Failed to open dev/uhid");
		return false;
	}

	struct uhid_event ev;
	ev.type = UHID_CREATE2;
	memset(&ev.u.create2, 0, sizeof(ev.u.create2));
	setDeviceAttributes(ev.u.create2);
	ev.u.create2.rd_size = hidDescriptorLength;
	memcpy(ev.u.create2.rd_data, hidDescriptor, hidDescriptorLength);

	//Send CREATE2 event
	if (write(fd, &ev, sizeof(ev)) != sizeof(ev)) {
		LOG("Failed to send UHID_CREATE2 event: Wrong size");
		close();
		return false;
	}

	LOG("Opened");
	return true;
}

bool hiddev::UhidDriver::close() {
	if (fd >= 0) {
		::close(fd);
		fd = -1;
		LOG("Closed");
		return true;
	} else {
		return false;
	}
}

int hiddev::UhidDriver::getFD() {
	return fd;
}

hiddev::UhidDriver::operator bool() {
	return fd >= 0;
}

void hiddev::UhidDriver::setDeviceAttributes(struct uhid_create2_req &attributes) {
	strcpy((char*)attributes.name, "UHID Device");
}

bool hiddev::UhidDriver::sendInputReport(uint8_t reportNum, const uint8_t* reportBuffer, uint16_t reportSize) {
	if (fd < 0)
		return false;

	struct uhid_event ev;
	ev.type = UHID_INPUT2;
	if (device.isNumberedReport(ReportType::Input)) {
		ev.u.input2.size = reportSize+1;
		ev.u.input2.data[0] = reportNum;
		memcpy(ev.u.input2.data + 1, reportBuffer, reportSize);
	} else {
		ev.u.input2.size = reportSize;
		memcpy(ev.u.input2.data, reportBuffer, reportSize);
	}

	if (write(fd, &ev, sizeof(ev)) != sizeof(ev)) {
		LOG("Failed to send UHID_INPUT2 event: Wrong size");
		close();
		return false;
	}

	return true;
}

bool hiddev::UhidDriver::handleMessage() {
	if (fd < 0)
		return false;

	struct uhid_event ev;
	if (read(fd, &ev, sizeof(ev)) != sizeof(ev)) {
		LOG("Failed to read event: Wrong size");
		close();
		return false;
	}

	switch (ev.type) {
		case UHID_START:
		case UHID_STOP: {
			// Those can be ignored
			return true;
		}

		case UHID_OPEN: {
			device.start();
			return true;
		}

		case UHID_CLOSE: {
			device.stop();
			return true;
		}

		case UHID_OUTPUT: {
			ReportType reportType = mapReportType(ev.u.output.rtype);
			bool isNumberedReport = device.isNumberedReport(reportType);
			uint8_t reportNum = isNumberedReport ? ev.u.output.data[0] : 0;
			uint8_t* reportBuffer = isNumberedReport ? ev.u.output.data + 1 : ev.u.output.data;
			uint16_t reportLength = isNumberedReport ? ev.u.output.size - 1 : ev.u.output.size;

			device.setReport(reportType, reportNum, reportBuffer, reportLength);
			return true;
		}

		case UHID_GET_REPORT: {
			uint32_t id =  ev.u.get_report.id;
			uint8_t reportNum = ev.u.get_report.rnum;
			ReportType reportType = mapReportType(ev.u.get_report.rtype);
			bool isNumberedReport = device.isNumberedReport(reportType);
			const uint8_t* reportBuffer = nullptr;
			uint16_t reportLength = 0;
			bool ret = device.getReport(reportType, reportNum, reportBuffer, reportLength);

			ev.type = UHID_GET_REPORT_REPLY;
			ev.u.get_report_reply.id = id;
			if (!ret) {
				ev.u.get_report_reply.err = EIO;
				ev.u.get_report_reply.size = 0;
			} else {
				ev.u.get_report_reply.err = 0;
				if (isNumberedReport) {
					ev.u.get_report_reply.size = reportLength + 1;
					ev.u.get_report_reply.data[0] = reportNum;
					memcpy(ev.u.get_report_reply.data + 1, reportBuffer, reportLength);
				} else {
					ev.u.get_report_reply.size = reportLength;
					memcpy(ev.u.get_report_reply.data, reportBuffer, reportLength);
				}
			}

			if (write(fd, &ev, sizeof(ev)) != sizeof(ev)) {
				LOG("Failed to send UHID_GET_REPORT_REPLY event: Wrong size");
				close();
				return false;
			}
			return true;
		}

		case UHID_SET_REPORT: {
			uint32_t id =  ev.u.set_report.id;
			uint8_t reportNum = ev.u.set_report.rnum;
			ReportType reportType = mapReportType(ev.u.set_report.rtype);
			bool isNumberedReport = device.isNumberedReport(reportType);
			const uint8_t* reportBuffer = ev.u.set_report.data;
			uint16_t reportLength = ev.u.set_report.size;
			bool ret = device.setReport(reportType, reportNum, reportBuffer, reportLength);

			ev.type = UHID_SET_REPORT_REPLY;
			ev.u.set_report_reply.id = id;
			if (!ret) {
				ev.u.get_report_reply.err = EIO;
			} else {
				ev.u.get_report_reply.err = 0;
			}

			if (write(fd, &ev, sizeof(ev)) != sizeof(ev)) {
				LOG("Failed to send UHID_SET_REPORT_REPLY event: Wrong size");
				close();
				return false;
			}
			return true;
		}
	}

}

bool hiddev::UhidDriver::handleMessageLoop() {
	 // Handle messages while no error conditions arises
	while (handleMessage());
	// Some error condition happened
	return false;
}

#endif  // ifdef __linux__
