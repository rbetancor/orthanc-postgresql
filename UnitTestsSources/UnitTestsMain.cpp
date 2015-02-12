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


#include "../Core/PostgreSQLConnection.h"

#include <gtest/gtest.h>
#include <memory>
#include <boost/lexical_cast.hpp>

static int argc_;
static char** argv_;

OrthancPlugins::PostgreSQLConnection* CreateTestConnection()
{
  std::auto_ptr<OrthancPlugins::PostgreSQLConnection> pg(new OrthancPlugins::PostgreSQLConnection);

  pg->SetHost(argv_[1]);
  pg->SetPortNumber(boost::lexical_cast<uint16_t>(argv_[2]));
  pg->SetUsername(argv_[3]);
  pg->SetPassword(argv_[4]);
  pg->SetDatabase(argv_[5]);
  
  pg->Open();
  pg->ClearAll();

  return pg.release();
}



int main(int argc, char **argv)
{
  if (argc != 6)
  {
    std::cerr << "Usage: " << argv[0] << " <host> <port> <username> <password> <database>"
              << std::endl << std::endl
              << "Example: " << argv[0] << " localhost 5432 postgres postgres orthanctests"
              << std::endl << std::endl;
    return -1;
  }

  argc_ = argc;
  argv_ = argv;  

  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
