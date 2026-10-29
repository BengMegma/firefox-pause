# Firefox Pause (Geode mod)

Pauses Firefox media when you pass a chosen percent in a level. Resumes on death / exit.

## 1. Linux helper (Arch)
    sudo pacman -S playerctl python
    mkdir -p ~/.local/bin ~/.config/systemd/user
    cp helper/firefox_pause_helper.py ~/.local/bin/ && chmod +x ~/.local/bin/firefox_pause_helper.py
    cp helper/firefox-pause-helper.service ~/.config/systemd/user/
    systemctl --user enable --now firefox-pause-helper

Test without the game:  `echo -n pause | nc -u -w1 127.0.0.1 48620`
(Firefox needs `media.hardwaremediakeys.enabled = true` in about:config, the default.)

## 2. Build the mod
GD on Linux runs the *Windows* build, so build for Windows (Geode CLI cross-compile
via clang-cl + xwin, or use the Geode GitHub Actions template):
    geode sdk install   # if you haven't
    geode build --platform win   # or push to GitHub and let Actions build it
Put the resulting `yourname.firefox-pause.geode` in your GD `geode/mods` folder.

## 3. Configure
In-game: Geode > Mods > Firefox Pause > Settings. Set "Pause at percent".
Change the port in both the mod settings and the helper (`firefox_pause_helper.py 12345`) if needed.
