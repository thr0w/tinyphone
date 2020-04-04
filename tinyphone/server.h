#pragma once

#ifndef SERVER_HEADER_FILE_H
#define SERVER_HEADER_FILE_H

#include <iostream>
#include <fstream>
#include <pjsua2.hpp>
#include <chrono>	
#include "metrics.h"
#include "phone.h"

class TinyPhoneService {
public:
	pj::Endpoint* endpoint;
private:
	tp::TinyPhone* tinyPhone;
	std::atomic<bool> running;

public:
	TinyPhoneService(pj::Endpoint* ep) {
		endpoint = ep;
	}
	int deviceCount();
	int getDevice(int index);
};

#endif
 