#ifndef SWARM_SERVICE_HPP
#define SWARM_SERVICE_HPP

#include"service/server.hpp"
#include"network/tcp_server.hpp"
#include"protocols.hpp"
#include"commandline/parameter_parser.hpp"

#include"http/server.hpp"
#include"http/middleware/allow_origin.hpp"
#include"http/middleware/color_log.hpp"


class FetchSwarmService : public fetch::protocols::SwarmProtocol  
{
public:
  FetchSwarmService(uint16_t port, uint16_t http_port, std::string const& pk,
    fetch::network::ThreadManager *tm ) :
    fetch::protocols::SwarmProtocol( tm,  fetch::protocols::FetchProtocols::SWARM, details_),
    thread_manager_( tm ),
    service_(port, thread_manager_),
    http_server_(http_port, thread_manager_)
  {
    using namespace fetch::protocols;
    
    std::cout << "Listening for peers on " << (port) << ", clients on " << (http_port ) << std::endl;
      
    details_.public_key() = pk;
    details_.default_port() = port;
    details_.default_http_port() = http_port;

    service_.Add(fetch::protocols::FetchProtocols::SWARM, this);

    // Setting callback to resolve IP
    this->SetClientIPCallback([this](uint64_t const &n) -> std::string {
        return service_.GetAddress(n);        
      });

    // Creating a http server based on the swarm protocol
    http_server_.AddMiddleware( fetch::http::middleware::AllowOrigin("*") );       
    http_server_.AddMiddleware( fetch::http::middleware::ColorLog);
    http_server_.AddModule(*this);
    
  }

  ~FetchSwarmService() 
  {

  }

private:
  fetch::network::ThreadManager *thread_manager_;    
  fetch::service::ServiceServer< fetch::network::TCPServer > service_;
  fetch::http::HTTPServer http_server_;  
  
//  fetch::protocols::SwarmProtocol *swarm_ = nullptr;
  fetch::protocols::NodeDetails details_;
};

#endif