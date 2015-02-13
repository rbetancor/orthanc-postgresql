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


#include "PostgreSQLWrapper.h"

#include "EmbeddedResources.h"

#include "../Core/Configuration.h"
#include "../Core/PostgreSQLException.h"
#include "../Core/PostgreSQLTransaction.h"

#include <boost/lexical_cast.hpp>

namespace OrthancPlugins
{
  PostgreSQLWrapper::PostgreSQLWrapper(PostgreSQLConnection* connection,
                                       bool useLock,
                                       bool allowUnlock) :
    connection_(connection),
    globalProperties_(*connection, useLock, GlobalProperty_IndexLock)
  {
    globalProperties_.Lock(allowUnlock);

    Prepare();


    /**
     * Below are the PostgreSQL precompiled statements that are used
     * in more than 1 method of this class.
     **/

    getPublicId_.reset
      (new PostgreSQLStatement
       (*connection_, "SELECT publicId FROM Resources WHERE internalId=$1"));
    getPublicId_->DeclareInputInteger64(0);

    clearDeletedFiles_.reset
      (new PostgreSQLStatement
       (*connection_, "DELETE FROM DeletedFiles"));

    clearDeletedResources_.reset
      (new PostgreSQLStatement
       (*connection_, "DELETE FROM DeletedResources"));
  }


  void PostgreSQLWrapper::SignalDeletedFilesAndResources()
  {
    if (getDeletedFiles_.get() == NULL ||
        getDeletedResources_.get() == NULL)
    {
      getDeletedFiles_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT * FROM DeletedFiles"));

      getDeletedResources_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT * FROM DeletedResources"));
    }

    {
      PostgreSQLResult result(*getDeletedFiles_);

      while (!result.IsDone())
      {
        GetOutput().SignalDeletedAttachment(result.GetString(0).c_str(),
                                            result.GetInteger(1),
                                            result.GetInteger64(3),
                                            result.GetString(5).c_str(),
                                            result.GetInteger(4),
                                            result.GetInteger64(2),
                                            result.GetString(6).c_str());

        result.Step();
      }
    }

    {
      PostgreSQLResult result(*getDeletedResources_);

      while (!result.IsDone())
      {
        OrthancPluginResourceType type = static_cast<OrthancPluginResourceType>(result.GetInteger(0));
        GetOutput().SignalDeletedResource(result.GetString(1), type);

        result.Step();
      }
    }
  }


  void PostgreSQLWrapper::Prepare()
  {
    PostgreSQLTransaction t(*connection_);

    if (!connection_->DoesTableExist("Resources"))
    {
      std::string query;
      EmbeddedResources::GetFileResource(query, EmbeddedResources::POSTGRESQL_PREPARE);

      connection_->Execute(query);
    }

    
    // Check the version of the database
    std::string version = "unknown";
    if (!LookupGlobalProperty(version, GlobalProperty_DatabaseSchemaVersion))
    {
      throw PostgreSQLException("The database is corrupted. Drop it manually for Orthanc to recreate it");
    }

    bool ok = false;

    try
    {
      unsigned int v = boost::lexical_cast<unsigned int>(version);
      ok = (v == 5);
    }
    catch (boost::bad_lexical_cast&)
    {
    }
   
    if (!ok)
    {
      std::string message = "Incompatible version of the Orthanc PostgreSQL database: " + version;
      throw PostgreSQLException(message);
    }
          
    t.Commit();
  }


  PostgreSQLWrapper::~PostgreSQLWrapper()
  {
    globalProperties_.Unlock();
  }


  void PostgreSQLWrapper::AddAttachment(int64_t id,
                                        const OrthancPluginAttachment& attachment)
  {
    if (attachFile_.get() == NULL)
    {
      attachFile_.reset
        (new PostgreSQLStatement
         (*connection_, "INSERT INTO AttachedFiles VALUES($1, $2, $3, $4, $5, $6, $7, $8)"));
      attachFile_->DeclareInputInteger64(0);
      attachFile_->DeclareInputInteger(1);
      attachFile_->DeclareInputString(2);
      attachFile_->DeclareInputInteger64(3);
      attachFile_->DeclareInputInteger64(4);
      attachFile_->DeclareInputInteger(5);
      attachFile_->DeclareInputString(6);
      attachFile_->DeclareInputString(7);
    }

    attachFile_->BindInteger64(0, id);
    attachFile_->BindInteger(1, attachment.contentType);
    attachFile_->BindString(2, attachment.uuid);
    attachFile_->BindInteger64(3, attachment.compressedSize);
    attachFile_->BindInteger64(4, attachment.uncompressedSize);
    attachFile_->BindInteger(5, attachment.compressionType);
    attachFile_->BindString(6, attachment.uncompressedHash);
    attachFile_->BindString(7, attachment.compressedHash);
    attachFile_->Run();
  }


  void PostgreSQLWrapper::AttachChild(int64_t parent,
                                      int64_t child)
  {
    if (attachChild_.get() == NULL)
    {
      attachChild_.reset
        (new PostgreSQLStatement
         (*connection_, "UPDATE Resources SET parentId = $1 WHERE internalId = $2"));
      attachChild_->DeclareInputInteger64(0);
      attachChild_->DeclareInputInteger64(1);
    }

    attachChild_->BindInteger64(0, parent);
    attachChild_->BindInteger64(1, child);
    attachChild_->Run();
  }


  void PostgreSQLWrapper::ClearTable(const std::string& tableName)
  {
    connection_->Execute("DELETE FROM " + tableName);    
  }


  int64_t PostgreSQLWrapper::CreateResource(const char* publicId,
                                            OrthancPluginResourceType type)
  {
    if (createResource_.get() == NULL)
    {
      createResource_.reset
        (new PostgreSQLStatement
         (*connection_, "INSERT INTO Resources VALUES(DEFAULT, $1, $2, NULL) RETURNING internalId"));
      createResource_->DeclareInputInteger(0);
      createResource_->DeclareInputString(1);
    }

    createResource_->BindInteger(0, static_cast<int>(type));
    createResource_->BindString(1, publicId);
   
    PostgreSQLResult result(*createResource_);
    if (result.IsDone())
    {
      throw PostgreSQLException();
    }

    return result.GetInteger64(0);
  }


  void PostgreSQLWrapper::DeleteAttachment(int64_t id,
                                           int32_t attachment)
  {
    clearDeletedFiles_->Run();
    clearDeletedResources_->Run();

    if (deleteAttachment_.get() == NULL)
    {
      deleteAttachment_.reset
        (new PostgreSQLStatement
         (*connection_, "DELETE FROM AttachedFiles WHERE id=$1 AND fileType=$2"));
      deleteAttachment_->DeclareInputInteger64(0);
      deleteAttachment_->DeclareInputInteger(1);
    }

    deleteAttachment_->BindInteger64(0, id);
    deleteAttachment_->BindInteger(1, static_cast<int>(attachment));
    deleteAttachment_->Run();

    SignalDeletedFilesAndResources();
  }


  void PostgreSQLWrapper::DeleteMetadata(int64_t id,
                                         int32_t type)
  {
    if (deleteMetadata_.get() == NULL)
    {
      deleteMetadata_.reset
        (new PostgreSQLStatement
         (*connection_, "DELETE FROM Metadata WHERE id=$1 and type=$2"));
      deleteMetadata_->DeclareInputInteger64(0);
      deleteMetadata_->DeclareInputInteger(1);
    }

    deleteMetadata_->BindInteger64(0, id);
    deleteMetadata_->BindInteger(1, static_cast<int>(type));
    deleteMetadata_->Run();
  }


  void PostgreSQLWrapper::DeleteResource(int64_t id)
  {
    if (clearRemainingAncestor_.get() == NULL ||
        getRemainingAncestor_.get() == NULL)
    {
      clearRemainingAncestor_.reset
        (new PostgreSQLStatement
         (*connection_, "DELETE FROM RemainingAncestor"));

      getRemainingAncestor_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT * FROM RemainingAncestor"));
    }

    clearDeletedFiles_->Run();
    clearDeletedResources_->Run();
    clearRemainingAncestor_->Run();

    if (deleteResource_.get() == NULL)
    {
      deleteResource_.reset
        (new PostgreSQLStatement
         (*connection_, "DELETE FROM Resources WHERE internalId=$1"));
      deleteResource_->DeclareInputInteger64(0);
    }

    deleteResource_->BindInteger64(0, id);
    deleteResource_->Run();

    PostgreSQLResult result(*getRemainingAncestor_);
    if (!result.IsDone())
    {
      GetOutput().SignalRemainingAncestor(result.GetString(1),
                                          static_cast<OrthancPluginResourceType>(result.GetInteger(0)));

      // There is at most 1 remaining ancestor
      assert((result.Step(), result.IsDone()));
    }

    SignalDeletedFilesAndResources();
  }


  void PostgreSQLWrapper::GetAllPublicIds(std::list<std::string>& target,
                                          OrthancPluginResourceType resourceType)
  {
    if (getAllPublicIds_.get() == NULL)
    {
      getAllPublicIds_.reset
        (new PostgreSQLStatement(*connection_, 
                                 "SELECT publicId FROM Resources WHERE resourceType=$1"));
      getAllPublicIds_->DeclareInputInteger(0);
    }

    getAllPublicIds_->BindInteger(0, static_cast<int>(resourceType));
    PostgreSQLResult result(*getAllPublicIds_);

    target.clear();

    while (!result.IsDone())
    {
      target.push_back(result.GetString(0));
      result.Step();
    }
  }


  void PostgreSQLWrapper::GetChangesInternal(bool& done,
                                             PostgreSQLStatement& s,
                                             uint32_t maxResults)
  {
    PostgreSQLResult result(s);
    uint32_t count = 0;

    while (count < maxResults && !result.IsDone())
    {
      GetOutput().AnswerChange(result.GetInteger64(0),
                               result.GetInteger(1),
                               static_cast<OrthancPluginResourceType>(result.GetInteger(3)),
                               GetPublicId(result.GetInteger64(2)),
                               result.GetString(4));
      result.Step();
      count++;
    }

    done = !(count == maxResults && !result.IsDone());
  }

  
  void PostgreSQLWrapper::GetChanges(bool& done,
                                     int64_t since,
                                     uint32_t maxResults)
  {
    if (getChanges_.get() == NULL)
    {
      getChanges_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT * FROM Changes WHERE seq>$1 ORDER BY seq LIMIT $2"));
      getChanges_->DeclareInputInteger64(0);
      getChanges_->DeclareInputInteger(1);
    }

    getChanges_->BindInteger64(0, since);
    getChanges_->BindInteger(1, maxResults + 1);
    GetChangesInternal(done, *getChanges_, maxResults);
  }

  void PostgreSQLWrapper::GetLastChange()
  {
    if (getLastChange_.get() == NULL)
    {
      getLastChange_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT * FROM Changes ORDER BY seq DESC LIMIT 1"));
    }

    bool done;  // Ignored
    GetChangesInternal(done, *getLastChange_, 1);
  }


  void PostgreSQLWrapper::GetChildrenInternalId(std::list<int64_t>& target,
                                                int64_t id)
  {
    if (getChildrenInternalId_.get() == NULL)
    {
      getChildrenInternalId_.reset
        (new PostgreSQLStatement(*connection_, 
                                 "SELECT a.internalId FROM Resources AS a, Resources AS b  "
                                 "WHERE a.parentId = b.internalId AND b.internalId = $1"));
      getChildrenInternalId_->DeclareInputInteger64(0);
    }

    getChildrenInternalId_->BindInteger64(0, id);
    PostgreSQLResult result(*getChildrenInternalId_);

    target.clear();

    while (!result.IsDone())
    {
      target.push_back(result.GetInteger64(0));
      result.Step();
    }
  }



  void PostgreSQLWrapper::GetChildrenPublicId(std::list<std::string>& target,
                                              int64_t id)
  {
    if (getChildrenPublicId_.get() == NULL)
    {
      getChildrenPublicId_.reset
        (new PostgreSQLStatement(*connection_, 
                                 "SELECT a.publicId FROM Resources AS a, Resources AS b  "
                                 "WHERE a.parentId = b.internalId AND b.internalId = $1"));
      getChildrenPublicId_->DeclareInputInteger64(0);
    }

    getChildrenPublicId_->BindInteger64(0, id);
    PostgreSQLResult result(*getChildrenPublicId_);

    target.clear();

    while (!result.IsDone())
    {
      target.push_back(result.GetString(0));
      result.Step();
    }
  }


  void PostgreSQLWrapper::GetExportedResourcesInternal(bool& done,
                                                       PostgreSQLStatement& s,
                                                       uint32_t maxResults)
  {
    PostgreSQLResult result(s);
    uint32_t count = 0;

    while (count < maxResults && !result.IsDone())
    {
      int64_t seq = result.GetInteger64(0);
      OrthancPluginResourceType resourceType = static_cast<OrthancPluginResourceType>(result.GetInteger(1));
      std::string publicId = result.GetString(2);

      GetOutput().AnswerExportedResource(seq, 
                                         resourceType,
                                         publicId,
                                         result.GetString(3),  // modality
                                         result.GetString(8),  // date
                                         result.GetString(4),  // patient ID
                                         result.GetString(5),  // study instance UID
                                         result.GetString(6),  // series instance UID
                                         result.GetString(7)); // sop instance UID

      result.Step();
      count++;
    }

    done = !(count == maxResults && !result.IsDone());
  }

  void PostgreSQLWrapper::GetExportedResources(bool& done,
                                               int64_t since,
                                               uint32_t maxResults)
  {
    if (getExports_.get() == NULL)
    {
      getExports_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT * FROM ExportedResources WHERE seq>$1 ORDER BY seq LIMIT $2"));
      getExports_->DeclareInputInteger64(0);
      getExports_->DeclareInputInteger(1);
    }

    getExports_->BindInteger64(0, since);
    getExports_->BindInteger(1, maxResults + 1);
    GetExportedResourcesInternal(done, *getExports_, maxResults);
  }

  void PostgreSQLWrapper::GetLastExportedResource()
  {
    if (getLastExport_.get() == NULL)
    {
      getLastExport_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT * FROM ExportedResources ORDER BY seq DESC LIMIT 1"));
    }

    bool done;  // Ignored
    GetExportedResourcesInternal(done, *getLastExport_, 1);
  }


  void PostgreSQLWrapper::GetMainDicomTags(int64_t id)
  {
    if (getMainDicomTags1_.get() == NULL ||
        getMainDicomTags2_.get() == NULL)
    {
      getMainDicomTags1_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT * FROM MainDicomTags WHERE id=$1"));
      getMainDicomTags1_->DeclareInputInteger64(0);

      getMainDicomTags2_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT * FROM DicomIdentifiers WHERE id=$1"));
      getMainDicomTags2_->DeclareInputInteger64(0);
    }

    {
      getMainDicomTags1_->BindInteger64(0, id);
      PostgreSQLResult result(*getMainDicomTags1_);

      while (!result.IsDone())
      {
        GetOutput().AnswerDicomTag(static_cast<uint16_t>(result.GetInteger(1)),
                                   static_cast<uint16_t>(result.GetInteger(2)),
                                   result.GetString(3));
        result.Step();
      }
    }

    {
      getMainDicomTags2_->BindInteger64(0, id);
      PostgreSQLResult result(*getMainDicomTags2_);

      while (!result.IsDone())
      {
        GetOutput().AnswerDicomTag(static_cast<uint16_t>(result.GetInteger(1)),
                                   static_cast<uint16_t>(result.GetInteger(2)),
                                   result.GetString(3));
        result.Step();
      }
    }
  }


  std::string PostgreSQLWrapper::GetPublicId(int64_t resourceId)
  {
    getPublicId_->BindInteger64(0, resourceId);
    
    PostgreSQLResult result(*getPublicId_);
    if (result.IsDone())
    { 
      throw PostgreSQLException("Unknown resource");
    }

    return result.GetString(0);
  }



  uint64_t PostgreSQLWrapper::GetResourceCount(OrthancPluginResourceType resourceType)
  {
    if (getResourceCount_.get() == NULL)
    {
      getResourceCount_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT CAST(COUNT(*) AS BIGINT) FROM Resources WHERE resourceType=$1"));
      getResourceCount_->DeclareInputInteger(0);
    }

    getResourceCount_->BindInteger(0, static_cast<int>(resourceType));

    PostgreSQLResult result(*getResourceCount_);
    if (result.IsDone())
    {
      throw PostgreSQLException();
    }

    if (result.IsNull(0))
    {
      return 0;
    }
    else
    {
      return result.GetInteger64(0);
    }
  }



  OrthancPluginResourceType PostgreSQLWrapper::GetResourceType(int64_t resourceId)
  {
    if (getResourceType_.get() == NULL)
    {
      getResourceType_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT resourceType FROM Resources WHERE internalId=$1"));
      getResourceType_->DeclareInputInteger64(0);
    }

    getResourceType_->BindInteger64(0, resourceId);
    
    PostgreSQLResult result(*getResourceType_);
    if (result.IsDone())
    { 
      throw PostgreSQLException("Unknown resource");
    }

    return static_cast<OrthancPluginResourceType>(result.GetInteger(0));
  }


  uint64_t PostgreSQLWrapper::GetTotalCompressedSize()
  {
    if (getTotalCompressedSize_.get() == NULL)
    {
      getTotalCompressedSize_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT CAST(SUM(compressedSize) AS BIGINT) FROM AttachedFiles"));
    }

    PostgreSQLResult result(*getTotalCompressedSize_);
    if (result.IsDone())
    {
      throw PostgreSQLException();
    }

    if (result.IsNull(0))
    {
      return 0;
    }
    else
    {
      return result.GetInteger64(0);
    }
  }
    

  uint64_t PostgreSQLWrapper::GetTotalUncompressedSize()
  {
    if (getTotalUncompressedSize_.get() == NULL)
    {
      getTotalUncompressedSize_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT CAST(SUM(uncompressedSize) AS BIGINT) FROM AttachedFiles"));
    }

    PostgreSQLResult result(*getTotalUncompressedSize_);
    if (result.IsDone())
    {
      throw PostgreSQLException();
    }

    if (result.IsNull(0))
    {
      return 0;
    }
    else
    {
      return result.GetInteger64(0);
    }
  }


  bool PostgreSQLWrapper::IsExistingResource(int64_t internalId)
  {
    getPublicId_->BindInteger64(0, internalId);
    PostgreSQLResult result(*getPublicId_);
    return !result.IsDone();
  }


  bool PostgreSQLWrapper::IsProtectedPatient(int64_t internalId)
  {
    if (isProtectedPatient_.get() == NULL)
    {
      isProtectedPatient_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT * FROM PatientRecyclingOrder WHERE patientId = $1"));
      isProtectedPatient_->DeclareInputInteger64(0);
    }

    isProtectedPatient_->BindInteger64(0, internalId);
    PostgreSQLResult result(*isProtectedPatient_);
    return result.IsDone();
  }


  void PostgreSQLWrapper::ListAvailableMetadata(std::list<int32_t>& target,
                                                int64_t id)
  {
    if (listMetadata_.get() == NULL)
    {
      listMetadata_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT type FROM Metadata WHERE id=$1"));
      listMetadata_->DeclareInputInteger64(0);
    }

    listMetadata_->BindInteger64(0, id);
    PostgreSQLResult result(*listMetadata_);

    target.clear();

    while (!result.IsDone())
    {
      target.push_back(static_cast<int32_t>(result.GetInteger(0)));
      result.Step();
    }
  }


  void PostgreSQLWrapper::ListAvailableAttachments(std::list<int32_t>& target,
                                                   int64_t id)
  {
    if (listAttachments_.get() == NULL)
    {
      listAttachments_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT fileType FROM AttachedFiles WHERE id=$1"));
      listAttachments_->DeclareInputInteger64(0);
    }

    listAttachments_->BindInteger64(0, id);
    PostgreSQLResult result(*listAttachments_);

    target.clear();

    while (!result.IsDone())
    {
      target.push_back(static_cast<int32_t>(result.GetInteger(0)));
      result.Step();
    }
  }


  void PostgreSQLWrapper::LogChange(const OrthancPluginChange& change)
  {
    if (logChange_.get() == NULL)
    {
      logChange_.reset
        (new PostgreSQLStatement
         (*connection_, "INSERT INTO Changes VALUES(DEFAULT, $1, $2, $3, $4)"));
      logChange_->DeclareInputInteger(0);
      logChange_->DeclareInputInteger64(1);
      logChange_->DeclareInputInteger(2);
      logChange_->DeclareInputString(3);
    }

    int64_t id;
    OrthancPluginResourceType type;
    if (!LookupResource(id, type, change.publicId) ||
        type != change.resourceType)
    {
      throw PostgreSQLException();
    }

    logChange_->BindInteger(0, change.changeType);
    logChange_->BindInteger64(1, id);
    logChange_->BindInteger(2, change.resourceType);
    logChange_->BindString(3, change.date);
    logChange_->Run();      
  }




  void PostgreSQLWrapper::LogExportedResource(const OrthancPluginExportedResource& resource)
  {
    if (logExport_.get() == NULL)
    {
      logExport_.reset
        (new PostgreSQLStatement
         (*connection_, "INSERT INTO ExportedResources VALUES(DEFAULT, $1, $2, $3, $4, $5, $6, $7, $8)"));
      logExport_->DeclareInputInteger(0);
      logExport_->DeclareInputString(1);
      logExport_->DeclareInputString(2);
      logExport_->DeclareInputString(3);
      logExport_->DeclareInputString(4);
      logExport_->DeclareInputString(5);
      logExport_->DeclareInputString(6);
      logExport_->DeclareInputString(7);
    }

    logExport_->BindInteger(0, resource.resourceType);
    logExport_->BindString(1, resource.publicId);
    logExport_->BindString(2, resource.modality);
    logExport_->BindString(3, resource.patientId);
    logExport_->BindString(4, resource.studyInstanceUid);
    logExport_->BindString(5, resource.seriesInstanceUid);
    logExport_->BindString(6, resource.sopInstanceUid);
    logExport_->BindString(7, resource.date);
    logExport_->Run();      
  }




  bool PostgreSQLWrapper::LookupAttachment(int64_t id,
                                           int32_t contentType)
  {
    if (lookupAttachment_.get() == NULL)
    {
      lookupAttachment_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT uuid, uncompressedSize, compressionType, compressedSize, "
          "uncompressedHash, compressedHash FROM AttachedFiles WHERE id=$1 AND fileType=$2"));
      lookupAttachment_->DeclareInputInteger64(0);
      lookupAttachment_->DeclareInputInteger(1);
    }

    lookupAttachment_->BindInteger64(0, id);
    lookupAttachment_->BindInteger(1, static_cast<int>(contentType));

    PostgreSQLResult result(*lookupAttachment_);
    if (!result.IsDone())
    {
      GetOutput().AnswerAttachment(result.GetString(0),
                                   contentType,
                                   result.GetInteger64(1),
                                   result.GetString(4),
                                   result.GetInteger(2),
                                   result.GetInteger64(3),
                                   result.GetString(5));
      return true;
    }
    else
    {
      return false;
    }
  }


  void PostgreSQLWrapper::LookupIdentifier(std::list<int64_t>& target,
                                           uint16_t group,
                                           uint16_t element,
                                           const char* value)
  {
    if (lookupIdentifier1_.get() == NULL)
    {
      lookupIdentifier1_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT id FROM DicomIdentifiers WHERE tagGroup=$1 AND tagElement=$2 and value=$3"));
      lookupIdentifier1_->DeclareInputInteger(0);
      lookupIdentifier1_->DeclareInputInteger(1);
      lookupIdentifier1_->DeclareInputBinary(2);
    }


    lookupIdentifier1_->BindInteger(0, group);
    lookupIdentifier1_->BindInteger(1, element);
    lookupIdentifier1_->BindString(2, value);

    PostgreSQLResult result(*lookupIdentifier1_);
    target.clear();

    while (!result.IsDone())
    {
      target.push_back(result.GetInteger64(0));
      result.Step();
    }
  }

  void PostgreSQLWrapper::LookupIdentifier(std::list<int64_t>& target,
                                           const char* value)
  {
    if (lookupIdentifier2_.get() == NULL)
    {
      lookupIdentifier2_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT id FROM DicomIdentifiers WHERE value=$1"));
      lookupIdentifier2_->DeclareInputBinary(0);
    }

    lookupIdentifier2_->BindString(0, value);

    PostgreSQLResult result(*lookupIdentifier2_);
    target.clear();

    while (!result.IsDone())
    {
      target.push_back(result.GetInteger64(0));
      result.Step();
    }
  }


  bool PostgreSQLWrapper::LookupMetadata(std::string& target,
                                         int64_t id,
                                         int32_t type)
  {
    if (lookupMetadata_.get() == NULL)
    {
      lookupMetadata_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT value FROM Metadata WHERE id=$1 and type=$2"));
      lookupMetadata_->DeclareInputInteger64(0);
      lookupMetadata_->DeclareInputInteger(1);
    }

    lookupMetadata_->BindInteger64(0, id);
    lookupMetadata_->BindInteger(1, static_cast<int>(type));

    PostgreSQLResult result(*lookupMetadata_);
    if (result.IsDone())
    {
      return false;
    }
    else
    {
      target = result.GetString(0);
      return true;
    }
  }


  bool PostgreSQLWrapper::LookupParent(int64_t& parentId,
                                       int64_t resourceId)
  {
    if (lookupParent_.get() == NULL)
    {
      lookupParent_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT parentId FROM Resources WHERE internalId=$1"));
      lookupParent_->DeclareInputInteger64(0);
    }

    lookupParent_->BindInteger64(0, resourceId);

    PostgreSQLResult result(*lookupParent_);
    if (result.IsDone())
    {
      throw PostgreSQLException("Unknown resource");
    }

    if (result.IsNull(0))
    {
      return false;
    }
    else
    {
      parentId = result.GetInteger64(0);
      return true;
    }
  }


  bool PostgreSQLWrapper::LookupResource(int64_t& id,
                                         OrthancPluginResourceType& type,
                                         const char* publicId)
  {
    if (lookupResource_.get() == NULL)
    {
      lookupResource_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT internalId, resourceType FROM Resources WHERE publicId=$1"));
      lookupResource_->DeclareInputString(0);
    }

    lookupResource_->BindString(0, publicId);

    PostgreSQLResult result(*lookupResource_);
    if (result.IsDone())
    {
      return false;
    }
    else
    {
      id = result.GetInteger64(0);
      type = static_cast<OrthancPluginResourceType>(result.GetInteger(1));
      return true;
    }
  }


  bool PostgreSQLWrapper::SelectPatientToRecycle(int64_t& internalId)
  {
    if (selectPatientToRecycle_.get() == NULL)
    {
      selectPatientToRecycle_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT patientId FROM PatientRecyclingOrder ORDER BY seq ASC LIMIT 1"));
    }

    PostgreSQLResult result(*selectPatientToRecycle_);

    if (result.IsDone())
    {
      // No patient remaining or all the patients are protected
      return false;
    }
    else
    {
      internalId = result.GetInteger64(0);
      return true;
    }   
  }


  bool PostgreSQLWrapper::SelectPatientToRecycle(int64_t& internalId,
                                                 int64_t patientIdToAvoid)
  {
    if (selectPatientToRecycleAvoid_.get() == NULL)
    {
      selectPatientToRecycleAvoid_.reset
        (new PostgreSQLStatement
         (*connection_, "SELECT patientId FROM PatientRecyclingOrder WHERE patientId != $1 ORDER BY seq ASC LIMIT 1"));
      selectPatientToRecycleAvoid_->DeclareInputInteger64(0);
    }

    selectPatientToRecycleAvoid_->BindInteger64(0, patientIdToAvoid);
    PostgreSQLResult result(*selectPatientToRecycleAvoid_);

    if (result.IsDone())
    {
      // No patient remaining or all the patients are protected
      return false;
    }
    else
    {
      internalId = result.GetInteger64(0);
      return true;
    }   
  }


  static void SetTagInternal(PostgreSQLStatement& s,
                             int64_t id,
                             uint16_t group,
                             uint16_t element,
                             const char* value)
  {
    s.BindInteger64(0, id);
    s.BindInteger(1, group);
    s.BindInteger(2, element);
    s.BindString(3, value);
    s.Run();
  }


  void PostgreSQLWrapper::SetMainDicomTag(int64_t id,
                                          uint16_t group,
                                          uint16_t element,
                                          const char* value)
  {
    if (setMainDicomTags_.get() == NULL)
    {
      setMainDicomTags_.reset
        (new PostgreSQLStatement
         (*connection_, "INSERT INTO MainDicomTags VALUES($1, $2, $3, $4)"));
      setMainDicomTags_->DeclareInputInteger64(0);
      setMainDicomTags_->DeclareInputInteger(1);
      setMainDicomTags_->DeclareInputInteger(2);
      setMainDicomTags_->DeclareInputBinary(3);
    }

    SetTagInternal(*setMainDicomTags_, id, group, element, value);
  }

  void PostgreSQLWrapper::SetIdentifierTag(int64_t id,
                                           uint16_t group,
                                           uint16_t element,
                                           const char* value)
  {
    if (setIdentifierTag_.get() == NULL)
    {
      setIdentifierTag_.reset
        (new PostgreSQLStatement
         (*connection_, "INSERT INTO DicomIdentifiers VALUES($1, $2, $3, $4)"));
      setIdentifierTag_->DeclareInputInteger64(0);
      setIdentifierTag_->DeclareInputInteger(1);
      setIdentifierTag_->DeclareInputInteger(2);
      setIdentifierTag_->DeclareInputBinary(3);
    }

    SetTagInternal(*setIdentifierTag_, id, group, element, value);
  }


  void PostgreSQLWrapper::SetMetadata(int64_t id,
                                      int32_t type,
                                      const char* value)
  {
    if (setMetadata1_.get() == NULL ||
        setMetadata2_.get() == NULL)
    {
      setMetadata1_.reset
        (new PostgreSQLStatement
         (*connection_, "DELETE FROM Metadata WHERE id=$1 AND type=$2"));
      setMetadata1_->DeclareInputInteger64(0);
      setMetadata1_->DeclareInputInteger(1);

      setMetadata2_.reset
        (new PostgreSQLStatement
         (*connection_, "INSERT INTO Metadata VALUES ($1, $2, $3)"));
      setMetadata2_->DeclareInputInteger64(0);
      setMetadata2_->DeclareInputInteger(1);
      setMetadata2_->DeclareInputString(2);
    }

    setMetadata1_->BindInteger64(0, id);
    setMetadata1_->BindInteger(1, static_cast<int>(type));
    setMetadata1_->Run();

    setMetadata2_->BindInteger64(0, id);
    setMetadata2_->BindInteger(1, static_cast<int>(type));
    setMetadata2_->BindString(2, value);
    setMetadata2_->Run();
  }



  void PostgreSQLWrapper::SetProtectedPatient(int64_t internalId, 
                                              bool isProtected)
  {
    if (protectPatient1_.get() == NULL ||
        protectPatient2_.get() == NULL)
    {
      protectPatient1_.reset
        (new PostgreSQLStatement
         (*connection_, "DELETE FROM PatientRecyclingOrder WHERE patientId=$1"));
      protectPatient1_->DeclareInputInteger64(0);
    
      protectPatient2_.reset
        (new PostgreSQLStatement
         (*connection_, "INSERT INTO PatientRecyclingOrder VALUES(DEFAULT, $1)"));
      protectPatient2_->DeclareInputInteger64(0);
    }

    if (isProtected)
    {
      protectPatient1_->BindInteger64(0, internalId);
      protectPatient1_->Run();
    }
    else if (IsProtectedPatient(internalId))
    {
      protectPatient2_->BindInteger64(0, internalId);
      protectPatient2_->Run();
    }
    else
    {
      // Nothing to do: The patient is already unprotected
    }
  }



  // For unit testing only!
  void PostgreSQLWrapper::GetChildren(std::list<std::string>& childrenPublicIds,
                                      int64_t id)
  {
    PostgreSQLStatement s(*connection_, "SELECT publicId FROM Resources WHERE parentId=$1");
    s.DeclareInputInteger64(0);
    s.BindInteger64(0, id);

    PostgreSQLResult result(s);

    childrenPublicIds.clear();

    while (!result.IsDone())
    {
      childrenPublicIds.push_back(result.GetString(0));
      result.Step();
    }
  }




  // For unit testing only!
  int64_t PostgreSQLWrapper::GetTableRecordCount(const std::string& table)
  {
    char buf[128];
    sprintf(buf, "SELECT CAST(COUNT(*) AS BIGINT) FROM %s", table.c_str());
    PostgreSQLStatement s(*connection_, buf);

    PostgreSQLResult result(s);
    if (result.IsDone())
    {
      throw PostgreSQLException();
    }

    if (result.IsNull(0))
    {
      return 0;
    }
    else
    {
      return result.GetInteger64(0);
    }
  }
    


  // For unit testing only!
  bool PostgreSQLWrapper::GetParentPublicId(std::string& target,
                                            int64_t id)
  {
    PostgreSQLStatement s(*connection_, 
                          "SELECT a.publicId FROM Resources AS a, Resources AS b "
                          "WHERE a.internalId = b.parentId AND b.internalId = $1");
    s.DeclareInputInteger64(0);
    s.BindInteger64(0, id);

    PostgreSQLResult result(s);

    if (result.IsDone())
    {
      return false;
    }
    else
    {
      target = result.GetString(0);
      return true;
    }
  }
}
