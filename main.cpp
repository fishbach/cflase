#include <laser/laser.h>

#include <cflib/dao/version.h>
#include <cflib/util/cmdline.h>
#include <cflib/util/log.h>
#include <cflib/util/unixsignal.h>

#include <numbers>
#include <cmath>

using namespace cflib::dao;
using namespace cflib::util;

USE_LOG(LogCat::Network)

namespace {

const Version version(1, 0, 0);

QTextStream out(stdout);
QTextStream err(stderr);

int showUsage(const QByteArray & executable)
{
    err
        << "Usage: " << executable << " [options] <cmd>"             << Qt::endl
        << "Options:"                                                << Qt::endl
        << "  -h, --help        => this help"                        << Qt::endl
        << "  -l, --log <level> => set log level 1 -> all, 7 -> off" << Qt::endl
        << "Commands:"                                               << Qt::endl
        << "  off               => turns Laser off"                  << Qt::endl
        << "  beam              => shows one soft beam at center"    << Qt::endl;
    return 1;
}

}

int main(int argc, char *argv[])
{
    CmdLine cmdLine(argc, argv);
    Option help  ('h', "help"     ); cmdLine << help;
    Option logOpt('l', "log", true); cmdLine << logOpt;
    Arg    cmdArg                  ; cmdLine << cmdArg;
    if (!cmdLine.parse() || help.isSet()) return showUsage(cmdLine.executable());

    QCoreApplication a(argc, argv);
    UnixSignal unixSignal(true);

    // start logging
    if (logOpt.isSet()) {
        Log::start("-");
        logInfo("cflase version %1 started", version);
        Log::setLogLevel(logOpt.value().toUShort());
    }

    Laser laser;
    laser.setErrorCallback([](const QString & error) {
        QTextStream(stderr) << "error: " << error << Qt::endl;
    });
    laser.setActiveCallback([](bool active) {
        QTextStream(stdout) << "laser: " << (active ? "on" : "off") << Qt::endl;
    });
    if (laser.hasError()) return 2;

    // commands
    const QByteArray cmd = cmdArg.value();
    if (cmd == "on") {
        out << "turning on ..." << Qt::endl;
        laser.on();
        laser.waitForFinish();
        return 0;
    } else if (cmd == "off") {
        out << "turning off ..." << Qt::endl;
        laser.off();
        laser.waitForFinish();
        return 0;
    } else if (cmd == "beam") {
        out << "showing beam ..." << Qt::endl;
        laser.show({.g = 35});
    } else if (cmd == "test") {
        out << "showing test ..." << Qt::endl;

        constexpr double Pi = std::numbers::pi_v<double>;

        int pc = 15000;
        Laser::Points points;
        for (int i = 0 ; i < pc ; ++i) {
            points << Laser::Point{
                .x = std::cos(2 * Pi * i / pc) / 4,
                .y = std::sin(2 * Pi * i / pc) / 4,
                .g = 45
            };
        }
        laser.show(points, true);
    } else if (!cmd.isEmpty()) return showUsage(cmdLine.executable());

    int retval = a.exec();
    logInfo("terminating softly with retval: %1", retval);
    return retval;
}
