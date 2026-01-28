#include "mainservice.h"

#include <cflib/util/log.h>

USE_LOG(LogCat::Http)

namespace services {

MainService::MainService() :
    RMIService(serializeTypeInfo().typeName)
{
}

MainService::~MainService()
{
    stopVerifyThread();
}

}
