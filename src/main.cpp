
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <winsock.h>

using namespace geode::prelude;

// Fire-and-forget UDP message to the native Linux helper on localhost.
static void sendCommand(char const* cmd) {
	static bool wsaReady = false;
	if (!wsaReady) {
		WSADATA data;
		if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
			log::warn("WSAStartup failed");
			return;
		}
		wsaReady = true;
	}

	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET) return;

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<u_short>(Mod::get()->getSettingValue<int64_t>("port")));
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	sendto(sock, cmd, static_cast<int>(strlen(cmd)), 0,
	       reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	closesocket(sock);
}

class $modify(FirefoxPausePlayLayer, PlayLayer) {
	struct Fields {
		bool m_sentPause = false;
	};

	void resumeIfNeeded(char const* settingKey) {
		if (!m_fields->m_sentPause) return;
		m_fields->m_sentPause = false;
		if (Mod::get()->getSettingValue<bool>(settingKey)) {
			sendCommand("play");
		}
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);

		auto mod = Mod::get();
		if (!mod->getSettingValue<bool>("enabled")) return;
		if (m_fields->m_sentPause) return;
		if (m_player1 && m_player1->m_isDead) return;

		int threshold = static_cast<int>(mod->getSettingValue<int64_t>("percent"));
		if (this->getCurrentPercent() >= static_cast<float>(threshold)) {
			m_fields->m_sentPause = true;
			sendCommand("pause");
		}
	}

	void resetLevel() {
		resumeIfNeeded("resume-on-death");
		PlayLayer::resetLevel();
	}

	void levelComplete() {
		PlayLayer::levelComplete();
		resumeIfNeeded("resume-on-exit");
	}

	void onQuit() {
		resumeIfNeeded("resume-on-exit");
		PlayLayer::onQuit();
	}
};
