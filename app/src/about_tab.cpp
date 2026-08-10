#include "about_tab.hpp"
#include <fmt/core.h>

#include <utility>

#ifdef __SWITCH__
#include <switch.h>
#endif

using namespace brls;

void openWebpage(std::string url) {
    Application::getPlatform()->openBrowser(std::move(url));
}

AboutTab::AboutTab() {
    this->inflateFromXMLRes("xml/tabs/about.xml");

    ThemeVariant variant = Application::getThemeVariant();
    std::string themePart =
        variant == brls::ThemeVariant::DARK ? "_dark" : "_light";

    versionLabel->setSubtitle(fmt::format(
        fmt::runtime("about/version"_i18n), APP_VERSION));

    std::string githubLink = "https://github.com/antoinebou12/Artemis-Switch";
    github->addGestureRecognizer(new TapGestureRecognizer(github));
    github->title->setText("about/link_github"_i18n);
    github->subtitle->setText(githubLink);
    github->image->setImageFromRes("img/links/github" + themePart + ".png");
    github->registerClickAction([githubLink](View* view) {
        openWebpage(githubLink);
        return true;
    });

    std::string moonlightSwitchLink =
        "https://github.com/XITRIX/Moonlight-Switch";
    moonlightSwitch->addGestureRecognizer(
        new TapGestureRecognizer(moonlightSwitch));
    moonlightSwitch->title->setText("about/link_moonlight_switch"_i18n);
    moonlightSwitch->subtitle->setText(moonlightSwitchLink);
    moonlightSwitch->image->setImageFromRes(
        "img/links/github" + themePart + ".png");
    moonlightSwitch->registerClickAction([moonlightSwitchLink](View* view) {
        openWebpage(moonlightSwitchLink);
        return true;
    });

    std::string apolloLink = "https://github.com/ClassicOldSong/Apollo";
    apollo->addGestureRecognizer(new TapGestureRecognizer(apollo));
    apollo->title->setText("about/link_apollo"_i18n);
    apollo->subtitle->setText(apolloLink);
    apollo->image->setImageFromRes("img/links/github" + themePart + ".png");
    apollo->registerClickAction([apolloLink](View* view) {
        openWebpage(apolloLink);
        return true;
    });

    std::string artemisLink = "https://github.com/ClassicOldSong/moonlight-android";
    artemis->addGestureRecognizer(new TapGestureRecognizer(artemis));
    artemis->title->setText("about/link_artemis"_i18n);
    artemis->subtitle->setText(artemisLink);
    artemis->image->setImageFromRes("img/links/github" + themePart + ".png");
    artemis->registerClickAction([artemisLink](View* view) {
        openWebpage(artemisLink);
        return true;
    });

    std::string patreonLink = "https://www.patreon.com/xitrix";
    patreon->addGestureRecognizer(new TapGestureRecognizer(patreon));
    patreon->title->setText("about/link_patreon"_i18n);
    patreon->subtitle->setText(patreonLink);
    patreon->image->setImageFromRes("img/links/patreon.png");
    patreon->registerClickAction([patreonLink](View* view) {
        openWebpage(patreonLink);
        return true;
    });

    std::string gbatempLink =
        "https://gbatemp.net/threads/"
        "moonlight-switch-nvidia-game-stream-client.591408/";
    gbatemp->addGestureRecognizer(new TapGestureRecognizer(gbatemp));
    gbatemp->title->setText("about/link_gbatemp"_i18n);
    gbatemp->subtitle->setText(gbatempLink);
    gbatemp->image->setImageFromRes("img/links/gbatemp.png");
    gbatemp->registerClickAction([gbatempLink](View* view) {
        openWebpage(gbatempLink);
        return true;
    });
}

brls::View* AboutTab::create() { return new AboutTab(); }
