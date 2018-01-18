#ifndef COMMANDS_HPP
#define COMMANDS_HPP

enum RemoteCommands {
  GET_INFO = 1,
  CONNECT = 2
};

enum PeerToPeerCommands {
  SEND_MESSAGE = 1,
  GET_MESSAGES = 2
};

enum FetchProtocols {
  REMOTE_CONTROL = 1,
  PEER_TO_PEER = 2
};

#endif
