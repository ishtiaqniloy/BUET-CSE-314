///=========================================================================
///PARAMETERS
///=========================================================================
#define INVALID_SOCKET_INDEX -99
#define INVALID_PORT -88
#define INVALID_PID -77

#define SOCKET_OPERATION_SUCCESSFUL 0


///==========================================================================
///ERRORS
///==========================================================================
#define E_NOTFOUND      -1025 ///Accessing a socket that is not in the stable
#define E_ACCESS_DENIED -1026 ///Accessing a socket from wrong process
#define E_WRONG_STATE   -1027 ///Attempts to send or receive, when the socket is not connected
#define E_FAIL          -1028 ///If no more socket can be opened (limit exceeded)
#define E_INVALID_ARG   -1029 ///Parameter issues
#define E_CONNECTION_ERROR -1030
