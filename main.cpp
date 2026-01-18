#include <easylase.h>

#include <cflib/dao/version.h>
#include <cflib/util/cmdline.h>
#include <cflib/util/log.h>
#include <cflib/util/unixsignal.h>

#include <time.h>

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

inline quint64 currentMSecs()
{
    struct ::timespec ts;
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return (quint64)ts.tv_sec * Q_UINT64_C(1000) + (quint64)ts.tv_nsec / Q_UINT64_C(1000000);
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

    EasyLase easyLase;
    if (!easyLase.connect()) {
        err << "cannot connect to EasyLase: " << easyLase.error() << Qt::endl;
        return 2;
    }

    // commands
    const QByteArray cmd = cmdArg.value();
    if (cmd == "on") {
        out << "turning on ...";
        if (easyLase.on()) out << " done" << Qt::endl;
        else               out << " error: " << easyLase.error() << Qt::endl;
        return 0;
    }
    if (cmd == "off") {
        out << "turning off ...";
        if (easyLase.off()) out << " done" << Qt::endl;
        else                out << " error: " << easyLase.error() << Qt::endl;
        return 0;
    }
    if (cmd == "idle") {
        out << "setting idle ...";
        if (easyLase.idle()) out << " done" << Qt::endl;
        else                 out << " error: " << easyLase.error() << Qt::endl;
        return 0;
    }
    if (cmd == "beam") {
        out << "showing beam ...";
        EasyLase::Point p;
        p.g = 35;
        if (easyLase.show(p)) out << " done" << Qt::endl;
        else                  out << " error: " << easyLase.error() << Qt::endl;
        return 0;
    }
    if (cmd == "test") {
        out << "showing test ..." << Qt::endl;
        EasyLase::Points points;
        EasyLase::Point p;
        p.g = 35;
        points = EasyLase::Points(1000);
        quint64 ts = currentMSecs();
        out << "s1: " << (easyLase.isReady() ? "ready" : "not ready") << Qt::endl;
        easyLase.idle();
        out << "s2: " << (easyLase.isReady() ? "ready" : "not ready") << Qt::endl;
        while (!easyLase.isReady());
        for (int i = 0 ; i < 10 ; ++i) {
            out << Qt::endl;
            out << "t2: " << currentMSecs() - ts << Qt::endl;
            easyLase.show(500, points);
            out << "t3: " << currentMSecs() - ts << Qt::endl;
            out << "s4: " << (easyLase.isReady() ? "ready" : "not ready") << Qt::endl;
            out << "t4: " << currentMSecs() - ts << Qt::endl;
            while (!easyLase.isReady());
        }
        out << Qt::endl;
        out << "t5: " << currentMSecs() - ts << Qt::endl;
        return 0;
    }
    if (!cmd.isEmpty()) return showUsage(cmdLine.executable());

    int retval = a.exec();
    logInfo("terminating softly with retval: %1", retval);
    return retval;
}
