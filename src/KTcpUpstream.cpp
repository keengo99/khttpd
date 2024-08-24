#include "KTcpUpstream.h"
#include "KPoolableSocketContainer.h"
#include "khttp.h"
#include "KTcpServerSink.h"

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
		buf += got;
	}
	return 0;
}
int KTcpUpstream::write_all(const kbuf* buf, int length)
{
	return kangle::write_buf(cn, buf, length, nullptr);
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
