import json, re, sys

# ---- mod.json: add the keybind setting right after "enabled" ----
with open("mod.json") as f:
    mod = json.load(f)

if "toggle-keybind" not in mod["settings"]:
    new = {}
    for k, v in mod["settings"].items():
        new[k] = v
        if k == "enabled":
            new["toggle-keybind"] = {
                "type": "keybind",
                "name": "Toggle keybind",
                "description": "Press this key to turn Firefox Pause on or off while playing.",
                "default": "F8",
                "category": "gameplay"
            }
    mod["settings"] = new
    with open("mod.json", "w") as f:
        json.dump(mod, f, indent="\t")
        f.write("\n")

# ---- src/main.cpp ----
with open("src/main.cpp") as f:
    s = f.read()

if "toggle-keybind" not in s:
    s = s.replace(
        "#include <Geode/modify/PlayLayer.hpp>\n",
        "#include <Geode/modify/PlayLayer.hpp>\n#include <Geode/loader/GameEvent.hpp>\n#include <Geode/loader/SettingV3.hpp>\n",
        1)

    old = '\t\tif (!mod->getSettingValue<bool>("enabled")) return;\n'
    new = ('\t\tif (!mod->getSettingValue<bool>("enabled")) {\n'
           '\t\t\t// turned off (e.g. via keybind): forget any pause we sent\n'
           '\t\t\tm_fields->m_sentPause = false;\n'
           '\t\t\treturn;\n'
           '\t\t}\n')
    if old not in s:
        sys.exit("Could not find the 'enabled' check in main.cpp - paste your file and I'll adapt the patch.")
    s = s.replace(old, new, 1)

    s = s.rstrip("\n") + '''

// Keybind: toggle the mod on/off while playing
$on_game(Loaded) {
	listenForKeybindSettingPresses("toggle-keybind", [](Keybind const&, bool down, bool repeat, double) {
		if (!down || repeat) return;

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
	});
}
'''
    with open("src/main.cpp", "w") as f:
        f.write(s)

print("patched OK")
