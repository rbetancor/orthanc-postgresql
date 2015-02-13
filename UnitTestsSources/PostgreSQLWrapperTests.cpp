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


#include "EmbeddedResources.h"

#include <gtest/gtest.h>

#include "../Core/PostgreSQLTransaction.h"
#include "../Core/PostgreSQLResult.h"
#include "../IndexPlugin/PostgreSQLWrapper.h"

using namespace OrthancPlugins;

extern PostgreSQLConnection* CreateTestConnection();


// From Orthanc enumerations 
static const int32_t  GlobalProperty_DatabaseSchemaVersion = 1;
static const int32_t  GlobalProperty_AnonymizationSequence = 3;
static const int32_t  MetadataType_ModifiedFrom = 5;
static const int32_t  MetadataType_LastUpdate = 7;
static const int32_t  FileContentType_Dicom = 1;
static const int32_t  FileContentType_DicomAsJson = 2;
static const int32_t  CompressionType_None = 1;
static const int32_t  CompressionType_Zlib = 2;


static std::auto_ptr<OrthancPluginAttachment>  expectedAttachment;
static std::list<OrthancPluginDicomTag>  expectedDicomTags;
static std::auto_ptr<OrthancPluginExportedResource>  expectedExported;

static void CheckAttachment(const OrthancPluginAttachment& attachment)
{
  ASSERT_STREQ(expectedAttachment->uuid, attachment.uuid);
  ASSERT_EQ(expectedAttachment->contentType, attachment.contentType);
  ASSERT_EQ(expectedAttachment->uncompressedSize, attachment.uncompressedSize);
  ASSERT_STREQ(expectedAttachment->uncompressedHash, attachment.uncompressedHash);
  ASSERT_EQ(expectedAttachment->compressionType, attachment.compressionType);
  ASSERT_EQ(expectedAttachment->compressedSize, attachment.compressedSize);
  ASSERT_STREQ(expectedAttachment->compressedHash, attachment.compressedHash);
}

static void CheckExportedResource(const OrthancPluginExportedResource& exported)
{
  ASSERT_EQ(expectedExported->seq, exported.seq);
  ASSERT_EQ(expectedExported->resourceType, exported.resourceType);
  ASSERT_STREQ(expectedExported->publicId, exported.publicId);
  ASSERT_STREQ(expectedExported->modality, exported.modality);
  ASSERT_STREQ(expectedExported->date, exported.date);
  ASSERT_STREQ(expectedExported->patientId, exported.patientId);
  ASSERT_STREQ(expectedExported->studyInstanceUid, exported.studyInstanceUid);
  ASSERT_STREQ(expectedExported->seriesInstanceUid, exported.seriesInstanceUid);
  ASSERT_STREQ(expectedExported->sopInstanceUid, exported.sopInstanceUid);
}

static void CheckDicomTag(const OrthancPluginDicomTag& tag)
{
  for (std::list<OrthancPluginDicomTag>::const_iterator
         it = expectedDicomTags.begin(); it != expectedDicomTags.end(); it++)
  {
    if (it->group == tag.group &&
        it->element == tag.element &&
        !strcmp(it->value, tag.value))
    {
      // OK, match
      return;
    }
  }

  ASSERT_TRUE(0);  // Error
}

static int32_t InvokeService(struct _OrthancPluginContext_t* context,
                             _OrthancPluginService service,
                             const void* params)
{
  if (service == _OrthancPluginService_DatabaseAnswer)
  {
    const _OrthancPluginDatabaseAnswer& answer = 
      *reinterpret_cast<const _OrthancPluginDatabaseAnswer*>(params);

    switch (answer.type)
    {
      case _OrthancPluginDatabaseAnswerType_Attachment:
      {
        const OrthancPluginAttachment& attachment = 
          *reinterpret_cast<const OrthancPluginAttachment*>(answer.valueGeneric);
        CheckAttachment(attachment);
        break;
      }

      case _OrthancPluginDatabaseAnswerType_ExportedResource:
      {
        const OrthancPluginExportedResource& attachment = 
          *reinterpret_cast<const OrthancPluginExportedResource*>(answer.valueGeneric);
        CheckExportedResource(attachment);
        break;
      }

      case _OrthancPluginDatabaseAnswerType_DicomTag:
      {
        const OrthancPluginDicomTag& tag = 
          *reinterpret_cast<const OrthancPluginDicomTag*>(answer.valueGeneric);
        CheckDicomTag(tag);
        break;
      }

      default:
        printf("Unhandled message: %d\n", answer.type);
        break;
    }
  }

  return 0;
}


TEST(PostgreSQLWrapper, Basic)
{
  std::auto_ptr<PostgreSQLConnection> pg(CreateTestConnection());

  OrthancPluginContext context;
  context.pluginsManager = NULL;
  context.orthancVersion = "mainline";
  context.Free = ::free;
  context.InvokeService = InvokeService;

  PostgreSQLWrapper db(pg.release(), true);
  db.RegisterOutput(new DatabaseBackendOutput(&context, NULL));

  std::string s;
  ASSERT_TRUE(db.LookupGlobalProperty(s, GlobalProperty_DatabaseSchemaVersion));
  ASSERT_EQ("5", s);

  ASSERT_FALSE(db.LookupGlobalProperty(s, GlobalProperty_AnonymizationSequence));
  db.SetGlobalProperty(GlobalProperty_AnonymizationSequence, "Hello");
  ASSERT_TRUE(db.LookupGlobalProperty(s, GlobalProperty_AnonymizationSequence));
  ASSERT_EQ("Hello", s);
  db.SetGlobalProperty(GlobalProperty_AnonymizationSequence, "HelloWorld");
  ASSERT_TRUE(db.LookupGlobalProperty(s, GlobalProperty_AnonymizationSequence));
  ASSERT_EQ("HelloWorld", s);

  int64_t a = db.CreateResource("study", OrthancPluginResourceType_Study);
  ASSERT_TRUE(db.IsExistingResource(a));
  ASSERT_FALSE(db.IsExistingResource(a + 1));

  int64_t b;
  OrthancPluginResourceType t;
  ASSERT_FALSE(db.LookupResource(b, t, "world"));
  ASSERT_TRUE(db.LookupResource(b, t, "study"));
  ASSERT_EQ(a, b);
  ASSERT_EQ(OrthancPluginResourceType_Study, t);
  
  b = db.CreateResource("series", OrthancPluginResourceType_Series);
  ASSERT_NE(a, b);

  ASSERT_EQ("study", db.GetPublicId(a));
  ASSERT_EQ("series", db.GetPublicId(b));
  ASSERT_EQ(OrthancPluginResourceType_Study, db.GetResourceType(a));
  ASSERT_EQ(OrthancPluginResourceType_Series, db.GetResourceType(b));

  db.AttachChild(a, b);

  int64_t c;
  ASSERT_FALSE(db.LookupParent(c, a));
  ASSERT_TRUE(db.LookupParent(c, b));
  ASSERT_EQ(a, c);

  c = db.CreateResource("series2", OrthancPluginResourceType_Series);
  db.AttachChild(a, c);

  ASSERT_EQ(3, db.GetTableRecordCount("Resources"));
  ASSERT_EQ(0, db.GetResourceCount(OrthancPluginResourceType_Patient));
  ASSERT_EQ(1, db.GetResourceCount(OrthancPluginResourceType_Study));
  ASSERT_EQ(2, db.GetResourceCount(OrthancPluginResourceType_Series));

  ASSERT_FALSE(db.GetParentPublicId(s, a));
  ASSERT_TRUE(db.GetParentPublicId(s, b));  ASSERT_EQ("study", s);
  ASSERT_TRUE(db.GetParentPublicId(s, c));  ASSERT_EQ("study", s);

  std::list<std::string> children;
  db.GetChildren(children, a);
  ASSERT_EQ(2u, children.size());
  db.GetChildren(children, b);
  ASSERT_EQ(0u, children.size());
  db.GetChildren(children, c);
  ASSERT_EQ(0u, children.size());

  std::list<std::string> cp;
  db.GetChildrenPublicId(cp, a);
  ASSERT_EQ(2, cp.size());
  ASSERT_TRUE(cp.front() == "series" || cp.front() == "series2");
  ASSERT_TRUE(cp.back() == "series" || cp.back() == "series2");
  ASSERT_NE(cp.front(), cp.back());

  std::list<std::string> pub;
  db.GetAllPublicIds(pub, OrthancPluginResourceType_Patient);
  ASSERT_EQ(0, pub.size());
  db.GetAllPublicIds(pub, OrthancPluginResourceType_Study);
  ASSERT_EQ(1, pub.size());
  ASSERT_EQ("study", pub.front());
  db.GetAllPublicIds(pub, OrthancPluginResourceType_Series);
  ASSERT_EQ(2, pub.size());
  ASSERT_TRUE(pub.front() == "series" || pub.front() == "series2");
  ASSERT_TRUE(pub.back() == "series" || pub.back() == "series2");
  ASSERT_NE(pub.front(), pub.back());

  std::list<int64_t> ci;
  db.GetChildrenInternalId(ci, a);
  ASSERT_EQ(2, ci.size());
  ASSERT_TRUE(ci.front() == b || ci.front() == c);
  ASSERT_TRUE(ci.back() == b || ci.back() == c);
  ASSERT_NE(ci.front(), ci.back());

  db.SetMetadata(a, MetadataType_ModifiedFrom, "modified");
  db.SetMetadata(a, MetadataType_LastUpdate, "update2");
  ASSERT_FALSE(db.LookupMetadata(s, b, MetadataType_LastUpdate));
  ASSERT_TRUE(db.LookupMetadata(s, a, MetadataType_LastUpdate));
  ASSERT_EQ("update2", s);
  db.SetMetadata(a, MetadataType_LastUpdate, "update");
  ASSERT_TRUE(db.LookupMetadata(s, a, MetadataType_LastUpdate));
  ASSERT_EQ("update", s);

  std::list<int32_t> md;
  db.ListAvailableMetadata(md, a);
  ASSERT_EQ(2, md.size());
  ASSERT_TRUE(md.front() == MetadataType_ModifiedFrom || md.back() == MetadataType_ModifiedFrom);
  ASSERT_TRUE(md.front() == MetadataType_LastUpdate || md.back() == MetadataType_LastUpdate);
  std::string mdd;
  ASSERT_TRUE(db.LookupMetadata(mdd, a, MetadataType_ModifiedFrom));
  ASSERT_EQ("modified", mdd);
  ASSERT_TRUE(db.LookupMetadata(mdd, a, MetadataType_LastUpdate));
  ASSERT_EQ("update", mdd);

  db.ListAvailableMetadata(md, b);
  ASSERT_EQ(0, md.size());

  db.DeleteMetadata(a, MetadataType_LastUpdate);
  db.DeleteMetadata(b, MetadataType_LastUpdate);
  ASSERT_FALSE(db.LookupMetadata(s, a, MetadataType_LastUpdate));

  db.ListAvailableMetadata(md, a);
  ASSERT_EQ(1, md.size());
  ASSERT_EQ(MetadataType_ModifiedFrom, md.front());

  ASSERT_EQ(0, db.GetTotalCompressedSize());
  ASSERT_EQ(0, db.GetTotalUncompressedSize());


  std::list<int32_t> fc;

  OrthancPluginAttachment a1;
  a1.uuid = "uuid1";
  a1.contentType = FileContentType_Dicom;
  a1.uncompressedSize = 42;
  a1.uncompressedHash = "md5_1";
  a1.compressionType = CompressionType_None;
  a1.compressedSize = 42;
  a1.compressedHash = "md5_1";
    
  OrthancPluginAttachment a2;
  a2.uuid = "uuid2";
  a2.contentType = FileContentType_DicomAsJson;
  a2.uncompressedSize = 4242;
  a2.uncompressedHash = "md5_2";
  a2.compressionType = CompressionType_None;
  a2.compressedSize = 4242;
  a2.compressedHash = "md5_2";
    
  db.AddAttachment(a, a1);
  db.ListAvailableAttachments(fc, a);
  ASSERT_EQ(1, fc.size());
  ASSERT_EQ(FileContentType_Dicom, fc.front());
  db.AddAttachment(a, a2);
  db.ListAvailableAttachments(fc, a);
  ASSERT_EQ(2, fc.size());
  ASSERT_FALSE(db.LookupAttachment(b, FileContentType_Dicom));

  ASSERT_EQ(4284, db.GetTotalCompressedSize());
  ASSERT_EQ(4284, db.GetTotalUncompressedSize());

  expectedAttachment.reset(new OrthancPluginAttachment);
  expectedAttachment->uuid = "uuid1";
  expectedAttachment->contentType = FileContentType_Dicom;
  expectedAttachment->uncompressedSize = 42;
  expectedAttachment->uncompressedHash = "md5_1";
  expectedAttachment->compressionType = CompressionType_None;
  expectedAttachment->compressedSize = 42;
  expectedAttachment->compressedHash = "md5_1";
  ASSERT_TRUE(db.LookupAttachment(a, FileContentType_Dicom));

  expectedAttachment.reset(new OrthancPluginAttachment);
  expectedAttachment->uuid = "uuid2";
  expectedAttachment->contentType = FileContentType_DicomAsJson;
  expectedAttachment->uncompressedSize = 4242;
  expectedAttachment->uncompressedHash = "md5_2";
  expectedAttachment->compressionType = CompressionType_None;
  expectedAttachment->compressedSize = 4242;
  expectedAttachment->compressedHash = "md5_2";
  ASSERT_TRUE(db.LookupAttachment(a, FileContentType_DicomAsJson));

  db.ListAvailableAttachments(fc, b);
  ASSERT_EQ(0, fc.size());
  db.DeleteAttachment(a, FileContentType_Dicom);
  db.ListAvailableAttachments(fc, a);
  ASSERT_EQ(1, fc.size());
  ASSERT_EQ(FileContentType_DicomAsJson, fc.front());
  db.DeleteAttachment(a, FileContentType_DicomAsJson);
  db.ListAvailableAttachments(fc, a);
  ASSERT_EQ(0, fc.size());


  db.SetIdentifierTag(a, 0x0010, 0x0020, "patient");
  db.SetIdentifierTag(a, 0x0020, 0x000d, "study");

  expectedDicomTags.clear();
  expectedDicomTags.push_back(OrthancPluginDicomTag());
  expectedDicomTags.push_back(OrthancPluginDicomTag());
  expectedDicomTags.front().group = 0x0010;
  expectedDicomTags.front().element = 0x0020;
  expectedDicomTags.front().value = "patient";
  expectedDicomTags.back().group = 0x0020;
  expectedDicomTags.back().element = 0x000d;
  expectedDicomTags.back().value = "study";
  db.GetMainDicomTags(a);


  db.LookupIdentifier(ci, 0x0010, 0x0020, "patient");
  ASSERT_EQ(1, ci.size());
  ASSERT_EQ(a, ci.front());
  db.LookupIdentifier(ci, 0x0010, 0x0020, "study");
  ASSERT_EQ(0, ci.size());
  db.LookupIdentifier(ci, "study");
  ASSERT_EQ(1, ci.size());
  ASSERT_EQ(a, ci.front());


  OrthancPluginExportedResource exp;
  exp.seq = -1;
  exp.resourceType = OrthancPluginResourceType_Study;
  exp.publicId = "id";
  exp.modality = "remote";
  exp.date = "date";
  exp.patientId = "patient";
  exp.studyInstanceUid = "study";
  exp.seriesInstanceUid = "series";
  exp.sopInstanceUid = "instance";
  db.LogExportedResource(exp);
    
  expectedExported.reset(new OrthancPluginExportedResource());
  *expectedExported = exp;
  expectedExported->seq = 1;

  bool done;
  db.GetExportedResources(done, 0, 10);
  

  db.GetAllPublicIds(pub, OrthancPluginResourceType_Patient); ASSERT_EQ(0, pub.size());
  db.GetAllPublicIds(pub, OrthancPluginResourceType_Study); ASSERT_EQ(1, pub.size());
  db.GetAllPublicIds(pub, OrthancPluginResourceType_Series); ASSERT_EQ(2, pub.size());
  db.GetAllPublicIds(pub, OrthancPluginResourceType_Instance); ASSERT_EQ(0, pub.size());
  ASSERT_EQ(3, db.GetTableRecordCount("Resources"));

  ASSERT_EQ(0, db.GetTableRecordCount("PatientRecyclingOrder"));  // No patient was inserted
  ASSERT_TRUE(db.IsExistingResource(c));
  db.DeleteResource(c);
  ASSERT_FALSE(db.IsExistingResource(c));
  ASSERT_TRUE(db.IsExistingResource(a));
  ASSERT_TRUE(db.IsExistingResource(b));
  ASSERT_EQ(2, db.GetTableRecordCount("Resources"));
  db.DeleteResource(a);
  ASSERT_EQ(0, db.GetTableRecordCount("Resources"));
  ASSERT_FALSE(db.IsExistingResource(a));
  ASSERT_FALSE(db.IsExistingResource(b));
  ASSERT_FALSE(db.IsExistingResource(c));

  ASSERT_EQ(0, db.GetTableRecordCount("Resources"));
  ASSERT_EQ(0, db.GetTableRecordCount("PatientRecyclingOrder"));
  int64_t p1 = db.CreateResource("patient1", OrthancPluginResourceType_Patient);
  int64_t p2 = db.CreateResource("patient2", OrthancPluginResourceType_Patient);
  int64_t p3 = db.CreateResource("patient3", OrthancPluginResourceType_Patient);
  ASSERT_EQ(3, db.GetTableRecordCount("PatientRecyclingOrder"));
  int64_t r;
  ASSERT_TRUE(db.SelectPatientToRecycle(r));
  ASSERT_EQ(p1, r);
  ASSERT_TRUE(db.SelectPatientToRecycle(r, p1));
  ASSERT_EQ(p2, r);
  ASSERT_FALSE(db.IsProtectedPatient(p1));
  db.SetProtectedPatient(p1, true);
  ASSERT_TRUE(db.IsProtectedPatient(p1));
  ASSERT_TRUE(db.SelectPatientToRecycle(r));
  ASSERT_EQ(p2, r);
  db.SetProtectedPatient(p1, false);
  ASSERT_FALSE(db.IsProtectedPatient(p1));
  ASSERT_TRUE(db.SelectPatientToRecycle(r));
  ASSERT_EQ(p2, r);
  db.DeleteResource(p2);
  ASSERT_TRUE(db.SelectPatientToRecycle(r, p3));
  ASSERT_EQ(p1, r);
}
