
#include "luat_base.h"
#include "luat_ota.h"
#include "luat_fs.h"
#include "luat_timer.h"
#include "luat_malloc.h"
#include "luat_flash.h"

#define LUAT_LOG_TAG "ota"
#include "luat_log.h"

int luat_bin_unpack(const char* path, int writeOut);

int luat_ota_need_update(void) {
  return (luat_fs_fexist(UPDATE_BIN_PATH)) ? 1 : 0;
}

int luat_ota_need_rollback(void) {
  return (luat_fs_fexist(ROLLBACK_MARK_PATH)) ? 1 : 0;
}

int luat_ota_mark_rollback(void) {
  // 既然是异常退出,那肯定出错了!!!
  // 如果升级过, 那么就写入标志文件
  {
    if (luat_fs_fexist(UPDATE_MARK)) {
      FILE* fd = luat_fs_fopen("/rollback_mark", "wb");
      if (fd) {
        luat_fs_fclose(fd);
      }
    }
    else {
      // 没升级过, 那就是线刷, 不存在回滚
    }
  }
  return 0;
}

static void luat_bin_exec_update(void);
static void luat_bin_exec_rollback(void);

LUAT_WEAK void luat_ota_reboot(int timeout_ms) {
  if (timeout_ms > 0)
    luat_timer_mdelay(timeout_ms);
  luat_os_reboot(1);
}

void luat_ota_exec_update(void) {
#if LUAT_OTA_MODE == 1 
  luat_bin_exec_update();
#elif LUAT_OTA_MODE == 2
  luat_db_exec_update();
#endif
}

void luat_ota_exec_rollback(void) {
#if LUAT_OTA_MODE == 1 
  luat_bin_exec_rollback();
#elif LUAT_OTA_MODE == 2
  luat_db_exec_rollback();
#endif
}


int luat_ota_update_or_rollback(void) {
  if (luat_ota_need_update()) {
    luat_ota_exec_update();
    LLOGW("update: reboot at 5 secs");
    luat_ota_reboot(5000);
    return 1;
  }
  if (luat_ota_need_rollback()) {
    luat_ota_exec_rollback();
    LLOGW("rollback: reboot at 5 secs");
    luat_ota_reboot(5000);
    return 1;
  }
  return 0;
}


static void luat_bin_exec_update(void) {
  // 找到了, 检查一下大小
  LLOGI("found " UPDATE_BIN_PATH " ...");
  size_t fsize = luat_fs_fsize(UPDATE_BIN_PATH);
  if (fsize < 256) {
    // 太小了, 肯定不合法, 直接移除, 正常启动
    LLOGW(UPDATE_BIN_PATH " is too small, delete it");
    luat_fs_remove(UPDATE_BIN_PATH);
    return;
  }
  // 写入标志文件.
  // 必须提前写入, 即使解包失败, 仍标记为升级过,这样报错就能回滚
  LLOGI("write " UPDATE_MARK  " first");
  FILE* fd = luat_fs_fopen(UPDATE_MARK, "wb");
  if (fd) {
    luat_fs_fclose(fd);
    // TODO 连标志文件都写入失败,怎么办?
  }
  // 检测升级包合法性
  if (luat_bin_unpack(UPDATE_BIN_PATH, 0) != LUA_OK) {
    LLOGE("%s is invaild!!", UPDATE_BIN_PATH);
  }
  else {
    // 开始解包升级文件
    if (luat_bin_unpack(UPDATE_BIN_PATH, 1) == LUA_OK) {
      LLOGI("update OK, remove " UPDATE_BIN_PATH);
    }
    else {
      LLOGW("update FAIL, remove " UPDATE_BIN_PATH);
    }
  }
  // 无论是否成功,都一定要删除升级文件, 防止升级死循环
  luat_fs_remove(UPDATE_BIN_PATH);
}

static void luat_bin_exec_rollback(void) {
  // 回滚文件存在,
  LLOGW("Found " ROLLBACK_MARK_PATH  ", check rollback");
  // 首先,移除回滚标志, 防止重复N次的回滚
  luat_fs_remove("/rollback_mark"); // TODO 如果删除也失败呢?
  // 然后检查原始文件, flashx.bin
  if (!luat_fs_fexist(FLASHX_PATH)) {
    LLOGW("NOT " FLASHX_PATH " , can't rollback");
    return;
  }
  // 存在原始flashx.bin
  LLOGD("found " FLASHX_PATH  ", unpack it");
  // 开始回滚操作
  if (luat_bin_unpack(FLASHX_PATH, 1) == LUA_OK) {
    LLOGI("rollback complete!");
  }
  else {
    LLOGE("rollback FAIL");
  }
  // 执行完成, 准备重启
  LLOGW("rollback: reboot at 5 secs");
  // 延迟5秒后,重启
  luat_timer_mdelay(5*1000);
  luat_os_reboot(0); // 重启
}

#include "luat_crypto.h"
#include "luat_md5.h"

#ifdef LUAT_USE_CRYPTO
#include "mbedtls/md5.h"
#undef luat_md5_init
#undef luat_md5_update
#undef luat_md5_finalize

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
#define luat_md5_init mbedtls_md5_init
#define luat_md5_starts mbedtls_md5_starts
#define luat_md5_update mbedtls_md5_update
#define luat_md5_finalize mbedtls_md5_finish
#else
#define luat_md5_init mbedtls_md5_init
#define luat_md5_starts mbedtls_md5_starts_ret
#define luat_md5_update mbedtls_md5_update_ret
#define luat_md5_finalize mbedtls_md5_finish_ret
#endif
#endif

void luat_str_fromhex(char* str, size_t len, char* buff);
void luat_str_tohex(char* str, size_t len, char* buff);

#define OTA_CHECK_BUFF_SIZE (64) // 超过64字节的话, luat_md5报错, 待查
typedef struct ota_md5
{
    uint8_t buff[OTA_CHECK_BUFF_SIZE];
    #ifdef LUAT_USE_CRYPTO
    mbedtls_md5_context context;
    #else
    struct md5_context context;
    #endif
    struct md5_digest digest;
}ota_md5_t;


int luat_ota_checkfile(const char* path) {
    int ret = 0;
    
    FILE * fd = luat_fs_fopen(path, "rb");
    if (fd == NULL) {
        LLOGE("no such file");
        return -1;
    }
    size_t binsize = luat_fs_fsize(path);
    if (binsize < 512 || binsize > 1024*1024) {
        luat_fs_fclose(fd);
        LLOGE("%s is too small/big %d", path, binsize);
        return -1;
    }
    ota_md5_t* ota = luat_heap_malloc(sizeof(ota_md5_t));
    if (ota == NULL) {
        luat_fs_fclose(fd);
        LLOGE("out of memory when check ota file md5");
        return -1;
    }
    
    unsigned int len = 0;
    int remain = binsize - 16;

    luat_md5_init(&ota->context);
#ifdef LUAT_USE_CRYPTO
    luat_md5_starts(&ota->context);
#endif
    while (remain > 0) {
        if (remain > OTA_CHECK_BUFF_SIZE) {
            len = luat_fs_fread(ota->buff, OTA_CHECK_BUFF_SIZE, 1, fd);
        }
        else {
            len = luat_fs_fread(ota->buff, remain, 1, fd);
        }
        //LLOGD("ota read %d byte", len);
        if (len == 0) { // 不可能的事
            break;
        }
        if (len < 0 || len > 512) {
            luat_heap_free(ota);
            luat_fs_fclose(fd);
            LLOGE("read file fail");
            return -1;
        }

        remain -= len;
        luat_md5_update(&ota->context, ota->buff, len);
    }
    luat_md5_finalize(&ota->context, &ota->digest);
    #ifdef LUAT_USE_CRYPTO
    mbedtls_md5_free(&ota->context);
    #endif
    // 应该还有16字节的md5
    //memset(ota->buff, 0, OTA_CHECK_BUFF_SIZE);
    luat_fs_fread(ota->buff, 16, 1, fd);
    // 读完就可以关了
    luat_fs_fclose(fd);

    // 判断一下md5
    // uint8_t *expect_md5 = ota->buff + 64;
    // uint8_t *face_md5   = ota->buff + 128;

    if (!memcmp(ota->buff, ota->digest.bytes, 16)) {
        LLOGD("ota file MD5 ok");
        ret = 0;
    }
    else {
        LLOGE("ota file MD5 FAIL");
        ret = -1;
    }

    luat_heap_free(ota);
    return ret;
}


#ifdef LUAT_USE_OTA

int luat_ota(uint32_t luadb_addr){
#ifdef LUAT_USE_ZLIB 
    FILE *fd_out = NULL;
    FILE *fd_in = NULL;
    extern int zlib_decompress(FILE *source, FILE *dest);
    //检测是否有压缩升级文件
    if(luat_fs_fexist(UPDATE_TGZ_PATH)){
        LLOGI("found update.tgz, decompress ...");
        fd_in = luat_fs_fopen(UPDATE_TGZ_PATH, "r");
        if (fd_in == NULL){
            LLOGE("open the input file : %s error!", UPDATE_TGZ_PATH);
            goto _close_decompress;
        }
        luat_fs_remove(UPDATE_BIN_PATH);
        fd_out = luat_fs_fopen(UPDATE_BIN_PATH, "w+");
        if (fd_out == NULL){
            LLOGE("open the output file : %s error!", UPDATE_BIN_PATH);
            goto _close_decompress;
        }
        int ret = zlib_decompress(fd_in, fd_out);
        if (ret != 0){
            LLOGE("decompress file error!");
        }
_close_decompress:
        if(fd_in != NULL){
            luat_fs_fclose(fd_in);
        }
        if(fd_out != NULL){
            luat_fs_fclose(fd_out);
        }
        //不论成功与否都删掉避免每次启动都执行一遍
        luat_fs_remove(UPDATE_TGZ_PATH);
    }
#endif

    int ret  = -1;
    //检测是否有升级文件
    if(luat_fs_fexist(UPDATE_BIN_PATH)){
        LLOGI("found update.bin, checking");
        if (luat_ota_checkfile(UPDATE_BIN_PATH) == 0) {
            LLOGI("update.bin ok, updating... %08X", luadb_addr);
            #define UPDATE_BUFF_SIZE 4096
            uint8_t* buff = luat_heap_malloc(UPDATE_BUFF_SIZE);
            int len = 0;
            int offset = 0;
            if (buff != NULL) {
              FILE* fd = luat_fs_fopen(UPDATE_BIN_PATH, "rb");
              if (fd){
                while (1) {
                  memset(buff, 0, UPDATE_BUFF_SIZE);
                  len = luat_fs_fread(buff, sizeof(uint8_t), UPDATE_BUFF_SIZE, fd);
                  if (len < 1)
                      break;
                  luat_flash_erase(luadb_addr + offset, UPDATE_BUFF_SIZE);
                  luat_flash_write((char*)buff, luadb_addr + offset, UPDATE_BUFF_SIZE);
                  offset += len;
                }
              }else{
                ret = -1;
                LLOGW("update.bin open error");
              }
              ret = 0;
            }
        }
        else {
            ret = -1;
            LLOGW("update.bin NOT ok, skip");
        }
        luat_fs_remove(UPDATE_BIN_PATH);
    }
    return ret;
}
#endif
