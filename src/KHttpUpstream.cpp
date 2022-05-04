#include "KHttpUpstream.h"
#include "KHttpKeyValue.h"
#include "KHttpParser.h"

bool KHttpUpstream::send_connection(const char* val, hlen_t val_len)
{
	return send_header(kgl_expand_string("Connection"), val, val_len);
}
bool KHttpUpstream::send_method_path(uint16_t meth, const char* path, hlen_t path_len)
{
	assert(this->send_header_buffer == NULL);
	if (this->send_header_buffer == NULL) {
		send_header_buffer = krw_buffer_new(4096);
	}
	const char* meth_str = KHttpKeyValue::getMethod(meth);
	krw_write_str(send_header_buffer, meth_str, strlen(meth_str));
	krw_write_str(send_header_buffer, kgl_expand_string(" "));
	krw_write_str(send_header_buffer, path, path_len);
	krw_write_str(send_header_buffer, kgl_expand_string(" HTTP/1.1\r\n"));
	return true;
}
bool KHttpUpstream::send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len)
{
	if (this->send_header_buffer == NULL) {
		return false;
	}
	krw_write_str(send_header_buffer, attr, attr_len);
	krw_write_str(send_header_buffer, kgl_expand_string(": "));
	krw_write_str(send_header_buffer, val, val_len);
	krw_write_str(send_header_buffer, kgl_expand_string("\r\n"));
	return true;
}
bool KHttpUpstream::send_host(const char* host, hlen_t host_len)
{
	return send_header(kgl_expand_string("Host"), host, host_len);
}
void KHttpUpstream::set_content_length(int64_t content_length)
{
	if (content_length == -1) {
		//unknow
		send_header(kgl_expand_string("Transfer-Encoding"), kgl_expand_string("chunked"));
	}
	return;
}
KGL_RESULT KHttpUpstream::send_header_complete()
{
	if (this->send_header_buffer == NULL) {
		return KGL_ENOT_READY;
	}
	krw_write_str(send_header_buffer, kgl_expand_string("\r\n"));
	KGL_RESULT result = KGL_OK;
	WSABUF buf[32];
	for (;;) {
		int bc = krw_get_read_buffers(send_header_buffer, buf, kgl_countof(buf));
		if (bc == 0) {
			break;
		}
		//fwrite(buf[0].iov_base, 1, buf[0].iov_len, stdout);
		int got = kfiber_net_writev(cn, buf, bc);
		if (got <= 0) {
			result = KGL_ESOCKET_BROKEN;
			break;
		}
		if (!krw_read_success(send_header_buffer, got)) {
			break;
		}
	}
	krw_buffer_destroy(send_header_buffer);
	send_header_buffer = NULL;
	return result;
}
KGL_RESULT KHttpUpstream::read_header()
{
	assert(read_buffer == NULL);
	if (read_buffer != NULL) {
		return KGL_EUNKNOW;
	}
	assert(stack.header);
	KGL_RESULT result = KGL_OK;
	khttp_parser parser;
	memset(&parser, 0, sizeof(parser));
	read_buffer = ks_buffer_new(8192);
	int64_t begin_time_msec = kgl_current_msec;
	for (;;) {
continue_read:
		int got = kfiber_net_read(cn, read_buffer->buf, read_buffer->buf_size);
		if (got <= 0) {
			return KGL_EDATA_FORMAT;
		}
		ks_write_success(read_buffer, got);
		khttp_parse_result rs;
		char* hot = read_buffer->buf;
		int len = read_buffer->used;
		for(;;) {
			memset(&rs, 0, sizeof(rs));
			kgl_parse_result parse_result = khttp_parse(&parser, &hot, &len, &rs);
			switch (parse_result) {
			case kgl_parse_continue:
			{
				if (kgl_current_msec - begin_time_msec > 60000) {
					result = KGL_EIO;
					goto out;
				}
				if (parser.header_len > MAX_HTTP_HEAD_SIZE) {
					result = KGL_EDATA_FORMAT;
					goto out;
				}
				ks_save_point(read_buffer, hot, len);
				goto continue_read;
			}
			case kgl_parse_success:
			{
				if (!stack.header(this, stack.arg, rs.attr, rs.attr_len, rs.val, rs.val_len, rs.is_first)) {
					goto out;
				}
				break;
			}
			case kgl_parse_finished:
				ks_save_point(read_buffer, hot, len);
				goto out;
			default:
				result = KGL_EDATA_FORMAT;
				goto out;
			}
		}

	}
out:
	return result;
}
int KHttpUpstream::read(WSABUF * buf, int bc)
{
	if (read_buffer) {
		if (read_buffer->used > 0) {
			int len = MIN(buf[0].iov_len, read_buffer->used);
			kgl_memcpy(buf[0].iov_base, read_buffer->buf, len);
			ks_save_point(read_buffer, read_buffer->buf + len, read_buffer->used - len);
			return len;
		}
		ks_buffer_destroy(read_buffer);
		read_buffer = NULL;
	}
	return KTcpUpstream::read(buf, bc);
}
void KHttpUpstream::clean()
{
	KTcpUpstream::clean();
	if (send_header_buffer) {
		krw_buffer_destroy(send_header_buffer);
		send_header_buffer = NULL;
	}
	if (read_buffer) {
		ks_buffer_destroy(read_buffer);
		read_buffer = NULL;
	}
}