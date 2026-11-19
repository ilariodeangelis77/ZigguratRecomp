// ZiGGURAT launcher application: first-run wizard + tabbed settings + game launch
// (LAUNCHER_PLAN.md §5, §6, §7.1-7.3, §7.5, §7.10, §7.11).
#pragma once
#include "launcher/TestHooks.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/Config.h"
#include "core/InputMap.h"
#include "core/DumpImport.h"
#include "core/Steam.h"
#include "launcher/FileBrowser.h"
#include "launcher/screens/DataTab.h"
#include "launcher/screens/EnhancementsTab.h"
#include "launcher/screens/Extras.h"
#include "launcher/Launch.h"
#include "launcher/ui/ZigUI.h"

namespace zl {

struct Options {
    bool play = false, fullscreen = false, reimport = false, fromSteam = false, fallbackTheme = false;
    int scale = 0;
    std::string wizardDump;                    // test: simulate a drop in the wizard
    std::vector<std::pair<int, std::string>> shots;  // test: ms -> bmp file (exit after last)
    int startTab = -1;
    int startPage = -1;  // test-only (--page N): page of the start tab's page selector
    std::string steamRoot;                     // test/sandbox override for Add to Steam
};

// Background job (dump scan/verify/import) with progress.
struct Job {
    std::thread th;
    std::atomic<bool> running{false}, done{false}, cancel{false};
    std::atomic<int> cur{0}, total{0};
    std::mutex m;
    std::string item;
    void join() { if (th.joinable()) th.join(); }
};

class App {
public:
    int run(const Options& o);

private:
    enum class Screen { Wizard, Main };
    enum class Wiz { Welcome, Locate, Checking, Result, Importing, Setup, SteamAsk, Done };
    enum Tab { Home, Display, Audio, Controls, Language, Game, Data, Steam, About, TabCount };

    // frames
    void wizardFrame();
    void mainFrame();
    void tabHome(zui::Rect c);
    void tabDisplay(zui::Rect c);
    void tabAudio(zui::Rect c);
    void tabControls(zui::Rect c);
    void tabLanguage(zui::Rect c);
    void tabGame(zui::Rect c);
    void tabData(zui::Rect c);
    void tabAbout(zui::Rect c);
    void tabSteam(zui::Rect c);
    void overlays();
    void footer(bool tabs);

    // actions
    void startScan(const std::string& path);
    void startImport(bool allowUnverified);
    void onImported();
    void play(const std::vector<std::string>& extra = {});
    void startPreview();
    void pollGame();
    void saveConfig();
    void reloadTheme(bool crossfade);
    bool assetsReady() const;
    std::string msgText(const zig::DumpMessage& m) const;
    std::string supportInfo() const;
    void openFolder(const std::string& path);
    void enterWizard(Wiz step);
    void loadStats();

    // state
    Options opt_;
    zig::Config& cfg_ = zig::Config::get();
    Screen screen_ = Screen::Main;
    Wiz wiz_ = Wiz::Welcome;
    int tab_ = Home;
    bool quit_ = false;
    double now_ = 0;
    std::string fallbackFont_;

    // wizard
    Job job_;
    std::string dumpPath_;
    zig::ScanResult scan_;
    zig::VerifyResult verify_;
    zig::ImportResult import_;
    std::mutex dialogMutex_;
    std::string dialogResult_;
    bool dialogPending_ = false;
    FileBrowser browser_;
    DataTab dataTab_;   // L7: DATA tab pages YOUR RECORDS / SAVES
    Extras extras_;   // P2: DATA>MODS & EXTRAS, STEAM>EXPERIMENTAL, ABOUT>UPDATES
    EnhancementsTab enhTab_;   // L8: GAME tab pages ACCESSIBILITY / ORIGINAL BUGS / PRACTICE / DEVELOPER
    int setupPreset_ = 0;

    // game process
    GameProcess game_;
    bool previewRunning_ = false, askKeep_ = false, crash_ = false;
    int crashCode_ = 0;
    std::vector<std::string> crashLines_;
    zig::DisplayCfg displayBeforeTest_;
    std::string toast_;
    double toastUntil_ = 0;

    // display tab caches
    std::vector<std::string> monitors_, fsModes_;
    std::vector<int> scales_;
    std::vector<std::string> audioDevices_;
    void refreshDisplayLists();
    void refreshAudioDevices();

    // home stats
    int games_ = 0, bestKills_ = 0, totalKills_ = 0;
    float bestSeconds_ = 0;

    // theme crossfade
    zui::Tex fadeFrom_;
    double fadeStart_ = -10;
    bool confirmRemove_ = false;
    double musicStopAt_ = 0;
    std::string pendingFocus_;   // applied at the start of the next frame
    // controls tab (L5)
    int ctlPage_ = 0;
    std::string captureAction_;  // "pad:<action>" / "key:<action>" while waiting for a press
    double captureUntil_ = 0;
    uint32_t captureBaseline_ = 0;
    zig::InputMap testMap_;
    Uint64 testLastNs_ = 0;
    void controlsTest(zui::Rect area);
    zig::PadState readLauncherPad() const;
    void setFocus(const std::string& id) { pendingFocus_ = id; }
    // Steam tab (L6)
    void steamRefresh(bool regenArt);
    void steamAdd();
    zig::ShortcutSpec steamSpec() const;
    std::string steamArtDir() const;
    zig::SteamStatus steam_;
    zig::EntryStatus steamEntry_;
    int steamAcc_ = 0;
    double steamNextPoll_ = 0;
    bool steamAskClose_ = false, steamWaitClose_ = false;
    int steamPending_ = 0;   // action to run once Steam has closed: 1 add, 2 remove, 3 undo
    void steamDo(int action);
    void steamPoll();
    double steamWaitUntil_ = 0;
    std::vector<zui::Tex> steamThumbs_;   // portrait, wide, hero, icon
    bool steamArtReady_ = false;
    bool deskLink_ = false, startLink_ = false;
};

}  // namespace zl
