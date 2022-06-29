#pragma once

namespace routes {
/**
 * DB storage format:
 * status: [numeric status:char][filename:string]
 */
namespace job_file_import {
	const int DB_TYPE = 1;

	enum {
		ENQUEUED = 0,
		PROCESSING = 1,
		STALE = 2,
		UPLOADING = 3,
		DONE = 4,
	};

	inline const char *STATUS[] = {
		"enqueued",
		"processing",
		"stale",
		"uploading",
		"done",
	};

	inline const int STATUS_LEN = sizeof(STATUS) / sizeof(const char *);
}
}