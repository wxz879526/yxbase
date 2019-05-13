#include "pch.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "URLRequestContextGetter.h"

URLRequestContextGetter::URLRequestContextGetter(scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
	: network_task_runner_(network_task_runner)
{

}

net::URLRequestContext* URLRequestContextGetter::GetURLRequestContext()
{
	CHECK(network_task_runner_->BelongsToCurrentThread());
	if (!url_quest_context_)
	{
		net::URLRequestContextBuilder builder;
		builder.set_user_agent("yxbase url request");
		builder.DisableHttpCache();
		builder.set_proxy_config_service(base::MakeUnique<net::ProxyConfigServiceFixed>(
			net::ProxyConfig::CreateDirect()));
		url_quest_context_ = builder.Build();
	}

	return url_quest_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner> URLRequestContextGetter::GetNetworkTaskRunner() const
{
	return network_task_runner_;
}

URLRequestContextGetter::~URLRequestContextGetter()
{
}
