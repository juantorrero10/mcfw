#ifndef PUBLIC_QUERY_H_
#define PUBLIC_QUERY_H_

#define QUERY(schema_type) .query_schema_typename = #schema_type

typedef struct __MCFW_HTTP_REQUEST__ HttpRequest;

void* query_params_get(const HttpRequest* req);

#endif // PUBLIC_QUERY_H_