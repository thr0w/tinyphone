#include "stdafx.h"
#include "phone.h"
#include "metrics.h"
#include "utils.h"
#include "json.h"
#include "config.h"
#include <iomanip>

using json = nlohmann::json;

namespace tp {

	void TinyPhone::SetCodecs() {
		pjsua_codec_info codec[32];
		ZeroMemory(codec, sizeof(codec));
		unsigned uCount = 32;
		if (pjsua_enum_codecs(codec, &uCount) == PJ_SUCCESS) {
			for (unsigned i = 0; i<uCount; ++i) {
				PJ_LOG(3, (__FILENAME__, "supported codec  %d\t%.*s", codec[i].priority, (int)codec[i].codec_id.slen, codec[i].codec_id.ptr));
			}
		}

		const pj_str_t ID_ALL = { "*", 1 };
		pjsua_codec_set_priority(&ID_ALL, PJMEDIA_CODEC_PRIO_DISABLED);
		int priority = 0;
		BOOST_FOREACH(std::string codec, ApplicationConfig.audioCodecs) {
			EnableCodec(codec, PJMEDIA_CODEC_PRIO_NORMAL);
		}
	}

	void TinyPhone::EnableCodec(std::string codec_name, pj_uint8_t priority) {
		auto codec = pj_str(codec_name);
		auto status = pjsua_codec_set_priority(&codec, priority);
		if (status == PJ_SUCCESS)
			PJ_LOG(3, (__FILENAME__, "%s activated, priority %d", codec.ptr, priority));
		else
			PJ_LOG(3, (__FILENAME__, "Failed activating %s, err=%d", codec.ptr, status));
		free(codec.ptr);
	}

	bool TinyPhone::TestAudioDevice() {
		try {
			AudDevManager& audio_manager = Endpoint::instance().audDevManager();
			audio_manager.refreshDevs();
			AudioMedia& cap_med = audio_manager.getCaptureDevMedia();
			AudioMedia& play_med = audio_manager.getPlaybackDevMedia();
			cap_med.startTransmit(play_med);
			pj_thread_sleep(50);
			cap_med.stopTransmit(play_med);
			return true;
		}
		catch (Error& err) {
			PJ_LOG(1, (__FILENAME__, "TestAudioDevice Error %s", err.reason.c_str()));
			return false;
		}
	}

	std::vector<SIPAccount *> TinyPhone::Accounts() {
		std::vector<SIPAccount *> acc_vector;
		BOOST_FOREACH(map_string_acc::value_type &pair, accounts ){
			acc_vector.push_back(pair.second);
		}
		return acc_vector;
	}

	bool TinyPhone::HasAccounts() {
		return accounts.size() > 0;
	}

	SIPAccount* TinyPhone::PrimaryAccount() {
		if (!HasAccounts())
			return nullptr;
		else {
			BOOST_FOREACH(map_string_acc::value_type &pair, accounts ){
				if (pair.second->getInfo().regStatus == pjsip_status_code::PJSIP_SC_OK)
					return pair.second;
			}
			return nullptr;
		}
	}

	SIPAccount* TinyPhone::AccountByName(string name) {
		auto it = accounts.find(name);
		if ( it != accounts.end()){
			return it->second;
		} else {
			return nullptr;
		}
	}

	void TinyPhone::Logout(SIPAccount* acc) throw(pj::Error) {
		synchronized(add_acc_mutex) {
			try {
				acc->UnRegister();
			}
			catch (Error& err) {
				PJ_LOG(1, (__FILENAME__, "Logout Account UnRegister Error %s", err.reason.c_str()));
			}
			accounts.erase(acc->Name());
			delete (acc);
			// SaveAccounts();
		}
	}

	int TinyPhone::Logout() throw(pj::Error) {
		int loggedOutAccounts(0);
		synchronized(add_acc_mutex) {
			auto it = accounts.begin();
			while (it != accounts.end()) {
				SIPAccount* acc = it->second;
				auto callCount = acc->calls.size();
				if (callCount > 0) {
					PJ_LOG(1, (__FILENAME__, "Skipping UnRegister for %s as calls active %d", acc->Name().c_str(), callCount));
					it++;
					continue;
				}
				try {
					acc->UnRegister();
				}
				catch (Error& err) {
					PJ_LOG(1, (__FILENAME__, "Logout UnRegister Error %s", err.reason.c_str()));
				}
				delete (acc);
				loggedOutAccounts++;
				it = accounts.erase(it);
			}			
			// SaveAccounts();
		}
		return loggedOutAccounts;
	}

	void TinyPhone::EnableAccount(SIPAccount* account) throw (std::exception) {
		if (ApplicationConfig.testAudioDevice && !TestAudioDevice()) {
			throw std::domain_error(MSG_DEVICE_ERROR);
		}
		account->reRegister();
	}

	
	SIPCall* TinyPhone::MakeCall(string uri) throw(pj::Error) {
		SIPAccount* account = PrimaryAccount();
		return MakeCall(uri, account);
	}

	SIPCall* TinyPhone::MakeCall(string uri, SIPAccount* account) throw(pj::Error) {
		addTransportSuffix(uri);
		SIPCall *call = new SIPCall(*account);
		CallOpParam prm(true);
		prm.opt.audioCount = 1;
		prm.opt.videoCount = 0;
		call->makeCall(uri, prm);
		account->calls.push_back(call);
		account->onCallEstablished(call);
		return call;
	}

	std::vector<SIPCall*>TinyPhone::Calls() {
		std::vector<SIPCall *> calls;
		BOOST_FOREACH(map_string_acc::value_type &pair, accounts) {
			auto account_calls = (pair.second)->calls;
			calls.insert(std::end(calls), std::begin(account_calls), std::end(account_calls));
		}
		return calls;
	}

	SIPCall* TinyPhone::CallById(int call_id) {
		BOOST_FOREACH(SIPCall* c, Calls()) {
			if (c->getId() == call_id)
				return c;
		}
		return nullptr;
	}

	void TinyPhone::HoldOtherCalls(SIPCall* call) {
		auto all_calls = Calls();
		all_calls.erase(std::remove(all_calls.begin(), all_calls.end(), call), all_calls.end());
		BOOST_FOREACH(SIPCall* c, all_calls) {
			c->HoldCall();
		}
	}

	void TinyPhone::Hangup(SIPCall* call) {
		call->Hangup();
	}

	void TinyPhone::HangupAllCalls() {
		endpoint->hangupAllCalls();
	}

	void TinyPhone::ConfigureAudioDevices(){
		AudDevManager& audio_manager = Endpoint::instance().audDevManager();
		audio_manager.refreshDevs();
		AudioDevInfoVector devices = audio_manager.enumDev();
		PJ_LOG(3, (__FILENAME__, "Refreshed Device Count: %d", (int)devices.size()));
		int dev_idx = 0;
		BOOST_FOREACH(AudioDevInfo* info, devices) {
			PJ_LOG(3, (__FILENAME__, "Found Device #%d %s", dev_idx++, info->name.c_str()));
		}
		if (ApplicationConfig.useDefaultAudioDevice) {
			PJ_LOG(3, (__FILENAME__, "Using Default Audio Device(s)"));
			input_audio_dev = -1;
			output_audio_dev = -2;
		} else {
			BOOST_FOREACH(string& search_string, ApplicationConfig.prefferedAudioDevices) {
				int dev_idx = 0;
				BOOST_FOREACH(AudioDevInfo* info, devices) {
					string dev_name = info->name;
					boost::to_lower(dev_name);
					if (dev_name.find(search_string) != string::npos) {
						if (info->inputCount > 0 && input_audio_dev <= 0) {
							input_audio_dev = dev_idx;
							PJ_LOG(3, (__FILENAME__, "Selected Input #%d %s", dev_idx, info->name.c_str()));
						}
						if (info->outputCount > 0 && output_audio_dev <= 0) {
							output_audio_dev = dev_idx;
							PJ_LOG(3, (__FILENAME__, "Selected Output #%d %s", dev_idx, info->name.c_str()));
						}
					}
					dev_idx++;
				}
			}
		}
		audio_manager.setCaptureDev(input_audio_dev);
		audio_manager.setPlaybackDev(output_audio_dev);
	}

	void TinyPhone::refreshDevices() {
		PJ_LOG(4, (__FILENAME__, "Refreshing Audio Devices"));
		ConfigureAudioDevices();
	}

	bool TinyPhone::RestoreAccounts(){
		if(ApplicationConfig.persistAccounts){
			std::ifstream ucfg(userConfigFile);

			if (!ucfg.is_open()) {
				PJ_LOG(3, (__FILENAME__, "UserAccount ConfigFile Not Found, Nothing to Restore %s",userConfigFile.c_str()));
				return false;
			}

			json j;
			try{
				ucfg >> j;
				ucfg.close();
				tp::tpUserConfig uc = j.get<tpUserConfig>();

				PJ_LOG(3, (__FILENAME__, "Restoring User Accounts %d", uc.accounts.size()));
				try
				{
					BOOST_FOREACH(AccountConfig acfg,uc.accounts) {
						AddAccount(acfg);
					}
				}
				catch(const std::domain_error e)
				{
					PJ_LOG(3, (__FILENAME__, "Restoring User Accounts Error %s", e.what()));
					tp::MetricsClient.increment("api.login.error.device_error");
					if (ApplicationConfig.deviceErrorAlert) {
						tp::DisplayError(MSG_CONTACT_IT_SUPPORT, tp::OPS::ASYNC);
					}
				}
			} catch(...) {
				PJ_LOG(3, (__FILENAME__, "Restoring User Failed due to Error"));
				return false;
			}
		}
		return true;
	}

	bool TinyPhone::SaveAccounts(){
		if(ApplicationConfig.persistAccounts){
			tp::tpUserConfig uc;
			BOOST_FOREACH(SIPAccount* acc, Accounts()){
				uc.accounts.push_back(acc->accConfig);
			}
			try
			{
				json j = uc;
				std::ofstream o(userConfigFile);
				o << std::setw(4) << j << std::endl;
				o.close();
				return true;
			} catch(const std::exception& e) {
				PJ_LOG(1, (__FILENAME__, "SaveAccounts:: Error %s", e.what()));
				return false;
			}
		}
		return true;
	}

	std::future<int> TinyPhone::AddAccount(AccountConfig& config) throw (std::exception) {
		string account_name = SIP_ACCOUNT_NAME(config.username, config.domain);
		tp::MetricsClient.increment("login");
		synchronized(add_acc_mutex){
			auto exits = AccountByName(account_name);
			if (exits != nullptr) {
				throw std::invalid_argument("Account already exists");
			}
			else {
				if (ApplicationConfig.testAudioDevice && !TestAudioDevice()) {
					throw std::domain_error(MSG_DEVICE_ERROR);
				}

				pj::AccountConfig acc_cfg;
				acc_cfg.idUri = ("sip:" + account_name);
				acc_cfg.regConfig.registrarUri = ("sip:" + config.domain);
				
				addTransportSuffix(acc_cfg.regConfig.registrarUri);
				AuthCredInfo authCred;
				authCred.username = config.username;
				authCred.realm = ApplicationConfig.realmAny ? "*" : "asterisk";
				authCred.scheme = "Digest";
				authCred.dataType = PJSIP_CRED_DATA_PLAIN_PASSWD;
				authCred.data = config.password;
				
				acc_cfg.sipConfig.authCreds.push_back(authCred);
				
				if (config.proxy.size() > 0) {
					acc_cfg.sipConfig.proxies = { config.proxy };
				}

				acc_cfg.sipConfig.authInitialEmpty = ApplicationConfig.authInitialEmpty;

				acc_cfg.regConfig.timeoutSec = ApplicationConfig.timeoutSec;
				acc_cfg.regConfig.delayBeforeRefreshSec = ApplicationConfig.refreshIntervalSec;
				acc_cfg.regConfig.retryIntervalSec = ApplicationConfig.retryIntervalSec;
				acc_cfg.regConfig.firstRetryIntervalSec = ApplicationConfig.firstRetryIntervalSec;
				acc_cfg.regConfig.dropCallsOnFail = ApplicationConfig.dropCallsOnFail;

				acc_cfg.videoConfig.autoTransmitOutgoing = PJ_FALSE;
				acc_cfg.videoConfig.autoShowIncoming = PJ_FALSE;

				acc_cfg.callConfig.timerUse = ApplicationConfig.timerUse; // ;
				acc_cfg.mediaConfig.srtpUse = ApplicationConfig.srtpUse; // PJMEDIA_SRTP_OPTIONAL, PJMEDIA_SRTP_MANDATORY, PJMEDIA_SRTP_DISABLED

				if (!ApplicationConfig.useStunServer) {
					acc_cfg.mediaConfig.transportConfig.publicAddress = ApplicationConfig.publicClientAddress;
				}
				acc_cfg.natConfig.iceEnabled = ApplicationConfig.iceEnabled;
				acc_cfg.natConfig.viaRewriteUse = ApplicationConfig.allowRewrite ? PJ_TRUE : PJ_FALSE;
				acc_cfg.natConfig.sdpNatRewriteUse = ApplicationConfig.allowRewrite ? PJ_TRUE : PJ_FALSE;
				acc_cfg.natConfig.contactRewriteUse = ApplicationConfig.allowRewrite ? 2 : PJ_FALSE;
				acc_cfg.presConfig.publishEnabled = ApplicationConfig.publish ? PJ_TRUE : PJ_FALSE;

				SIPAccount *acc(new SIPAccount(this, account_name, eventStream, config));
				acc->domain = config.domain;
				auto res = acc->Create(acc_cfg);

				accounts.insert(std::pair<string, SIPAccount*>(account_name, acc));

				// SaveAccounts();
				return res;
			}
		}
	}

	void TinyPhone::InitMetricsClient() {
		std::string productVersion;
		std::vector<std::string> tags{};
		int status;

		if (ApplicationConfig.enableMetrics) {

			std::string hostname = *tp::random(ApplicationConfig.metricsServerHosts.begin(), ApplicationConfig.metricsServerHosts.end());
			PJ_LOG(3, (__FILENAME__, "Creating Metrics Client to %s:%d over %s", hostname.c_str(), ApplicationConfig.metricsServerPort, ApplicationConfig.metricsProto.c_str()));

			if (ApplicationConfig.metricsProto == "TCP"){
				status = tp::MetricsClient.open(hostname, ApplicationConfig.metricsServerPort, SOCK_STREAM);
			} else {
				status = tp::MetricsClient.open(hostname, ApplicationConfig.metricsServerPort, SOCK_DGRAM);
			}
			if (status != 0) {
				PJ_LOG(2, (__FILENAME__, "Metrics Client create failed"));
			} else {
				tp::MetricsClient.setPrefix("tinyphone.");
				GetProductVersion(productVersion);
				tags.push_back(std::string("version=") + productVersion);
				statsd::setGlobalTags(tags);
				tp::MetricsClient.increment("launch");
			}
		} 
	}

}
