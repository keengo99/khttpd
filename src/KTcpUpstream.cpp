#include "KTcpUpstream.h"
#include "KPoolableSocketContainer.h"
#include "khttp.h"
void KTcpUpstream::unbind_selector()
{
#ifndef _WIN32
	if (cn->st.base.selector) {
		selectable_remove(&cn->st);
		cn->st.base.selector = NULL;
	}
	kassert(KBIT_TEST(cn->st.base.st_flags, STF_READ | STF_WRITE ) == 0);
#endif
}
bool KTcpUpstream::set_header_callback(void* arg, kgl_header_callback cb)
{
	stack.arg = arg;
	stack.header = cb;
	return true;
}
KGL_RESULT KTcpUpstream::read_header()
{
	read_header_time = kgl_current_sec;
	return KGL_ENOT_SUPPORT;
}
void KTcpUpstream::gc(int life_time)
{
	clean();
	if (container == NULL) {
		Destroy();
		return;
	}
#ifndef NDEBUG
	if (cn->st.base.selector) {
		kassert(cn->st.base.queue.next == NULL);
	}
#endif
	container->gcSocket(this, life_time);
}
int KTcpUpstream::read(char* buf, int len)
{
	return kfiber_net_read(cn, buf, len);
}
int KTcpUpstream::write_all(const char* buf, int length) {
	while (length > 0) {
		int got = kfiber_net_write(cn, buf, length);
		if (got <= 0) {
			return length;
		}
		length -= got;
	}
	return 0;
}
int KTcpUpstream::write_all(const kbuf* buf, int length)
{
#define KGL_RQ_WRITE_BUF_COUNT 64
	kgl_iovec iovec_buf[KGL_RQ_WRITE_BUF_COUNT];
	while (length > 0) {
		/* prepare iovec_buf */
		int bc = 0;
		for (; bc < KGL_RQ_WRITE_BUF_COUNT && length>0; ++bc) {
			iovec_buf[bc].iov_len = KGL_MIN(length, buf->used);
			iovec_buf[bc].iov_base = buf->data;
			length -= iovec_buf[bc].iov_len;
			buf = buf->next;
		}
		/*
		if (length == 0 && suffix) {
			iovec_buf[bc++] = *suffix;
		}
		*/
		kgl_iovec* hot_buf = iovec_buf;
		while (bc > 0) {
			/* write iovec_buf */
			int got = kfiber_net_writev(cn, hot_buf, bc);
			if (got <= 0) {
				return length;
			}
			//add_down_flow(got);
			length -= got;
			/* see iovec_buf left data */
			while (got > 0) {
				if ((int)hot_buf->iov_len > got) {
					hot_buf->iov_len -= got;
					hot_buf->iov_base = (char*)(hot_buf->iov_base) + got;
					break;
				}
				got -= hot_buf->iov_len;
				hot_buf++;
				bc--;
			}
		}
	}
	return 0;
}

void KTcpUpstream::bind_selector(kselector *selector)
{
	kassert(cn->st.base.selector == NULL || cn->st.base.selector == selector);
	if (cn->st.base.selector == NULL) {
		selectable_bind(&cn->st, selector);
	}
}
bool KTcpUpstream::send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len)
{
	return false;
}
bool KTcpUpstream::send_trailer(const char* name, hlen_t name_len, const char* val, hlen_t val_len) {
	return false;
}
bool KTcpUpstream::send_method_path(uint16_t meth, const char* path, hlen_t path_len)
{
	return false;
}
bool KTcpUpstream::send_host(const char* host, hlen_t host_len)
{
	return false;
}
KGL_RESULT KTcpUpstream::send_header_complete()
{
	return KGL_ENOT_SUPPORT;
}
