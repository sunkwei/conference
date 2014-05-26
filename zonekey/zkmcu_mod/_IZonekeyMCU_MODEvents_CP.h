#pragma once

template<class T>
class CProxy_IZonekeyMCU_MODEvents :
	public ATL::IConnectionPointImpl<T, &__uuidof(_IZonekeyMCU_MODEvents)>
{
public:
	HRESULT Fire_CBMediaFrameGot(LONG payload, DOUBLE stamp, VARIANT data, BOOL key)
	{
		HRESULT hr = S_OK;
		T * pThis = static_cast<T *>(this);
		int cConnections = m_vec.GetSize();

		for (int iConnection = 0; iConnection < cConnections; iConnection++)
		{
			pThis->Lock();
			CComPtr<IUnknown> punkConnection = m_vec.GetAt(iConnection);
			pThis->Unlock();

			IDispatch * pConnection = static_cast<IDispatch *>(punkConnection.p);

			if (pConnection)
			{
				CComVariant avarParams[4];
				avarParams[3] = payload;
				avarParams[3].vt = VT_I4;
				avarParams[2] = stamp;
				avarParams[2].vt = VT_R8;
				avarParams[1] = data;
				avarParams[0].vt = VT_BOOL;
				avarParams[0] = key;
				DISPPARAMS params = { avarParams, NULL, 4, 0 };
				hr = pConnection->Invoke(1, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, NULL, NULL, NULL);
			}
		}
		return hr;
	}

	HRESULT Fire_CBError(LONG code, BSTR info)
	{
		HRESULT hr = S_OK;
		T *pThis = static_cast<T*>(this);
		int cConnections = m_vec.GetSize();

		for (int iConnection = 0; iConnection < cConnections; iConnection++) {
			pThis->Lock();
			CComPtr<IUnknown> punkConnection = m_vec.GetAt(iConnection);
			pThis->Unlock();

			IDispatch *pConnection = static_cast<IDispatch*>(punkConnection.p);

			if (pConnection) {
				CComVariant var[2];
				var[1] = code;
				var[1].vt = VT_I4;
				var[0] = info;
				var[0].vt = VT_BSTR;
				DISPPARAMS params = { var, NULL, 2, 0 };
				hr = pConnection->Invoke(2, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, NULL, NULL, NULL);
			}
		}

		return hr;
	}
};

