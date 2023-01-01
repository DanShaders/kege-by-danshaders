#pragma once

namespace routes {
/**
 * DB storage format:
 * status: [numeric status:char][filename:string]
 */
namespace job_file_import {
  int const DB_TYPE = 1;

  enum {
    ENQUEUED = 0,
    PROCESSING = 1,
    STALE = 2,
    UPLOADING = 3,
    DONE = 4,
  };

  inline char const* STATUS[] = {
      "enqueued", "processing", "stale", "uploading", "done",
  };

  inline int const STATUS_LEN = sizeof(STATUS) / sizeof(char const*);
}  // namespace job_file_import
}  // namespace routes