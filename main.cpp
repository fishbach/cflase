#include <laser/laser.h>
#include <services/laserservice.h>
#include <stream.h>

#include <cflib/dao/version.h>
#include <cflib/net/fileserver.h>
#include <cflib/net/httpserver.h>
#include <cflib/net/rmiserver.h>
#include <cflib/net/wscommmanager.h>
#include <cflib/util/cmdline.h>
#include <cflib/util/log.h>
#include <cflib/util/unixsignal.h>

using namespace cflib::dao;
using namespace cflib::net;
using namespace cflib::util;
using namespace services;

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
    Option help     ('h', "help"        ); cmdLine << help;
    Option logOpt   ('l', "log",    true); cmdLine << logOpt;
    Option exportOpt('e', "export", true); cmdLine << exportOpt;
    Arg    cmdArg                        ; cmdLine << cmdArg;
    if (!cmdLine.parse() || help.isSet()) return showUsage(cmdLine.executable());

    // application loop
    QCoreApplication a(argc, argv);
    UnixSignal unixSignal(true);
    auto runLoop = [&a]() {
        int retval = a.exec();
        logInfo("terminating softly with retval: %1", retval);
        return retval;
    };

    // start logging
    if (logOpt.isSet()) {
        Log::start("-");
        logInfo("cflase version %1 started", version);
        Log::setLogLevel(logOpt.value().toUShort());
    }

    auto initLaser = []() {
        std::unique_ptr<Laser> laser = std::make_unique<Laser>();
        laser->setErrorCallback([](const QString & error) {
            QTextStream(stderr) << "error: " << error << Qt::endl;
        });
        laser->setActiveCallback([](bool active) {
            QTextStream(stdout) << "laser: " << (active ? "on" : "off") << Qt::endl;
        });
        laser->reset();
        if (laser->hasError()) laser = {};
        return laser;
    };

    // commands
    const QByteArray cmd = cmdArg.value();
    if (cmd == "on") {
        auto laser = initLaser();
        if (!laser) return 2;
        out << "turning on ..." << Qt::endl;
        laser->on();
        laser->waitForFinish();
        return 0;
    } else if (cmd == "off") {
        auto laser = initLaser();
        if (!laser) return 2;
        out << "turning off ..." << Qt::endl;
        laser->off();
        laser->waitForFinish();
        return 0;
    } else if (cmd == "beam") {
        auto laser = initLaser();
        if (!laser) return 2;
        out << "showing beam ..." << Qt::endl;
        laser->show({.g = 35});
    } else if (cmd == "test") {
        Stream stream;
        auto laser = initLaser();
        if (!laser) return 2;
        out << "showing test ..." << Qt::endl;
        laser->setFinishedCallback([&]() { laser->show(stream.getNext()); });
        laser->show(stream.getFirst());
        return runLoop();
    } else if (cmd == "web" || exportOpt.isSet()) {
        // RMI server
        WSCommManager<int> commMgr("/ws");
        RMIServer<int>     rmiServer(commMgr);

        LaserService laserService; rmiServer.registerService(laserService);

        if (exportOpt.isSet()) {
            rmiServer.exportTo(exportOpt.value());
            logInfo("export finished (dest: %1)", exportOpt.value());
            return 0;
        }

        FileServer fs("../htdocs", true);
        fs.setAccessControlAllowOrigin("*");

        HttpServer serv(1);
        serv.registerHandler(commMgr);
        serv.registerHandler(rmiServer);
        serv.registerHandler(fs);
        serv.start("0.0.0.0", 8080);

        return runLoop();
    }

    return showUsage(cmdLine.executable());
}
