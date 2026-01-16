#include <easylase.h>

#include <cflib/dao/version.h>
#include <cflib/util/cmdline.h>
#include <cflib/util/log.h>
#include <cflib/util/unixsignal.h>

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

    Easylase easylase;
    if (!easylase.connect()) {
        err << "cannot connect to Easylase: " << easylase.error() << Qt::endl;
        return 2;
    }

    // commands
    const QByteArray cmd = cmdArg.value();
    if (cmd == "on") {
        out << "turning on ...";
        if (easylase.on()) out << " done" << Qt::endl;
        else               out << " error: " << easylase.error() << Qt::endl;
        return 0;
    }
    if (cmd == "off") {
        out << "turning off ...";
        if (easylase.off()) out << " done" << Qt::endl;
        else                out << " error: " << easylase.error() << Qt::endl;
        return 0;
    }
    if (cmd == "idle") {
        out << "setting idle ...";
        if (easylase.idle()) out << " done" << Qt::endl;
        else                 out << " error: " << easylase.error() << Qt::endl;
        return 0;
    }
    if (cmd == "beam") {
        out << "showing beam ..." << Qt::endl;
        if (easylase.beam()) out << " done" << Qt::endl;
        else                 out << " error: " << easylase.error() << Qt::endl;
        return 0;
    }
    if (!cmd.isEmpty()) return showUsage(cmdLine.executable());

    int retval = a.exec();
    logInfo("terminating softly with retval: %1", retval);
    return retval;
}
