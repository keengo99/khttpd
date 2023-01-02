#include "KSockPoolHelper.h"
#include "kthread.h"
#include "kaddr.h"
#include "kselector_manager.h"
#include "KHttp2Upstream.h"
#include "KTsUpstream.h"
#include "KHttpUpstream.h"
#include "KHttpServer.h"
#include "klog.h"
#include <sstream>

using namespace std;

static int kgl_ssl_verify_callback(int ok, X509_STORE_CTX* x509_store) {
#ifndef NDEBUG
	char* subject, * issuer;
	int                err, depth;
	X509* cert;
	X509_NAME* sname, * iname;


	cert = X509_STORE_CTX_get_current_cert(x509_store);
	err = X509_STORE_CTX_get_error(x509_store);
	depth = X509_STORE_CTX_get_error_depth(x509_store);

	sname = X509_get_subject_name(cert);

	if (sname) {
		subject = X509_NAME_oneline(sname, NULL, 0);
		if (subject == NULL) {
			klog(KLOG_DEBUG, "X509_NAME_oneline() failed\n");
		}

	} else {
		subject = NULL;
	}

	iname = X509_get_issuer_name(cert);

	if (iname) {
		issuer = X509_NAME_oneline(iname, NULL, 0);
		if (issuer == NULL) {
			klog(KLOG_DEBUG, "X509_NAME_oneline() failed\n");
		}

	} else {
		issuer = NULL;
	}

	klog(KLOG_DEBUG, "verify:%d, error:%d %s, depth:%d, "
		"subject:\"%s\", issuer:\"%s\"\n",
		ok, err, X509_verify_cert_error_string(err), depth,
		subject ? subject : "(none)",
		issuer ? issuer : "(none)");
	if (subject) {
		OPENSSL_free(subject);
	}
	if (issuer) {
		OPENSSL_free(issuer);
	}
#endif
	return 1;
}
static int monitor_fiber(void* arg, int got) {
	KSockPoolHelper* sock_pool = (KSockPoolHelper*)arg;
	sock_pool->start_monitor_call_back();	
	sock_pool->release();
	return 0;
}
void KSockPoolHelper::start_monitor_call_back() {
	//10 seconds rand sleep
	kfiber_msleep(rand() % 10000);
	for (;;) {
#ifdef MALLOCDEBUG
		if (quit_program_flag > 0) {
			break;
		}
#endif
		if (!monitor) {
			//monitor is stoped
			break;
		}
		auto monitor_start_time = kgl_current_msec;
		auto us = get_upstream(KSOCKET_FLAGS_SKIP_POOL, NULL);
		if (!us) {
			disable();
			kfiber_msleep(get_error_try_time() * 1000);
			continue;
		}
		//caculate monitor tick time
		int monitor_tick = (int)(kgl_current_msec - monitor_start_time);
		if (monitor_tick <= 0) {
			monitor_tick = 1;
		}
		if (monitor_tick > 30000) {
			monitor_tick = 30000;
		}
		if (avg_monitor_tick == 0) {
			avg_monitor_tick = monitor_tick;
		} else {
			avg_monitor_tick = (int)(0.9 * float(avg_monitor_tick) + 0.1 * float(monitor_tick));
		}
		us->Destroy();
		enable();
	}
	monitor = 0;
}
KSockPoolHelper::KSockPoolHelper() {
	ip = NULL;
	error_count = 0;
	flags = 0;
	max_error_count = 5;
	hit = 0;
	weight = 1;
#ifdef ENABLE_UPSTREAM_SSL
#ifdef ENABLE_UPSTREAM_HTTP2
	alpn = 0;
#endif
	ssl_ctx = NULL;
#endif
}
KSockPoolHelper::~KSockPoolHelper() {
#ifdef ENABLE_UPSTREAM_SSL
	if (ssl_ctx) {
		SSL_CTX_free(ssl_ctx);
	}
#endif
	if (ip) {
		free(ip);
	}
}
void KSockPoolHelper::startMonitor() {
	if (monitor) {
		return;
	}
	monitor = 1;
	addRef();
	if (kfiber_create(monitor_fiber, this, 0, 0, NULL) != 0) {
		monitor = 0;
		release();
	}
}
KUpstream* KSockPoolHelper::get_upstream(uint32_t flags, const char* sni_host) {
	KUpstream* socket = NULL;
	if (!KBIT_TEST(flags, KSOCKET_FLAGS_SKIP_POOL)) {
		//如果是发生错误重连或upgrade的连接，则排除连接池
		socket = get_pool_socket();
		if (socket) {
			return socket;
		}
	}
	sockaddr_i* bind_addr = NULL;
	sockaddr_i bind_tmp_addr;
	const char* bind_ip = ip;
	if (bind_ip) {
		bind_addr = &bind_tmp_addr;
		if (!ksocket_getaddr(bind_ip, 0, AF_UNSPEC, AI_NUMERICHOST, bind_addr)) {
			return NULL;
		}
	}

	kgl_addr* addr = NULL;
	if (kfiber_net_getaddr(host.c_str(), &addr) != 0) {
		assert(addr == NULL);
		health(NULL, HealthStatus::Err);
		return NULL;
	}
	int tproxy_mask = 0;
	katom_inc64((void*)&total_connect);
	kconnection* cn = kfiber_net_open2(addr->addr, port);
	kgl_addr_release(addr);
	if (kfiber_net_connect(cn, bind_addr, tproxy_mask) != 0) {
		kfiber_net_close(cn);
		health(NULL, HealthStatus::Err);
		return NULL;
	}
#ifdef ENABLE_UPSTREAM_SSL
	if (this->ssl_ctx) {
		if (this->no_sni) {
			sni_host = NULL;
		}
		if (!kconnection_ssl_connect(cn, ssl_ctx, sni_host)) {
			klog(KLOG_ERR, "cann't bind_fd for ssl socket.\n");
			kfiber_net_close(cn);
			return NULL;
		}
#ifdef ENABLE_UPSTREAM_HTTP2
#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
		if (alpn > 0 && cn->st.ssl && KBIT_TEST(flags, KSOCKET_FLAGS_WEBSOCKET)) {
			//websocket will turn off http2
			SSL_set_alpn_protos(cn->st.ssl->ssl, (unsigned char*)KGL_HTTP_NPN_ADVERTISE, sizeof(KGL_HTTP_NPN_ADVERTISE) - 1);
		}
#endif
#endif
		if (kfiber_ssl_handshake(cn) != 0) {
			kfiber_net_close(cn);
			health(NULL, HealthStatus::Err);
			return NULL;
		}
		if (need_verify()) {
			if (X509_V_OK != SSL_get_verify_result(cn->st.ssl->ssl)) {
				klog(KLOG_ERR, "cann't verify [%s:%d] ssl certificate.\n", host.c_str(), port);
				kfiber_net_close(cn);
				health(NULL, HealthStatus::Err);
				return NULL;
			}
		}
#ifdef ENABLE_UPSTREAM_HTTP2
		const unsigned char* protocol_data = NULL;
		unsigned len = 0;
		kgl_ssl_get_next_proto_negotiated(cn->st.ssl->ssl, &protocol_data, &len);
		if (len == sizeof(KGL_HTTP_V2_NPN_NEGOTIATED) - 1 &&
			memcmp(protocol_data, KGL_HTTP_V2_NPN_NEGOTIATED, len) == 0) {
			KHttp2* http2 = new KHttp2();
			selectable_bind_opaque(&cn->st, http2);
			KHttp2Upstream* http2_us = http2->client(cn);
			bind(http2_us);
			return http2_us;
		}
#endif
	}
#endif
	socket = new_upstream(cn);
	bind(socket);
	return socket;
}
bool KSockPoolHelper::setHostPort(std::string host, int port, const char* ssl) {
	bool destChanged = false;
	lock.Lock();
	if (this->host != host || this->port != port) {
		destChanged = true;
	}
	this->host = host;
	this->port = port;
#ifdef ENABLE_UPSTREAM_SSL
	char* ssl_buf = NULL;
	char* protocols = NULL;
	char* chiper = NULL;
	if (ssl) {
		this->ssl = ssl;
		ssl_buf = strdup(ssl);
		protocols = strchr(ssl_buf, '/');
		if (protocols) {
			*protocols = '\0';
			protocols++;
			chiper = strchr(protocols, '/');
			if (chiper) {
				*chiper = '\0';
				chiper++;
			}
		}
	} else {
		this->ssl.clear();
	}
#ifdef ENABLE_UPSTREAM_HTTP2
	this->alpn = KGL_ALPN_HTTP1;
	if (ssl_buf && strchr(ssl_buf, 'p')) {
		this->alpn = KGL_ALPN_HTTP2;
	}
#endif
	if (ssl_buf && strchr(ssl_buf, 'n')) {
		this->no_sni = 1;
	}
	if (ssl_ctx) {
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
	}
	if (ssl) {
		void* ssl_ctx_data = NULL;
#ifdef ENABLE_UPSTREAM_HTTP2
		ssl_ctx_data = &alpn;
#endif
		kgl_refs_string* ssl_client_chiper;
		kgl_refs_string* ssl_client_protocols;
		kgl_refs_string* ca_path;
		khttp_server_refs_ssl_config(&ca_path, &ssl_client_chiper, &ssl_client_protocols);
		ssl_ctx = kgl_ssl_ctx_new_client(ca_path ? ca_path->data : NULL, NULL, ssl_ctx_data);
		if (ssl_ctx) {
			if (need_verify()) {
				SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, kgl_ssl_verify_callback);
				if (!ca_path) {
					SSL_CTX_set_default_verify_paths(ssl_ctx);
				}
			}
			kgl_ssl_ctx_set_protocols(ssl_ctx, protocols);
			if (chiper) {
				kgl_ssl_ctx_set_cipher_list(ssl_ctx, chiper);
			}
			if (chiper == NULL || protocols == NULL) {
				if (chiper == NULL && ssl_client_chiper) {
					SSL_CTX_set_cipher_list(ssl_ctx, ssl_client_chiper->data);
				}
				if (protocols == NULL && ssl_client_protocols) {
					kgl_ssl_ctx_set_protocols(ssl_ctx, ssl_client_protocols->data);
				}
			}
		}
		kstring_release(ca_path);
		kstring_release(ssl_client_chiper);
		kstring_release(ssl_client_protocols);
	}
#endif
	if (destChanged) {
		//clean();
	}
#ifdef KSOCKET_UNIX
	if (strncasecmp(this->host.c_str(), "unix:", 5) == 0) {
		is_unix = 1;
		this->host = this->host.substr(5);
	}
	if (this->host[0] == '/') {
		is_unix = 1;
	}
#endif
	lock.Unlock();
#ifdef ENABLE_UPSTREAM_SSL
	if (ssl_buf) {
		free(ssl_buf);
	}
#endif
	return true;
}
bool KSockPoolHelper::setHostPort(std::string host, const char* port) {
	const char* ssl = port;
	while (*ssl && IS_DIGIT(*ssl)) {
		ssl++;
	}
	if (*ssl != 's' && *ssl != 'S') {
		ssl = NULL;
	}
	return setHostPort(host, atoi(port), ssl);
}
void KSockPoolHelper::disable() {
	disable_flag = 1;
	
}
bool KSockPoolHelper::is_enabled() {
	if (disable_flag) {
		return false;
	}
	return true;
}
void KSockPoolHelper::enable() {
	disable_flag = 0;
	katom_set16((void*)&error_count, 0);
}

bool KSockPoolHelper::parse(std::map<std::string, std::string>& attr) {
	setHostPort(attr["host"], attr["port"].c_str());
	setLifeTime(atoi(attr["life_time"].c_str()));
	SetParam(attr["param"].c_str());
#ifdef HTTP_PROXY
	auth_user = attr["auth_user"];
	auth_passwd = attr["auth_passwd"];
#endif
	setIp(attr["self_ip"].c_str());
	sign = (attr["sign"] == "1");
	return true;
}
void KSockPoolHelper::build(std::map<std::string, std::string>& attr) {
	attr["host"] = host;
	std::stringstream s;
	s << (int)port;
#ifdef ENABLE_UPSTREAM_SSL
	if (!ssl.empty()) {
		s << ssl;
	}
#endif
	attr["port"] = s.str();
	attr["life_time"] = std::to_string(getLifeTime());
	kgl_refs_string* str = GetParam();
	if (str) {
		attr["param"] = str->data;
	}
	kstring_release(str);
#ifdef HTTP_PROXY
	if (auth_user.size() > 0) {
		attr["auth_user"] = auth_user;
	}
	if (auth_passwd.size() > 0) {
		attr["auth_passwd"] = auth_passwd;
	}
#endif
	if (ip && *ip) {
		attr["self_ip"] = ip;
	}
	if (sign) {
		attr["sign"] = "1";
	}
}