#pragma once

#include <memory>
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/url_request/url_request_context_getter.h"

#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"
#include "base/synchronization/waitable_event.h"

namespace base {
	class SingleThreadTaskRunner;
}

namespace net {
	class URLRequestContextGetter;
}

class URLRequestContextGetter : public net::URLRequestContextGetter
{
public:
	explicit URLRequestContextGetter(scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);
	net::URLRequestContext* GetURLRequestContext() override;
	scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner() const override;

private:
	~URLRequestContextGetter() override;

	scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

	std::unique_ptr<net::URLRequestContext> url_quest_context_;

	DISALLOW_COPY_AND_ASSIGN(URLRequestContextGetter);
};

class SyncUrlFetcher : public net::URLFetcherDelegate {
public:
	SyncUrlFetcher(const GURL& url,
		URLRequestContextGetter* getter,
		std::string *response)
		: url_(url),
		getter_(getter),
		response_(response),
		event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
			base::WaitableEvent::InitialState::NOT_SIGNALED)
	{

	}

	~SyncUrlFetcher() override {

	}

	bool Fetch() {
		getter_->GetNetworkTaskRunner()->PostTask(
			FROM_HERE,
			base::Bind(&SyncUrlFetcher::FetchOnIOThread, base::Unretained(this)));
		event_.Wait();
		return success_;
	}

	void FetchOnIOThread() {
		fetcher_ = net::URLFetcher::Create(url_, net::URLFetcher::GET, this);
		fetcher_->SetRequestContext(getter_);
		fetcher_->Start();
	}

	void OnURLFetchComplete(const net::URLFetcher* source) override {
		success_ = (source->GetResponseCode() == 200);
		if (success_)
			success_ = source->GetResponseAsString(response_);

		fetcher_.reset();
		event_.Signal();
	}

private:
	GURL url_;
	URLRequestContextGetter *getter_;
	std::string *response_;
	base::WaitableEvent event_;
	std::unique_ptr<net::URLFetcher> fetcher_;
	bool success_;
};