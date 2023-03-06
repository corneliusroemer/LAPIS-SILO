#ifndef SILO_INFOHANDLER_H
#define SILO_INFOHANDLER_H

#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "silo/database.h"

namespace silo_api {
class InfoHandler : public Poco::Net::HTTPRequestHandler {
   private:
   silo::Database& database;

   public:
   explicit InfoHandler(silo::Database& database) : database(database) {}

   void handleRequest(Poco::Net::HTTPServerRequest&, Poco::Net::HTTPServerResponse& response) override;
};
}

#endif //SILO_INFOHANDLER_H
