import re, sys

with open("src/main.cpp") as f:
    s = f.read()

if "FirefoxPausePauseLayer" in s:
    print("already patched"); sys.exit(0)

# 1. include PauseLayer hook
inc = "#include <Geode/modify/PlayLayer.hpp>\n"
if inc not in s:
    sys.exit("Couldn't find the PlayLayer include in main.cpp")
s = s.replace(inc, inc + "#include <Geode/modify/PauseLayer.hpp>\n", 1)

# 2. shared toggle helper, placed just before the PlayLayer hook
marker = "class $modify(FirefoxPausePlayLayer, PlayLayer) {"
if marker not in s:
    sys.exit("Couldn't find the PlayLayer hook in main.cpp")
helper = '''// Flip the "enabled" setting (used by the keybind and the pause-menu button)
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

'''
s = s.replace(marker, helper + marker, 1)

# 3. replace the old keybind block (from the previous patch) with one that uses the helper
start = s.find("// Keybind: toggle the mod on/off while playing")
if start == -1:
    sys.exit("Couldn't find the keybind block - did you apply patch_keybind.py first?")
s = s[:start].rstrip("\n") + '''

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
'''
with open("src/main.cpp", "w") as f:
    f.write(s)
print("patched OK")
