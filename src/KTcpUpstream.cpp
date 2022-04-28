#include "KTcpUpstream.h"
#include "KPoolableSocketContainer.h"
#include "khttp.h"
void KTcpUpstream::OnPushContainer()
{
#ifndef _WIN32
	if (cn->st.selector) {
		selectable_remove(&cn->st);
		cn->st.selector = NULL;
	}
	kassert(KBIT_TEST(cn->st.st_flags, STF_READ | STF_WRITE | STF_REV | STF_WEV) == 0);
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
	return KGL_EUNKNOW;
}
void KTcpUpstream::gc(int life_time,time_t base_time)
{
	if (container == NULL) {
		Destroy();
		return;
	}
#ifndef NDEBUG
	if (cn->st.selector) {
		kassert(cn->st.queue.next == NULL);
	}
#endif
	container->gcSocket(this, life_time, base_time);
}
int KTcpUpstream::Read(char* buf, int len)
{
	return kfiber_net_read(cn, buf, len);
}
int KTcpUpstream::Write(WSABUF* buf, int bc)
{
	return kfiber_net_writev(cn, buf, bc);
}

void KTcpUpstream::BindSelector(kselector *selector)
{
	kassert(cn->st.selector == NULL || cn->st.selector == selector);
	selectable_bind(&cn->st, selector);
}
bool KTcpUpstream::send_header(const char* attr, hlen_t attr_len, const char* val, hlen_t val_len)
{
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
bool KTcpUpstream::send_content_length(int64_t content_length)
{
	return false;
}
bool KTcpUpstream::send_header_complete(int64_t post_body_len)
{
	return false;
}
