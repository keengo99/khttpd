#ifndef KTCPSERVERSINK_H
#define KTCPSERVERSINK_H
#include "KSink.h"
class KTcpServerSink : public KSink
{
public:
	KTcpServerSink(kgl_pool_t* pool) : KSink(pool)
	{

	}
	virtual ~KTcpServerSink()
	{

	}
	virtual uint32_t get_server_model() override
	{
		return get_server()->flags;
	}
	virtual KOPAQUE get_server_opaque() override
	{
		return kserver_get_opaque(get_server());
	}
	kselector* get_selector() override
	{
		return get_connection()->st.base.selector;
	}
	kgl_pool_t* get_connection_pool() override
	{
		return get_connection()->pool;
	}
	kssl_session* get_ssl() override {
		kconnection* cn = get_connection();
		return selectable_get_ssl(&cn->st);
	}
	sockaddr_i* get_peer_addr() override
	{
		kconnection* cn = get_connection();
		return &cn->addr;
	}
	bool get_self_addr(sockaddr_i* addr) override
	{
		return 0 == kconnection_self_addr(get_connection(), addr);
	}
#ifdef ENABLE_PROXY_PROTOCOL
	kgl_proxy_protocol* get_proxy_info() override
	{
		return get_connection()->proxy;
	}
#endif
#ifdef KSOCKET_SSL
	void* get_sni() override
	{
		auto cn = get_connection();
		void* sni = cn->sni;
		cn->sni = NULL;
		return sni;
	}
#endif
	bool send_alt_svc_header()
	{
#ifdef KSOCKET_SSL
		if (KBIT_TEST(data.flags, RQ_CONNECTION_UPGRADE)) {
			return false;
		}
		kconnection* cn = get_connection();
		kssl_session* ssl = selectable_get_ssl(&cn->st);
		if (ssl == nullptr) {
			return false;
		}
		if (ssl->alt_svc_sent) {
			return false;
		}
		kserver* server = cn->server;
		if (!KBIT_TEST(server->flags, WORK_MODEL_ALT_H3)) {
			return false;
		}
		char buf[128];
		int len = snprintf(buf, sizeof(buf), "h3=\":%d\"", ksocket_addr_port(&server->addr));
		if (len > 0) {
			ssl->alt_svc_sent = 1;
			return response_altsvc_header(buf, len);
		}
#endif
		return false;

	}
protected:
	virtual bool response_altsvc_header(const char* val, int val_len)
	{
		return false;
	}
	virtual kconnection* get_connection() = 0;
	virtual kserver* get_server()
	{
		return get_connection()->server;
	}
};
class KSingleConnectionSink : public KTcpServerSink
{
public:
	KSingleConnectionSink(kconnection* cn, kgl_pool_t* pool) : KTcpServerSink(pool) {
		this->cn = cn;
	}
	virtual ~KSingleConnectionSink() {
		if (cn) {
			kconnection_destroy(cn);
		}
	}
	virtual bool support_sendfile() override {
		return selectable_support_sendfile(&cn->st);
	}
	virtual int sendfile(kfiber_file* fp, int len) override {
		int got = on_success_response(kfiber_sendfile(cn, fp, len));
		if (got > 0) {
			add_down_flow(got);
		}
		return got;
	}
	int64_t get_response_left() override {
		return response_left;
	}
	virtual int write_all(const char* buf, int length) override {
		while (length > 0) {
			int got = kfiber_net_write(cn, buf, length);
			if (got <= 0) {
				return length;
			}
			add_down_flow(got);
			length -= got;
		}
		return 0;
	}
	virtual int write_all(const kbuf* buf, int length) override {
		return write_buf(buf, length, nullptr);
	}
	int write_buf(const kbuf* buf, int length, const kgl_iovec *suffix=nullptr) {
#define KGL_RQ_WRITE_BUF_COUNT 64
		kgl_iovec iovec_buf[KGL_RQ_WRITE_BUF_COUNT];
		while (length > 0) {
			/* prepare iovec_buf */
			int bc = 0;
			for (; bc < KGL_RQ_WRITE_BUF_COUNT-1 && length>0; ++bc) {
				iovec_buf[bc].iov_len = KGL_MIN(length, buf->used);
				iovec_buf[bc].iov_base = buf->data;
				length -= iovec_buf[bc].iov_len;
				buf = buf->next;
			}
			if (length == 0 && suffix) {
				iovec_buf[bc++] = *suffix;
			}
			kgl_iovec* hot_buf = iovec_buf;
			while (bc > 0) {
				/* write iovec_buf */
				int got = kfiber_net_writev(cn, hot_buf, bc);
				if (got <= 0) {
					return length;
				}
				add_down_flow(got);
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
	kconnection* cn;
protected:
	int on_success_response(int len) {
		if (len > 0) {
			data.send_size += len;
			if (response_left >= 0) {
				assert(response_left >= len);
				response_left -= len;
				if (response_left < 0) {
					response_left = 0;
				}
			}
		}
		return len;
	}
	int64_t response_left = -1;
};
#endif
