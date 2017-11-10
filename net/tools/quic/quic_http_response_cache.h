// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_HTTP_RESPONSE_CACHE_H_
#define NET_TOOLS_QUIC_QUIC_HTTP_RESPONSE_CACHE_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "net/quic/core/spdy_utils.h"
#include "net/quic/platform/api/quic_mutex.h"
#include "net/quic/platform/api/quic_string_piece.h"
#include "net/quic/platform/api/quic_url.h"
#include "net/spdy/core/spdy_framer.h"

namespace net {

// In-memory cache for HTTP responses.
// Reads from disk cache generated by:
// `wget -p --save_headers <url>`
class QuicHttpResponseCache {
 public:
  // A ServerPushInfo contains path of the push request and everything needed in
  // comprising a response for the push request.
  struct ServerPushInfo {
    ServerPushInfo(QuicUrl request_url,
                   SpdyHeaderBlock headers,
                   SpdyPriority priority,
                   std::string body);
    ServerPushInfo(const ServerPushInfo& other);
    QuicUrl request_url;
    SpdyHeaderBlock headers;
    SpdyPriority priority;
    std::string body;
  };

  enum SpecialResponseType {
    REGULAR_RESPONSE,  // Send the headers and body like a server should.
    CLOSE_CONNECTION,  // Close the connection (sending the close packet).
    IGNORE_REQUEST,    // Do nothing, expect the client to time out.
  };

  // Container for response header/body pairs.
  class Response {
   public:
    Response();
    ~Response();

    SpecialResponseType response_type() const { return response_type_; }
    const SpdyHeaderBlock& headers() const { return headers_; }
    const SpdyHeaderBlock& trailers() const { return trailers_; }
    const QuicStringPiece body() const { return QuicStringPiece(body_); }

    void set_response_type(SpecialResponseType response_type) {
      response_type_ = response_type;
    }
    void set_headers(SpdyHeaderBlock headers) { headers_ = std::move(headers); }
    void set_trailers(SpdyHeaderBlock trailers) {
      trailers_ = std::move(trailers);
    }
    void set_body(QuicStringPiece body) {
      body_.assign(body.data(), body.size());
    }

   private:
    SpecialResponseType response_type_;
    SpdyHeaderBlock headers_;
    SpdyHeaderBlock trailers_;
    std::string body_;

    DISALLOW_COPY_AND_ASSIGN(Response);
  };

  // Class to manage loading a resource file into memory.  There are
  // two uses: called by InitializeFromDirectory to load resources
  // from files, and recursively called when said resources specify
  // server push associations.
  class ResourceFile {
   public:
    explicit ResourceFile(const base::FilePath& file_name);
    virtual ~ResourceFile();

    void Read();

    // |base| is |file_name_| with |cache_directory| prefix stripped.
    void SetHostPathFromBase(QuicStringPiece base);

    const std::string& file_name() { return file_name_string_; }

    QuicStringPiece host() { return host_; }
    void set_host(QuicStringPiece host) { host_ = host; }

    QuicStringPiece path() { return path_; }
    void set_path(QuicStringPiece path) { path_ = path; }

    const SpdyHeaderBlock& spdy_headers() { return spdy_headers_; }

    QuicStringPiece body() { return body_; }

    const std::vector<QuicStringPiece>& push_urls() { return push_urls_; }

   protected:
    void HandleXOriginalUrl();
    void HandlePushUrls(const std::vector<QuicStringPiece>& push_urls);
    QuicStringPiece RemoveScheme(QuicStringPiece url);

    const std::string cache_directory_;
    const base::FilePath file_name_;
    const std::string file_name_string_;
    std::string file_contents_;
    QuicStringPiece body_;
    SpdyHeaderBlock spdy_headers_;
    QuicStringPiece x_original_url_;
    std::vector<QuicStringPiece> push_urls_;

   private:
    QuicStringPiece host_;
    QuicStringPiece path_;
    QuicHttpResponseCache* cache_;

    DISALLOW_COPY_AND_ASSIGN(ResourceFile);
  };

  QuicHttpResponseCache();
  ~QuicHttpResponseCache();

  // Retrieve a response from this cache for a given host and path..
  // If no appropriate response exists, nullptr is returned.
  const Response* GetResponse(QuicStringPiece host, QuicStringPiece path) const;

  // Adds a simple response to the cache.  The response headers will
  // only contain the "content-length" header with the length of |body|.
  void AddSimpleResponse(QuicStringPiece host,
                         QuicStringPiece path,
                         int response_code,
                         QuicStringPiece body);

  // Add a simple response to the cache as AddSimpleResponse() does, and add
  // some server push resources(resource path, corresponding response status and
  // path) associated with it.
  // Push resource implicitly come from the same host.
  void AddSimpleResponseWithServerPushResources(
      QuicStringPiece host,
      QuicStringPiece path,
      int response_code,
      QuicStringPiece body,
      std::list<ServerPushInfo> push_resources);

  // Add a response to the cache.
  void AddResponse(QuicStringPiece host,
                   QuicStringPiece path,
                   SpdyHeaderBlock response_headers,
                   QuicStringPiece response_body);

  // Add a response, with trailers, to the cache.
  void AddResponse(QuicStringPiece host,
                   QuicStringPiece path,
                   SpdyHeaderBlock response_headers,
                   QuicStringPiece response_body,
                   SpdyHeaderBlock response_trailers);

  // Simulate a special behavior at a particular path.
  void AddSpecialResponse(QuicStringPiece host,
                          QuicStringPiece path,
                          SpecialResponseType response_type);

  // Sets a default response in case of cache misses.  Takes ownership of
  // 'response'.
  void AddDefaultResponse(Response* response);

  // |cache_cirectory| can be generated using `wget -p --save-headers <url>`.
  void InitializeFromDirectory(const std::string& cache_directory);

  // Find all the server push resources associated with |request_url|.
  std::list<ServerPushInfo> GetServerPushResources(std::string request_url);

 private:
  void AddResponseImpl(QuicStringPiece host,
                       QuicStringPiece path,
                       SpecialResponseType response_type,
                       SpdyHeaderBlock response_headers,
                       QuicStringPiece response_body,
                       SpdyHeaderBlock response_trailers);

  std::string GetKey(QuicStringPiece host, QuicStringPiece path) const;

  // Add some server push urls with given responses for specified
  // request if these push resources are not associated with this request yet.
  void MaybeAddServerPushResources(QuicStringPiece request_host,
                                   QuicStringPiece request_path,
                                   std::list<ServerPushInfo> push_resources);

  // Check if push resource(push_host/push_path) associated with given request
  // url already exists in server push map.
  bool PushResourceExistsInCache(std::string original_request_url,
                                 ServerPushInfo resource);

  // Cached responses.
  std::unordered_map<std::string, std::unique_ptr<Response>> responses_
      GUARDED_BY(response_mutex_);

  // The default response for cache misses, if set.
  std::unique_ptr<Response> default_response_ GUARDED_BY(response_mutex_);

  // A map from request URL to associated server push responses (if any).
  std::multimap<std::string, ServerPushInfo> server_push_resources_
      GUARDED_BY(response_mutex_);

  // Protects against concurrent access from test threads setting responses, and
  // server threads accessing those responses.
  mutable QuicMutex response_mutex_;

  DISALLOW_COPY_AND_ASSIGN(QuicHttpResponseCache);
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_HTTP_RESPONSE_CACHE_H_
