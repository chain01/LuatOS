
/*
@module  iotauth
@summary IoT鉴权库, 用于生成各种云平台的参数
@version core V0007
@date    2022.08.06
@demo iotauth
*/
#include "luat_base.h"
#include "luat_crypto.h"
#include "luat_malloc.h"
#include "time.h"
#include "luat_str.h"
#include "luat_mcu.h"

#define LUAT_LOG_TAG "iotauth"
#include "luat_log.h"

#define CLIENT_ID_LEN 64
#define USER_NAME_LEN 128
#define PASSWORD_LEN 256

static char client_id[CLIENT_ID_LEN]={0};
static char user_name[USER_NAME_LEN]={0};
static char password[PASSWORD_LEN]={0};

static const unsigned char hexchars[] = "0123456789abcdef";
static void str_tohex(const char* str, size_t str_len, char* hex) {
    for (size_t i = 0; i < str_len; i++)
    {
        char ch = *(str+i);
        hex[i*2] = hexchars[(unsigned char)ch >> 4];
        hex[i*2+1] = hexchars[(unsigned char)ch & 0xF];
    }
}

static void aliyun_token(const char* product_key,const char* device_name,const char* device_secret,long long cur_timestamp,const char* method,char* client_id, char* user_name, char* password){
    char deviceId[64] = {0};
    char macSrc[200] = {0};
    char macRes[32] = {0};
    char timestamp_value[20] = {0};
    char mqtt_clinetid_kv[96] = {0};
    sprintf_(timestamp_value,"%lld",cur_timestamp);
    if (!strcmp("hmacmd5", method)||!strcmp("HMACMD5", method)) {
        sprintf_(mqtt_clinetid_kv,"|timestamp=%s,_v=paho-c-1.0.0,securemode=3,signmethod=%s,lan=C|",timestamp_value,"hmacmd5");
    }else if (!strcmp("hmacsha1", method)||!strcmp("HMACSHA1", method)) {
        sprintf_(mqtt_clinetid_kv,"|timestamp=%s,_v=paho-c-1.0.0,securemode=3,signmethod=%s,lan=C|",timestamp_value,"hmacsha1");
    }else if (!strcmp("hmacsha256", method)||!strcmp("HMACSHA256", method)) {
        sprintf_(mqtt_clinetid_kv,"|timestamp=%s,_v=paho-c-1.0.0,securemode=3,signmethod=%s,lan=C|",timestamp_value,"hmacsha256");
    }else{
        LLOGE("not support: %s",method);
        return;
    }
    /* setup deviceId */
    memcpy(deviceId, device_name, strlen(device_name));
    memcpy(deviceId + strlen(deviceId), "&", strlen("&"));
    memcpy(deviceId + strlen(deviceId), product_key, strlen(product_key));
    /* setup clientid */
    memcpy(client_id, deviceId, strlen(deviceId));
    memcpy(client_id + strlen(deviceId), mqtt_clinetid_kv, strlen(mqtt_clinetid_kv));
    memset(client_id + strlen(deviceId) + strlen(mqtt_clinetid_kv), 0, 1);
    /* setup username */
    memcpy(user_name, deviceId, strlen(deviceId));
    memset(user_name + strlen(deviceId), 0, 1);
    /* setup password */
    memcpy(macSrc, "clientId", strlen("clientId"));
    memcpy(macSrc + strlen(macSrc), deviceId, strlen(deviceId));
    memcpy(macSrc + strlen(macSrc), "deviceName", strlen("deviceName"));
    memcpy(macSrc + strlen(macSrc), device_name, strlen(device_name));
    memcpy(macSrc + strlen(macSrc), "productKey", strlen("productKey"));
    memcpy(macSrc + strlen(macSrc), product_key, strlen(product_key));
    memcpy(macSrc + strlen(macSrc), "timestamp", strlen("timestamp"));
    memcpy(macSrc + strlen(macSrc), timestamp_value, strlen(timestamp_value));
    if (!strcmp("hmacmd5", method)||!strcmp("HMACMD5", method)) {
        luat_crypto_hmac_md5_simple(macSrc, strlen(macSrc),device_secret, strlen(device_secret),  macRes);
    }else if (!strcmp("hmacsha1", method)||!strcmp("HMACSHA1", method)) {
        luat_crypto_hmac_sha1_simple(macSrc, strlen(macSrc),device_secret, strlen(device_secret),  macRes);
    }else if (!strcmp("hmacsha256", method)||!strcmp("HMACSHA256", method)) {
        luat_crypto_hmac_sha256_simple(macSrc, strlen(macSrc),device_secret, strlen(device_secret),  macRes);
    }else{
        LLOGE("not support: %s",method);
        return;
    }
    luat_str_tohex(macRes, sizeof(macRes), password);
}

/*
阿里云物联网平台三元组生成
@api iotauth.aliyun(product_key, device_name,device_secret,method,cur_timestamp)
@string product_key 
@string device_name 
@string device_secret 
@string method 加密方式,"hmacmd5" "hmacsha1" "hmacsha256" 可选,默认"hmacsha256"
@number cur_timestamp 可选
@return string mqtt三元组 client_id
@return string mqtt三元组 user_name
@return string mqtt三元组 password
@usage
local client_id,user_name,password = iotauth.aliyun("123456789","abcdefg","Y877Bgo8X5owd3lcB5wWDjryNPoB")
print(client_id,user_name,password)
*/
static int l_iotauth_aliyun(lua_State *L) {
    memset(client_id, 0, CLIENT_ID_LEN);
    memset(user_name, 0, USER_NAME_LEN);
    memset(password, 0, PASSWORD_LEN);
    size_t len;
    const char* product_key = luaL_checklstring(L, 1, &len);
    const char* device_name = luaL_checklstring(L, 2, &len);
    const char* device_secret = luaL_checklstring(L, 3, &len);
    const char* method = luaL_optlstring(L, 4, "hmacsha256", &len);
    long long cur_timestamp = luaL_optinteger(L, 5,time(NULL) + 3600);
    aliyun_token(product_key,device_name,device_secret,cur_timestamp,method,client_id,user_name,password);
    lua_pushlstring(L, client_id, strlen(client_id));
    lua_pushlstring(L, user_name, strlen(user_name));
    lua_pushlstring(L, password, strlen(password));
    return 3;
}

typedef struct {
    char et[32];
    char version[12];
    char method[12];
    char res[64];
    char sign[64];
} sign_msg;

typedef  struct {
    char* old_str;
    char* str;
}URL_PARAMETES;

static int url_encoding_for_token(sign_msg* msg,char *token){
    int i,j,k,slen;
    sign_msg* temp_msg = msg;
    URL_PARAMETES url_patametes[] = {
        {"+","%2B"},
        {" ","%20"},
        {"/","%2F"},
        {"?","%3F"},
        {"%","%25"},
        {"#","%23"},
        {"&","%26"},
        {"=","%3D"},
    };
    char temp[64]     = {0};
    slen = strlen(temp_msg->res);
    for (i = 0,j = 0; i < slen; i++) {
        for(k = 0; k < 8; k++){
            if(temp_msg->res[i] == url_patametes[k].old_str[0]) {
                memcpy(&temp[j],url_patametes[k].str,strlen(url_patametes[k].str));
                j+=3;
                break;
            }
        }
        if (k == 8) {
            temp[j++] = temp_msg->res[i];
        }
	}
    memcpy(temp_msg->res,temp,strlen(temp));
    temp_msg->res[strlen(temp)] = 0;
    memset(temp,0x00,sizeof(temp));
    slen = strlen(temp_msg->sign);
    for (i = 0,j = 0; i < slen; i++) {
        for(k = 0; k < 8; k++){
            if(temp_msg->sign[i] == url_patametes[k].old_str[0]) {
                memcpy(&temp[j],url_patametes[k].str,strlen(url_patametes[k].str));
                j+=3;
                break;
            }
        }
        if(k == 8){
            temp[j++] = temp_msg->sign[i];
        }
	}
    memcpy(temp_msg->sign,temp,strlen(temp));
    temp_msg->sign[strlen(temp)] = 0;
    if(snprintf_(token,PASSWORD_LEN, "version=%s&res=%s&et=%s&method=%s&sign=%s", temp_msg->version, temp_msg->res, temp_msg->et, temp_msg->method, temp_msg->sign)<0){
        return -1;
    }
    return strlen(token);
}

static void onenet_token(const char* product_id,const char* device_name,const char* device_secret,long long cur_timestamp,char * method,char * version,char *token){
    size_t  declen = 0, enclen =  0;
    char plaintext[64]     = { 0 };
    char hmac[64]          = { 0 };
    char StringForSignature[256] = { 0 };
    sign_msg sign = {0};
    memcpy(sign.method, method, strlen(method));
    memcpy(sign.version, version, strlen(version));
    sprintf_(sign.et,"%lld",cur_timestamp);
    sprintf_(sign.res,"products/%s/devices/%s",product_id,device_name);
    luat_str_base64_decode((unsigned char *)plaintext, sizeof(plaintext), &declen, (const unsigned char * )device_secret, strlen((char*)device_secret));
    sprintf_(StringForSignature, "%s\n%s\n%s\n%s", sign.et, sign.method, sign.res, sign.version);
    if (!strcmp("md5", method)||!strcmp("MD5", method)) {
        luat_crypto_hmac_md5_simple(StringForSignature, strlen(StringForSignature), plaintext, declen, hmac);
    }else if (!strcmp("sha1", method)||!strcmp("SHA1", method)) {
        luat_crypto_hmac_sha1_simple(StringForSignature, strlen(StringForSignature),plaintext, declen,  hmac);
    }else if (!strcmp("sha256", method)||!strcmp("SHA256", method)) {
        luat_crypto_hmac_sha256_simple(StringForSignature, strlen(StringForSignature),plaintext, declen,  hmac);
    }else{
        LLOGE("not support: %s",method);
        return;
    }
    luat_str_base64_encode((unsigned char *)sign.sign, sizeof(sign.sign), &enclen, (const unsigned char * )hmac, strlen(hmac));
    url_encoding_for_token(&sign,token);
}

/*
中国移动物联网平台三元组生成
@api iotauth.onenet(produt_id, device_name,key,method,cur_timestamp,version)
@string produt_id 
@string device_name 
@string key 
@string method 加密方式,"md5" "sha1" "sha256" 可选,默认"md5"
@number cur_timestamp 可选
@string version 可选 默认"2018-10-31"
@return string mqtt三元组 client_id
@return string mqtt三元组 user_name
@return string mqtt三元组 password
@usage
local client_id,user_name,password = iotauth.onenet("123456789","test","KuF3NT/jUBJ62LNBB/A8XZA9CqS3Cu79B/ABmfA1UCw=","md5",1658920369,"2018-10-31")
print(client_id,user_name,password)
*/
static int l_iotauth_onenet(lua_State *L) {
    memset(password, 0, PASSWORD_LEN);
    size_t len;
    const char* produt_id = luaL_checklstring(L, 1, &len);
    const char* device_name = luaL_checklstring(L, 2, &len);
    const char* key = luaL_checklstring(L, 3, &len);
    const char* method = luaL_optlstring(L, 4, "md5", &len);
    long long cur_timestamp = luaL_optinteger(L, 5,time(NULL) + 3600);
    const char* version = luaL_optlstring(L, 6, "2018-10-31", &len);
    onenet_token(produt_id,device_name,key,cur_timestamp,method,version,password);
    lua_pushlstring(L, device_name, strlen(device_name));
    lua_pushlstring(L, produt_id, strlen(produt_id));
    lua_pushlstring(L, password, strlen(password));
    return 3;
}

static void iotda_token(const char* device_id,const char* device_secret,long long cur_timestamp,int ins_timestamp,char* client_id,const char* password){
    char hmac[64] = {0};
    char timestamp[12] = {0};
    struct tm *timeinfo = localtime( &cur_timestamp );
    if(snprintf_(timestamp, 12, "%04d%02d%02d%02d", (timeinfo->tm_year)+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour)<0){
        return;
    }
    snprintf_(client_id, CLIENT_ID_LEN, "%s_0_%d_%s", device_id,ins_timestamp,timestamp);
    luat_crypto_hmac_sha256_simple(device_secret, strlen(device_secret),timestamp, strlen(timestamp), hmac);
    str_tohex(hmac, strlen(hmac), password);
}

/*
华为物联网平台三元组生成
@api iotauth.iotda(device_id,device_secret,ins_timestamp,cur_timestamp)
@string device_id 
@string device_secret 
@number ins_timestamp 是否校验时间戳 1:校验 0:不校验
@number cur_timestamp 可选
@return string mqtt三元组 client_id
@return string mqtt三元组 user_name
@return string mqtt三元组 password
@usage
local client_id,user_name,password = iotauth.iotda("6203cc94c7fb24029b110408_88888888","123456789",1,1659495778)
print(client_id,user_name,password)
*/
static int l_iotauth_iotda(lua_State *L) {
    memset(client_id, 0, CLIENT_ID_LEN);
    memset(password, 0, PASSWORD_LEN);
    size_t len;
    const char* device_id = luaL_checklstring(L, 1, &len);
    const char* device_secret = luaL_checklstring(L, 2, &len);
    int ins_timestamp = luaL_optinteger(L, 3, 0);
    long long cur_timestamp = luaL_optinteger(L, 4,time(NULL));
    ins_timestamp = ins_timestamp==0?0:1;
    iotda_token(device_id,device_secret,cur_timestamp,ins_timestamp,client_id,password);
    lua_pushlstring(L, client_id, strlen(client_id));
    lua_pushlstring(L, device_id, strlen(device_id));
    lua_pushlstring(L, password, strlen(password));
    return 3;
}

/* Max size of base64 encoded PSK = 64, after decode: 64/4*3 = 48*/
#define DECODE_PSK_LENGTH 48
/* Max size of conn Id  */
#define MAX_CONN_ID_LEN (6)

static void get_next_conn_id(char *conn_id){
    int i;
    srand((unsigned)luat_mcu_ticks());
    for (i = 0; i < MAX_CONN_ID_LEN - 1; i++) {
        int flag = rand() % 3;
        switch (flag) {
            case 0:
                conn_id[i] = (rand() % 26) + 'a';
                break;
            case 1:
                conn_id[i] = (rand() % 26) + 'A';
                break;
            case 2:
                conn_id[i] = (rand() % 10) + '0';
                break;
        }
    }
    conn_id[MAX_CONN_ID_LEN - 1] = '\0';
}

static void qcloud_token(const char* product_id,const char* device_name,const char* device_secret,long long cur_timestamp,const char* method,const char* sdk_appid,char* username,char* password){
    char  conn_id[MAX_CONN_ID_LEN] = {};
    char username_sign[41] = {0};
    char   psk_base64decode[DECODE_PSK_LENGTH];
    size_t psk_base64decode_len = 0;
    luat_str_base64_decode((unsigned char *)psk_base64decode, DECODE_PSK_LENGTH, &psk_base64decode_len,(unsigned char *)device_secret, strlen(device_secret));
    get_next_conn_id(conn_id);
    sprintf_(username, "%s%s;%s;%s;%lld", product_id, device_name, sdk_appid,conn_id, cur_timestamp);
    if (!strcmp("sha1", method)||!strcmp("SHA1", method)) {
        luat_crypto_hmac_sha1_simple(username, strlen(username),psk_base64decode, psk_base64decode_len, username_sign);
    }else if (!strcmp("sha256", method)||!strcmp("SHA256", method)) {
        luat_crypto_hmac_sha256_simple(username, strlen(username),psk_base64decode, psk_base64decode_len, username_sign);
    }else{
        LLOGE("not support: %s",method);
        return;
    }
    char *username_sign_hex  = (char *)luat_heap_malloc(strlen(username_sign)*2+1);
    memset(username_sign_hex, 0, strlen(username_sign)*2+1);
    str_tohex(username_sign, strlen(username_sign), username_sign_hex);
    if (!strcmp("sha1", method)||!strcmp("SHA1", method)) {
        sprintf_(password, "%s;hmacsha1", username_sign_hex);
    }else if (!strcmp("sha256", method)||!strcmp("SHA256", method)) {
        sprintf_(password, "%s;hmacsha256", username_sign_hex);
    }
    luat_heap_free(username_sign_hex);
}

/*
腾讯联网平台三元组生成
@api iotauth.qcloud(product_id, device_name,device_secret,method,cur_timestamp,sdk_appid)
@string product_id 
@string device_name 
@string device_secret 
@string method 加密方式,"sha1" "sha256" 可选,默认"sha256"
@number cur_timestamp 可选
@string sdk_appid 可选 默认为"12010126"
@return string mqtt三元组 client_id
@return string mqtt三元组 user_name
@return string mqtt三元组 password
@usage
local client_id,user_name,password = iotauth.qcloud("LD8S5J1L07","test","acyv3QDJrRa0fW5UE58KnQ==", "sha1",1660103393)
print(client_id,user_name,password)
*/
static int l_iotauth_qcloud(lua_State *L) {
    memset(client_id, 0, CLIENT_ID_LEN);
    memset(user_name, 0, USER_NAME_LEN);
    memset(password, 0, PASSWORD_LEN);
    size_t len;
    const char* product_id = luaL_checklstring(L, 1, &len);
    const char* device_name = luaL_checklstring(L, 2, &len);
    const char* device_secret = luaL_checklstring(L, 3, &len);
    const char* method = luaL_optlstring(L, 4, "sha256", &len);
    long long cur_timestamp = luaL_optinteger(L, 5,time(NULL) + 3600);
    const char* sdk_appid = luaL_optlstring(L, 6, "12010126", &len);
    qcloud_token(product_id, device_name,device_secret,cur_timestamp,method,sdk_appid,user_name,password);
    snprintf_(client_id, CLIENT_ID_LEN,"%s%s", product_id,device_name);
    lua_pushlstring(L, client_id, strlen(client_id));
    lua_pushlstring(L, user_name, strlen(user_name));
    lua_pushlstring(L, password, strlen(password));
    return 3;
}

static void tuya_token(const char* device_id,const char* device_secret,long long cur_timestamp,const char* password){
    char hmac[64] = {0};
    char *token_temp  = (char *)luat_heap_malloc(100);
    memset(token_temp, 0, 100);
    snprintf_(token_temp, 100, "deviceId=%s,timestamp=%lld,secureMode=1,accessType=1", device_id, cur_timestamp);
    luat_crypto_hmac_sha256_simple(token_temp, strlen(token_temp),device_secret, strlen(device_secret), hmac);
    for (int i = 0; i < 32; i++) {
        sprintf_(password + 2*i, "%02x", hmac[i]);
    }
    luat_heap_free(token_temp);
}

/*
涂鸦联网平台三元组生成
@api iotauth.tuya(device_id,device_secret,cur_timestamp)
@string device_id
@string device_secret 
@number cur_timestamp 可选
@return string mqtt三元组 client_id
@return string mqtt三元组 user_name
@return string mqtt三元组 password
@usage
local client_id,user_name,password = iotauth.tuya("6c95875d0f5ba69607nzfl","fb803786602df760",1607635284)
print(client_id,user_name,password)
*/
static int l_iotauth_tuya(lua_State *L) {
    memset(client_id, 0, CLIENT_ID_LEN);
    memset(user_name, 0, USER_NAME_LEN);
    memset(password, 0, PASSWORD_LEN);
    size_t len;
    const char* device_id = luaL_checklstring(L, 1, &len);
    const char* device_secret = luaL_checklstring(L, 2, &len);
    long long cur_timestamp = luaL_optinteger(L, 3,time(NULL) + 3600);
    tuya_token(device_id,device_secret,cur_timestamp,password);
    snprintf_(client_id, CLIENT_ID_LEN, "tuyalink_%s", device_id);
    snprintf_(user_name, USER_NAME_LEN, "%s|signMethod=hmacSha256,timestamp=%lld,secureMode=1,accessType=1", device_id,cur_timestamp);
    lua_pushlstring(L, client_id, strlen(client_id));
    lua_pushlstring(L, user_name, strlen(user_name));
    lua_pushlstring(L, password, strlen(password));
    return 3;
}

static void baidu_token(const char* iot_core_id,const char* device_key,const char* device_secret,const char* method,long long cur_timestamp,char* username,char* password){
    char crypto[64] = {0};
    char *token_temp  = (char *)luat_heap_malloc(100);
    memset(token_temp, 0, 100);
    if (!strcmp("MD5", method)||!strcmp("md5", method)) {
        sprintf_(username, "thingidp@%s|%s|%lld|%s",iot_core_id,device_key,cur_timestamp,"MD5");
        snprintf_(token_temp, 100, "%s&%lld&%s%s",device_key,cur_timestamp,"MD5",device_secret);
        luat_crypto_md5_simple(token_temp, strlen(token_temp),crypto);
    }else if (!strcmp("SHA256", method)||!strcmp("sha256", method)) {
        sprintf_(username, "thingidp@%s|%s|%lld|%s",iot_core_id,device_key,cur_timestamp,"SHA256");
        snprintf_(token_temp, 100, "%s&%lld&%s%s",device_key,cur_timestamp,"SHA256",device_secret);
        luat_crypto_sha256_simple(token_temp, strlen(token_temp),crypto);
    }else{
        LLOGE("not support: %s",method);
        return;
    }
    str_tohex(crypto, strlen(crypto), password);
    luat_heap_free(token_temp);
}

/*
百度物联网平台三元组生成
@api iotauth.baidu(iot_core_id, device_key,device_secret,method,cur_timestamp)
@string iot_core_id 
@string device_key 
@string device_secret 
@string method 加密方式,"MD5" "SHA256" 可选,默认"MD5"
@number cur_timestamp 可选
@return string mqtt三元组 client_id
@return string mqtt三元组 user_name
@return string mqtt三元组 password
@usage
local client_id,user_name,password = iotauth.baidu("abcd123","mydevice","ImSeCrEt0I1M2jkl","MD5")
print(client_id,user_name,password)
*/
static int l_iotauth_baidu(lua_State *L) {
    memset(user_name, 0, USER_NAME_LEN);
    memset(password, 0, PASSWORD_LEN);
    size_t len;
    const char* iot_core_id = luaL_checklstring(L, 1, &len);
    const char* device_key = luaL_checklstring(L, 2, &len);
    const char* device_secret = luaL_checklstring(L, 3, &len);
    const char* method = luaL_optlstring(L, 4, "MD5", &len);
    long long cur_timestamp = luaL_optinteger(L, 5,time(NULL) + 3600);
    baidu_token(iot_core_id,device_key,device_secret,method,cur_timestamp,user_name,password);
    lua_pushlstring(L, iot_core_id, strlen(iot_core_id));
    lua_pushlstring(L, user_name, strlen(user_name));
    lua_pushlstring(L, password, strlen(password));
    return 3;
}

#include "rotable2.h"
static const rotable_Reg_t reg_iotauth[] =
{
    { "aliyun" ,          ROREG_FUNC(l_iotauth_aliyun)},
    { "onenet" ,          ROREG_FUNC(l_iotauth_onenet)},
    { "iotda" ,           ROREG_FUNC(l_iotauth_iotda)},
    { "qcloud" ,          ROREG_FUNC(l_iotauth_qcloud)},
    { "tuya" ,            ROREG_FUNC(l_iotauth_tuya)},
    { "baidu" ,           ROREG_FUNC(l_iotauth_baidu)},
	{ NULL,               ROREG_INT(0)}
};

LUAMOD_API int luaopen_iotauth( lua_State *L ) {
    luat_newlib2(L, reg_iotauth);
    return 1;
}
