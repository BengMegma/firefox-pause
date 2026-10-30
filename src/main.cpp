
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
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

// Flip the "enabled" setting (used by the keybind and the pause-menu button)
static void toggleFirefoxPause() {
	auto mod = Mod::get();
	bool nowEnabled = !mod->getSettingValue<bool>("enabled");
	mod->setSettingValue<bool>("enabled", nowEnabled);

	if (!nowEnabled) {
		// helper only resumes players it paused itself, so this is safe
		sendCommand("play");
	}

	Notification::create(
		nowEnabled ? "Firefox Pause: ON" : "Firefox Pause: OFF",
		nowEnabled ? NotificationIcon::Success : NotificationIcon::Warning,
		1.0f
	)->show();
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
		if (!mod->getSettingValue<bool>("enabled")) {
			// turned off (e.g. via keybind): forget any pause we sent
			m_fields->m_sentPause = false;
			return;
		}
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

// Keybind: toggle the mod on/off while playing
$on_game(Loaded) {
	listenForKeybindSettingPresses("toggle-keybind", [](Keybind const&, bool down, bool repeat, double) {
		if (down && !repeat) toggleFirefoxPause();
	});
}

// Pause-menu button
class $modify(FirefoxPausePauseLayer, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		CCMenuItemToggler* toggle = nullptr;
		auto onSpr = CCSprite::createWithSpriteFrameName("GJ_musicOnBtn_001.png");
		auto offSpr = CCSprite::createWithSpriteFrameName("GJ_musicOffBtn_001.png");
		if (onSpr && offSpr) {
			onSpr->setScale(0.8f);
			offSpr->setScale(0.8f);
			toggle = CCMenuItemToggler::create(
				offSpr, onSpr, this, menu_selector(FirefoxPausePauseLayer::onToggleFirefoxPause)
			);
		}
		else {
			// sprite frames not found: fall back to the standard checkbox
			toggle = CCMenuItemToggler::createWithStandardSprites(
				this, menu_selector(FirefoxPausePauseLayer::onToggleFirefoxPause), 0.7f
			);
		}
		toggle->toggle(Mod::get()->getSettingValue<bool>("enabled"));
		toggle->setID("firefox-pause-toggle"_spr);

		CCNode* menu = this->getChildByID("right-button-menu");
		if (!menu) {
			// couldn't find the vanilla menu: make our own in the bottom-right corner
			auto win = CCDirector::get()->getWinSize();
			auto newMenu = CCMenu::create();
			newMenu->setPosition({win.width - 30.f, 90.f});
			this->addChild(newMenu);
			menu = newMenu;
		}
		menu->addChild(toggle);
		menu->updateLayout();
	}

	void onToggleFirefoxPause(CCObject*) {
		// the toggler flips its own visual; we just flip the setting
		toggleFirefoxPause();
	}
};
