#define Len_Error_Code 8
#define Len_Error_Msg 100
#define default_txt "Error_msg_EN.txt"
#define ERR_OUT_LEN 1024
#define ERR_IN_LEN 100

//mqtt sent
#define ADDRESS     "tcp://192.168.0.107:1883"
#define CLIENTID    "Error_Handler"
#define TOPIC_PUB   "Error/topic"
#define QOS         1
#define TIME        200L

//mqtt receive
#define TOPIC_SUB   "FD/ERROR"

//DEFAULT
#define sevCode_DEFAULT "4"
#define App_DEFAULT "appXXXX"
#define ErroCode_DEFAULT "ABCXXXX"
#define ErroMsg_DEFAULT "ABC"