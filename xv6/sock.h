///done

///======================================================
/// TODO: Define an enumeration to represent socket state
///======================================================
enum sockstate { CLOSED, LISTENING, CONNECTED };
enum boolean { TRUE, FALSE };

///===============================================
/// TODO: Define a structure to represent a socket
///===============================================

struct sock {
  int local_port;
  int remote_port;
  enum sockstate state;        ///socket state
  int owner_pid;                     ///owner Process ID
  char buffer[SOCKET_BUFFER_SIZE+5];
  enum boolean data_available;  ///boolean: TRUE or FALSE


};
