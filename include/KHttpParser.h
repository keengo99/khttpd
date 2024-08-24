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

typedef struct {
	char* val;
	int val_len;
} khttp_field;

typedef struct {
	char* attr;
	char* val;
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

#define KHTTP_ISSPACE(c) (c==' ')
INLINE kgl_parse_result khttp_parse_header(khttp_parser* parser, char* header, char* end, khttp_parse_result* rs) {
	char* val;
	if (rs->is_first && !parser->first_same) {
		val = (char *)memchr(header, ' ', end-header);
		rs->request_line = 1;
	} else {
		val = (char*)memchr(header, ':', end - header);
	}
	if (val == NULL) {
		return kgl_parse_continue;
	}
	*val = '\0';
	val++;
	while (*val && KHTTP_ISSPACE(*val)) {
		val++;
	}
	char* header_end = header;
	while (*header_end && !KHTTP_ISSPACE(*header_end)) {
		header_end++;
	}
	rs->attr = header;
	rs->attr_len = (int)(header_end - header);
	*header_end = '\0';
	rs->val_len = (int)(end - val);
	rs->val = val;
	return kgl_parse_success;
}
INLINE kgl_parse_result khttp_parse(khttp_parser* parser, char** start, char* end, khttp_parse_result* rs) {
restart:
	kassert(end >= *start);
	if (*start == end) {
		return kgl_parse_continue;
	}
	char* pn = (char*)memchr(*start, '\n', end - *start);
	if (pn == NULL) {
		return kgl_parse_continue;
	}
	if (*start[0] == '\n' || *start[0] == '\r') {
		int checked = (int)(pn + 1 - *start);
		parser->header_len += checked;
		*start += checked;
		assert(*start <= end);
		if (!parser->started) {
			goto restart;
		}
		parser->finished = 1;
		return kgl_parse_finished;
	}
	if (parser->started) {
		/*
		 * ���ǻ�Ҫ�������http����û�л��У���rfc2616.
		 *        LWS            = [CRLF] 1*( SP | HT )
		 *        ���ǻ�Ҫ������һ�е�һ���ַ��Ƿ��ǿ��С�
		 */
		if (pn == end - 1) {
			/*
			 * ���\n�������ַ�,��Ҫcontinue.
			 */
			return kgl_parse_continue;
		}
		/*
		 * �����һ�п�ͷ�ַ���SP��HT����Ҫ���д�����\r��\n������SP
		 */
		while (pn[1] == ' ' || pn[1] == '\t') {
			*pn = ' ';
			int checked = (int)(pn + 1 - *start);
			char* pr = (char*)memchr(*start, '\r', checked);
			if (pr) {
				*pr = ' ';
			}
			pn = (char*)memchr(pn, '\n', end - pn);
			if (pn == NULL) {
				return kgl_parse_continue;
			}
		}
	}
	int checked = (int)(pn + 1 - *start);
	parser->header_len += checked;
	char* hot = *start;
	*start += checked;
	assert(*start <= end);
	if (checked > 3 && *(pn - 1) == '\r') {
		pn--;
	}
	*pn = '\0';
	rs->is_first = !parser->started;
	parser->started = 1;
	return khttp_parse_header(parser, hot, pn, rs);
}
#endif

