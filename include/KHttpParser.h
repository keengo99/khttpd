#ifndef KHTTPPARSER_H
#define KHTTPPARSER_H
#include "kfeature.h"
typedef enum {
	kgl_parse_error,//protocol error
	kgl_parse_success,//have header parsed in result
	kgl_parse_finished,//no header finished
	kgl_parse_want_read,
	kgl_parse_continue //no data parsed, need more data
} kgl_parse_result;

typedef enum {
	kgl_header_failed,
	kgl_header_success,
	kgl_header_no_insert,
	kgl_header_insert_begin
} kgl_header_result;

typedef struct {
	char *val;
	int val_len;
} khttp_field;

typedef struct {
	char *attr;
	char *val;
	int attr_len;
	int val_len;
	uint8_t is_first : 1;
	uint8_t request_line : 1;
} khttp_parse_result;

typedef struct {
	int header_len;
	uint8_t started : 1;
	uint8_t finished : 1;
	uint8_t first_same : 1;
} khttp_parser;

kgl_parse_result khttp_parse(khttp_parser *parser, char **buf, char *end, khttp_parse_result *rs);

#endif

