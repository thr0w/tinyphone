// tinyphone-cli.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <pjsua2.hpp>
#include <iostream>
#include <memory>
#include <pj/file_access.h>

#define THIS_FILE 	"pjsua2_demo.cpp"

#ifdef _DEBUG
#pragma comment(lib, "libpjproject-i386-Win32-vc14-Debug-Static.lib")
#else
#pragma comment(lib, "libpjproject-i386-Win32-vc14-Release-Static.lib")
#endif


using namespace pj;


class MyAccount;

class MyCall : public Call
{
private:
	MyAccount *myAcc;

public:
	MyCall(Account &acc, int call_id = PJSUA_INVALID_ID)
		: Call(acc, call_id)
	{
		myAcc = (MyAccount *)&acc;
	}

	virtual void onCallState(OnCallStateParam &prm);

	virtual void onCallMediaState(OnCallMediaStateParam &prm)
	{
		CallInfo ci = getInfo();

		const char *status_name[] = {
			"None",
			"Active",
			"Local hold",
			"Remote hold",
			"Error"
		};

		// Iterate all the call medias
		for (unsigned i = 0; i < ci.media.size(); i++) {
			if (ci.media[i].type == PJMEDIA_TYPE_AUDIO && getMedia(i)) {
				if (ci.media[i].status == PJSUA_CALL_MEDIA_ACTIVE || ci.media[i].status == PJSUA_CALL_MEDIA_REMOTE_HOLD) {
					AudioMedia *aud_med = (AudioMedia *)getMedia(i);
					// Connect the call audio media to sound device
					AudDevManager& mgr = Endpoint::instance().audDevManager();
					PJ_LOG(3, (THIS_FILE, "Connecting Call to Media Device Input #%d , Output # %d", mgr.getCaptureDev(), mgr.getPlaybackDev()));
					aud_med->startTransmit(mgr.getPlaybackDevMedia());
					mgr.getCaptureDevMedia().startTransmit(*aud_med);
				}
				else {
					pj_assert(ci.media[i].status <= PJ_ARRAY_SIZE(status_name));
					PJ_LOG(3, (THIS_FILE, "Call [%d] OnCallMediaState Media State %s", ci.id, status_name[ci.media[i].status]));
				}
			}
		}
	}


	bool HoldCall() {

		auto call_info = getInfo();
		if (call_info.state == PJSIP_INV_STATE_CONFIRMED) {
			//must have a local media
			if (call_info.media.size() > 0) {
				auto current_media = call_info.media.front();
				if (current_media.status != PJSUA_CALL_MEDIA_LOCAL_HOLD && current_media.status != PJSUA_CALL_MEDIA_NONE) {
					PJ_LOG(3, (THIS_FILE, "Call %d Hold Triggered", call_info.id));
					CallOpParam prm;
					prm.options = PJSUA_CALL_UPDATE_CONTACT;
					setHold(prm);
					//pjsua_call_set_hold(call_info.id, NULL);
					return true;
				}
				else
					PJ_LOG(3, (THIS_FILE, "Hold Failed, already on hold maybe?"));
			}
			else
				PJ_LOG(3, (THIS_FILE, "Hold Failed, Call Doesn't have any media"));
		}
		else
			PJ_LOG(3, (THIS_FILE, "Hold Failed, Call Not in Confirmed State"));
		return false;
	}

	bool UnHoldCall() {

		auto call_info = getInfo();
		if (call_info.state == PJSIP_INV_STATE_CONFIRMED) {
			if (call_info.media.size() > 0) {
				auto current_media = call_info.media.front();
				if (current_media.status == PJSUA_CALL_MEDIA_LOCAL_HOLD || current_media.status == PJSUA_CALL_MEDIA_NONE) {
					CallOpParam prm(true);
					prm.opt.audioCount = 1;
					prm.opt.videoCount = 0;
					prm.opt.flag |= PJSUA_CALL_UNHOLD;
					reinvite(prm);
					return true;
				}
				else
					PJ_LOG(3, (THIS_FILE, "UnHold Failed, already active maybe?"));
			}
			else
				PJ_LOG(3, (THIS_FILE, "UnHold Failed, Call Doesn't have any media"));
		}
		else
			PJ_LOG(3, (THIS_FILE, "UnHold Failed, Call Not in Confirmed State"));
		return false;
	}
};

class MyAccount : public Account
{
public:
	std::vector<Call *> calls;

public:
	MyAccount()
	{}

	~MyAccount()
	{
		std::cout << "*** Account is being deleted: No of calls="
			<< calls.size() << std::endl;
	}

	void removeCall(Call *call)
	{
		for (std::vector<Call *>::iterator it = calls.begin();
			it != calls.end(); ++it)
		{
			if (*it == call) {
				calls.erase(it);
				break;
			}
		}
	}

	virtual void onRegState(OnRegStateParam &prm)
	{
		AccountInfo ai = getInfo();
		std::cout << (ai.regIsActive ? "*** Register: code=" : "*** Unregister: code=")
			<< prm.code << std::endl;
	}

	virtual void onIncomingCall(OnIncomingCallParam &iprm)
	{
		Call *call = new MyCall(*this, iprm.callId);
		CallInfo ci = call->getInfo();
		CallOpParam prm;

		std::cout << "*** Incoming Call: " << ci.remoteUri << " ["
			<< ci.stateText << "]" << std::endl;

		calls.push_back(call);
		prm.statusCode = (pjsip_status_code)200;
		call->answer(prm);
	}

	
};

void MyCall::onCallState(OnCallStateParam &prm)
{
	PJ_UNUSED_ARG(prm);

	CallInfo ci = getInfo();
	std::cout << "*** Call: " << ci.remoteUri << " [" << ci.stateText
		<< "]" << std::endl;

	if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
		myAcc->removeCall(this);
		/* Delete the call */
		delete this;
	}
}


int main()
{

	int ret = 0;
	Endpoint ep;

	ep.libCreate();

	// Init library
	EpConfig ep_cfg;
	ep_cfg.logConfig.level = 4;
	ep.libInit(ep_cfg);

	// Transport
	TransportConfig tcfg;
	tcfg.port = 5060;
	ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg);

	// Start library
	ep.libStart();
	std::cout << "*** PJSUA2 STARTED ***" << std::endl;

	// Add account
	AccountConfig acc_cfg;
	acc_cfg.idUri = "sip:hello@preprod.fktel.ga";
	acc_cfg.regConfig.registrarUri = "sip:preprod.fktel.ga";
	acc_cfg.sipConfig.authCreds.push_back(AuthCredInfo("digest", "*","hello", 0, "nopassword"));
	std::auto_ptr<MyAccount> acc(new MyAccount);
	acc->create(acc_cfg);

	pj_thread_sleep(2000);

	// Make outgoing call
	MyCall *call = new MyCall(*acc);
	acc->calls.push_back(call);
	CallOpParam prm(true);
	prm.opt.audioCount = 1;
	prm.opt.videoCount = 0;
	call->makeCall("sip:+917829869145@preprod.fktel.ga", prm);



	/* Wait until user press "q" to quit. */
	for (;;) {
		char option[10];

		puts("Press 'h' to hangup all calls, H, R to hold/resume, 'q' to quit");
		if (fgets(option, sizeof(option), stdin) == NULL) {
			puts("EOF while reading stdin, will quit now..");
			break;
		}

		if (option[0] == 'q')
			break;

		if (option[0] == 'H') {
			call->HoldCall();
		}


		if (option[0] == 'R') {
			call->UnHoldCall();
		}
			
		if (option[0] == 'h') {
			ep.hangupAllCalls();
		}
			
	}


	// Destroy library
	std::cout << "*** PJSUA2 SHUTTING DOWN ***" << std::endl;


	try {
		ep.libDestroy();
	}
	catch (Error &err) {
		std::cout << "Exception: " << err.info() << std::endl;
		ret = 1;
	}

    return 0;
}

