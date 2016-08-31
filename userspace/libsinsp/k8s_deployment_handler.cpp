//
// k8s_deployment_handler.cpp
//

#include "k8s_deployment_handler.h"
#include "sinsp.h"
#include "sinsp_int.h"

// filters normalize state and event JSONs, so they can be processed generically:
// event is turned into a single-entry array, state is turned into an array of ADDED events

std::string k8s_deployment_handler::EVENT_FILTER =
	"{"
	" type: .type,"
	" apiVersion: .object.apiVersion,"
	" kind: .object.kind,"
	" items:"
	" ["
	"  .object |"
	"  {"
	"   name: .metadata.name,"
	"   uid: .metadata.uid,"
	"   timestamp: .metadata.creationTimestamp,"
	"   specReplicas: .spec.replicas,"
	"   statReplicas: .status.replicas,"
	"   selector: .spec.selector.matchLabels,"
	"   labels: .metadata.labels"
	"  }"
	" ]"
	"}";

std::string k8s_deployment_handler::STATE_FILTER =
	"{"
	" type: \"ADDED\","
	" apiVersion: .apiVersion,"
	" kind: \"Deployment\", "
	" items:"
	" ["
	"  .items[] | "
	"  {"
	"   name: .metadata.name,"
	"   uid: .metadata.uid,"
	"   timestamp: .metadata.creationTimestamp,"
	"   specReplicas: .spec.replicas,"
	"   statReplicas: .status.replicas,"
	"   selector: .spec.selector.matchLabels,"
	"   labels: .metadata.labels"
	"   }"
	" ]"
	"}";

k8s_deployment_handler::k8s_deployment_handler(k8s_state_t& state,
	collector_t& collector,
	std::string url,
	const std::string& http_version,
	ssl_ptr_t ssl,
	bt_ptr_t bt):
		k8s_handler(collector, "k8s_deployment_handler", true,
					url, "/apis/extensions/v1beta1/deployments",
					STATE_FILTER, EVENT_FILTER, http_version,
					1000L, ssl, bt, &state)
{
}

k8s_deployment_handler::~k8s_deployment_handler()
{
}

void k8s_deployment_handler::handle_component(const Json::Value& json, const msg_data* data)
{
	if(data)
	{
		if(m_state)
		{
			if((data->m_reason == k8s_component::COMPONENT_ADDED) ||
			   (data->m_reason == k8s_component::COMPONENT_MODIFIED))
			{
				k8s_deployment_t& deployment =
					m_state->get_component<k8s_deployments, k8s_deployment_t>(m_state->get_deployments(),
																  data->m_name, data->m_uid);
				k8s_pair_list entries = k8s_component::extract_object(json, "labels");
				if(entries.size() > 0)
				{
					deployment.set_labels(std::move(entries));
				}
				handle_selectors(deployment, json["selector"]);
				const Json::Value& spec = json["specReplicas"];
				const Json::Value& stat = json["statReplicas"];
				if(!spec.isNull() && spec.isConvertibleTo(Json::intValue) &&
				   !stat.isNull() && stat.isConvertibleTo(Json::intValue))
				{
					deployment.set_replicas(spec.asInt(), stat.asInt());
				}
			}
			else if(data->m_reason == k8s_component::COMPONENT_DELETED)
			{
				if(!m_state->delete_component(m_state->get_deployments(), data->m_uid))
				{
					log_not_found(*data);
				}
			}
			else if(data->m_reason != k8s_component::COMPONENT_ERROR)
			{
				g_logger.log(std::string("Unsupported K8S " + name() + " event reason: ") +
							 std::to_string(data->m_reason), sinsp_logger::SEV_ERROR);
			}
		}
		else
		{
			throw sinsp_exception("K8s node handler: state is null.");
		}
	}
	else
	{
		throw sinsp_exception("K8s node handler: data is null.");
	}
}
