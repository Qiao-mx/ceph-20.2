/*
 * NB:  This file is machine generated, DO NOT EDIT!
 *
 * Edit and run lib/libvcc/generate.py instead.
 */


VCL_STRING VRT_r_client_identity(VRT_CTX);
void VRT_l_client_identity(VRT_CTX, const char *, VCL_STRANDS);

VCL_IP VRT_r_client_ip(VRT_CTX);

VCL_STRING VRT_r_server_hostname(VRT_CTX);

VCL_STRING VRT_r_server_identity(VRT_CTX);

VCL_IP VRT_r_server_ip(VRT_CTX);

VCL_IP VRT_r_remote_ip(VRT_CTX);

VCL_STRING VRT_r_local_endpoint(VRT_CTX);

VCL_IP VRT_r_local_ip(VRT_CTX);

VCL_STRING VRT_r_local_socket(VRT_CTX);

VCL_HTTP VRT_r_req(VRT_CTX);

VCL_BACKEND VRT_r_req_backend_hint(VRT_CTX);
void VRT_l_req_backend_hint(VRT_CTX, VCL_BACKEND);

VCL_BOOL VRT_r_req_can_gzip(VRT_CTX);

VCL_BOOL VRT_r_req_esi(VRT_CTX);
void VRT_l_req_esi(VRT_CTX, VCL_BOOL);

VCL_INT VRT_r_req_esi_level(VRT_CTX);

VCL_STRING VRT_r_req_filters(VRT_CTX);
void VRT_l_req_filters(VRT_CTX, const char *, VCL_STRANDS);

VCL_DURATION VRT_r_req_grace(VRT_CTX);
void VRT_l_req_grace(VRT_CTX, VCL_DURATION);
void VRT_u_req_grace(VRT_CTX);

VCL_BLOB VRT_r_req_hash(VRT_CTX);

VCL_BOOL VRT_r_req_hash_always_miss(VRT_CTX);
void VRT_l_req_hash_always_miss(VRT_CTX, VCL_BOOL);

VCL_BOOL VRT_r_req_hash_ignore_busy(VRT_CTX);
void VRT_l_req_hash_ignore_busy(VRT_CTX, VCL_BOOL);

VCL_BOOL VRT_r_req_hash_ignore_vary(VRT_CTX);
void VRT_l_req_hash_ignore_vary(VRT_CTX, VCL_BOOL);

void VRT_u_req_http(VRT_CTX);



VCL_BOOL VRT_r_req_is_hitmiss(VRT_CTX);

VCL_BOOL VRT_r_req_is_hitpass(VRT_CTX);

VCL_STRING VRT_r_req_method(VRT_CTX);
void VRT_l_req_method(VRT_CTX, const char *, VCL_STRANDS);

VCL_STRING VRT_r_req_proto(VRT_CTX);
void VRT_l_req_proto(VRT_CTX, const char *, VCL_STRANDS);


VCL_INT VRT_r_req_restarts(VRT_CTX);

VCL_STEVEDORE VRT_r_req_storage(VRT_CTX);
void VRT_l_req_storage(VRT_CTX, VCL_STEVEDORE);

VCL_TIME VRT_r_req_time(VRT_CTX);

VCL_BOOL VRT_r_req_trace(VRT_CTX);
void VRT_l_req_trace(VRT_CTX, VCL_BOOL);

VCL_STRING VRT_r_req_transport(VRT_CTX);

VCL_DURATION VRT_r_req_max_age(VRT_CTX);
void VRT_l_req_max_age(VRT_CTX, VCL_DURATION);
void VRT_u_req_max_age(VRT_CTX);

VCL_DURATION VRT_r_req_ttl(VRT_CTX);
void VRT_l_req_ttl(VRT_CTX, VCL_DURATION);
void VRT_u_req_ttl(VRT_CTX);

VCL_STRING VRT_r_req_url(VRT_CTX);
void VRT_l_req_url(VRT_CTX, const char *, VCL_STRANDS);

VCL_INT VRT_r_req_xid(VRT_CTX);


VCL_STRING VRT_r_req_top_method(VRT_CTX);

VCL_STRING VRT_r_req_top_proto(VRT_CTX);

VCL_TIME VRT_r_req_top_time(VRT_CTX);

VCL_STRING VRT_r_req_top_url(VRT_CTX);

VCL_HTTP VRT_r_bereq(VRT_CTX);

VCL_BACKEND VRT_r_bereq_backend(VRT_CTX);
void VRT_l_bereq_backend(VRT_CTX, VCL_BACKEND);

VCL_DURATION VRT_r_bereq_between_bytes_timeout(VRT_CTX);
void VRT_l_bereq_between_bytes_timeout(VRT_CTX, VCL_DURATION);
void VRT_u_bereq_between_bytes_timeout(VRT_CTX);

void VRT_u_bereq_body(VRT_CTX);

VCL_DURATION VRT_r_bereq_connect_timeout(VRT_CTX);
void VRT_l_bereq_connect_timeout(VRT_CTX, VCL_DURATION);
void VRT_u_bereq_connect_timeout(VRT_CTX);

VCL_STRING VRT_r_bereq_filters(VRT_CTX);
void VRT_l_bereq_filters(VRT_CTX, const char *, VCL_STRANDS);

VCL_DURATION VRT_r_bereq_first_byte_timeout(VRT_CTX);
void VRT_l_bereq_first_byte_timeout(VRT_CTX, VCL_DURATION);
void VRT_u_bereq_first_byte_timeout(VRT_CTX);

VCL_BLOB VRT_r_bereq_hash(VRT_CTX);

void VRT_u_bereq_http(VRT_CTX);



VCL_BOOL VRT_r_bereq_is_bgfetch(VRT_CTX);

VCL_BOOL VRT_r_bereq_is_hitmiss(VRT_CTX);

VCL_BOOL VRT_r_bereq_is_hitpass(VRT_CTX);

VCL_STRING VRT_r_bereq_method(VRT_CTX);
void VRT_l_bereq_method(VRT_CTX, const char *, VCL_STRANDS);

VCL_STRING VRT_r_bereq_proto(VRT_CTX);
void VRT_l_bereq_proto(VRT_CTX, const char *, VCL_STRANDS);


VCL_INT VRT_r_bereq_retries(VRT_CTX);

VCL_BOOL VRT_r_bereq_retry_connect(VRT_CTX);
void VRT_l_bereq_retry_connect(VRT_CTX, VCL_BOOL);

VCL_DURATION VRT_r_bereq_task_deadline(VRT_CTX);
void VRT_l_bereq_task_deadline(VRT_CTX, VCL_DURATION);
void VRT_u_bereq_task_deadline(VRT_CTX);

VCL_TIME VRT_r_bereq_time(VRT_CTX);

VCL_BOOL VRT_r_bereq_trace(VRT_CTX);
void VRT_l_bereq_trace(VRT_CTX, VCL_BOOL);

VCL_BOOL VRT_r_bereq_uncacheable(VRT_CTX);

VCL_STRING VRT_r_bereq_url(VRT_CTX);
void VRT_l_bereq_url(VRT_CTX, const char *, VCL_STRANDS);

VCL_INT VRT_r_bereq_xid(VRT_CTX);

VCL_HTTP VRT_r_beresp(VRT_CTX);

VCL_DURATION VRT_r_beresp_age(VRT_CTX);

VCL_BACKEND VRT_r_beresp_backend(VRT_CTX);

VCL_IP VRT_r_beresp_backend_ip(VRT_CTX);

VCL_STRING VRT_r_beresp_backend_name(VRT_CTX);

void VRT_l_beresp_body(VRT_CTX, enum lbody_e, const char *, const void *);

VCL_BOOL VRT_r_beresp_do_esi(VRT_CTX);
void VRT_l_beresp_do_esi(VRT_CTX, VCL_BOOL);

VCL_BOOL VRT_r_beresp_do_gunzip(VRT_CTX);
void VRT_l_beresp_do_gunzip(VRT_CTX, VCL_BOOL);

VCL_BOOL VRT_r_beresp_do_gzip(VRT_CTX);
void VRT_l_beresp_do_gzip(VRT_CTX, VCL_BOOL);

VCL_BOOL VRT_r_beresp_do_stream(VRT_CTX);
void VRT_l_beresp_do_stream(VRT_CTX, VCL_BOOL);

VCL_STRING VRT_r_beresp_filters(VRT_CTX);
void VRT_l_beresp_filters(VRT_CTX, const char *, VCL_STRANDS);

VCL_DURATION VRT_r_beresp_grace(VRT_CTX);
void VRT_l_beresp_grace(VRT_CTX, VCL_DURATION);

void VRT_u_beresp_http(VRT_CTX);



VCL_DURATION VRT_r_beresp_keep(VRT_CTX);
void VRT_l_beresp_keep(VRT_CTX, VCL_DURATION);

VCL_STRING VRT_r_beresp_proto(VRT_CTX);
void VRT_l_beresp_proto(VRT_CTX, const char *, VCL_STRANDS);


VCL_STRING VRT_r_beresp_reason(VRT_CTX);
void VRT_l_beresp_reason(VRT_CTX, const char *, VCL_STRANDS);

VCL_INT VRT_r_beresp_status(VRT_CTX);
void VRT_l_beresp_status(VRT_CTX, VCL_INT);

VCL_STEVEDORE VRT_r_beresp_storage(VRT_CTX);
void VRT_l_beresp_storage(VRT_CTX, VCL_STEVEDORE);

VCL_TIME VRT_r_beresp_time(VRT_CTX);

VCL_BYTES VRT_r_beresp_transit_buffer(VRT_CTX);
void VRT_l_beresp_transit_buffer(VRT_CTX, VCL_BYTES);

VCL_DURATION VRT_r_beresp_ttl(VRT_CTX);
void VRT_l_beresp_ttl(VRT_CTX, VCL_DURATION);

VCL_BOOL VRT_r_beresp_uncacheable(VRT_CTX);
void VRT_l_beresp_uncacheable(VRT_CTX, VCL_BOOL);

VCL_BOOL VRT_r_beresp_was_304(VRT_CTX);

VCL_DURATION VRT_r_obj_stale_age(VRT_CTX);

VCL_BOOL VRT_r_obj_stale_can_esi(VRT_CTX);

VCL_DURATION VRT_r_obj_stale_grace(VRT_CTX);

VCL_INT VRT_r_obj_stale_hits(VRT_CTX);


VCL_DURATION VRT_r_obj_stale_keep(VRT_CTX);

VCL_STRING VRT_r_obj_stale_proto(VRT_CTX);

VCL_STRING VRT_r_obj_stale_reason(VRT_CTX);

VCL_INT VRT_r_obj_stale_status(VRT_CTX);

VCL_STEVEDORE VRT_r_obj_stale_storage(VRT_CTX);

VCL_TIME VRT_r_obj_stale_time(VRT_CTX);

VCL_DURATION VRT_r_obj_stale_ttl(VRT_CTX);

VCL_BOOL VRT_r_obj_stale_uncacheable(VRT_CTX);

VCL_BOOL VRT_r_obj_stale_is_valid(VRT_CTX);

VCL_DURATION VRT_r_obj_age(VRT_CTX);

VCL_BOOL VRT_r_obj_can_esi(VRT_CTX);

VCL_DURATION VRT_r_obj_grace(VRT_CTX);

VCL_INT VRT_r_obj_hits(VRT_CTX);


VCL_DURATION VRT_r_obj_keep(VRT_CTX);

VCL_STRING VRT_r_obj_proto(VRT_CTX);

VCL_STRING VRT_r_obj_reason(VRT_CTX);

VCL_INT VRT_r_obj_status(VRT_CTX);

VCL_STEVEDORE VRT_r_obj_storage(VRT_CTX);

VCL_TIME VRT_r_obj_time(VRT_CTX);

VCL_DURATION VRT_r_obj_ttl(VRT_CTX);

VCL_BOOL VRT_r_obj_uncacheable(VRT_CTX);

VCL_HTTP VRT_r_resp(VRT_CTX);

void VRT_l_resp_body(VRT_CTX, enum lbody_e, const char *, const void *);

VCL_BOOL VRT_r_resp_do_esi(VRT_CTX);
void VRT_l_resp_do_esi(VRT_CTX, VCL_BOOL);

VCL_STRING VRT_r_resp_filters(VRT_CTX);
void VRT_l_resp_filters(VRT_CTX, const char *, VCL_STRANDS);

void VRT_u_resp_http(VRT_CTX);



VCL_BOOL VRT_r_resp_is_streaming(VRT_CTX);

VCL_STRING VRT_r_resp_proto(VRT_CTX);
void VRT_l_resp_proto(VRT_CTX, const char *, VCL_STRANDS);


VCL_STRING VRT_r_resp_reason(VRT_CTX);
void VRT_l_resp_reason(VRT_CTX, const char *, VCL_STRANDS);

VCL_INT VRT_r_resp_status(VRT_CTX);
void VRT_l_resp_status(VRT_CTX, VCL_INT);

VCL_TIME VRT_r_resp_time(VRT_CTX);

VCL_TIME VRT_r_now(VRT_CTX);

VCL_DURATION VRT_r_sess_idle_send_timeout(VRT_CTX);
void VRT_l_sess_idle_send_timeout(VRT_CTX, VCL_DURATION);
void VRT_u_sess_idle_send_timeout(VRT_CTX);

VCL_DURATION VRT_r_sess_send_timeout(VRT_CTX);
void VRT_l_sess_send_timeout(VRT_CTX, VCL_DURATION);
void VRT_u_sess_send_timeout(VRT_CTX);

VCL_DURATION VRT_r_sess_timeout_idle(VRT_CTX);
void VRT_l_sess_timeout_idle(VRT_CTX, VCL_DURATION);
void VRT_u_sess_timeout_idle(VRT_CTX);

VCL_DURATION VRT_r_sess_timeout_linger(VRT_CTX);
void VRT_l_sess_timeout_linger(VRT_CTX, VCL_DURATION);
void VRT_u_sess_timeout_linger(VRT_CTX);

VCL_INT VRT_r_sess_xid(VRT_CTX);

VCL_DURATION VRT_r_param_backend_idle_timeout(VRT_CTX);

VCL_INT VRT_r_param_backend_wait_limit(VRT_CTX);

VCL_DURATION VRT_r_param_backend_wait_timeout(VRT_CTX);

VCL_DURATION VRT_r_param_between_bytes_timeout(VRT_CTX);

VCL_DURATION VRT_r_param_connect_timeout(VRT_CTX);

VCL_DURATION VRT_r_param_default_grace(VRT_CTX);

VCL_DURATION VRT_r_param_default_keep(VRT_CTX);

VCL_DURATION VRT_r_param_default_ttl(VRT_CTX);

VCL_DURATION VRT_r_param_first_byte_timeout(VRT_CTX);

VCL_DURATION VRT_r_param_idle_send_timeout(VRT_CTX);

VCL_INT VRT_r_param_max_esi_depth(VRT_CTX);

VCL_INT VRT_r_param_max_restarts(VRT_CTX);

VCL_INT VRT_r_param_max_retries(VRT_CTX);

VCL_DURATION VRT_r_param_pipe_task_deadline(VRT_CTX);

VCL_DURATION VRT_r_param_pipe_timeout(VRT_CTX);

VCL_DURATION VRT_r_param_send_timeout(VRT_CTX);

VCL_DURATION VRT_r_param_shortlived(VRT_CTX);

VCL_DURATION VRT_r_param_timeout_idle(VRT_CTX);

VCL_BYTES VRT_r_param_transit_buffer(VRT_CTX);

VCL_DURATION VRT_r_param_uncacheable_ttl(VRT_CTX);
VCL_BYTES VRT_stevedore_free_space(VCL_STEVEDORE);
VCL_BYTES VRT_stevedore_used_space(VCL_STEVEDORE);
VCL_BOOL VRT_stevedore_happy(VCL_STEVEDORE);
