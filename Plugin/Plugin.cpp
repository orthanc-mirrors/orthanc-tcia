/**
 * TCIA plugin for Orthanc
 * Copyright (C) 2021 Sebastien Jodogne, UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#if !defined(ORTHANC_STANDALONE)
#  error Macro ORTHANC_STANDALONE must be defined
#endif

#include "TciaImportJob.h"
#include "HttpCache.h"
#include "CsvParser.h"

#include <EmbeddedResources.h>

#include <Logging.h>
#include <OrthancException.h>
#include <SerializationToolbox.h>
#include <Toolbox.h>

#if ORTHANC_STANDALONE != 1
#  include <SystemToolbox.h>
#endif



static OrthancPluginJob* TciaJobUnserializer(const char *jobType,
                                             const char *serialized)
{
  try
  {
    Json::Value value;

    if (std::string(jobType) == OrthancPlugins::TciaImportJob::GetJobType() &&
        OrthancPlugins::ReadJson(value, serialized))
    {
      return OrthancPlugins::OrthancJob::Create(OrthancPlugins::TciaImportJob::Unserialize(value));
    }
    else
    {
      return NULL;
    }
  }
  catch (Orthanc::OrthancException& e)
  {
    return NULL;
  }
}



static void TciaHttpProxy(OrthancPluginRestOutput* output,
                          const char* url,
                          const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(OrthancPlugins::GetGlobalContext(), output, "GET");
  }
  else
  {
    assert(TCIA_BASE_URL[strlen(TCIA_BASE_URL) - 1] != '/' &&
           TCIA_BASE_URL[strlen(TCIA_BASE_URL)] == 0);
    
    std::string tcia = std::string(TCIA_BASE_URL) + "/" + std::string(request->groups[0]);

    for (uint32_t i = 0; i < request->getCount; i++)
    {
      if (i == 0)
      {
        tcia += "?";
      }
      else
      {
        tcia += "&";
      }

      std::string encoded;
      Orthanc::Toolbox::UriEncode(encoded, request->getValues[i]);
    
      tcia += std::string(request->getKeys[i]) + "=" + encoded;
    }

    std::string oldBody, oldMime;
    if (OrthancPlugins::HttpCache::GetInstance().Read(oldBody, oldMime, tcia))
    {
      OrthancPluginAnswerBuffer(OrthancPlugins::GetGlobalContext(), output,
                                oldBody.empty() ? NULL : oldBody.c_str(), oldBody.size(), oldMime.c_str());
    }
    else
    {
      OrthancPlugins::MemoryBuffer body, headersBuffer;
      uint16_t status;
    
      if (OrthancPluginErrorCode_Success == OrthancPluginHttpClient(
            OrthancPlugins::GetGlobalContext(), *body, *headersBuffer, &status,
            OrthancPluginHttpMethod_Get, tcia.c_str(),
            0, NULL, NULL, NULL, 0, "" /* username */, "" /* password */,
            0 /* default timeout */, NULL, NULL, NULL, 0))
      {
        Json::Value headers;
        headersBuffer.ToJson(headers);

        static const char* const CONTENT_TYPE = "Content-Type";
      
        std::string mime = "application/octet-stream";
        if (headers.type() == Json::objectValue &&
            headers.isMember(CONTENT_TYPE))
        {
          mime = Orthanc::SerializationToolbox::ReadString(headers, CONTENT_TYPE);
        }

        OrthancPlugins::HttpCache::GetInstance().Write(tcia, body.GetData(), body.GetSize(), mime);

        OrthancPluginAnswerBuffer(OrthancPlugins::GetGlobalContext(), output, body.GetData(), body.GetSize(), mime.c_str());
      }
      else
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_InexistentItem,
                                        "Cannot proxy HTTP request to TCIA: " + tcia);
      }
    }
  }
}


static void TciaImport(OrthancPluginRestOutput* output,
                       const char* url,
                       const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Post)
  {
    OrthancPluginSendMethodNotAllowed(OrthancPlugins::GetGlobalContext(), output, "POST");
  }
  else
  {
    static const char* const CONTENT = "Content";

    Json::Value body;
    if (OrthancPlugins::ReadJson(body, request->body, request->bodySize))
    {
      if (Orthanc::SerializationToolbox::ReadString(body, "Type") == "NbiaClientSpreadsheet")
      {
        std::string csv;
        Orthanc::Toolbox::DecodeBase64(csv, Orthanc::SerializationToolbox::ReadString(body, CONTENT));
      
        std::unique_ptr<OrthancPlugins::TciaImportJob> job(new OrthancPlugins::TciaImportJob);
        job->AddNbiaClientSpreadsheet(csv);
      
        OrthancPlugins::OrthancJob::SubmitFromRestApiPost(output, body, job.release());
      }
      else if (Orthanc::SerializationToolbox::ReadString(body, "Type") == "Series" &&
               body.isMember(CONTENT) &&
               body[CONTENT].type() == Json::arrayValue)
      {
        std::unique_ptr<OrthancPlugins::TciaImportJob> job(new OrthancPlugins::TciaImportJob);

        for (Json::Value::ArrayIndex i = 0; i < body[CONTENT].size(); i++)
        {
          job->AddSeries(OrthancPlugins::TciaImportJob::Series::Unserialize(body[CONTENT][i]));
        }
        
        OrthancPlugins::OrthancJob::SubmitFromRestApiPost(output, body, job.release());
      }
      else
      {
        throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat);
      }
    }
    else
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_BadFileFormat);
    }
  }
}


static void ServeHtml(OrthancPluginRestOutput* output,
                      const char* url,
                      const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(OrthancPlugins::GetGlobalContext(), output, "GET");
  }
  else
  {
    std::string s;

#if ORTHANC_STANDALONE == 1
    Orthanc::EmbeddedResources::GetFileResource(s, Orthanc::EmbeddedResources::TCIA_HTML);
#else
    Orthanc::SystemToolbox::ReadFile(s, std::string(PLUGIN_SOURCE_DIR) + "/WebApplication/index.html");
#endif
    
    OrthancPluginAnswerBuffer(OrthancPlugins::GetGlobalContext(), output,
                              s.c_str(), s.size(), "text/html");
  }
}


static void ServeJavaScript(OrthancPluginRestOutput* output,
                            const char* url,
                            const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(OrthancPlugins::GetGlobalContext(), output, "GET");
  }
  else
  {
    std::string s;

#if ORTHANC_STANDALONE == 1
    Orthanc::EmbeddedResources::GetFileResource(s, Orthanc::EmbeddedResources::TCIA_JS);
#else
    Orthanc::SystemToolbox::ReadFile(s, std::string(PLUGIN_SOURCE_DIR) + "/WebApplication/app.js");
#endif

    OrthancPluginAnswerBuffer(OrthancPlugins::GetGlobalContext(), output,
                              s.c_str(), s.size(), "application/javascript");
  }
}


static void ClearCache(OrthancPluginRestOutput* output,
                       const char* url,
                       const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Post)
  {
    OrthancPluginSendMethodNotAllowed(OrthancPlugins::GetGlobalContext(), output, "POST");
  }
  else
  {
    OrthancPlugins::HttpCache::GetInstance().Clear();
    LOG(WARNING) << "The TCIA cache has been cleared";
    OrthancPluginAnswerBuffer(OrthancPlugins::GetGlobalContext(), output, "", 0, "text/plain");
  }
}


template <enum Orthanc::EmbeddedResources::FileResourceId resource,
          enum Orthanc::MimeType mime>
static void ServeEmbeddedResource(OrthancPluginRestOutput* output,
                                  const char* url,
                                  const OrthancPluginHttpRequest* request)
{
  if (request->method != OrthancPluginHttpMethod_Get)
  {
    OrthancPluginSendMethodNotAllowed(OrthancPlugins::GetGlobalContext(), output, "GET");
  }
  else
  {
    OrthancPluginAnswerBuffer(OrthancPlugins::GetGlobalContext(), output,
                              reinterpret_cast<const char*>(Orthanc::EmbeddedResources::GetFileResourceBuffer(resource)),
                              Orthanc::EmbeddedResources::GetFileResourceSize(resource),
                              Orthanc::EnumerationToString(mime));
  }
}


template <enum Orthanc::EmbeddedResources::FileResourceId resource,
          enum Orthanc::MimeType mime>
static void RegisterEmbeddedResource(const std::string& uri)
{
  OrthancPlugins::RegisterRestCallback<
    ServeEmbeddedResource<resource, mime> >(uri, true /* thread safe */);
}


extern "C"
{
  ORTHANC_PLUGINS_API int32_t OrthancPluginInitialize(OrthancPluginContext* context)
  {
    OrthancPlugins::SetGlobalContext(context);
    Orthanc::Logging::InitializePluginContext(context);
    Orthanc::Logging::EnableInfoLevel(true);

    /* Check the version of the Orthanc core */
    if (OrthancPluginCheckVersion(context) == 0)
    {
      OrthancPlugins::ReportMinimalOrthancVersion(ORTHANC_PLUGINS_MINIMAL_MAJOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_MINOR_NUMBER,
                                                  ORTHANC_PLUGINS_MINIMAL_REVISION_NUMBER);
      return -1;
    }

    OrthancPluginSetDescription(context, "Interface with TCIA (The Cancer Imaging Archive).");
    OrthancPluginSetRootUri(context, "/tcia/app/index.html");

    {
      std::string explorer;
      Orthanc::EmbeddedResources::GetFileResource(
        explorer, Orthanc::EmbeddedResources::ORTHANC_EXPLORER_JS);
      OrthancPluginExtendOrthancExplorer(OrthancPlugins::GetGlobalContext(), explorer.c_str());
    }
  
    OrthancPluginRegisterJobsUnserializer(context, TciaJobUnserializer);

    OrthancPlugins::RegisterRestCallback<ServeHtml>("/tcia/app/index.html", true /* thread safe */);
    OrthancPlugins::RegisterRestCallback<ServeJavaScript>("/tcia/app/app.js", true /* thread safe */);
    OrthancPlugins::RegisterRestCallback<ClearCache>("/tcia/clear-cache", true /* thread safe */);
    OrthancPlugins::RegisterRestCallback<TciaHttpProxy>("/tcia/proxy/(.*)", true /* thread safe */);
    OrthancPlugins::RegisterRestCallback<TciaImport>("/tcia/import", true /* thread safe */);

    {
      using namespace Orthanc;
      
      RegisterEmbeddedResource<EmbeddedResources::BOOTSTRAP_MIN_CSS, MimeType_Css>(
        "/tcia/app/css/bootstrap.min.css");

      RegisterEmbeddedResource<EmbeddedResources::BOOTSTRAP_MIN_CSS_MAP, MimeType_Binary>(
        "/tcia/app/css/bootstrap.min.css.map");

      RegisterEmbeddedResource<EmbeddedResources::AXIOS_MIN_JS, MimeType_JavaScript>(
        "/tcia/app/js/axios.min.js");

      RegisterEmbeddedResource<EmbeddedResources::AXIOS_MIN_MAP, MimeType_JavaScript>(
        "/tcia/app/js/axios.min.map");

      RegisterEmbeddedResource<EmbeddedResources::VUE_MIN_JS, MimeType_JavaScript>(
        "/tcia/app/js/vue.min.js");

      RegisterEmbeddedResource<EmbeddedResources::TCIA_LOGO, MimeType_Png>(
        "/tcia/app/tcia-logo.png");

      RegisterEmbeddedResource<EmbeddedResources::ORTHANC_LOGO, MimeType_Png>(
        "/tcia/app/orthanc-logo.png");

      RegisterEmbeddedResource<EmbeddedResources::NBIA_EXPORT, MimeType_Png>(
        "/tcia/app/nbia-export.png");
    }

    return 0;
  }


  ORTHANC_PLUGINS_API void OrthancPluginFinalize()
  {
    OrthancPlugins::LogWarning("TCIA plugin is finalizing");
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetName()
  {
    return "tcia";
  }


  ORTHANC_PLUGINS_API const char* OrthancPluginGetVersion()
  {
    return ORTHANC_PLUGIN_VERSION;
  }
}
