#ifndef REMOTE_FUNCTIONALITY_HPP
#define REMOTE_FUNCTIONALITY_HPP
#include"node_functionality.hpp"

#include<vector>
#include<string>
#include<iostream>

class RemoteFunctionality {
public:
  RemoteFunctionality(std::string node_info): node_info_(node_info) {  }
    
  void Connect(std::string address, uint16_t port) {
    if(node_ != nullptr) {
      std::cout << "Remote asking to connect to " << address << " " << port << std::endl;
      node_->Connect(address, port);
    }
  }
  
  void Disconnect(uint64_t handle) { }

  std::string get_info()  {
    std::cout << "Sending info to client" << std::endl;
    return node_info_;
  }
  
  std::vector< std::string > const& peers() const {
    return peers_;
  }

  void set_node(NodeFunctionality *node) {
    node_ = node;
  }
private:
  NodeFunctionality *node_ = nullptr;
  std::string node_info_;
  std::vector< std::string > peers_;
};

#endif
