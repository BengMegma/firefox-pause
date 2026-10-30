
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>
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

// ---- per-level percent ----
// Key that identifies a level: its online ID, or its name for local/editor levels.
static std::string levelKeyFor(GJGameLevel* level) {
	if (!level) return "";
	int id = level->m_levelID.value();
	if (id > 0) return fmt::format("id-{}", id);
	return fmt::format("local-{}", std::string(level->m_levelName));
}

// Last value we wrote into the "percent" setting ourselves (so we don't
// mistake our own change for the user editing it).
static int64_t s_lastApplied = -1;

// Load this level's saved percent (or the default) into the "percent" setting.
static void applyPercentForLevel(GJGameLevel* level) {
	auto mod = Mod::get();
	int64_t value = mod->getSettingValue<int64_t>("default-percent");

	auto key = levelKeyFor(level);
	if (!key.empty()) {
		int64_t saved = mod->getSavedValue<int64_t>("level-percent-" + key, -1);
		if (saved >= 1 && saved <= 100) value = saved;
	}

	s_lastApplied = value;
	if (mod->getSettingValue<int64_t>("percent") != value) {
		mod->setSettingValue<int64_t>("percent", value);
	}
}

class $modify(FirefoxPausePlayLayer, PlayLayer) {
	struct Fields {
		bool m_sentPause = false;
	};

	void setupHasCompleted() {
		PlayLayer::setupHasCompleted();
		applyPercentForLevel(m_level);
	}

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

// Save the percent per level whenever it's changed while inside a level
$on_mod(Loaded) {
	listenForSettingChanges<int64_t>("percent", [](int64_t value) {
		if (value == s_lastApplied) return;   // our own change
		auto pl = PlayLayer::get();
		if (!pl || !pl->m_level) return;      // not in a level
		auto key = levelKeyFor(pl->m_level);
		if (key.empty()) return;
		Mod::get()->setSavedValue<int64_t>("level-percent-" + key, value);
		s_lastApplied = value;
	});

	// If the default changes while in a level that has no saved percent, follow it
	listenForSettingChanges<int64_t>("default-percent", [](int64_t) {
		auto pl = PlayLayer::get();
		if (pl && pl->m_level) applyPercentForLevel(pl->m_level);
	});
}

// Pause-menu button: opens this mod's settings
class $modify(FirefoxPausePauseLayer, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		CCSprite* spr = CCSprite::createWithSpriteFrameName("GJ_musicOnBtn_001.png");
		if (!spr) spr = ButtonSprite::create("FP");
		spr->setScale(0.8f);
		// dim the icon while the mod is turned off
		if (!Mod::get()->getSettingValue<bool>("enabled")) spr->setOpacity(120);

		auto btn = CCMenuItemSpriteExtra::create(
			spr, this, menu_selector(FirefoxPausePauseLayer::onFirefoxPauseSettings)
		);
		btn->setID("firefox-pause-settings"_spr);

		CCNode* menu = this->getChildByID("right-button-menu");
		if (!menu) {
			// couldn't find the vanilla menu: make our own in the bottom-right corner
			auto win = CCDirector::get()->getWinSize();
			auto newMenu = CCMenu::create();
			newMenu->setPosition({win.width - 30.f, 90.f});
			this->addChild(newMenu);
			menu = newMenu;
		}
		menu->addChild(btn);
		menu->updateLayout();
	}

	void onFirefoxPauseSettings(CCObject*) {
		openSettingsPopup(Mod::get(), false);
	}
};
