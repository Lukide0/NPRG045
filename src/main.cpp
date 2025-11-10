
#include "App.h"
#include "logging/Log.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QStyleHints>

int main(int argc, char* argv[]) {

    QApplication app(argc, argv);
    QApplication::setApplicationName("git shuffle");
    QApplication::setApplicationVersion("0.1.0");

    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setStyle("fusion");

    QIcon::setThemeName("hicolor");

    QCommandLineParser parser;
    auto help = parser.addHelpOption();

    QCommandLineOption verbose("verbose", "Enable verbose logging output");
    parser.addOption(verbose);

    QCommandLineOption debug("debug", "Enable debug logging output");
    parser.addOption(debug);

    parser.addPositionalArgument("path", "Todo file or repo directory");

    parser.process(app);

    using namespace logging;

    if (parser.isSet(help)) {
        parser.showHelp(1);
    }

    if (parser.isSet(verbose)) {
        Log::set_filter(Type::INFO | Type::ERR | Type::WARN);
    } else {
        Log::set_filter(Type::ERR);
    }

    Log::enable_debug(parser.isSet(debug));

    const auto args = parser.positionalArguments();

    App main_window;

    if (!args.empty()) {
        main_window.openRepoCLI(args.first().toStdString());
    }

    main_window.show();

    return QApplication::exec();
}
