
#include "App.h"
#include "build.h"
#include "gui/style/load_style.h"
#include "logging/Log.h"
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFontDatabase>
#include <QStyleHints>
#include <Qt>

int main(int argc, char* argv[]) {

    QApplication app(argc, argv);
    QApplication::setApplicationName(build::app_name);
    QApplication::setApplicationVersion(build::version);

    QCommandLineParser parser;
    auto help = parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption verbose("verbose", "Enable verbose logging output");
    parser.addOption(verbose);

    QCommandLineOption debug("debug", "Enable debug logging output");
    parser.addOption(debug);

    QCommandLineOption no_style("no-style", "Disable all styles and fonts; use system defaults.");
    parser.addOption(no_style);

    parser.addPositionalArgument("path", "Todo file or repo directory");

    parser.process(app);

    using namespace logging;

    if (parser.isSet(help)) {
        parser.showHelp(1);
    }

    Log::init();

    if (parser.isSet(verbose)) {
        Log::set_filter(Type::INFO | Type::ERR | Type::WARN);
    } else {
        Log::set_filter(Type::ERR);
    }

    Log::enable_debug(parser.isSet(debug));

    QApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle("fusion");

    if (!parser.isSet(no_style)) {

        if (!gui::style::load_fonts(
                {
                    // App font
                    ":/fonts/CodeNewRoman/CodeNewRomanNerdFontPropo-Bold.otf",
                    ":/fonts/CodeNewRoman/CodeNewRomanNerdFontPropo-Italic.otf",

                    // Editor font
                    ":/fonts/CodeNewRoman/CodeNewRomanNerdFontMono-Bold.otf",
                    ":/fonts/CodeNewRoman/CodeNewRomanNerdFontMono-Italic.otf",
                    ":/fonts/CodeNewRoman/CodeNewRomanNerdFontMono-Regular.otf",
                }
            )) {
            LOG_WARN("Could not load fonts");
        }

        if (!gui::style::load_and_set_font(":/fonts/CodeNewRoman/CodeNewRomanNerdFontPropo-Regular.otf")) {
            LOG_WARN("Using default font: '{}'", QApplication::font().family().toStdString());
        }

        if (!gui::style::load_style(app, ":/styles/light.qss")) {
            LOG_WARN("Using default Qt style");
        }
    }
    const auto args = parser.positionalArguments();

    App main_window;

    if (!args.empty()) {
        main_window.openRepoCLI(args.first().toStdString());
    }

    main_window.show();

    return QApplication::exec();
}
