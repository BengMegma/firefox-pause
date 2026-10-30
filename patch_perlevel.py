import json, sys

# ---------------- mod.json ----------------
with open("mod.json") as f:
    mod = json.load(f)

st = mod["settings"]
if "default-percent" not in st:
    st["percent"]["name"] = "Pause at percent (current level)"
    st["percent"]["description"] = ("Percent at which Firefox pauses in the level you're playing. "
                                    "It's remembered separately for each level.")
    new = {}
    for k, v in st.items():
        new[k] = v
        if k == "percent":
            new["default-percent"] = {
                "type": "int",
                "name": "Default percent (new levels)",
                "description": "Used for levels you haven't set a percent for yet.",
                "default": 50,
                "min": 1,
                "max": 100,
                "control": {"slider": True, "input": True}
            }
    mod["settings"] = new
    with open("mod.json", "w") as f:
        json.dump(mod, f, indent="\t")
        f.write("\n")

# ---------------- src/main.cpp ----------------
with open("src/main.cpp") as f:
    s = f.read()

if "default-percent" in s:
    print("main.cpp already patched")
    sys.exit(0)

def need(text, what):
    if text not in s:
        sys.exit(f"Couldn't find {what} in main.cpp - did the earlier patches apply? "
                 "Run: grep -c FirefoxPausePauseLayer src/main.cpp")

# 1. include for openSettingsPopup
need("#include <Geode/modify/PauseLayer.hpp>\n", "the PauseLayer include")
s = s.replace("#include <Geode/modify/PauseLayer.hpp>\n",
              "#include <Geode/modify/PauseLayer.hpp>\n#include <Geode/ui/GeodeUI.hpp>\n", 1)

# 2. per-level helpers, just before the PlayLayer hook
marker = "class $modify(FirefoxPausePlayLayer, PlayLayer) {"
need(marker, "the PlayLayer hook")
helpers = '''// ---- per-level percent ----
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

'''
s = s.replace(marker, helpers + marker, 1)

# 3. apply when a level finishes loading
anchor = "\tvoid resumeIfNeeded(char const* settingKey) {"
need(anchor, "resumeIfNeeded")
s = s.replace(anchor, '''\tvoid setupHasCompleted() {
\t\tPlayLayer::setupHasCompleted();
\t\tapplyPercentForLevel(m_level);
\t}

''' + anchor, 1)

# 4. replace the pause-menu block (toggle button) with an "open settings" button,
#    and add the setting-change listeners
start = s.find("// Pause-menu button")
need("// Pause-menu button", "the pause-menu block")
s = s[:start].rstrip("\n") + '''

// Save the percent per level whenever it's changed while inside a level
$on_mod(Loaded) {
	listenForSettingChanges("percent", [](int64_t value) {
		if (value == s_lastApplied) return;   // our own change
		auto pl = PlayLayer::get();
		if (!pl || !pl->m_level) return;      // not in a level
		auto key = levelKeyFor(pl->m_level);
		if (key.empty()) return;
		Mod::get()->setSavedValue<int64_t>("level-percent-" + key, value);
		s_lastApplied = value;
	});

	// If the default changes while in a level that has no saved percent, follow it
	listenForSettingChanges("default-percent", [](int64_t) {
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
'''
with open("src/main.cpp", "w") as f:
    f.write(s)
print("patched OK")
