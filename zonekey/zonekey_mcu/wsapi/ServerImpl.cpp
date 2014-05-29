#include "soapStub.h"
#include "zkmcu.nsmap"

int zkmcu__getVersion(soap *soap, void *, char **info)
{
	*info = soap_strdup(soap, "");
	return SOAP_OK;
}

int zkmcu__getSysInfo(soap *soap, void *, zkmcu__SysInfoResponse &res)
{
	return SOAP_OK;
}

int zkmcu__createConference(soap *soap, struct zkmcu__CreateConference *req, struct zkmcu__CreateConferenceResponse &res)
{
	return SOAP_OK;
}

int zkmcu__destroyConference(struct soap*, int cid)
{
	return SOAP_OK;
}

int zkmcu__listConferences(struct soap*, void *_param_4, struct zkmcu__ConferenceIdArray &res)
{
	return SOAP_OK;
}

int zkmcu__infoConference(struct soap*, int cid, struct zkmcu__ConferenceInfo &res)
{
	return SOAP_OK;
}

int zkmcu__addSource(struct soap*, int cid, struct zkmcu__AddSourceRequest *req, struct zkmcu__AddSourceResponse &res)
{
	return SOAP_OK;
}

int zkmcu__delSource(struct soap*, int cid, int sourceid)
{
	return SOAP_OK;
}

int zkmcu__addSink(struct soap*, int cid, struct zkmcu__AddSinkRequest *req, struct zkmcu__AddSinkResponse &res)
{
	return SOAP_OK;
}

int zkmcu__delSink(struct soap*, int cid, int sinkid)
{
	return SOAP_OK;
}

int zkmcu__addStream(struct soap*, int cid, struct zkmcu__AddStreamRequest *req, struct zkmcu__AddStreamResponse &res)
{
	return SOAP_OK;
}

int zkmcu__delStream(struct soap*, int cid, int sourceid)
{
	return SOAP_OK;
}

int zkmcu__setParams(struct soap*, int cid, struct zkmcu__KeyValueArray *req)
{
	return SOAP_OK;
}

int zkmcu__getParams(struct soap*, int cid, struct zkmcu__KeyValueArray &res)
{
	return SOAP_OK;
}
