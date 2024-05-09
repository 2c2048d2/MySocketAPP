//
// Created by 2c2048d2 on 24-4-29.
//

#ifndef MYSOCKETAPP_MYDATAPACK_H
#define MYSOCKETAPP_MYDATAPACK_H
#include <stdbool.h>

enum myDataPackTypeCommand {
  DATA_PACK_TYPE_COMMAND_QUIT,
  DATA_PACK_TYPE_COMMAND_GET_INFO,
  DATA_PACK_TYPE_COMMAND_MKDIR,
};

enum myDataPackTypeFile {
  DATA_PACK_TYPE_FILE_START,
  DATA_PACK_TYPE_FILE_SENDING,
  DATA_PACK_TYPE_FILE_END,
};

enum myDataPackTypeStatus {
  DATA_PACK_TYPE_STATUS_OK,
  DATA_PACK_TYPE_STATUS_FAILED,
};

enum myDataPackTypeInfo {
  DATA_PACK_TYPE_INFO_NORMAL,
};

enum myDataPackType {
  DATA_PACK_TYPE_INFO,
  DATA_PACK_TYPE_COMMAND,
  DATA_PACK_TYPE_FILE,
  DATA_PACK_TYPE_STATUS,
};

union myDataPackSubtype {
  enum myDataPackTypeCommand command_type;
  enum myDataPackTypeFile file_type;
  enum myDataPackTypeStatus status_type;
  enum myDataPackTypeInfo info_type;
};

struct myDataPack {
  enum myDataPackType type;
  union myDataPackSubtype subtype;
  unsigned long data_length;
  char payload[0];
};

struct myDataPack *gen_data_pack(enum myDataPackType type,
                                 union myDataPackSubtype info,
                                 unsigned long data_length, const char *payload,
                                 struct myDataPack *dataPack);

struct myDataPack *receive_data_pack(int sock_fd);

void send_data_pack(struct myDataPack *data_pack, int sock_fd,
                    bool free_data_pack);

#endif // MYSOCKETAPP_MYDATAPACK_H
