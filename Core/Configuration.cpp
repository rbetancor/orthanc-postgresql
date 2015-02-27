/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2015 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "Configuration.h"

#include "PostgreSQLException.h"

#include <fstream>
#include <json/reader.h>
#include <memory>


// For UUID generation
extern "C"
{
#ifdef WIN32
#include <rpc.h>
#else
#include <uuid/uuid.h>
#endif
}


namespace OrthancPlugins
{
  bool ReadConfiguration(Json::Value& configuration,
                         OrthancPluginContext* context)
  {
    std::string path;

    {
      char* pathTmp = OrthancPluginGetConfigurationPath(context);
      if (pathTmp == NULL)
      {
        OrthancPluginLogError(context, "No configuration file is provided");
        return false;
      }

      path = std::string(pathTmp);

      OrthancPluginFreeString(context, pathTmp);
    }

    std::ifstream f(path.c_str());

    Json::Reader reader;
    if (!reader.parse(f, configuration) ||
        configuration.type() != Json::objectValue)
    {
      std::string s = "Unable to parse the configuration file: " + std::string(path);
      OrthancPluginLogError(context, s.c_str());
      return false;
    }

    return true;
  }


  std::string GetStringValue(const Json::Value& configuration,
                             const std::string& key,
                             const std::string& defaultValue)
  {
    if (configuration.type() != Json::objectValue ||
        !configuration.isMember(key) ||
        configuration[key].type() != Json::stringValue)
    {
      return defaultValue;
    }
    else
    {
      return configuration[key].asString();
    }
  }


  int GetIntegerValue(const Json::Value& configuration,
                      const std::string& key,
                      int defaultValue)
  {
    if (configuration.type() != Json::objectValue ||
        !configuration.isMember(key) ||
        configuration[key].type() != Json::intValue)
    {
      return defaultValue;
    }
    else
    {
      return configuration[key].asInt();
    }
  }


  PostgreSQLConnection* CreateConnection(bool& useLock,
                                         OrthancPluginContext* context)
  {
    Json::Value configuration;
    if (!ReadConfiguration(configuration, context))
    {
      return NULL;
    }

    useLock = true;  // Use locking by default
    std::auto_ptr<PostgreSQLConnection> connection(new PostgreSQLConnection);

    if (configuration.isMember("PostgreSQL"))
    {
      Json::Value c = configuration["PostgreSQL"];
      if (c.isMember("ConnectionUri"))
      {
        connection->SetConnectionUri(c["ConnectionUri"].asString());
      }
      else
      {
        connection->SetHost(GetStringValue(c, "Host", "localhost"));
        connection->SetPortNumber(GetIntegerValue(c, "Port", 5432));
        connection->SetDatabase(GetStringValue(c, "Database", "orthanc"));
        connection->SetUsername(GetStringValue(c, "Username", "orthanc"));
        connection->SetPassword(GetStringValue(c, "Password", "orthanc"));
      }

      if (c.isMember("Lock") &&
          c["Lock"].type() == Json::booleanValue)
      {
        useLock = c["Lock"].asBool();
      }
    }

    if (!useLock)
    {
      OrthancPluginLogWarning(context, "Locking of the PostgreSQL database is disabled");
    }

    connection->Open();

    return connection.release();
  }


  std::string GenerateUuid()
  {
#ifdef WIN32
    UUID uuid;
    UuidCreate ( &uuid );

    unsigned char * str;
    UuidToStringA ( &uuid, &str );

    std::string s( ( char* ) str );

    RpcStringFreeA ( &str );
#else
    uuid_t uuid;
    uuid_generate_random ( uuid );
    char s[37];
    uuid_unparse ( uuid, s );
#endif
    return s;
  }


  bool IsFlagInCommandLineArguments(OrthancPluginContext* context,
                                    const std::string& flag)
  {
    uint32_t count = OrthancPluginGetCommandLineArgumentsCount(context);

    for (uint32_t i = 0; i < count; i++)
    {
      char* tmp = OrthancPluginGetCommandLineArgument(context, i);
      std::string arg(tmp);
      OrthancPluginFreeString(context, tmp);

      if (arg == flag)
      {
        return true;
      }
    }

    return false;
  }

}
