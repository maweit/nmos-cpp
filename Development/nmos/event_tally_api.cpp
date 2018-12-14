#include "nmos/event_tally_api.h"

#include "nmos/api_downgrade.h"
#include "nmos/api_utils.h"
#include "nmos/model.h"

namespace nmos
{
    namespace experimental
    {
        web::http::experimental::listener::api_router make_unmounted_event_tally_api(nmos::model& model, slog::base_gate& gate);

        web::http::experimental::listener::api_router make_event_tally_api(nmos::model& model, slog::base_gate& gate)
        {
            using namespace web::http::experimental::listener::api_router_using_declarations;

            api_router event_tally_api;

            event_tally_api.support(U("/?"), methods::GET, [](http_request, http_response res, const string_t&, const route_parameters&)
            {
                set_reply(res, status_codes::OK, nmos::make_sub_routes_body({ U("x-nmos/") }, res));
                return pplx::task_from_result(true);
            });
            
            event_tally_api.support(U("/x-nmos/?"), methods::GET, [](http_request, http_response res, const string_t&, const route_parameters&)
            {
                set_reply(res, status_codes::OK, nmos::make_sub_routes_body({ U("events/") }, res));
                return pplx::task_from_result(true);
            });

            event_tally_api.support(U("/x-nmos/") + nmos::patterns::eventTally_api.pattern + U("/?"), methods::GET, [](http_request, http_response res, const string_t&, const route_parameters&)
            {
                set_reply(res, status_codes::OK, nmos::make_sub_routes_body({ U("v1.0/") }, res));
                return pplx::task_from_result(true);
            });

            event_tally_api.mount(U("/x-nmos/") + nmos::patterns::eventTally_api.pattern + U("/") + nmos::patterns::is04_version.pattern, make_unmounted_event_tally_api(model, gate));

            return event_tally_api;
        }

        auto find_event_resource(nmos::resources& resources, const nmos::type & type, const utility::string_t & id) -> resources::iterator {
            resources::nth_index<0>::type &idx = resources.get<0>();
            for( resources::nth_index<0>::type::iterator it = idx.begin(); it != idx.end(); ++it ) {
                if ( it->type == type && !it->event.data_state.is_null()  ) {
                    if ( it->event.data_state.has_field("identity") ) {
                        std::cout<<it->event.data_state<<std::endl;
                        if( it->event.data_state.at("identity").has_field("source_id") ) {
                            if ( it->event.data_state.at("identity").at("source_id").as_string() == id )
                                return it;
                        }
                    }
                }
            }
            return resources.end();
        }

        web::http::experimental::listener::api_router make_unmounted_event_tally_api(nmos::model& model, slog::base_gate& gate)
        {
            using namespace web::http::experimental::listener::api_router_using_declarations;
            
            api_router event_tally_api;
            
            event_tally_api.support(U("/?"), methods::GET, [](http_request, http_response res, const string_t&, const route_parameters&)
            {
                set_reply(res, status_codes::OK, nmos::make_sub_routes_body({ U("sources/") }, res));
                return pplx::task_from_result(true);
            });
            
            event_tally_api.support(U("/") + nmos::patterns::eventTallyType.pattern + U("/?"), methods::GET, [&model, &gate](http_request req, http_response res, const string_t&, const route_parameters& parameters)
            {
                auto lock = model.read_lock();
                auto& resources = model.node_resources;
                
                const string_t resourceType = parameters.at(nmos::patterns::eventTallyType.name);
                
                const auto match = [&](const nmos::resources::value_type& resource) { return resource.type == nmos::type_from_resourceType(resourceType) && !resource.event.data_state.is_null(); };
                
                size_t count = 0;
                
                set_reply(res, status_codes::OK,
                          web::json::serialize_if(resources,
                                                  match,
                                                  [&count](const nmos::resources::value_type& resource) { ++count; return value(resource.id + U("/")); }),
                          U("application/json"));
                
                slog::log<slog::severities::info>(gate, SLOG_FLF) << nmos::api_stash(req, parameters) << "Returning " << count << " matching " << resourceType;
                
                return pplx::task_from_result(true);
            });
            
            event_tally_api.support(U("/") + nmos::patterns::eventTallyType.pattern + U("/") + nmos::patterns::resourceId.pattern + U("/?"), methods::GET, [&model, &gate](http_request req, http_response res, const string_t&, const route_parameters& parameters)
            {
                auto lock = model.read_lock();
                auto& resources = model.node_resources;
                
                const string_t resourceType = parameters.at(nmos::patterns::connectorType.name);
                const string_t resourceId = parameters.at(nmos::patterns::resourceId.name);
                
                auto resource = find_event_resource(resources, nmos::type_from_resourceType(resourceType), resourceId);
                if (resources.end() != resource) {
                    set_reply(res, status_codes::OK, nmos::make_sub_routes_body({ U("state/"), U("type/") }, res));
                }
                else {
                    set_reply(res, status_codes::NotFound);
                }
                return pplx::task_from_result(true);
            });
 
            event_tally_api.support(U("/") + nmos::patterns::eventTallyType.pattern + U("/") + nmos::patterns::resourceId.pattern + U("/") + nmos::patterns::eventTallyStateType.pattern , methods::GET, [&model, &gate](http_request req, http_response res, const string_t&, const route_parameters& parameters)
            {
                auto lock = model.read_lock();
                auto& resources = model.node_resources;
                
                const string_t resourceType = parameters.at(nmos::patterns::connectorType.name);
                const string_t resourceId = parameters.at(nmos::patterns::resourceId.name);
                const string_t eventTallyType = parameters.at(nmos::patterns::eventTallyStateType.name);
                
                auto resource = find_event_resource(resources, nmos::type_from_resourceType(resourceType), resourceId);
                if (resources.end() != resource) {
                    web::json::value answer;
                    utility::string_t creation = make_version(resource->created);
                    if (nmos::fields::endpoint_event_tally_state.key == eventTallyType ) {
                        answer = resource->event.data_state;
                    }
                    else if (nmos::fields::endpoint_event_tally_type.key == eventTallyType ) {
                        answer = resource->event.data_type;
                    }
                    set_reply(res, status_codes::OK, answer);
                }
                else {
                    set_reply(res, status_codes::NotFound);
                }
                return pplx::task_from_result(true);
            });

            return event_tally_api;
        }
     
    }
}
